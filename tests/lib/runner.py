"""Runs one UI test.

Two phases, the same file twice. The outer phase builds the sandbox -- temp
root, X server, session bus -- and re-runs itself inside it; the inner phase is
the one that starts medit and drives it. The split exists because the session
bus comes from dbus-run-session, which replaces the process it is given.

    runner.py --test tests/app/about_dialog/test.py \\
              --binary build3/src/medit --gtk 3 --log-dir build3/ui-tests/about

The exit code is the test result. Everything the run produced -- medit's output,
the X server's, the sanitizer logs, sanitizer.json, a screenshot if it failed --
is left in the log directory whether it passed or not.
"""

import argparse
import collections
import glob
import importlib.util
import os
import re
import subprocess
import sys
import time
import traceback


# Importing lib.* would otherwise leave a __pycache__ in the source tree, and
# the source tree is not where a test run should write anything.
sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
TESTS = os.path.dirname(HERE)

if TESTS not in sys.path:
    sys.path.insert(0, TESTS)

from lib import browser              # noqa: E402
from lib import sandbox              # noqa: E402
from lib import sanitizer            # noqa: E402


# medit exits in about 0.15s when it crashes, and prints nothing when it does,
# so "no criticals on stderr" is not evidence that it ran. The exit code is.
QUIT_TIMEOUT = 20

CRITICAL_MARKERS = ("CRITICAL", "assertion failed", "Segmentation fault")
WARNING_MARKERS = ("WARNING",)


def parse_args(argv):
    p = argparse.ArgumentParser()
    p.add_argument("--test", required=True, help="path to the test.py to run")
    p.add_argument("--binary", required=True, help="the medit to drive")
    p.add_argument("--gtk", required=True, help="toolkit the binary was built with")
    p.add_argument("--log-dir", required=True, help="where to leave the evidence")
    p.add_argument("--sanitizers", default="", help="what -fsanitize= was built with")
    p.add_argument("--suppressions", default="", help="LSan suppression file")
    p.add_argument("--tmp-root", default=sandbox.TMP_ROOT,
                   help="where the sandbox root is created; must be a short path")
    p.add_argument("--screen", default="1400x900x24")
    p.add_argument("--timeout", type=int, default=300)
    p.add_argument("--keep", action="store_true", help="do not remove the sandbox root")
    p.add_argument("--inner", action="store_true", help=argparse.SUPPRESS)
    return p.parse_args(argv)


# ---------------------------------------------------------------------------
# Outer phase: the sandbox
# ---------------------------------------------------------------------------

def clean_log_dir(path):
    os.makedirs(path, exist_ok=True)
    for pattern in ("asan.*", "ubsan.*", "*.log", "*.json", "*.png"):
        for stale in glob.glob(os.path.join(path, pattern)):
            os.unlink(stale)


def x_server_state(xvfb, display):
    """Whether the display the test was given is still there.

    Printed after a failure, because "cannot open display" in medit.log leaves
    two very different possibilities open: the server died under the test, or it
    was never the server the number pointed at.
    """
    alive = xvfb is not None and xvfb.poll() is None

    return "    the X server on %s: %s, and it %s answer now" % (
        display,
        "still running" if alive else "gone",
        "does" if sandbox.display_answers(display, timeout=1) else "does not")


def outer(args):
    log_dir = os.path.abspath(args.log_dir)
    clean_log_dir(log_dir)

    root, dirs = sandbox.make_root(args.tmp_root)
    xvfb = None

    try:
        url_log = browser.install(root, dirs["XDG_DATA_HOME"], dirs["XDG_CONFIG_HOME"])
        xvfb, display = sandbox.start_x(root, os.path.join(log_dir, "xvfb.log"), args.screen)

        env = dict(os.environ)
        env.update(dirs)
        env["DISPLAY"] = display
        env["MUI_ROOT"] = root
        env["MUI_URL_LOG"] = url_log

        # GTK+2 has no accessibility of its own: the tree comes from libgail,
        # and the bridge to the at-spi bus from atk-bridge. Naming both is
        # harmless on GTK+3, which loads what it needs itself.
        env["GTK_MODULES"] = "gail:atk-bridge"
        env["NO_AT_BRIDGE"] = "0"
        env["GTK_A11Y"] = "atspi"

        # Tests match on the names of widgets, so the language has to be pinned.
        # Otherwise the same test passes on a machine in English and fails on
        # the developer's in Russian.
        env["LC_ALL"] = "C.UTF-8"
        env["LANG"] = "C.UTF-8"
        env["LANGUAGE"] = ""

        # Warns when a deprecated GObject property or signal is used -- a class
        # of deprecation the compiler cannot see, because it is named by string.
        env["G_ENABLE_DIAGNOSTIC"] = "1"

        # Always, not only when the build is known to be sanitized. The runtime
        # reads these variables if it is there and ignores them if it is not,
        # and leaving them out of a sanitized run is worse than useless: the
        # default is detect_leaks=1, which reports fontconfig's caches as leaks
        # and makes medit exit non-zero, so the test fails for a reason that has
        # nothing to do with what it tested.
        env.update(sanitizer.options(
            log_dir,
            suppressions=args.suppressions or None,
            leak_check=os.environ.get("UI_TEST_LEAK_CHECK") == "1"))

        inner = [sys.executable, os.path.abspath(__file__), "--inner"] + [
            "--test", os.path.abspath(args.test),
            "--binary", os.path.abspath(args.binary),
            "--gtk", args.gtk,
            "--log-dir", log_dir,
            "--sanitizers", args.sanitizers,
        ]

        try:
            done = subprocess.run(["dbus-run-session", "--"] + inner,
                                  env=env, timeout=args.timeout)

            if done.returncode != 0:
                print(x_server_state(xvfb, display))

            return done.returncode
        except subprocess.TimeoutExpired:
            print("FAIL: the test did not finish within %ds" % args.timeout)
            return 1

    finally:
        sandbox.stop(xvfb)
        if args.keep:
            print("sandbox kept at %s" % root)
        else:
            sandbox.remove_root(root)


# ---------------------------------------------------------------------------
# Inner phase: the application
# ---------------------------------------------------------------------------

def load_test(path):
    spec = importlib.util.spec_from_file_location("mui_test", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    if not hasattr(module, "run"):
        raise AssertionError("%s defines no run(t)" % path)

    return module


# What gtk prints when gdk_display_open() returns nothing.
NO_DISPLAY = "cannot open display"


def start_medit(binary, log_dir, files=(), attempts=3, settle=2.0):
    """Start medit, and start it again if it could not open the display.

    Not a retry of anything else: only this one failure, and only when medit
    said that is why it went. It happens perhaps once in five runs of the whole
    suite in parallel, never once on its own -- forty starts in a row on one
    display, none of them refused -- and both this process and the outer one
    have asked the display and been answered, before and after, so the server
    is there and it is the connection to it that fails.

    A medit that crashes at startup exits with a different message, is not
    retried, and its log is the same log this appends to.
    """
    path = os.path.join(log_dir, "medit.log")

    # --new-app is not optional: medit is single instance, and without it a
    # second copy hands its arguments to the first and exits immediately.
    argv = [binary, "--new-app"] + list(files)

    for attempt in range(attempts):
        # Where this attempt's output starts, so that the attempt is judged by
        # what it said and not by what the one before it said.
        written = os.path.getsize(path) if os.path.exists(path) else 0

        log = open(path, "ab")
        proc = subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT)
        log.close()

        time.sleep(settle)

        if proc.poll() is None or not refused_the_display(path, written):
            return proc

        print("    medit could not open %s and exited with %s; starting it again "
              "(attempt %d of %d)"
              % (os.environ.get("DISPLAY"), proc.returncode, attempt + 1, attempts))

    return proc


def refused_the_display(path, since=0):
    try:
        with open(path, errors="replace") as f:
            f.seek(since)
            return NO_DISPLAY in f.read()
    except (FileNotFoundError, OSError):
        return False


def quit_medit(t, proc):
    """Quit through the UI and wait for the exit code.

    Not a signal: a sanitizer only reports at exit, and a killed process has no
    ordinary exit to report at.
    """
    try:
        t.menu("File", "Quit")
    except Exception as error:
        print("    could not quit through the menu: %s" % error)

    deadline = time.time() + QUIT_TIMEOUT
    while time.time() < deadline:
        if proc.poll() is not None:
            return proc.returncode, True
        time.sleep(0.2)

    print("    medit did not quit within %ds, terminating it" % QUIT_TIMEOUT)
    sandbox.stop(proc)
    return proc.returncode, False


def last_words(log_dir, was_alive, code, lines=10):
    """What medit was saying when the test gave up on it.

    A test that fails because the program is not there -- it died at startup,
    or it never reached the accessibility bus -- says nothing about why on its
    own, and the reason is usually the last line medit printed.
    """
    path = os.path.join(log_dir, "medit.log")
    state = "medit was still running" if was_alive else "medit had exited with %s" % code

    try:
        with open(path, errors="replace") as f:
            tail = [line.rstrip() for line in f if line.strip()][-lines:]
    except FileNotFoundError:
        tail = []

    if not tail:
        return "    %s, and printed nothing" % state

    return "    %s, and its last words were:\n%s" % (
        state, "\n".join("      " + line for line in tail))


# What G_ENABLE_DIAGNOSTIC prints: "The property GtkFoo:bar is deprecated and
# shouldn't be used anymore", and the same sentence for a signal.
DEPRECATED = re.compile(r"The (?:property|signal) ([\w.:-]+) is deprecated")


def scan_log(log_dir):
    """Count what glib printed, and name the deprecations. Reported, not a verdict.

    Named, because a count alone cannot be acted on: the number moves with the
    toolkit's version as much as with medit's code, and the only question worth
    asking of it -- which of these are ours -- needs the names.
    """
    path = os.path.join(log_dir, "medit.log")
    counts = {"criticals": 0, "warnings": 0, "deprecated": 0}
    deprecated = collections.Counter()

    try:
        with open(path, errors="replace") as f:
            for line in f:
                if any(marker in line for marker in CRITICAL_MARKERS):
                    counts["criticals"] += 1
                elif "is deprecated" in line:
                    counts["deprecated"] += 1
                    found = DEPRECATED.search(line)
                    if found:
                        deprecated[found.group(1)] += 1
                elif any(marker in line for marker in WARNING_MARKERS):
                    counts["warnings"] += 1
    except FileNotFoundError:
        pass

    counts["names"] = deprecated

    return counts


def prepare(module, log_dir):
    """Run the test's setup function, if it has one, before medit starts."""
    from lib.setup import Setup

    sandbox = Setup(os.environ["MUI_ROOT"], os.environ["XDG_DATA_HOME"], log_dir)

    if hasattr(module, "setup"):
        module.setup(sandbox)
        written = sandbox.commit()
        if written:
            print("    settings written to %s" % written)

    return sandbox


def inner(args):
    from lib import a11y
    from lib.context import Test

    log_dir = args.log_dir

    # The display, from in here, before anything is started on it. The outer
    # phase checked it too, but in its own environment; if the two disagree the
    # answer is that environment, and that is worth one xdotool call to know.
    if not sandbox.display_answers(os.environ.get("DISPLAY", ""), timeout=10):
        print("FAIL: the display %s does not answer inside the test's environment"
              % os.environ.get("DISPLAY"))
        return 1

    # Loaded before medit starts, not after: a test may have a setup function,
    # and what it puts in place has to be there when medit reads its settings.
    module = load_test(args.test)
    prepared = prepare(module, log_dir)

    proc = start_medit(args.binary, log_dir, prepared.files)

    failure = None
    clean_exit = False
    code = None
    alive_at_failure = None

    try:
        app = a11y.application("medit", timeout=60)
        t = Test(app, args.gtk, os.environ["MUI_URL_LOG"], log_dir, sys.stdout,
                 sandbox=prepared)

        module.run(t)

    except BaseException as error:          # noqa: BLE001 -- the report is the point
        failure = error
        # Before anything below stops it: "medit exited with -15" would only
        # say that this is what stopped it.
        alive_at_failure = proc.poll() is None
        try:
            from lib import input as ui
            ui.screenshot(os.path.join(log_dir, "failure.png"))
        except Exception:
            pass

    finally:
        if proc.poll() is None and failure is None:
            code, clean_exit = quit_medit(t, proc)
        elif proc.poll() is None:
            sandbox.stop(proc)
            code = proc.returncode
        else:
            code = proc.returncode

    ok = failure is None

    if failure is not None:
        print("FAIL: %s" % failure)
        if not isinstance(failure, AssertionError):
            traceback.print_exc()
        print(last_words(log_dir, alive_at_failure, proc.returncode))

    elif not clean_exit:
        print("FAIL: medit did not quit when asked")
        ok = False

    elif code != 0:
        print("FAIL: medit exited with %d" % code)
        ok = False

    summary, sanitizers_ok = sanitizer.report(log_dir, args.sanitizers)
    print(sanitizer.format_summary(summary))

    counts = scan_log(log_dir)
    print("glib: %d criticals, %d warnings, %d deprecated properties"
          % (counts["criticals"], counts["warnings"], counts["deprecated"]))

    if counts["names"]:
        print("    deprecated: %s" % ", ".join(
            name if n == 1 else "%s (%d)" % (name, n)
            for name, n in sorted(counts["names"].items(),
                                  key=lambda item: (-item[1], item[0]))))

    if not sanitizers_ok:
        print("FAIL: the sanitizers reported findings, see %s" % log_dir)
        ok = False

    return 0 if ok else 1


def main(argv):
    args = parse_args(argv)
    return inner(args) if args.inner else outer(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

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
import glob
import importlib.util
import os
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


def start_medit(binary, log_dir):
    log = open(os.path.join(log_dir, "medit.log"), "wb")

    # --new-app is not optional: medit is single instance, and without it a
    # second copy hands its arguments to the first and exits immediately.
    return subprocess.Popen([binary, "--new-app"], stdout=log, stderr=subprocess.STDOUT)


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


def scan_log(log_dir):
    """Count what glib printed. Reported, not a verdict -- yet."""
    path = os.path.join(log_dir, "medit.log")
    counts = {"criticals": 0, "warnings": 0, "deprecated": 0}

    try:
        with open(path, errors="replace") as f:
            for line in f:
                if any(marker in line for marker in CRITICAL_MARKERS):
                    counts["criticals"] += 1
                elif "is deprecated" in line:
                    counts["deprecated"] += 1
                elif any(marker in line for marker in WARNING_MARKERS):
                    counts["warnings"] += 1
    except FileNotFoundError:
        pass

    return counts


def inner(args):
    from lib import a11y
    from lib.context import Test

    log_dir = args.log_dir
    proc = start_medit(args.binary, log_dir)
    failure = None
    clean_exit = False
    code = None

    try:
        app = a11y.application("medit", timeout=60)
        t = Test(app, args.gtk, os.environ["MUI_URL_LOG"], log_dir, sys.stdout)

        module = load_test(args.test)
        module.run(t)

    except BaseException as error:          # noqa: BLE001 -- the report is the point
        failure = error
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

    if not sanitizers_ok:
        print("FAIL: the sanitizers reported findings, see %s" % log_dir)
        ok = False

    return 0 if ok else 1


def main(argv):
    args = parse_args(argv)
    return inner(args) if args.inner else outer(args)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

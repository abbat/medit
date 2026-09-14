"""Runs one unit test under the sanitizers.

The unit tests live inside medit and need nothing a UI test needs -- no X
server, no sandbox, no bus -- so this is deliberately not runner.py and not much
of anything: it sets the sanitizer environment, runs `medit --unit-test <path>`,
and reads the logs the runtime left with the same analysis the UI tests use.

What that buys over running the binary straight from ctest, which is what this
used to be, is a verdict that says what it saw. Leak checking is on here -- the
unit tests run before gtk_init(), so none of the library noise a GUI produces is
there -- and left to itself the runtime reports a leak by exiting non-zero, with
a status that carries nothing about what it found and that glib also uses for a
failed assertion. So the runtime is given an exit code of its own, and the logs
decide: what sanitizer.gates() lets through is not a failure, whatever the
status was.
"""

import argparse
import os
import subprocess
import sys


# Importing lib.* would otherwise leave a __pycache__ in the source tree, and
# the source tree is not where a test run should write anything.
sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
TESTS = os.path.dirname(HERE)

if TESTS not in sys.path:
    sys.path.insert(0, TESTS)

from lib import sanitizer            # noqa: E402


def parse_args(argv):
    p = argparse.ArgumentParser()
    p.add_argument("--binary", required=True, help="the medit to run")
    p.add_argument("--log-dir", required=True, help="where the sanitizer logs go")
    p.add_argument("--sanitizers", default="", help="what -fsanitize= was built with")
    p.add_argument("--timeout", type=int, default=55, help="seconds the test may take")
    p.add_argument("test", help="the test path, e.g. /mooutils/accel/parse")
    return p.parse_args(argv)


def main(argv):
    args = parse_args(argv)

    log_dir = os.path.abspath(args.log_dir)

    sanitizer.clean_logs(log_dir)

    env = dict(os.environ)
    env.update(sanitizer.options(log_dir, leak_check=True))

    try:
        rc = subprocess.run([args.binary, "--unit-test", args.test],
                            env=env, timeout=args.timeout).returncode
    except subprocess.TimeoutExpired:
        print("FAIL: %s did not finish within %d s" % (args.test, args.timeout))
        return 1

    summary, ok = sanitizer.report(log_dir, args.sanitizers)
    print(sanitizer.format_summary(summary))

    if not ok:
        return 1

    # Nothing survived the filter, so the status the runtime forced over what
    # did not survive it says nothing either.
    return 0 if rc == sanitizer.EXIT_CODE else rc


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

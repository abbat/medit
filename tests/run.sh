#!/bin/sh
#
# Run the UI tests over one toolkit or both.
#
#   tests/run.sh                      # both toolkits, every test
#   tests/run.sh --gtk 3 -R about     # one toolkit, tests matching a regex
#   tests/run.sh --gtk 2 -L terminal  # one toolkit, one subsystem
#   tests/run.sh -j 4                 # four at a time instead of UI_TEST_PARALLEL
#
# ctest itself is the runner; this only picks the build directories and reports
# both results at the end. Inside one build directory ctest is enough:
#
#   cd buildu3 && ctest -R about_dialog --output-on-failure
#
# The build directories are buildu2 and buildu3, alongside build2 and build3.
# They are separate because a test build is configured differently -- UI tests
# on, sanitizers on -- and because a sanitized binary is three times the size
# and visibly slower, which is not what an ordinary build should become.

set -eu

top=$(cd "$(dirname "$0")/.." && pwd)

gtk=both
jobs=
ctest_args=

usage () {
    sed -n '3,17p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --gtk) gtk="$2"; shift 2 ;;
        -j|--parallel) jobs="$2"; shift 2 ;;
        -h|--help) usage 0 ;;
        --) shift; break ;;
        *) break ;;
    esac
done

ctest_args="$*"

case "$gtk" in
    2|3) toolkits="$gtk" ;;
    both) toolkits="2 3" ;;
    *) echo "--gtk takes 2, 3 or both" >&2; exit 2 ;;
esac

status=0

for v in $toolkits; do
    eval "build=\${MUI_BUILD$v:-$top/buildu$v}"

    if [ ! -f "$build/CMakeCache.txt" ]; then
        echo "no build directory at $build. Configure one with:" >&2
        echo >&2
        echo "  cmake -S $top -B $build -DGTK_VERSION=$v -DENABLE_UI_TESTS=ON \\" >&2
        echo "        -DENABLE_SANITIZERS=address,undefined" >&2
        echo "  cmake --build $build -j\"\$(nproc)\"" >&2
        status=1
        continue
    fi

    echo "=== GTK+$v ($build)"

    # The binary the tests drive has to be the current one, or a green run says
    # nothing about the change that is being tested.
    cmake --build "$build" -j"$(nproc)" >/dev/null

    if [ -n "$jobs" ]; then
        parallel="-j $jobs"
    else
        parallel="-j $(cmake -L -N "$build" 2>/dev/null |
                       sed -n 's/^UI_TEST_PARALLEL:STRING=//p')"
    fi

    # shellcheck disable=SC2086 -- both are deliberately word-split
    (cd "$build" && ctest $parallel --output-on-failure $ctest_args) || status=1
done

exit $status

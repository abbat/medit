#!/bin/sh
#
# Run the UI tests over one toolkit or both.
#
#   tests/run.sh                      # both toolkits, every test
#   tests/run.sh --gtk 3 -R about     # one toolkit, tests matching a regex
#   tests/run.sh --gtk 2 -L terminal  # one toolkit, one subsystem
#   tests/run.sh -j 4                 # four at a time instead of UI_TEST_PARALLEL
#   tests/run.sh --verbose            # every line ctest prints, as it prints it
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
#
# A passing run says one line per toolkit and nothing else: the compile and the
# per-test "Passed" lines go to <build dir>/run.log, which is worth reading only
# when something is wrong. A failing run prints what failed and where the log
# is. The exit code is the whole answer on a green run -- 0, or 1 if either
# toolkit failed -- which is the point: reading 120 lines of "Passed" to learn
# what one number already said is a waste of whoever is reading, human or not.

set -eu

top=$(cd "$(dirname "$0")/.." && pwd)

gtk=both
jobs=
verbose=
ctest_args=

usage () {
    sed -n '3,26p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --gtk) gtk="$2"; shift 2 ;;
        -j|--parallel) jobs="$2"; shift 2 ;;
        -v|--verbose) verbose=1; shift ;;
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

# What ctest says at the end, turned into one line. The two lines it is read
# from are "100% tests passed, 0 tests failed out of 50" and "Total Test time
# (real) = 96.20 sec"; a run whose filter matched nothing has neither.
summarize () {
    _log=$1
    _tag=$2
    _counts=$(sed -n 's/^.*tests passed, \([0-9]*\) tests failed out of \([0-9]*\).*$/\1 \2/p' "$_log")
    if [ -z "$_counts" ]; then
        echo "$_tag  $(tail -1 "$_log")"
        return
    fi
    _failed=${_counts% *}
    _total=${_counts#* }
    _secs=$(sed -n 's/^Total Test time (real) = *\([0-9]*\)\..*$/\1/p' "$_log")
    _passed=$((_total - _failed))
    if [ "$_failed" = 0 ]; then
        echo "$_tag  $_passed/$_total passed  ${_secs:-?}s"
    else
        echo "$_tag  $_passed/$_total passed, $_failed FAILED  ${_secs:-?}s"
    fi
}

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

    log="$build/run.log"

    if [ -n "$jobs" ]; then
        parallel="-j $jobs"
    else
        parallel="-j $(cmake -L -N "$build" 2>/dev/null |
                       sed -n 's/^UI_TEST_PARALLEL:STRING=//p')"
    fi

    if [ -n "$verbose" ]; then
        echo "=== GTK+$v ($build)"
        # The binary the tests drive has to be the current one, or a green run
        # says nothing about the change that is being tested.
        cmake --build "$build" -j"$(nproc)" >/dev/null
        # shellcheck disable=SC2086 -- both are deliberately word-split
        (cd "$build" && ctest $parallel --output-on-failure $ctest_args) || status=1
        continue
    fi

    if ! cmake --build "$build" -j"$(nproc)" > "$log" 2>&1; then
        echo "GTK+$v  build failed, last 40 lines of $log:"
        tail -40 "$log"
        status=1
        continue
    fi

    # shellcheck disable=SC2086 -- both are deliberately word-split
    if (cd "$build" && ctest $parallel --output-on-failure $ctest_args) > "$log" 2>&1; then
        summarize "$log" "GTK+$v"
    else
        summarize "$log" "GTK+$v"
        # Everything the log holds except the roll call: what a failing test
        # printed, and the list at the end.
        grep -Ev '^ *[0-9]+/[0-9]+ Test +#[0-9]+:.*Passed|^ *Start +[0-9]+:|^Test project ' "$log"
        echo "--- the whole run is in $log"
        status=1
    fi
done

exit $status

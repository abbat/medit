# Run by the "coverage" target through cmake -P; see cmake/Coverage.cmake for
# what it is and how it is driven. Everything it needs is passed with -D:
#
#   MOO_COVERAGE_DIR      <build>/coverage, with raw/ inside it
#   MOO_COVERAGE_BINARY   the medit the profiles came out of
#   MOO_COVERAGE_ROOT     the top of the tree, taken out of the reported paths
#   MOO_COVERAGE_IGNORE   a regex of paths not to report
#   MOO_COVERAGE_HTML     ON to write annotated source as well
#   LLVM_PROFDATA         merges the raw profiles
#   LLVM_COV              turns the merged one into a report
#
# Nothing here is python: a build that only wants a number should not need an
# interpreter, and ENABLE_COVERAGE is usable without ENABLE_UI_TESTS.

function(moo_run)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE _result)
    if(NOT _result EQUAL 0)
        string(JOIN " " _printed ${ARGN})
        message(FATAL_ERROR "failed with ${_result}: ${_printed}")
    endif()
endfunction()

set(_raw_dir "${MOO_COVERAGE_DIR}/raw")
set(_profdata "${MOO_COVERAGE_DIR}/medit.profdata")
set(_info "${MOO_COVERAGE_DIR}/medit.info")

file(GLOB _raws "${_raw_dir}/*.profraw")

if(NOT _raws)
    message(FATAL_ERROR
        "no profiles in ${_raw_dir}. The tests write them as they run, so run "
        "them first: cmake --build . --target ui-test, or ctest.")
endif()

list(LENGTH _raws _count)
message(STATUS "Merging ${_count} profiles")

# -sparse drops the counters nothing reached, which is most of them in a program
# this size run by one test.
moo_run(${LLVM_PROFDATA} merge -sparse -o "${_profdata}" ${_raws})

# Consumed rather than kept. Left in place they would be merged again by the
# next run of this target, and the number would then be about every test ever
# run in this build directory instead of the ones that just ran.
file(REMOVE ${_raws})

moo_run(${LLVM_COV} export -format=lcov
        -instr-profile "${_profdata}"
        -ignore-filename-regex "${MOO_COVERAGE_IGNORE}"
        "${MOO_COVERAGE_BINARY}"
        OUTPUT_FILE "${_info}")

# llvm-cov names every file by the absolute path it was compiled from. What
# reads the report afterwards is not in that directory -- another job, another
# container, the machine that downloaded the artifact -- and the two toolkits
# have to name the same file the same way to be merged at all. So the root goes
# out of the paths here, once, rather than being guessed at by every reader.
file(READ "${_info}" _exported)
string(REPLACE "SF:${MOO_COVERAGE_ROOT}/" "SF:" _exported "${_exported}")
file(WRITE "${_info}" "${_exported}")

message(STATUS "Wrote ${_info}")

execute_process(
    COMMAND ${LLVM_COV} report
            -instr-profile "${_profdata}"
            -ignore-filename-regex "${MOO_COVERAGE_IGNORE}"
            -show-region-summary=false
            "${MOO_COVERAGE_BINARY}"
    OUTPUT_VARIABLE _report
    RESULT_VARIABLE _result)

if(NOT _result EQUAL 0)
    message(FATAL_ERROR "llvm-cov report failed with ${_result}")
endif()

# The whole table is hundreds of files long; what a person watching a build
# wants is the TOTAL line at the end of it. The file above has the rest.
string(REGEX MATCH "\nTOTAL[^\n]*" _total "${_report}")
file(WRITE "${MOO_COVERAGE_DIR}/report.txt" "${_report}")

message(STATUS "${_total}")

if(MOO_COVERAGE_HTML)
    moo_run(${LLVM_COV} show -format=html
            -instr-profile "${_profdata}"
            -ignore-filename-regex "${MOO_COVERAGE_IGNORE}"
            -output-dir "${MOO_COVERAGE_DIR}/html"
            "${MOO_COVERAGE_BINARY}")

    message(STATUS "Wrote ${MOO_COVERAGE_DIR}/html/index.html")
endif()

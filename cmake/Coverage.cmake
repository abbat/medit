# The "coverage" target: what the tests executed, measured by clang.
#
# Instrumentation comes from ENABLE_COVERAGE in CompilerFlags.cmake; this file
# is the reading end. It runs no tests -- the tests are ctest's, and running
# them twice to measure them once is a build nobody waits for. The order is
# always the same:
#
#   cmake -S . -B buildc3 -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
#         -DGTK_VERSION=3 -DENABLE_UI_TESTS=ON -DENABLE_COVERAGE=ON \
#         -DENABLE_SANITIZERS=address,undefined
#   cmake --build buildc3 -j"$(nproc)"
#   cmake --build buildc3 --target ui-test        # or ctest -R lsp, or -L unit
#   cmake --build buildc3 --target coverage       # merge, export, report
#
# Every run of the program under test leaves a .profraw in coverage/raw --
# ctest names each one after the test that produced it, and the runtime adds
# the pid, because a test may start medit more than once. The target merges
# whatever is there into one .profdata, exports it as lcov, prints the summary,
# and takes the raw files it consumed away, so that the next run answers about
# the next run rather than about every run since the build directory was made.
#
# tests/coverage.py reads the exported .info: it merges the two toolkits, which
# are separate builds and therefore separate profiles, and compares the result
# with tests/coverage.floor. The exported report names files the way the tree
# does, relative to its root, so that the two halves line up when they were
# built in different directories -- which in CI they were, in two containers.

if(NOT ENABLE_COVERAGE)
    return()
endif()

# The tools have to be the same llvm the compiler came from: a .profraw carries
# a version and llvm-profdata refuses to read one it does not know. Debian and
# Fedora both ship the versioned name beside the unversioned one, so the
# versioned name is asked for first and the plain one is the fallback for a
# distribution that has only that.
string(REGEX MATCH "^[0-9]+" _moo_llvm_version "${CMAKE_C_COMPILER_VERSION}")

find_program(LLVM_PROFDATA NAMES llvm-profdata-${_moo_llvm_version} llvm-profdata)
find_program(LLVM_COV NAMES llvm-cov-${_moo_llvm_version} llvm-cov)

if(NOT LLVM_PROFDATA OR NOT LLVM_COV)
    message(FATAL_ERROR
        "ENABLE_COVERAGE needs llvm-profdata and llvm-cov, version "
        "${_moo_llvm_version} to match the compiler. They are in the llvm "
        "package on debian and fedora, llvm on arch.")
endif()

set(MOO_COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")

# Created here rather than left to the runtime: the profile runtime does make
# the directory it was given, but a configure that has already made it is one
# less thing to wonder about when a test leaves nothing behind.
file(MAKE_DIRECTORY "${MOO_COVERAGE_DIR}/raw")

# Upstream code we carry verbatim, the same four the analyze target skips, plus
# everything outside the tree: the system headers glib and gtk inline into every
# file, and the sources cmake generates into the build directory. Measuring any
# of it would move the number without anybody being able to act on it.
set(MOO_COVERAGE_IGNORE
    "/usr/"
    "${CMAKE_BINARY_DIR}/"
    "src/gtksourceview/"
    "src/xdgmime/"
    "src/eggsmclient/"
    "src/plugins/ctags/readtags\\.c")

list(JOIN MOO_COVERAGE_IGNORE "|" _moo_coverage_ignore)

set(_moo_coverage_args
    -DMOO_COVERAGE_DIR=${MOO_COVERAGE_DIR}
    -DMOO_COVERAGE_BINARY=$<TARGET_FILE:medit>
    -DMOO_COVERAGE_ROOT=${CMAKE_SOURCE_DIR}
    -DMOO_COVERAGE_IGNORE=${_moo_coverage_ignore}
    -DLLVM_PROFDATA=${LLVM_PROFDATA}
    -DLLVM_COV=${LLVM_COV})

add_custom_target(coverage
    COMMAND ${CMAKE_COMMAND} ${_moo_coverage_args}
            -P "${CMAKE_SOURCE_DIR}/cmake/CoverageReport.cmake"
    COMMENT "Merging what the tests executed"
    USES_TERMINAL
    VERBATIM)

# The same thing with a directory of annotated source beside it. Separate
# because it takes appreciably longer than the summary and is only worth it
# when the question is "which lines", not "how many".
add_custom_target(coverage-html
    COMMAND ${CMAKE_COMMAND} ${_moo_coverage_args} -DMOO_COVERAGE_HTML=ON
            -P "${CMAKE_SOURCE_DIR}/cmake/CoverageReport.cmake"
    COMMENT "Merging what the tests executed, with annotated source"
    USES_TERMINAL
    VERBATIM)

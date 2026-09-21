# The unit tests, one ctest entry each.
#
# They are not part of tests/: they live inside the binary rather than under
# tests/, they need no X server, no accessibility bus and no sandbox, and they
# finish in milliseconds. Registered here so that a build with ENABLE_UNIT_TESTS
# but not ENABLE_UI_TESTS still has something to run.
#
# One entry per test rather than one for all of them, which is what this used
# to be: a failure then said "unit.all failed" and left the log to be read,
# nothing could be run alone through ctest, and 35 tests took one timeout
# between them. The list lives in the binary -- glib's own, since these are
# g_test suites -- so it is read out of the binary after it is linked, the way
# gtest_discover_tests does it. The cost is that ctest run against a configured
# but never built tree finds no unit tests; build first.
#
# The locale is pinned for the same reason the UI tests pin it: a name a test
# compares is translated, and a suite that passes in English and fails in
# Russian is worse than no suite.
#
# A sanitized build runs each test through tests/lib/unit.py instead of running
# the binary directly, so that the unit half of the suite is judged the way the
# UI half is: leak checking on, the runtime's output in a log of its own rather
# than mixed into stderr, a sanitizer.json beside it, and one line saying what
# the runtime saw. Left to itself the runtime says it by exiting non-zero, which
# is indistinguishable from glib's own status for a failed assertion and carries
# nothing about what was reported.

set(MOO_UNIT_TESTS_FILE "${CMAKE_BINARY_DIR}/unit-tests.cmake")

# ctest reads the file at the start of a run and stops if it is not there, so
# something has to exist before the first build writes the real one.
if(NOT EXISTS "${MOO_UNIT_TESTS_FILE}")
    file(WRITE "${MOO_UNIT_TESTS_FILE}"
        "# No unit tests discovered yet: build medit and they appear here.\n")
endif()

set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${MOO_UNIT_TESTS_FILE}")

if(ENABLE_COVERAGE)
    set(_moo_unit_coverage_dir "${MOO_COVERAGE_DIR}")
else()
    set(_moo_unit_coverage_dir "")
endif()

# The interpreter is looked for only when there is something for it to read --
# a build without sanitizers has no logs to analyse -- because "no interpreter
# at build time, none at run time" is a property of this fork (tests/CMakeLists
# .txt says why), and a sanitized build is a developer's build by definition.
# If it is not there the tests still run, straight from ctest, the way they did
# before this wrapper existed.
set(_moo_unit_python "")
set(_moo_unit_wrapper "")

if(ENABLE_SANITIZERS)
    find_package(Python3 COMPONENTS Interpreter QUIET)

    if(Python3_Interpreter_FOUND)
        set(_moo_unit_python "${Python3_EXECUTABLE}")
        set(_moo_unit_wrapper "${CMAKE_SOURCE_DIR}/tests/lib/unit.py")
    else()
        message(STATUS "Python3 not found: the unit tests will be judged by what "
                       "the sanitizer runtime exits with, not by what it wrote")
    endif()
endif()

# A target of its own rather than a POST_BUILD step, which can only be attached
# in the directory that created the target, and one that runs on every build
# rather than one keyed on a file, which would be skipped whenever a reconfigure
# left the list newer than a binary it knows nothing about. Listing costs about
# ten milliseconds; being wrong about which tests exist costs a debugging
# session.
add_custom_target(unit-tests-list ALL
    COMMAND ${CMAKE_COMMAND}
            "-DMEDIT=$<TARGET_FILE:medit>"
            "-DOUTPUT=${MOO_UNIT_TESTS_FILE}"
            "-DGTK_VERSION=${GTK_VERSION}"
            "-DCOVERAGE_DIR=${_moo_unit_coverage_dir}"
            "-DPYTHON=${_moo_unit_python}"
            "-DWRAPPER=${_moo_unit_wrapper}"
            "-DSANITIZERS=${ENABLE_SANITIZERS}"
            "-DLOG_ROOT=${CMAKE_BINARY_DIR}/unit-tests"
            -P "${CMAKE_SOURCE_DIR}/cmake/DiscoverUnitTests.cmake"
    BYPRODUCTS "${MOO_UNIT_TESTS_FILE}"
    COMMENT "Reading the unit tests out of medit"
    VERBATIM)

add_dependencies(unit-tests-list medit)

# Performance measurements, run by hand in an unsanitized release build: they are
# not in the list above (MOO_PERF hides them from discovery), so ctest and CI
# never see them. See src/mooedit/mooedit-perf.cpp.
add_custom_target(perf
    COMMAND ${CMAKE_COMMAND} -E env MOO_PERF=1
            "MOO_PERF_OUT=${CMAKE_BINARY_DIR}/perf.txt"
            $<TARGET_FILE:medit> --unit-test /perf/highlight
    DEPENDS medit
    USES_TERMINAL
    COMMENT "Measuring highlighting")

# The ui-test target runs ctest over everything, unit tests included, and is
# built by name rather than as part of ALL -- so without this it could run
# against a list left by an older build.
if(TARGET ui-test)
    add_dependencies(ui-test unit-tests-list)
endif()

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
# Russian is worse than no suite. The sanitizer options are left alone on
# purpose -- what the runtime does by default, including reporting leaks and
# exiting non-zero over them, is exactly what this should be judged on.

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
            -P "${CMAKE_SOURCE_DIR}/cmake/DiscoverUnitTests.cmake"
    BYPRODUCTS "${MOO_UNIT_TESTS_FILE}"
    COMMENT "Reading the unit tests out of medit"
    VERBATIM)

add_dependencies(unit-tests-list medit)

# The ui-test target runs ctest over everything, unit tests included, and is
# built by name rather than as part of ALL -- so without this it could run
# against a list left by an older build.
if(TARGET ui-test)
    add_dependencies(ui-test unit-tests-list)
endif()

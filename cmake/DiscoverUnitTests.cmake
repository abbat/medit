# Turns "medit --unit-test-list" into ctest entries, one per test.
#
# Run by cmake/UnitTests.cmake as a POST_BUILD step, because the list lives in
# the binary and there is no binary at configure time. The output is included
# by ctest through the TEST_INCLUDE_FILES property.
#
# Inputs: MEDIT, OUTPUT, GTK_VERSION, COVERAGE_DIR (empty when coverage is off).

# glib prints a TAP plan and a comment per suite around the paths; the paths
# are the lines that start with a slash.
# The profile file is named because the binary is instrumented in a coverage
# build and would otherwise drop one into the pool the tests write to, where
# the coverage target would count a run that was only a listing. cmake -E env
# rather than ENVIRONMENT_MODIFICATION, which wants cmake 3.25.
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${OUTPUT}.profraw"
            "${MEDIT}" --unit-test-list
    OUTPUT_VARIABLE _moo_list
    ERROR_VARIABLE _moo_err
    RESULT_VARIABLE _moo_rc)

if(NOT _moo_rc EQUAL 0)
    message(FATAL_ERROR
        "${MEDIT} --unit-test-list failed with ${_moo_rc}:\n${_moo_err}")
endif()

string(REPLACE "\n" ";" _moo_lines "${_moo_list}")

set(_moo_out "# Written by cmake/DiscoverUnitTests.cmake. Do not edit.\n")
set(_moo_count 0)

foreach(_moo_line IN LISTS _moo_lines)
    string(STRIP "${_moo_line}" _moo_path)

    if(NOT _moo_path MATCHES "^/")
        continue()
    endif()

    # /mooutils/accel/parse -> unit.mooutils.accel.parse, so that a name reads
    # the way the other ctest entries do and "ctest -R unit.lsp" selects a
    # subsystem.
    string(REGEX REPLACE "^/" "" _moo_name "${_moo_path}")
    string(REPLACE "/" "." _moo_name "${_moo_name}")
    set(_moo_name "unit.${_moo_name}")

    # The first component, which is the subsystem the UI tests label themselves
    # with too.
    string(REGEX REPLACE "^/([^/]+).*$" "\\1" _moo_group "${_moo_path}")

    set(_moo_env "LC_ALL=C.UTF-8;LANGUAGE=")

    if(COVERAGE_DIR)
        set(_moo_env "${_moo_env};LLVM_PROFILE_FILE=${COVERAGE_DIR}/raw/${_moo_name}-%p.profraw")
    endif()

    # RESOURCE_LOCK, so that one unit test runs at a time. They are one entry
    # each now, and ctest would otherwise start a dozen at once beside the UI
    # tests -- forty short medit processes under the address sanitizer, each
    # mapping its shadow, during the twenty-second waits a UI test is made of.
    # Measured: the file selector tests time out in a full run and pass in one
    # without the unit tests, on the same binary. Serialised they cost five
    # seconds between them, which is the load unit.all used to put on the
    # machine and is what this restores.
    string(APPEND _moo_out
        "add_test([==[${_moo_name}]==] [==[${MEDIT}]==] --unit-test [==[${_moo_path}]==])\n"
        "set_tests_properties([==[${_moo_name}]==] PROPERTIES\n"
        "    LABELS \"unit;gtk${GTK_VERSION};${_moo_group}\"\n"
        "    ENVIRONMENT \"${_moo_env}\"\n"
        "    RESOURCE_LOCK unit\n"
        "    TIMEOUT 60)\n")

    math(EXPR _moo_count "${_moo_count} + 1")
endforeach()

if(_moo_count EQUAL 0)
    message(FATAL_ERROR
        "${MEDIT} --unit-test-list named no tests. It printed:\n${_moo_list}")
endif()

file(WRITE "${OUTPUT}" "${_moo_out}")

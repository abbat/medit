# The UI tests: one ctest entry per tests/<subsystem>/<name>/test.py.
#
# Each entry runs lib/runner.py, which builds a sandbox of its own -- temp root,
# X server, session bus -- so the entries are safe to run in parallel and none
# of them touches the settings of the machine they run on.
#
#   cmake -S . -B buildu3 -DGTK_VERSION=3 -DENABLE_UI_TESTS=ON \
#         -DENABLE_SANITIZERS=address,undefined
#   cmake --build buildu3 -j"$(nproc)"
#
#   cd buildu3 && ctest -j8                      # every test of this toolkit
#   ctest -R about_dialog --output-on-failure     # one test
#   ctest -L app                                  # one subsystem
#   cmake --build buildu3 --target ui-test        # ctest -j UI_TEST_PARALLEL
#
# ../tests/run.sh --gtk both runs both toolkits from the source tree.
#
# The harness is python, and it is the only python in the tree. It is a
# developer's tool: not built, not installed, not packaged, and nothing below
# runs unless ENABLE_UI_TESTS was asked for, which is off by default -- so an
# ordinary build never looks for an interpreter. medit was dropped from Debian
# over python2 once; "no interpreter at build time, none at run time" is a
# property of this fork, and a test harness is not a reason to give it up.

include(ProcessorCount)
ProcessorCount(_moo_nproc)
if(_moo_nproc EQUAL 0)
    set(_moo_nproc 1)
endif()
set(UI_TEST_PARALLEL ${_moo_nproc} CACHE STRING "How many UI tests to run at once")

set(UI_TEST_TIMEOUT 300 CACHE STRING "Seconds one UI test may take")

# The sandbox root has to be a short path: the at-spi bus socket is created
# inside it, and a UNIX socket path is limited to about 108 bytes. A build
# directory is easily long enough to go over it, and the failure is silent --
# medit comes up, the accessibility tree never does.
set(UI_TEST_TMP_ROOT "/tmp" CACHE STRING "Where each test's sandbox root is created")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Everything the harness shells out to. Checked here rather than at run time so
# that a missing package is a configure error naming it, not a test that times
# out looking for a window.
set(_moo_ui_test_tools Xvfb xdotool dbus-run-session)

foreach(_tool ${_moo_ui_test_tools})
    string(MAKE_C_IDENTIFIER "UI_TEST_${_tool}" _var)
    find_program(${_var} NAMES ${_tool})
    if(NOT ${_var})
        message(FATAL_ERROR
            "${_tool} not found, and ENABLE_UI_TESTS needs it. The UI tests need "
            "Xvfb (xvfb), xdotool, dbus-run-session (dbus-daemon), at-spi2-core "
            "and the python at-spi bindings (python3-pyatspi); a GTK+2 build also "
            "needs libgail (libgail-common).")
    endif()
endforeach()

execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import pyatspi"
    RESULT_VARIABLE _moo_pyatspi
    OUTPUT_QUIET ERROR_QUIET)

if(NOT _moo_pyatspi EQUAL 0)
    message(FATAL_ERROR
        "${Python3_EXECUTABLE} cannot import pyatspi, and ENABLE_UI_TESTS needs it. "
        "It is python3-pyatspi on debian and fedora, python-atspi on arch.")
endif()

# CONFIGURE_DEPENDS re-runs the glob when the directory changes, so a new test
# is picked up by the next build instead of needing a reconfigure by hand.
file(GLOB _moo_ui_tests
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*/*/test.py")

if(NOT _moo_ui_tests)
    message(WARNING "ENABLE_UI_TESTS is on, but tests/*/*/test.py matched nothing")
endif()

# So that a test can require the toolkit itself, which is not otherwise a
# variable that says yes or no -- GTK_VERSION is "2" or "3" and both are true.
if(GTK_VERSION STREQUAL "3")
    set(MOO_GTK3 1)
else()
    set(MOO_GTK2 1)
endif()

# A test can name the cmake variables its subsystem needs in its header:
#
#     # requires: MOO_BUILD_TERMINAL
#
# A build without them still registers the test, disabled, so that "ctest -N"
# on a GTK+2 build lists the terminal tests and says why they did not run,
# rather than silently listing fewer tests than the other build.
function(moo_ui_test_requirements path out)
    file(STRINGS "${path}" _lines REGEX "^# *requires:")
    set(_missing "")

    foreach(_line ${_lines})
        string(REGEX REPLACE "^# *requires: *" "" _names "${_line}")
        string(REPLACE "," " " _names "${_names}")
        separate_arguments(_names)

        foreach(_name ${_names})
            set(_value "")
            if(DEFINED ${_name})
                set(_value "${${_name}}")
            endif()
            if(NOT _value)
                list(APPEND _missing ${_name})
            endif()
        endforeach()
    endforeach()

    set(${out} "${_missing}" PARENT_SCOPE)
endfunction()

foreach(_test ${_moo_ui_tests})
    get_filename_component(_dir "${_test}" DIRECTORY)
    string(REPLACE "/" ";" _parts "${_dir}")
    list(GET _parts 0 _subsystem)
    list(GET _parts 1 _name)

    set(_test_name "${_subsystem}.${_name}")
    set(_test_logs "${CMAKE_BINARY_DIR}/ui-tests/${_test_name}")

    add_test(NAME ${_test_name}
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/runner.py"
            --test "${CMAKE_CURRENT_SOURCE_DIR}/${_test}"
            --binary "$<TARGET_FILE:medit>"
            --gtk "${GTK_VERSION}"
            --log-dir "${_test_logs}"
            --sanitizers "${ENABLE_SANITIZERS}"
            --suppressions "${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp"
            --tmp-root "${UI_TEST_TMP_ROOT}"
            --timeout ${UI_TEST_TIMEOUT})

    # The toolkit is a label as well as a property of the build directory, so
    # that a run over both of them can still be filtered and so that the label
    # shows up in the output.
    set_tests_properties(${_test_name} PROPERTIES
        LABELS "${_subsystem};gtk${GTK_VERSION}"
        TIMEOUT ${UI_TEST_TIMEOUT})

    # The header is read at configure time, so cmake has to re-run when it
    # changes; the glob above only notices files coming and going.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${_test}")

    moo_ui_test_requirements("${CMAKE_CURRENT_SOURCE_DIR}/${_test}" _missing)

    if(_missing)
        set_tests_properties(${_test_name} PROPERTIES DISABLED TRUE)
        message(STATUS "UI test ${_test_name} is disabled: this build has no ${_missing}")
    endif()
endforeach()

add_custom_target(ui-test
    COMMAND ${CMAKE_CTEST_COMMAND} -j ${UI_TEST_PARALLEL} --output-on-failure
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running the UI tests, ${UI_TEST_PARALLEL} at a time"
    USES_TERMINAL)

add_dependencies(ui-test medit)

# The harness read rather than run: flake8 is pyflakes (an unused import, a name
# that is not defined, a variable assigned and never read) plus pycodestyle. The
# settings are in .flake8 at the top of the tree, and the same command is what
# the ui workflow gates on.
#
# Optional, and quietly so. It is a developer's tool for a developer's tool, and
# a machine without it must still be able to run the tests -- which is also why
# nothing here is a configure error the way Xvfb and pyatspi are.
find_program(UI_TEST_FLAKE8 NAMES flake8)

if(UI_TEST_FLAKE8)
    add_custom_target(ui-lint
        COMMAND ${UI_TEST_FLAKE8} "${CMAKE_CURRENT_SOURCE_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Reading the UI test harness with flake8"
        USES_TERMINAL)
else()
    add_custom_target(ui-lint
        # No semicolon in the message: cmake reads one as a list separator and
        # would take the rest of the sentence for another command to run.
        COMMAND ${CMAKE_COMMAND} -E echo
            "flake8 was not found -- it is python3-flake8 on debian and fedora"
        COMMENT "Reading the UI test harness with flake8")
endif()

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
endforeach()

add_custom_target(ui-test
    COMMAND ${CMAKE_CTEST_COMMAND} -j ${UI_TEST_PARALLEL} --output-on-failure
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running the UI tests, ${UI_TEST_PARALLEL} at a time"
    USES_TERMINAL)

add_dependencies(ui-test medit)

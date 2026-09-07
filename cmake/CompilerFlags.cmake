# Compiler flags, mirroring what m4/moo-flags.m4 used to check for.
#
# Every flag is probed before use, so an unusual compiler simply gets fewer of
# them instead of failing the build.

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# The C standard is set through a probed flag rather than CMAKE_C_STANDARD: cmake
# only learned the C17 dialect in 3.21, and Ubuntu 20.04 ships 3.16.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MOO_C_FLAGS "")
set(MOO_CXX_FLAGS "")

function(moo_try_c_flag flag)
    string(MAKE_C_IDENTIFIER "HAVE_C${flag}" var)
    check_c_compiler_flag(${flag} ${var})
    if(${var})
        set(MOO_C_FLAGS "${MOO_C_FLAGS};${flag}" PARENT_SCOPE)
    endif()
endfunction()

function(moo_try_cxx_flag flag)
    string(MAKE_C_IDENTIFIER "HAVE_CXX${flag}" var)
    check_cxx_compiler_flag(${flag} ${var})
    if(${var})
        set(MOO_CXX_FLAGS "${MOO_CXX_FLAGS};${flag}" PARENT_SCOPE)
    endif()
endfunction()

macro(moo_try_flag flag)
    moo_try_c_flag(${flag})
    moo_try_cxx_flag(${flag})
endmacro()

moo_try_c_flag(-std=gnu17)

foreach(flag
        -Wall
        -Wextra
        -fexceptions
        -fno-strict-aliasing
        -Wno-missing-field-initializers
        -Wno-format-y2k
        -Wno-overlength-strings)
    moo_try_flag(${flag})
endforeach()

# Deprecated GTK+ and glib API is used all over the tree; porting away from it
# is the GTK+4 work, not something an ordinary build should shout about. So an
# ordinary build stays quiet, and a strict build shows the warnings without
# failing on them -- they are the measure of how much of that work is left, and
# -Wno-error for this one warning is added after -Werror below.
if(NOT ENABLE_STRICT)
    moo_try_flag(-Wno-deprecated-declarations)
endif()

moo_try_cxx_flag(-fno-rtti)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    moo_try_flag(-ftrapv)
else()
    moo_try_cxx_flag(-fno-enforce-eh-specs)
endif()

if(ENABLE_STRICT)
    foreach(flag
            -Werror
            -Wpointer-arith
            -Wsign-compare
            -Wreturn-type
            -Wwrite-strings
            -Wmissing-format-attribute
            -Wdisabled-optimization
            -Wendif-labels
            -Wvla
            -Winit-self)
        moo_try_flag(${flag})
    endforeach()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        moo_try_flag(-Wlogical-op)
    else()
        moo_try_flag(-Wuninitialized)
    endif()

    foreach(flag -Wmissing-prototypes -Wnested-externs)
        moo_try_c_flag(${flag})
    endforeach()

    # After -Werror above, so that deprecations warn but do not fail the build.
    moo_try_flag(-Wno-error=deprecated-declarations)

    foreach(flag
            -fno-nonansi-builtins
            -fno-gnu-keywords
            -Wctor-dtor-privacy
            -Wstrict-null-sentinel
            -Woverloaded-virtual
            -Wsign-promo
            -Wnon-virtual-dtor
            -Wno-long-long)
        moo_try_cxx_flag(${flag})
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Sanitizers
# ---------------------------------------------------------------------------
#
# -fsanitize has to reach the linker as well as the compiler, and the probe has
# to link too -- a compile-only check accepts the flag on a toolchain that has
# no runtime library to link against. Hence CMAKE_REQUIRED_LINK_OPTIONS around
# the probe and add_link_options() after it; this file is included from the top
# level, so the link options reach src/ as well.
#
# LeakSanitizer comes with -fsanitize=address and only reports at exit, so a run
# that is killed rather than quit through the UI produces no leak report at all.
# That is why the UI tests quit medit through its own File/Quit and wait for the
# exit code instead of sending a signal.
if(ENABLE_SANITIZERS)
    set(_moo_sanitize "-fsanitize=${ENABLE_SANITIZERS}")

    set(CMAKE_REQUIRED_LINK_OPTIONS ${_moo_sanitize})
    check_c_compiler_flag(${_moo_sanitize} MOO_HAVE_SANITIZERS)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)

    if(NOT MOO_HAVE_SANITIZERS)
        message(FATAL_ERROR "The compiler does not accept ${_moo_sanitize}")
    endif()

    # -fno-omit-frame-pointer is what turns the sanitizer's stack traces from
    # addresses into function names, and -g keeps that true for a build type
    # that would otherwise carry no debug info.
    #
    # Except for the function check, which clang's undefined behaviour
    # sanitizer includes and gcc's does not. It reports a call made through a
    # pointer of a different type, and that is how every GObject callback is
    # called: g_signal_connect takes a G_CALLBACK, the marshaller casts it back
    # to the signature the signal has, and the sixteen it reports on this tree
    # are that pattern rather than sixteen defects. Turning it off keeps the
    # rest of the sanitizer, which does find real things -- the enum load in
    # mooeditor.cpp was one.
    check_c_compiler_flag(-fno-sanitize=function MOO_HAVE_NO_SANITIZE_FUNCTION)
    if(MOO_HAVE_NO_SANITIZE_FUNCTION)
        list(APPEND _moo_sanitize -fno-sanitize=function)
    endif()

    list(APPEND MOO_C_FLAGS ${_moo_sanitize} -fno-omit-frame-pointer -g)
    list(APPEND MOO_CXX_FLAGS ${_moo_sanitize} -fno-omit-frame-pointer -g)
    add_link_options(${_moo_sanitize})

    message(STATUS "Building with ${_moo_sanitize}")
endif()

# ---------------------------------------------------------------------------
# Coverage
# ---------------------------------------------------------------------------
#
# clang's source-based coverage: -fprofile-instr-generate puts the counters in
# the binary, -fcoverage-mapping puts the map from counter to source range in
# it, and both have to reach the linker as well -- hence the same
# CMAKE_REQUIRED_LINK_OPTIONS dance the sanitizers do above.
#
# gcc has no such thing. Its --coverage writes .gcda files beside the objects,
# in a format llvm-cov cannot read, so a gcc build here is refused rather than
# quietly given a different kind of coverage: what reads the result is
# llvm-profdata and llvm-cov, and they and the compiler have to be the same
# version of llvm.
#
# The counters are written by an atexit handler, which is the same property the
# leak checker has and the same reason it works here: the UI tests quit medit
# through File/Quit and wait for its exit code instead of sending a signal. A
# test that times out and is killed contributes nothing -- and it has failed
# anyway.
if(ENABLE_COVERAGE)
    set(_moo_coverage -fprofile-instr-generate -fcoverage-mapping)

    # The probe takes the two flags as one space-separated string, because that
    # is what CMAKE_REQUIRED_FLAGS is; the list above is what the link options
    # and the per-language flags want.
    set(CMAKE_REQUIRED_LINK_OPTIONS ${_moo_coverage})
    check_c_compiler_flag("-fprofile-instr-generate -fcoverage-mapping"
                          MOO_HAVE_COVERAGE)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)

    if(NOT MOO_HAVE_COVERAGE)
        message(FATAL_ERROR
            "ENABLE_COVERAGE needs clang: ${CMAKE_C_COMPILER} does not accept "
            "-fprofile-instr-generate -fcoverage-mapping. Configure with "
            "-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++.")
    endif()

    list(APPEND MOO_C_FLAGS ${_moo_coverage})
    list(APPEND MOO_CXX_FLAGS ${_moo_coverage})
    add_link_options(${_moo_coverage})

    message(STATUS "Building with ${_moo_coverage}")
endif()

set(MOO_COMPILE_DEFINITIONS
    XDG_PREFIX=_moo_edit_xdg
    G_LOG_DOMAIN="Moo"
    MOO_DATA_DIR="${MOO_DATA_DIR}"
    MOO_LIB_DIR="${MOO_LIB_DIR}"
    MOO_LOCALE_DIR="${MOO_LOCALE_DIR}"
    HAVE_CONFIG_H=1)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND MOO_COMPILE_DEFINITIONS
        ENABLE_DEBUG ENABLE_PROFILE G_ENABLE_DEBUG G_ENABLE_PROFILE MOO_DEBUG DEBUG)
elseif(ENABLE_UNIT_TESTS)
    # Everything the release build defines except G_DISABLE_ASSERT, which turns
    # g_assert_cmpint() and its family into nothing at all -- and glib's test
    # framework, which is what --unit-test runs, refuses to start when it finds
    # them disabled rather than quietly reporting that no-ops passed. So a
    # build with the unit tests compiled in keeps its assertions live, in
    # medit's own code as well as in the tests.
    list(APPEND MOO_COMPILE_DEFINITIONS
        NDEBUG=1 G_DISABLE_CAST_CHECKS)
else()
    list(APPEND MOO_COMPILE_DEFINITIONS
        NDEBUG=1 G_DISABLE_CAST_CHECKS G_DISABLE_ASSERT)
endif()

if(NOT ENABLE_STRICT)
    # Not defining G_DISABLE_DEPRECATED is no longer enough to keep glib quiet.
    list(APPEND MOO_COMPILE_DEFINITIONS GLIB_DISABLE_DEPRECATION_WARNINGS=1)
endif()

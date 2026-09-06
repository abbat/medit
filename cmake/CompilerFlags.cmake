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
    list(APPEND MOO_C_FLAGS ${_moo_sanitize} -fno-omit-frame-pointer -g)
    list(APPEND MOO_CXX_FLAGS ${_moo_sanitize} -fno-omit-frame-pointer -g)
    add_link_options(${_moo_sanitize})

    message(STATUS "Building with ${_moo_sanitize}")
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
else()
    list(APPEND MOO_COMPILE_DEFINITIONS
        NDEBUG=1 G_DISABLE_CAST_CHECKS G_DISABLE_ASSERT)
endif()

if(NOT ENABLE_STRICT)
    # Not defining G_DISABLE_DEPRECATED is no longer enough to keep glib quiet.
    list(APPEND MOO_COMPILE_DEFINITIONS GLIB_DISABLE_DEPRECATION_WARNINGS=1)
endif()

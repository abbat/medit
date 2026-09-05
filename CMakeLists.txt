cmake_minimum_required(VERSION 3.16)

# Version. Keep in sync with debian/changelog.
set(MOO_MAJOR_VERSION 1)
set(MOO_MINOR_VERSION 3)
set(MOO_MICRO_VERSION 2)
set(MOO_VERSION_SUFFIX "")

set(MOO_MODULE_MAJOR_VERSION 2)
set(MOO_MODULE_MINOR_VERSION 0)

set(MOO_VERSION "${MOO_MAJOR_VERSION}.${MOO_MINOR_VERSION}.${MOO_MICRO_VERSION}")
if(MOO_VERSION_SUFFIX)
    set(MOO_DISPLAY_VERSION "${MOO_VERSION}-${MOO_VERSION_SUFFIX}")
else()
    set(MOO_DISPLAY_VERSION "${MOO_VERSION}")
endif()

project(medit
    VERSION ${MOO_VERSION}
    LANGUAGES C CXX)

set(MOO_PACKAGE_NAME "medit")
set(MOO_EMAIL "emuntyan@users.sourceforge.net")
set(MOO_WEBSITE "https://github.com/abbat/medit")
set(MOO_WEBSITE_OLD "http://mooedit.sourceforge.net/")
set(MOO_BUGREPORT_URL "https://github.com/abbat/medit/issues/new")
set(MOO_COPYRIGHT "2004-2014 Yevgen Muntyan <${MOO_EMAIL}>")
set(MOO_FORK_COPYRIGHT "2023-2026 Anton Batenev <antonbatenev@yandex.ru>")

set(MOO_PREFS_XML_FILE_NAME "prefs.xml")
set(MOO_STATE_XML_FILE_NAME "state.xml")
set(MOO_SESSION_XML_FILE_NAME "session.xml")
set(MOO_NAMED_SESSION_XML_FILE_NAME "session-%s.xml")
set(MEDIT_PORTABLE_MAGIC_FILE_NAME "medit-portable")
set(MEDIT_PORTABLE_DATA_DIR "medit-portable-data")
set(MEDIT_PORTABLE_CACHE_DIR "medit-portable-cache")

set(GETTEXT_PACKAGE "${MOO_PACKAGE_NAME}")
set(GETTEXT_PACKAGE_GSV "${MOO_PACKAGE_NAME}-gsv")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

set(GTK_VERSION "2" CACHE STRING "GTK version to compile with (2, 3 or 4)")
set_property(CACHE GTK_VERSION PROPERTY STRINGS 2 3 4)

option(ENABLE_NLS "Native Language Support" ON)
option(ENABLE_STRICT "Enable all warnings and -Werror" OFF)
option(ENABLE_INSTALL_HOOKS "Run gtk-update-icon-cache during install" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    # -g -O2, matching what the autotools build used to default to
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)

set(MOO_DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${MOO_PACKAGE_NAME}")
set(MOO_LIB_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/${MOO_PACKAGE_NAME}")
set(MOO_DOC_DIR "${CMAKE_INSTALL_FULL_DATADIR}/doc/${MOO_PACKAGE_NAME}")
set(MOO_HELP_DIR "${MOO_DOC_DIR}/help")
set(MOO_LOCALE_DIR "${CMAKE_INSTALL_FULL_LOCALEDIR}")
set(MOO_TEXT_LANG_FILES_DIR "${MOO_DATA_DIR}/language-specs")

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

find_package(PkgConfig REQUIRED)

if(GTK_VERSION STREQUAL "2")
    set(GTK_PACKAGE gtk+-2.0)
elseif(GTK_VERSION STREQUAL "3")
    set(GTK_PACKAGE gtk+-3.0)
elseif(GTK_VERSION STREQUAL "4")
    # gtk4 dropped the "gtk+-" prefix from its pkg-config name
    set(GTK_PACKAGE gtk4)
else()
    message(FATAL_ERROR "Unsupported GTK version: ${GTK_VERSION}. Supported versions are 2, 3 and 4.")
endif()

pkg_check_modules(GTK REQUIRED IMPORTED_TARGET ${GTK_PACKAGE})
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0 gthread-2.0 gmodule-2.0)
pkg_check_modules(XML REQUIRED IMPORTED_TARGET libxml-2.0)

# X11 libraries, when gdk targets X11. Each one is optional.
pkg_get_variable(GDK_TARGET ${GTK_PACKAGE} target)
if(GDK_TARGET STREQUAL "" OR GDK_TARGET STREQUAL "x11")
    foreach(pkg x11 xext xrender ice sm)
        pkg_check_modules(${pkg} IMPORTED_TARGET ${pkg})
        if(${pkg}_FOUND)
            list(APPEND MOO_X_TARGETS PkgConfig::${pkg})
        endif()
    endforeach()
endif()

find_library(LIBM m)

# find_program() only learned REQUIRED in cmake 3.18, so check by hand.
find_program(GLIB_GENMARSHAL NAMES glib-genmarshal)
find_program(GLIB_COMPILE_RESOURCES NAMES glib-compile-resources)
find_program(GDK_PIXBUF_CSOURCE NAMES gdk-pixbuf-csource)

foreach(tool GLIB_GENMARSHAL GLIB_COMPILE_RESOURCES GDK_PIXBUF_CSOURCE)
    if(NOT ${tool})
        message(FATAL_ERROR "${tool} not found")
    endif()
endforeach()

if(ENABLE_NLS)
    find_program(MSGFMT NAMES msgfmt)
    find_program(INTLTOOL_MERGE NAMES intltool-merge)

    foreach(tool MSGFMT INTLTOOL_MERGE)
        if(NOT ${tool})
            message(FATAL_ERROR "${tool} not found, install gettext and intltool or pass -DENABLE_NLS=OFF")
        endif()
    endforeach()
endif()

# ---------------------------------------------------------------------------
# config.h
# ---------------------------------------------------------------------------

include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckSymbolExists)

check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(sys/utsname.h HAVE_SYS_UTSNAME_H)
check_include_file(signal.h HAVE_SIGNAL_H)
check_include_file(sys/wait.h HAVE_SYS_WAIT_H)
check_function_exists(mmap HAVE_MMAP)
check_symbol_exists(getc_unlocked stdio.h HAVE_GETC_UNLOCKED)
check_symbol_exists(bind_textdomain_codeset libintl.h HAVE_BIND_TEXTDOMAIN_CODESET)

# ctags plugin builds on unix only
set(MOO_BUILD_CTAGS 1)

# The terminal pane needs vte. vte dropped GTK+2 in 0.30 (2011), the last GTK+2
# release being 0.28.2, so the pane is built for the GTK+3 build only.
#
# Test GTK_PACKAGE, not GTK_VERSION: pkg_check_modules(GTK ...) above defines
# GTK_VERSION as the version of the module it found, shadowing the cache entry
# that selects the toolkit, so by now it reads 3.24.38 rather than 3.
set(ENABLE_TERMINAL "AUTO" CACHE STRING "Terminal pane, needs vte-2.91 (AUTO, ON or OFF)")
set_property(CACHE ENABLE_TERMINAL PROPERTY STRINGS AUTO ON OFF)

if(ENABLE_TERMINAL STREQUAL "OFF")
    message(STATUS "Building without the terminal pane")
elseif(NOT GTK_PACKAGE STREQUAL "gtk+-3.0")
    if(ENABLE_TERMINAL STREQUAL "ON")
        message(FATAL_ERROR "ENABLE_TERMINAL=ON needs vte-2.91, which exists for GTK+3 only")
    endif()
    message(STATUS "Not a GTK+3 build, building without the terminal pane")
else()
    pkg_check_modules(VTE IMPORTED_TARGET vte-2.91)

    if(VTE_FOUND)
        set(MOO_BUILD_TERMINAL 1)
    elseif(ENABLE_TERMINAL STREQUAL "ON")
        message(FATAL_ERROR "ENABLE_TERMINAL=ON, but vte-2.91 was not found")
    else()
        message(STATUS "vte-2.91 not found, building without the terminal pane")
    endif()
endif()

configure_file(cmake/config.h.in config.h @ONLY)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------

include(cmake/CompilerFlags.cmake)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------

add_subdirectory(src)
add_subdirectory(doc)

if(ENABLE_NLS)
    add_subdirectory(po)
    add_subdirectory(po-gsv)
endif()

#pragma once

#if (defined(DEBUG) && ((!DEBUG) || !(MOO_DEBUG) || !(ENABLE_DEBUG))) || \
    (!defined(DEBUG) && (defined(MOO_DEBUG) || defined(ENABLE_DEBUG)))
#  error "DEBUG, MOO_DEBUG, and ENABLE_DEBUG must either be all defined to non-zero or all undefined"
#endif

#undef MOO_CL_GCC
#undef MOO_CL_MINGW
#undef MOO_CL_MSVC
#define MOO_GCC_CHECK_VERSION(maj,min) (0)

#if defined(_MSC_VER)
#  define MOO_CL_MSVC 1
#elif defined(__GNUC__)
#  define MOO_CL_GCC 1
#  ifdef __GNUC_MINOR__
#    undef MOO_GCC_CHECK_VERSION
#    define MOO_GCC_CHECK_VERSION(maj,min) (((__GNUC__ << 16) + __GNUC_MINOR__) >= (((maj) << 16) + (min)))
#  endif
#  if defined(__MINGW32__)
#    define MOO_CL_MINGW 1
#  endif
#endif

#undef MOO_OS_UNIX
#undef MOO_OS_DARWIN

#if defined(__APPLE__)
#  define MOO_OS_DARWIN 1
#endif

#define MOO_OS_UNIX 1

#define MOO_CDECL
#define MOO_STDCALL

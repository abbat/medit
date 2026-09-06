# The "analyze" target: the clang static analyzer over medit's own sources.
#
# This is the local counterpart of the codeql workflow -- something to run while
# editing, rather than after pushing. It reads paths through the program instead
# of one statement at a time, which is where use-after-free, leaked allocations
# and null dereferences live; the compiler warnings in CompilerFlags.cmake do
# not go there.
#
#   cmake -S . -B builda -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
#   cmake --build builda --target analyze          # whole tree, a couple of minutes
#   clang-tidy -p builda src/mooutils/moopaned.c   # one file, a couple of seconds
#
# It needs a build directory of its own, configured with clang, next to build2
# and build3. That is not a preference: clang-tidy takes its flags from
# compile_commands.json, CompilerFlags.cmake probes every flag against the
# compiler that configured the tree, and a gcc-configured tree therefore records
# gcc-only flags -- -fno-enforce-eh-specs among them -- which clang rejects
# outright, on every C++ file. Hence the guard below.

include(ProcessorCount)
ProcessorCount(_moo_nproc)
if(_moo_nproc EQUAL 0)
    set(_moo_nproc 1)
endif()
set(MOO_ANALYZE_JOBS ${_moo_nproc} CACHE STRING "Parallel jobs for the analyze target")

find_program(CLANG_TIDY NAMES clang-tidy)
find_program(RUN_CLANG_TIDY NAMES run-clang-tidy run-clang-tidy.py)

# Upstream code we carry verbatim. gtksourceview, xdgmime and eggsmclient are
# whole directories; the ctags plugin is ours except for readtags.c, which comes
# from the ctags project. Analyzing them reports real enough findings that are
# still not ours to fix, and they would show up on every run.
set(MOO_ANALYZE_EXCLUDE
    "src/gtksourceview/"
    "src/xdgmime/"
    "src/eggsmclient/"
    "src/plugins/ctags/readtags\\.c")

list(JOIN MOO_ANALYZE_EXCLUDE "|" _moo_analyze_exclude)
# run-clang-tidy takes a python regex matched against each path in the
# compilation database, so the exclusion is a negative lookahead. Going through
# the database rather than a glob also means only files that are actually
# compiled in this configuration are looked at.
set(_moo_analyze_filter "^(?!.*(${_moo_analyze_exclude})).*\\.(c|cpp)$")

# The analyzer, and nothing else. clang-tidy's own lint families (modernize,
# readability, cppcoreguidelines) have no useful opinion about GObject C: they
# report every GTK_WIDGET() as a C-style cast, and an analysis nobody reads is
# worse than none.
#
# Two of the analyzer's own checkers go the same way. They are not in the set
# scan-build runs by default, and on this tree they are 517 of 616 findings:
#
#   optin.core.EnumCastOutOfRange   502, one per cast to a GObject enum
#   security.insecureAPI.
#     DeprecatedOrUnsafeBufferHandling
#                                    15, "use memcpy_s", which is MSVC's
#   security.insecureAPI.strcpy       3, and it fires on the name of the
#                                       function without looking at the guard
#                                       above it. All three sites here check the
#                                       length immediately before copying: two
#                                       against sizeof addr.sun_path in
#                                       mooappinput-unix.c, one against a 5000
#                                       byte string in mooutils-fs.cpp.
set(MOO_ANALYZE_CHECKS
    "-*,clang-analyzer-*,-clang-analyzer-optin.*,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,-clang-analyzer-security.insecureAPI.strcpy")

# -w turns off the compiler's own diagnostics, leaving only the analyzer's.
# They are not lost: the target depends on medit, so the build that runs first
# has just printed every one of them.
set(MOO_ANALYZE_EXTRA_ARG "-w")

# The target fails on any finding. It can, because there are none left: the
# handful the analyzer still reports are all wrong about glib, and each is
# marked at its own line with a NOLINTNEXTLINE and the reason -- reference
# counting it cannot follow, g_strfreev() it does not model, ownership passing
# into a GObject setter, a field set in _init(), g_strdupv()'s NULL-in-NULL-out
# contract.
#
# So a new report means new code, which is the point. When one is wrong, say
# why at the line rather than widening this list; when it is right, fix it.
set(MOO_ANALYZE_WARNINGS_AS_ERRORS "*")

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_custom_target(analyze
        COMMAND ${CMAKE_COMMAND} -E echo
            "This build directory is configured with ${CMAKE_C_COMPILER_ID}, and its"
        COMMAND ${CMAKE_COMMAND} -E echo
            "compile_commands.json records flags clang does not accept. Configure a"
        COMMAND ${CMAKE_COMMAND} -E echo
            "clang build directory of its own and run the target there:"
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo
            "  cmake -S ${CMAKE_SOURCE_DIR} -B builda -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"
        COMMAND ${CMAKE_COMMAND} -E echo
            "  cmake --build builda --target analyze"
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM)
elseif(NOT CLANG_TIDY)
    add_custom_target(analyze
        COMMAND ${CMAKE_COMMAND} -E echo
            "clang-tidy was not found. On debian and ubuntu it is in clang-tidy,"
        COMMAND ${CMAKE_COMMAND} -E echo
            "on fedora in clang-tools-extra, on arch in clang."
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM)
else()
    if(RUN_CLANG_TIDY)
        # run-clang-tidy drives one clang-tidy per file over the database, in
        # parallel; clang-tidy on its own would walk them one at a time.
        set(_moo_analyze_command
            ${RUN_CLANG_TIDY}
                -p "${CMAKE_BINARY_DIR}"
                -j ${MOO_ANALYZE_JOBS}
                -quiet
                -checks=${MOO_ANALYZE_CHECKS}
                -extra-arg=${MOO_ANALYZE_EXTRA_ARG}
                -warnings-as-errors=${MOO_ANALYZE_WARNINGS_AS_ERRORS}
                "${_moo_analyze_filter}")
    else()
        # Without the driver, hand clang-tidy the database and let it walk it.
        # No -j, and no way to filter by path, so this reports the vendored
        # directories too.
        set(_moo_analyze_command
            ${CLANG_TIDY}
                -p "${CMAKE_BINARY_DIR}"
                --quiet
                --checks=${MOO_ANALYZE_CHECKS}
                --extra-arg=${MOO_ANALYZE_EXTRA_ARG}
                --warnings-as-errors=${MOO_ANALYZE_WARNINGS_AS_ERRORS})
    endif()

    add_custom_target(analyze
        COMMAND ${_moo_analyze_command}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Running the clang static analyzer over medit's own sources"
        VERBATIM
        USES_TERMINAL)

    # marshals.c, resources.c and moo-pixbufs.h are generated, and the database
    # names them; without them clang-tidy fails on the entries that include
    # them. Building medit first is also what makes an incremental run cheap.
    add_dependencies(analyze medit)
endif()

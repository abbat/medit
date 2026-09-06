# Cross-check the ids the code asks GtkBuilder for against the ids the .ui files
# declare. Run in script mode:
#
#   cmake -DSRCDIR=<src> -P cmake/CheckBuilderIds.cmake
#
# GtkBuilder reports a missing id only when the dialog is opened, and some of
# them are hard to reach -- the drop dialog needs a real drag and drop. That is
# how one stale id survived in mootextprint.c. Comparing the two lists finds
# them all at once, without a display and without running anything.
#
# The scope is one source file at a time: every file in the tree that asks for
# an id also creates its own builder, so the ids it may ask for are those
# declared by the .ui files it names in moo_builder_new().

cmake_policy(SET CMP0057 NEW)   # if(... IN_LIST ...)

file(GLOB_RECURSE ui_files "${SRCDIR}/*.ui")
file(GLOB_RECURSE src_files "${SRCDIR}/*.c" "${SRCDIR}/*.cpp")

# id="..." -> a list of ids, keyed by the .ui basename
foreach(ui ${ui_files})
    get_filename_component(name "${ui}" NAME)
    file(READ "${ui}" text)
    string(REGEX MATCHALL "id=\"[^\"]*\"" matches "${text}")
    set(ids "")
    foreach(m ${matches})
        string(REGEX REPLACE "^id=\"(.*)\"$" "\\1" id "${m}")
        list(APPEND ids "${id}")
    endforeach()
    set("ui_ids_${name}" "${ids}")
endforeach()

set(problems "")

foreach(src ${src_files})
    file(READ "${src}" text)

    string(REGEX MATCHALL "moo_builder_new[ \t]*\\([ \t]*\"/ui/[^\"]*\"" loads "${text}")
    if(NOT loads)
        continue()
    endif()

    set(known "")
    foreach(l ${loads})
        string(REGEX REPLACE "^.*\"/ui/(.*)\"$" "\\1" name "${l}")
        list(APPEND known ${ui_ids_${name}})
    endforeach()

    string(REGEX MATCHALL
           "moo_builder_(get|take|reparent)[ \t]*\\([^,]*,[ \t]*\"[^\"]*\""
           asks "${text}")
    foreach(a ${asks})
        string(REGEX REPLACE "^.*\"(.*)\"$" "\\1" id "${a}")
        if(NOT id IN_LIST known)
            file(RELATIVE_PATH rel "${SRCDIR}" "${src}")
            set(problems "${problems}  ${rel}: no widget with id \"${id}\"\n")
        endif()
    endforeach()
endforeach()

if(problems)
    message(FATAL_ERROR
        "ids asked of GtkBuilder that no .ui file declares:\n${problems}")
endif()

# Code generators, mirroring the rules that used to live in src/Makefile.am.
#
# Every generator writes into the binary directory, keeping the source tree
# clean; src/ and ${CMAKE_CURRENT_BINARY_DIR} are both on the include path, so
# the generated headers are found by their plain names.

set(MOO_TOOLS_DIR "${CMAKE_SOURCE_DIR}/tools")

# moo_glade2c(<out-var> <glade file> ...)
#
# glade/foo.glade -> foo-gxml.h, in the directory holding glade/.
function(moo_glade2c out)
    set(generated "")
    foreach(glade ${ARGN})
        get_filename_component(name "${glade}" NAME_WE)
        get_filename_component(dir "${glade}" DIRECTORY)
        get_filename_component(dir "${dir}" DIRECTORY)   # drop the glade/ level
        set(header "${CMAKE_CURRENT_BINARY_DIR}/${dir}/${name}-gxml.h")
        add_custom_command(
            OUTPUT "${header}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${dir}"
            COMMAND ${Python3_EXECUTABLE} "${MOO_TOOLS_DIR}/glade2c.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/${glade}" > "${header}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${glade}" "${MOO_TOOLS_DIR}/glade2c.py"
            COMMENT "Generating ${dir}/${name}-gxml.h"
            VERBATIM)
        list(APPEND generated "${header}")
    endforeach()
    set(${out} "${generated}" PARENT_SCOPE)
endfunction()

# moo_xml2h(<out-var> <input> <symbol> [<input> <symbol> ...])
#
# foo.xml -> foo-ui.h holding the file contents as a C string named <symbol>.
function(moo_xml2h out)
    set(generated "")
    set(args ${ARGN})
    list(LENGTH args count)
    math(EXPR last "${count} - 1")
    foreach(i RANGE 0 ${last} 2)
        list(GET args ${i} input)
        math(EXPR j "${i} + 1")
        list(GET args ${j} symbol)
        get_filename_component(name "${input}" NAME_WE)
        get_filename_component(dir "${input}" DIRECTORY)
        set(header "${CMAKE_CURRENT_BINARY_DIR}/${dir}/${name}-ui.h")
        add_custom_command(
            OUTPUT "${header}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${dir}"
            COMMAND ${Python3_EXECUTABLE} "${MOO_TOOLS_DIR}/xml2h.py"
                    "${CMAKE_CURRENT_SOURCE_DIR}/${input}" "${header}" "${symbol}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${input}" "${MOO_TOOLS_DIR}/xml2h.py"
            COMMENT "Generating ${dir}/${name}-ui.h"
            VERBATIM)
        list(APPEND generated "${header}")
    endforeach()
    set(${out} "${generated}" PARENT_SCOPE)
endfunction()

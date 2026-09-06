
# ---------------------------------------------------------------------------
# Generated sources
# ---------------------------------------------------------------------------

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marshals.h"
    COMMAND ${GLIB_GENMARSHAL} --prefix=_moo_marshal --header
            "${CMAKE_CURRENT_SOURCE_DIR}/marshals.list" > "${CMAKE_CURRENT_BINARY_DIR}/marshals.h"
    DEPENDS marshals.list
    COMMENT "Generating marshals.h"
    VERBATIM)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marshals.c"
    COMMAND ${CMAKE_COMMAND} -E echo "#include \"marshals.h\"" > "${CMAKE_CURRENT_BINARY_DIR}/marshals.c"
    COMMAND ${GLIB_GENMARSHAL} --prefix=_moo_marshal --body
            "${CMAKE_CURRENT_SOURCE_DIR}/marshals.list" >> "${CMAKE_CURRENT_BINARY_DIR}/marshals.c"
    DEPENDS marshals.list "${CMAKE_CURRENT_BINARY_DIR}/marshals.h"
    COMMENT "Generating marshals.c"
    VERBATIM)

set(MOO_PIXMAPS
    MOO_HIDE_ICON         mooutils/pixmaps/hide.png
    MOO_CLOSE_ICON        mooutils/pixmaps/close.png
    MOO_STICKY_ICON       mooutils/pixmaps/sticky.png
    MOO_DETACH_ICON       mooutils/pixmaps/detach.png
    MOO_ATTACH_ICON       mooutils/pixmaps/attach.png
    MOO_KEEP_ON_TOP_ICON  mooutils/pixmaps/keepontop.png
    MEDIT_ICON            mooutils/pixmaps/medit.png
    SYMLINK_ARROW         mooutils/pixmaps/symlink.png
    SYMLINK_ARROW_SMALL   mooutils/pixmaps/symlink-small.png)

set(pixbuf_args "")
set(pixbuf_deps "")
list(LENGTH MOO_PIXMAPS pixmaps_count)
math(EXPR pixmaps_last "${pixmaps_count} - 1")
foreach(i RANGE 0 ${pixmaps_last} 2)
    list(GET MOO_PIXMAPS ${i} symbol)
    math(EXPR j "${i} + 1")
    list(GET MOO_PIXMAPS ${j} png)
    list(APPEND pixbuf_args ${symbol} "${CMAKE_CURRENT_SOURCE_DIR}/${png}")
    list(APPEND pixbuf_deps "${CMAKE_CURRENT_SOURCE_DIR}/${png}")
endforeach()

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/moo-pixbufs.h"
    COMMAND ${GDK_PIXBUF_CSOURCE} --static --build-list ${pixbuf_args}
            > "${CMAKE_CURRENT_BINARY_DIR}/moo-pixbufs.h"
    DEPENDS ${pixbuf_deps}
    COMMENT "Generating moo-pixbufs.h"
    VERBATIM)

# every .ui, at any depth: plugins keep theirs one level deeper
file(GLOB_RECURSE MOO_UI_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.ui")

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/resources.c"
    COMMAND ${GLIB_COMPILE_RESOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/resources.xml"
            --sourcedir "${CMAKE_CURRENT_SOURCE_DIR}"
            --target "${CMAKE_CURRENT_BINARY_DIR}/resources.c" --generate-source
    DEPENDS resources.xml ${MOO_UI_FILES} "${CMAKE_SOURCE_DIR}/THANKS"
            mooedit/medit.xml mooedit/mooedit.xml moofileview/moofileview.xml
            plugins/lsp/lsp.xml
    COMMENT "Generating resources.c"
    VERBATIM)

# ---------------------------------------------------------------------------
# XML the resource bundle does not check
# ---------------------------------------------------------------------------
#
# Everything listed in resources.xml with preprocess="xml-stripblanks" is run
# through xmllint on the way into the bundle, which is what makes malformed
# markup fail the build. Two sets of files miss that:
#
#   lsp.xml is bundled deliberately without stripblanks, because its formatting
#   is documentation for a file the user edits -- and it is exactly the file
#   most likely to be hand-edited. GMarkup, which medit parses it with, accepts
#   a "--" inside an XML comment that expat and every conforming parser reject,
#   so a file that loads here can be broken everywhere else.
#
#   The usertools descriptions are installed rather than bundled, so nothing
#   looks at them at build time at all.
set(MOO_UNCHECKED_XML
    plugins/lsp/lsp.xml
    plugins/usertools/menu.xml
    plugins/usertools/context.xml
    plugins/usertools/filters.xml)

list(TRANSFORM MOO_UNCHECKED_XML PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/"
     OUTPUT_VARIABLE MOO_UNCHECKED_XML_PATHS)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xml-validated.stamp"
    COMMAND ${CMAKE_COMMAND}
            "-DXMLLINT=${XMLLINT}"
            "-DFILES=${MOO_UNCHECKED_XML_PATHS}"
            -P "${CMAKE_SOURCE_DIR}/cmake/ValidateXml.cmake"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/xml-validated.stamp"
    DEPENDS ${MOO_UNCHECKED_XML_PATHS} "${CMAKE_SOURCE_DIR}/cmake/ValidateXml.cmake"
    COMMENT "Validating the xml that is not preprocessed into the bundle"
    VERBATIM)

add_custom_target(validate-xml
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/xml-validated.stamp")

# ---------------------------------------------------------------------------
# The application
# ---------------------------------------------------------------------------

add_executable(medit
    main.c
    medit-app/main.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/marshals.c"
    "${CMAKE_CURRENT_BINARY_DIR}/marshals.h"
    "${CMAKE_CURRENT_BINARY_DIR}/moo-pixbufs.h"
    "${CMAKE_CURRENT_BINARY_DIR}/resources.c")

add_dependencies(medit validate-xml)

target_include_directories(medit PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_SOURCE_DIR}/doc/built")

target_compile_definitions(medit PRIVATE ${MOO_COMPILE_DEFINITIONS})
target_compile_options(medit PRIVATE
    $<$<COMPILE_LANGUAGE:C>:${MOO_C_FLAGS}>
    $<$<COMPILE_LANGUAGE:CXX>:${MOO_CXX_FLAGS}>)

target_link_libraries(medit PRIVATE
    PkgConfig::GTK
    PkgConfig::GLIB
    PkgConfig::XML
    ${MOO_X_TARGETS})

if(LIBM)
    target_link_libraries(medit PRIVATE ${LIBM})
endif()

if(MOO_BUILD_TERMINAL)
    target_link_libraries(medit PRIVATE PkgConfig::VTE)
endif()

if(MOO_BUILD_LSP)
    target_link_libraries(medit PRIVATE PkgConfig::JSON_GLIB)
endif()

add_subdirectory(eggsmclient)
add_subdirectory(gtksourceview)
add_subdirectory(mooapp)
add_subdirectory(moocpp)
add_subdirectory(mooedit)
add_subdirectory(mooglib)
add_subdirectory(moofileview)
add_subdirectory(mooutils)
add_subdirectory(plugins)
add_subdirectory(xdgmime)

install(TARGETS medit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---------------------------------------------------------------------------
# Application data
# ---------------------------------------------------------------------------

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/usertools/filters.xml"
    DESTINATION "${MOO_DATA_DIR}")

if(MOO_BUILD_LSP)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/plugins/lsp/lsp.xml"
        DESTINATION "${MOO_DATA_DIR}")
endif()

install(FILES mooutils/pixmaps/medit.png
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps")

if(ENABLE_NLS)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/medit.desktop"
        COMMAND ${CMAKE_COMMAND} -E env LC_ALL=C ${INTLTOOL_MERGE} -d -u
                -c "${CMAKE_BINARY_DIR}/po/.intltool-merge-cache"
                "${CMAKE_SOURCE_DIR}/po"
                "${CMAKE_CURRENT_SOURCE_DIR}/medit-app/medit.desktop.in"
                "${CMAKE_CURRENT_BINARY_DIR}/medit.desktop"
        DEPENDS medit-app/medit.desktop.in
        COMMENT "Generating medit.desktop"
        VERBATIM)
else()
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/medit.desktop"
        COMMAND sed -e "s/^_//g" "${CMAKE_CURRENT_SOURCE_DIR}/medit-app/medit.desktop.in"
                > "${CMAKE_CURRENT_BINARY_DIR}/medit.desktop"
        DEPENDS medit-app/medit.desktop.in
        COMMENT "Generating medit.desktop"
        VERBATIM)
endif()

add_custom_target(medit-desktop ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/medit.desktop")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/medit.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

if(ENABLE_INSTALL_HOOKS)
    install(CODE "execute_process(COMMAND gtk-update-icon-cache -f -t \
        \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor\" ERROR_QUIET)")
endif()

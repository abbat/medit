include(${CMAKE_SOURCE_DIR}/cmake/Codegen.cmake)

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

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mooapp-credits.h"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/tools/xml2h.py"
            "${CMAKE_SOURCE_DIR}/THANKS" "${CMAKE_CURRENT_BINARY_DIR}/mooapp-credits.h" MOO_APP_CREDITS
    DEPENDS "${CMAKE_SOURCE_DIR}/THANKS" "${CMAKE_SOURCE_DIR}/tools/xml2h.py"
    COMMENT "Generating mooapp-credits.h"
    VERBATIM)

file(GLOB_RECURSE MOO_UI_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*/ui/*.ui")

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/resources.c"
    COMMAND ${GLIB_COMPILE_RESOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/resources.xml"
            --sourcedir "${CMAKE_CURRENT_SOURCE_DIR}"
            --target "${CMAKE_CURRENT_BINARY_DIR}/resources.c" --generate-source
    DEPENDS resources.xml ${MOO_UI_FILES}
    COMMENT "Generating resources.c"
    VERBATIM)

moo_glade2c(MOO_GXML_HEADERS
    mooedit/glade/mooeditprefs-file.glade
    mooedit/glade/mooeditprefs-filters.glade
    mooedit/glade/mooeditprefs-general.glade
    mooedit/glade/mooeditprefs-langs.glade
    mooedit/glade/mooeditprefs-view.glade
    mooedit/glade/mooeditprogress.glade
    mooedit/glade/mooeditsavemult.glade
    mooedit/glade/moopluginprefs.glade
    mooedit/glade/mooprint.glade
    mooedit/glade/mooquicksearch.glade
    mooedit/glade/moostatusbar.glade
    mooedit/glade/mootextfind-prompt.glade
    mooedit/glade/mootextfind.glade
    mooedit/glade/mootryencoding.glade
    moofileview/glade/moobookmark-editor.glade
    moofileview/glade/moocreatefolder.glade
    moofileview/glade/moofileprops.glade
    moofileview/glade/moofileview-drop.glade
    mooutils/glade/mooaccelbutton.glade
    mooutils/glade/mooaccelprefs.glade
    mooutils/glade/moologwindow.glade
    plugins/glade/moofileselector-prefs.glade
    plugins/glade/moofileselector.glade
    plugins/glade/moofind.glade
    plugins/glade/moogrep.glade
    plugins/usertools/glade/mooedittools-exe.glade
    plugins/usertools/glade/moousertools.glade)

moo_xml2h(MOO_UI_HEADERS
    mooedit/medit.xml            medit_ui_xml
    mooedit/mooedit.xml          mooedit_ui_xml
    moofileview/moofileview.xml  moofileview_ui_xml)

# Platform-specific menu and context files for the user tools plugin.
set(MOO_USERTOOLS_XML "")
foreach(name menu context)
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/plugins/usertools/${name}.xml"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/plugins/usertools"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/plugins/usertools/genplatxml.py"
                --enable=unix "${CMAKE_CURRENT_SOURCE_DIR}/plugins/usertools/${name}-tmpl.xml"
                > "${CMAKE_CURRENT_BINARY_DIR}/plugins/usertools/${name}.xml"
        DEPENDS plugins/usertools/${name}-tmpl.xml plugins/usertools/genplatxml.py
        COMMENT "Generating plugins/usertools/${name}.xml"
        VERBATIM)
    list(APPEND MOO_USERTOOLS_XML "${CMAKE_CURRENT_BINARY_DIR}/plugins/usertools/${name}.xml")
endforeach()

# ---------------------------------------------------------------------------
# The application
# ---------------------------------------------------------------------------

add_executable(medit
    main.c
    medit-app/main.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/marshals.c"
    "${CMAKE_CURRENT_BINARY_DIR}/marshals.h"
    "${CMAKE_CURRENT_BINARY_DIR}/moo-pixbufs.h"
    "${CMAKE_CURRENT_BINARY_DIR}/mooapp-credits.h"
    "${CMAKE_CURRENT_BINARY_DIR}/resources.c"
    ${MOO_GXML_HEADERS}
    ${MOO_UI_HEADERS}
    ${MOO_USERTOOLS_XML})

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
    ${MOO_USERTOOLS_XML}
    DESTINATION "${MOO_DATA_DIR}")

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

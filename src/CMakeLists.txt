
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
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marshals.cpp"
    COMMAND ${CMAKE_COMMAND} -E echo "#include \"marshals.h\"" > "${CMAKE_CURRENT_BINARY_DIR}/marshals.cpp"
    COMMAND ${GLIB_GENMARSHAL} --prefix=_moo_marshal --body
            "${CMAKE_CURRENT_SOURCE_DIR}/marshals.list" >> "${CMAKE_CURRENT_BINARY_DIR}/marshals.cpp"
    DEPENDS marshals.list "${CMAKE_CURRENT_BINARY_DIR}/marshals.h"
    COMMENT "Generating marshals.cpp"
    VERBATIM)


# every .ui, at any depth: plugins keep theirs one level deeper
file(GLOB_RECURSE MOO_UI_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.ui")
# the pixbufs in the bundle: editing one has to rebuild it
file(GLOB MOO_PIXMAPS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/mooutils/pixmaps/*.png")

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/resources.cpp"
    COMMAND ${GLIB_COMPILE_RESOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/resources.xml"
            --sourcedir "${CMAKE_CURRENT_SOURCE_DIR}"
            --target "${CMAKE_CURRENT_BINARY_DIR}/resources.cpp" --generate-source
    DEPENDS resources.xml ${MOO_UI_FILES} ${MOO_PIXMAPS} "${CMAKE_SOURCE_DIR}/THANKS"
            mooedit/medit.xml mooedit/mooedit.xml moofileview/moofileview.xml
            plugins/lsp/lsp.xml
    COMMENT "Generating resources.cpp"
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

# The ids the code asks GtkBuilder for, against the ids the .ui files declare.
# GtkBuilder itself only reports a missing one when the dialog is opened, and
# some of these dialogs are hard to reach.
file(GLOB_RECURSE MOO_SRC_FILES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/builder-ids.stamp"
    COMMAND ${CMAKE_COMMAND}
            "-DSRCDIR=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_SOURCE_DIR}/cmake/CheckBuilderIds.cmake"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/builder-ids.stamp"
    DEPENDS ${MOO_UI_FILES} ${MOO_SRC_FILES}
            "${CMAKE_SOURCE_DIR}/cmake/CheckBuilderIds.cmake"
    COMMENT "Checking GtkBuilder ids against the .ui files"
    VERBATIM)

add_custom_target(check-builder-ids
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/builder-ids.stamp")

# ---------------------------------------------------------------------------
# The application
# ---------------------------------------------------------------------------

add_executable(medit
    main.cpp
    medit-app/main.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/marshals.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/marshals.h"
    "${CMAKE_CURRENT_BINARY_DIR}/resources.cpp")

add_dependencies(medit validate-xml check-builder-ids)

target_include_directories(medit PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}")

target_compile_definitions(medit PRIVATE ${MOO_COMPILE_DEFINITIONS})
target_compile_options(medit PRIVATE
    $<$<COMPILE_LANGUAGE:C>:${MOO_C_FLAGS}>
    $<$<COMPILE_LANGUAGE:CXX>:${MOO_CXX_FLAGS}>)

# clang-tidy cannot consume the PCH generated for the clang build: its
# invocation changes the language options and clang rejects the mismatch.
# Keep the common header forced into every clang translation unit instead, so
# the clang build and its analyzer have the same declarations without a PCH.
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" AND
   NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_precompile_headers(medit PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/sysheaders.h")
else()
    target_compile_options(medit PRIVATE
        "-include${CMAKE_CURRENT_SOURCE_DIR}/sysheaders.h")
endif()

target_link_libraries(medit PRIVATE
    PkgConfig::GTKPKG
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

add_subdirectory(vendor/gtksourceview)
add_subdirectory(mooapp)
add_subdirectory(moocpp)
add_subdirectory(mooedit)
add_subdirectory(mooglib)
add_subdirectory(moofileview)
add_subdirectory(mooutils)
add_subdirectory(plugins)

install(TARGETS medit RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---------------------------------------------------------------------------
# Application data
# ---------------------------------------------------------------------------

# menu.xml and context.xml are the tools the Tools menu and the document's
# context menu offer out of the box; moousertools.cpp reads all three by name
# from the data directories, so all three have to be installed.
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/usertools/filters.xml"
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/usertools/menu.xml"
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/usertools/context.xml"
    DESTINATION "${MOO_DATA_DIR}")

if(MOO_BUILD_LSP)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/plugins/lsp/lsp.xml"
        DESTINATION "${MOO_DATA_DIR}")
endif()

install(FILES mooutils/pixmaps/medit.png
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps")

if(ENABLE_NLS)
    # The translations come from the catalogs, so a catalog change has to regenerate it
    file(STRINGS "${CMAKE_SOURCE_DIR}/po/LINGUAS" desktop_linguas REGEX "^[^#]")
    set(desktop_po)
    foreach(lang ${desktop_linguas})
        list(APPEND desktop_po "${CMAKE_SOURCE_DIR}/po/${lang}.po")
    endforeach()
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/medit.desktop"
        COMMAND ${CMAKE_COMMAND} -E env LC_ALL=C ${INTLTOOL_MERGE} -d -u
                -c "${CMAKE_BINARY_DIR}/po/.intltool-merge-cache"
                "${CMAKE_SOURCE_DIR}/po"
                "${CMAKE_CURRENT_SOURCE_DIR}/medit-app/medit.desktop.in"
                "${CMAKE_CURRENT_BINARY_DIR}/medit.desktop"
        DEPENDS medit-app/medit.desktop.in ${desktop_po}
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

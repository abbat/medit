# Compile the .po catalogs of one gettext domain.
#
# Besides installing them, the catalogs are laid out in a locale tree inside the
# build directory, the way gettext expects to find them. A binary run straight
# from the build tree then finds its translations without an install:
# moo_get_locale_dir() falls back to that tree when the configured locale
# directory holds no catalog.

function(moo_add_catalogs domain)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/LINGUAS" linguas REGEX "^[^#]")
    string(REPLACE " " ";" linguas "${linguas}")

    set(catalogs "")
    foreach(lang ${linguas})
        set(po "${CMAKE_CURRENT_SOURCE_DIR}/${lang}.po")
        set(mo "${CMAKE_BINARY_DIR}/locale/${lang}/LC_MESSAGES/${domain}.mo")
        add_custom_command(
            OUTPUT "${mo}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/locale/${lang}/LC_MESSAGES"
            COMMAND ${MSGFMT} -o "${mo}" "${po}"
            DEPENDS "${po}"
            COMMENT "Compiling ${domain} catalog for ${lang}"
            VERBATIM)
        list(APPEND catalogs "${mo}")
        install(FILES "${mo}"
            DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${lang}/LC_MESSAGES")
    endforeach()

    add_custom_target(${domain}-catalogs ALL DEPENDS ${catalogs})
endfunction()

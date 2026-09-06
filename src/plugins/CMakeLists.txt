target_sources(medit PRIVATE
    moofilelist.cpp
    moofileselector-prefs.cpp
    moofileselector.cpp
    moofileselector.h
    moofind.cpp
    mooplugin-builtin.cpp
    mooplugin-builtin.h
)

add_subdirectory(ctags)
add_subdirectory(support)
add_subdirectory(usertools)

if(MOO_BUILD_TERMINAL)
    add_subdirectory(terminal)
endif()

if(MOO_BUILD_LSP)
    add_subdirectory(lsp)
endif()

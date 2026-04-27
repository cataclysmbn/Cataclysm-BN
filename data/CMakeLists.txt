if (RELEASE)
    if (USE_PREFIX_DATA_DIR)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
                TYPE DATA
                PATTERN "CMakeLists.txt" EXCLUDE
                PATTERN "XDG" EXCLUDE
                PATTERN "mods/Arcana_BN" EXCLUDE
                PATTERN "external_tileset/README.md" EXCLUDE
                PATTERN "mapgen/lab/README.md" EXCLUDE)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/external/cdda-arcana-mod/Arcana_BN
                DESTINATION ${CMAKE_INSTALL_DATADIR}/mods)
    else()
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
                DESTINATION data
                PATTERN "CMakeLists.txt" EXCLUDE
                PATTERN "XDG" EXCLUDE
                PATTERN "mods/Arcana_BN" EXCLUDE
                PATTERN "external_tileset/README.md" EXCLUDE
                PATTERN "mapgen/lab/README.md" EXCLUDE)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/external/cdda-arcana-mod/Arcana_BN
                DESTINATION data/mods)
    endif()
endif ()

if (RELEASE)
    if (USE_PREFIX_DATA_DIR)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
                TYPE DATA
                PATTERN "CMakeLists.txt" EXCLUDE
                PATTERN "XDG" EXCLUDE
                PATTERN "external_tileset/README.md" EXCLUDE
                PATTERN "mapgen/lab/README.md" EXCLUDE)
    else()
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
                DESTINATION data
                PATTERN "CMakeLists.txt" EXCLUDE
                PATTERN "XDG" EXCLUDE
                PATTERN "external_tileset/README.md" EXCLUDE
                PATTERN "mapgen/lab/README.md" EXCLUDE)
    endif()
endif ()

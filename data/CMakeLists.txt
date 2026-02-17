set(CATACLYSM_DATA_DIRS
        font
        json
        mods
        names
        raw
        motd
        credits
        title
        help)

if (SOUND)
    list(APPEND CATACLYSM_DATA_DIRS sound)
endif ()

set(CATACLYSM_DATA_FILES
        changelog.txt
        cataicon.ico)

if (RELEASE)
    if (USE_PREFIX_DATA_DIR)
        install(DIRECTORY ${CATACLYSM_DATA_DIRS} TYPE DATA
                PATTERN "external_tileset/README.md" EXCLUDE
                PATTERN "mapgen/lab/README.md" EXCLUDE)
        install(FILES ${CATACLYSM_DATA_FILES} TYPE DATA)
    else()
        install(DIRECTORY ${CATACLYSM_DATA_DIRS} DESTINATION data
                PATTERN "external_tileset/README.md" EXCLUDE
                PATTERN "mapgen/lab/README.md" EXCLUDE)
        install(FILES ${CATACLYSM_DATA_FILES} DESTINATION data)
    endif()
endif ()

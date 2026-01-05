# macOS app bundle and DMG creation support

if(NOT APPLE)
    return()
endif()

# Function to create macOS app bundle
function(create_macosx_bundle target_name)
    set(BUNDLE_NAME "Cataclysm.app")
    set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/${BUNDLE_NAME}")
    set(BUNDLE_CONTENTS "${BUNDLE_DIR}/Contents")
    set(BUNDLE_MACOS "${BUNDLE_CONTENTS}/MacOS")
    set(BUNDLE_RESOURCES "${BUNDLE_CONTENTS}/Resources")

    # Create bundle structure
    add_custom_command(
        OUTPUT "${BUNDLE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_CONTENTS}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_MACOS}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_RESOURCES}"
        VERBATIM
    )

    # Copy Info.plist
    add_custom_command(
        OUTPUT "${BUNDLE_CONTENTS}/Info.plist"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/build-data/osx/Info.plist"
            "${BUNDLE_CONTENTS}/Info.plist"
        DEPENDS "${CMAKE_SOURCE_DIR}/build-data/osx/Info.plist"
        VERBATIM
    )

    # Copy launcher script
    add_custom_command(
        OUTPUT "${BUNDLE_MACOS}/Cataclysm.sh"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/build-data/osx/Cataclysm.sh"
            "${BUNDLE_MACOS}/Cataclysm.sh"
        COMMAND chmod +x "${BUNDLE_MACOS}/Cataclysm.sh"
        DEPENDS "${CMAKE_SOURCE_DIR}/build-data/osx/Cataclysm.sh"
        VERBATIM
    )

    # Copy app icon
    add_custom_command(
        OUTPUT "${BUNDLE_RESOURCES}/AppIcon.icns"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/build-data/osx/AppIcon.icns"
            "${BUNDLE_RESOURCES}/AppIcon.icns"
        DEPENDS "${CMAKE_SOURCE_DIR}/build-data/osx/AppIcon.icns"
        VERBATIM
    )

    # Copy executable
    add_custom_command(
        OUTPUT "${BUNDLE_MACOS}/${target_name}"
        COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:${target_name}>"
            "${BUNDLE_MACOS}/${target_name}"
        DEPENDS ${target_name}
        VERBATIM
    )

    # Copy game data
    add_custom_command(
        OUTPUT "${BUNDLE_RESOURCES}/data"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/data"
            "${BUNDLE_RESOURCES}/data"
        DEPENDS "${CMAKE_SOURCE_DIR}/data"
        VERBATIM
    )

    # Copy gfx for tiles build
    if(TILES)
        add_custom_command(
            OUTPUT "${BUNDLE_RESOURCES}/gfx"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/gfx"
                "${BUNDLE_RESOURCES}/gfx"
            DEPENDS "${CMAKE_SOURCE_DIR}/gfx"
            VERBATIM
        )
        set(GFX_OUTPUT "${BUNDLE_RESOURCES}/gfx")
    else()
        set(GFX_OUTPUT "")
    endif()

    # Copy localization
    if(LANGUAGES)
        add_custom_command(
            OUTPUT "${BUNDLE_RESOURCES}/lang"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/lang/mo"
                "${BUNDLE_RESOURCES}/lang/mo"
            DEPENDS translations_compile
            VERBATIM
        )
        set(LANG_OUTPUT "${BUNDLE_RESOURCES}/lang")
    else()
        set(LANG_OUTPUT "")
    endif()

    # Create app bundle target
    add_custom_target(
        osx-app
        DEPENDS
            "${BUNDLE_DIR}"
            "${BUNDLE_CONTENTS}/Info.plist"
            "${BUNDLE_MACOS}/Cataclysm.sh"
            "${BUNDLE_RESOURCES}/AppIcon.icns"
            "${BUNDLE_MACOS}/${target_name}"
            "${BUNDLE_RESOURCES}/data"
            ${GFX_OUTPUT}
            ${LANG_OUTPUT}
        COMMENT "Creating macOS app bundle"
    )

    # Create DMG target
    find_program(DMGBUILD_EXECUTABLE dmgbuild)
    find_program(PLUTIL_EXECUTABLE plutil)
    
    if(DMGBUILD_EXECUTABLE AND PLUTIL_EXECUTABLE)
        add_custom_target(
            osx-dmg
            COMMAND ${PLUTIL_EXECUTABLE} -convert binary1 "${BUNDLE_CONTENTS}/Info.plist"
            COMMAND ${DMGBUILD_EXECUTABLE} -s "${CMAKE_SOURCE_DIR}/build-data/osx/dmgsettings.py"
                -D app="${BUNDLE_DIR}"
                "Cataclysm BN"
                "${CMAKE_BINARY_DIR}/CataclysmBN-${GIT_VERSION}.dmg"
            DEPENDS osx-app
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Creating DMG package"
        )
    else()
        message(WARNING "dmgbuild or plutil not found. DMG creation will not be available.")
        message(WARNING "Install with: pip3 install dmgbuild biplist mac_alias")
    endif()
endfunction()

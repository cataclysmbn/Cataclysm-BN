include(GNUInstallDirs)

message(STATUS "Process LANGUAGES variable --\n")

if (LANGUAGES STREQUAL none)
    message(STATUS "Not compiling any languages.\n")
    return()
endif ()

if(NOT GETTEXT_MSGFMT_EXECUTABLE)
    message(FATAL_ERROR
            "Gettext not found. Install gettext package or disable \
             compilation of language files with: -DLANGUAGES=none. \
             See CMake compiling guide for details and more info.")
endif()

if (LANGUAGES STREQUAL all)
    set(LANGUAGES "")
    file(GLOB PO_FILES ${CMAKE_SOURCE_DIR}/lang/po/*.po)
    foreach (PO_FILE ${PO_FILES})
        get_filename_component(LANG ${PO_FILE} NAME_WE)
        if (NOT LANG STREQUAL "placeholder")
            list(APPEND LANGUAGES ${LANG})
        endif ()
    endforeach ()
    list(REMOVE_DUPLICATES LANGUAGES)
    # End of special case
endif ()

foreach (LANG ${LANGUAGES})
    message(VERBOSE "Add translation for ${LANG}: ${LANG}.po")
endforeach ()
message("\n")

add_custom_target(translations_prepare WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Auto-Compile translation on release builds only
if (RELEASE)
    add_custom_target(
        translations_compile ALL
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS translations_prepare)
else ()
    add_custom_target(
        translations_compile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS translations_prepare)
endif ()

set(MO_FILES)

foreach (LANG ${LANGUAGES})
    set(MO_OUTPUT ${CMAKE_SOURCE_DIR}/lang/mo/${LANG}/LC_MESSAGES/cataclysm-bn.mo)

    add_custom_command(
        OUTPUT ${MO_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/lang/mo/${LANG}/LC_MESSAGES
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -f ${CMAKE_SOURCE_DIR}/lang/po/${LANG}.po -o ${MO_OUTPUT}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS ${CMAKE_SOURCE_DIR}/lang/po/${LANG}.po
    )

    list(APPEND MO_FILES ${MO_OUTPUT})
endforeach ()

add_custom_target(translations ALL DEPENDS ${MO_FILES})

# Install translations
if (RELEASE)
    if (USE_PREFIX_DATA_DIR)
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/lang/mo
            DESTINATION ${CMAKE_INSTALL_LOCALEDIR})
    else()
        install(DIRECTORY ${CMAKE_SOURCE_DIR}/lang/mo
            DESTINATION lang)
    endif()
endif ()

#[=======================================================================[

Windows Lavapipe runtime setup
------------------------------

Downloads and stages the Mesa Lavapipe runtime used by SDL_GPU software mode
on Windows. This is a runtime fallback dependency, not a linked build library.

#]=======================================================================]

option(CATA_WINDOWS_LAVAPIPE
    "Download and stage the Windows Mesa Lavapipe runtime for SDL_GPU software mode."
    ON)
set(CATA_WINDOWS_LAVAPIPE_RELEASE "26.1.1" CACHE STRING
    "mesa-dist-win release to use for Lavapipe. Use a release tag such as 26.1.1, or 'latest'.")

function(cata_download_file url output)
    file(DOWNLOAD
        "${url}"
        "${output}"
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
        TLS_VERIFY ON
    )
    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        list(GET DOWNLOAD_STATUS 1 ERROR_MESSAGE)
        message(FATAL_ERROR "Failed to download ${url}: ${ERROR_MESSAGE}")
    endif()
endfunction()

function(cata_find_latest_mesa_dist_win_asset out_name out_url)
    set(RELEASE_JSON_PATH "${CMAKE_SOURCE_DIR}/build-data/mesa-dist-win-latest.json")
    cata_download_file(
        "https://api.github.com/repos/pal1000/mesa-dist-win/releases/latest"
        "${RELEASE_JSON_PATH}")
    file(READ "${RELEASE_JSON_PATH}" RELEASE_JSON)
    string(JSON ASSET_COUNT LENGTH "${RELEASE_JSON}" assets)
    if(ASSET_COUNT LESS 1)
        message(FATAL_ERROR "mesa-dist-win latest release has no downloadable assets")
    endif()

    math(EXPR LAST_ASSET_INDEX "${ASSET_COUNT} - 1")
    foreach(ASSET_INDEX RANGE ${LAST_ASSET_INDEX})
        string(JSON ASSET_NAME GET "${RELEASE_JSON}" assets ${ASSET_INDEX} name)
        if(ASSET_NAME MATCHES "^mesa3d-.+-release-msvc\\.7z$")
            string(JSON ASSET_URL GET "${RELEASE_JSON}" assets ${ASSET_INDEX} browser_download_url)
            set(${out_name} "${ASSET_NAME}" PARENT_SCOPE)
            set(${out_url} "${ASSET_URL}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    message(FATAL_ERROR "Could not find mesa-dist-win release-msvc asset")
endfunction()

function(cata_extract_mesa_archive archive extract_dir)
    file(REMOVE_RECURSE "${extract_dir}")
    file(MAKE_DIRECTORY "${extract_dir}")

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}"
        WORKING_DIRECTORY "${extract_dir}"
        RESULT_VARIABLE EXTRACT_RESULT
        OUTPUT_VARIABLE EXTRACT_OUTPUT
        ERROR_VARIABLE EXTRACT_ERROR
    )

    if(NOT EXTRACT_RESULT EQUAL 0)
        find_program(CATA_7Z_EXE
            NAMES 7z 7za
            HINTS
                "$ENV{ProgramFiles}/7-Zip"
                "C:/Program Files/7-Zip"
                "C:/Program Files (x86)/7-Zip")
        if(NOT CATA_7Z_EXE)
            message(FATAL_ERROR
                "Failed to extract Mesa runtime with CMake and could not find 7z. "
                "CMake error: ${EXTRACT_ERROR}")
        endif()

        execute_process(
            COMMAND "${CATA_7Z_EXE}" x "${archive}" "-o${extract_dir}" -y
            RESULT_VARIABLE SEVENZIP_RESULT
            OUTPUT_VARIABLE SEVENZIP_OUTPUT
            ERROR_VARIABLE SEVENZIP_ERROR
        )
        if(NOT SEVENZIP_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to extract Mesa runtime with 7z: ${SEVENZIP_ERROR}")
        endif()
    endif()
endfunction()

function(cata_setup_windows_lavapipe_runtime)
    if(NOT WIN32 OR NOT CATA_WINDOWS_LAVAPIPE)
        return()
    endif()

    set(CATA_WINDOWS_LAVAPIPE_RUNTIME_DIR
        "${CMAKE_SOURCE_DIR}/build-data/mesa/x64"
        CACHE INTERNAL "Windows Mesa Lavapipe runtime directory")

    set(LVP_ICD "${CATA_WINDOWS_LAVAPIPE_RUNTIME_DIR}/lvp_icd.x86_64.json")
    set(LVP_DLL "${CATA_WINDOWS_LAVAPIPE_RUNTIME_DIR}/vulkan_lvp.dll")
    if(EXISTS "${LVP_ICD}" AND EXISTS "${LVP_DLL}")
        message(STATUS "Windows Lavapipe runtime: ${CATA_WINDOWS_LAVAPIPE_RUNTIME_DIR}")
        add_custom_target(cata_windows_lavapipe_runtime DEPENDS "${LVP_ICD}" "${LVP_DLL}")
        return()
    endif()

    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/build-data")
    if(CATA_WINDOWS_LAVAPIPE_RELEASE STREQUAL "latest")
        cata_find_latest_mesa_dist_win_asset(MESA_ASSET_NAME MESA_ASSET_URL)
    else()
        set(MESA_ASSET_NAME "mesa3d-${CATA_WINDOWS_LAVAPIPE_RELEASE}-release-msvc.7z")
        set(MESA_ASSET_URL
            "https://github.com/pal1000/mesa-dist-win/releases/download/${CATA_WINDOWS_LAVAPIPE_RELEASE}/${MESA_ASSET_NAME}")
    endif()

    set(MESA_ARCHIVE "${CMAKE_SOURCE_DIR}/build-data/${MESA_ASSET_NAME}")
    set(MESA_EXTRACT_DIR "${CMAKE_SOURCE_DIR}/build-data/mesa-dist-win")

    message(STATUS "Downloading Windows Lavapipe runtime: ${MESA_ASSET_NAME}")
    cata_download_file("${MESA_ASSET_URL}" "${MESA_ARCHIVE}")
    message(STATUS "Extracting Windows Lavapipe runtime...")
    cata_extract_mesa_archive("${MESA_ARCHIVE}" "${MESA_EXTRACT_DIR}")

    set(MESA_SOURCE_DIR "${MESA_EXTRACT_DIR}/x64")
    if(NOT EXISTS "${MESA_SOURCE_DIR}/lvp_icd.x86_64.json" OR
            NOT EXISTS "${MESA_SOURCE_DIR}/vulkan_lvp.dll")
        message(FATAL_ERROR "Extracted mesa-dist-win archive does not contain the expected x64 Lavapipe runtime")
    endif()

    file(REMOVE_RECURSE "${CATA_WINDOWS_LAVAPIPE_RUNTIME_DIR}")
    file(MAKE_DIRECTORY "${CATA_WINDOWS_LAVAPIPE_RUNTIME_DIR}")
    file(COPY "${MESA_SOURCE_DIR}/" DESTINATION "${CATA_WINDOWS_LAVAPIPE_RUNTIME_DIR}")
    message(STATUS "Windows Lavapipe runtime: ${CATA_WINDOWS_LAVAPIPE_RUNTIME_DIR}")

    add_custom_target(cata_windows_lavapipe_runtime DEPENDS "${LVP_ICD}" "${LVP_DLL}")
endfunction()

function(cata_copy_windows_lavapipe_runtime target_name)
    if(NOT WIN32 OR NOT TARGET cata_windows_lavapipe_runtime)
        return()
    endif()

    add_dependencies(${target_name} cata_windows_lavapipe_runtime)
    add_custom_command(
        TARGET ${target_name} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${target_name}>/mesa/x64"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CATA_WINDOWS_LAVAPIPE_RUNTIME_DIR}"
                "$<TARGET_FILE_DIR:${target_name}>/mesa/x64"
        COMMENT "Copying Windows Lavapipe runtime for ${target_name}"
        VERBATIM
    )
endfunction()

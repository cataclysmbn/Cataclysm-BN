#[=======================================================================[

windows-tiles-sounds-x64-msvc
-----------------------------

Pre-load script for Microsoft Visual Studio builds.

Used by CMakePresets.json -> "cacheVariables" -> "CMAKE_PROJECT_INCLUDE_BEFORE".

When CMake does not run under VS environment, it sources the VsDevCmd.bat on it own.
It then writes CMakeUserPresets.json -> "buildPresets" -> "environment"

#]=======================================================================]

# Ref https://github.com/actions/virtual-environments/blob/win19/20220515.1/images/win/Windows2019-Readme.md#environment-variables
if (NOT $ENV{VCPKG_INSTALLATION_ROOT} STREQUAL "")
    set(ENV{VCPKG_ROOT} $ENV{VCPKG_INSTALLATION_ROOT})
endif()
# Ref https://vcpkg.io/en/docs/users/config-environment.html#vcpkg_root
if ("$ENV{VCPKG_ROOT}" STREQUAL "" AND WIN32)
    set(ENV{VCPKG_ROOT} $CACHE{VCPKG_ROOT})
endif()

include(${CMAKE_SOURCE_DIR}/build-scripts/VsDevCmd.cmake)

# It's fine to keep @_MSVC_DEVENV@ undefined
set(BUILD_PRESET_NAME "windows-tiles-sounds-x64-msvc")
set(CONFIGURE_PRESET "windows-tiles-sounds-x64-msvc")
configure_file(
    ${CMAKE_SOURCE_DIR}/build-scripts/CMakeUserPresets.json.in
    ${CMAKE_SOURCE_DIR}/CMakeUserPresets.json
    @ONLY
)

# ccache integration for MSVC with Visual Studio generator
# Ref: https://github.com/ccache/ccache/wiki/MS-Visual-Studio#usage-with-cmake
find_program(CCACHE_EXE ccache)
if(CCACHE_EXE)
    file(COPY_FILE
        ${CCACHE_EXE} ${CMAKE_BINARY_DIR}/cl.exe
        ONLY_IF_DIFFERENT)

    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

    set(CMAKE_VS_GLOBALS
        "CLToolExe=cl.exe"
        "CLToolPath=${CMAKE_BINARY_DIR}"
        "UseMultiToolTask=true"
    )
endif()

# Automatic gettext setup for Windows
# Download pre-built gettext binaries to avoid vcpkg MSYS2 build issues
set(GETTEXT_VERSION "1.0-v1.18-r1")
set(GETTEXT_DIR "${CMAKE_SOURCE_DIR}/build-data/gettext")
set(GETTEXT_ARCHIVE "${CMAKE_SOURCE_DIR}/build-data/gettext-${GETTEXT_VERSION}.zip")
set(GETTEXT_URL "https://github.com/mlocati/gettext-iconv-windows/releases/download/v${GETTEXT_VERSION}/gettext1.0-iconv1.18-static-64.zip")

if(NOT EXISTS "${GETTEXT_DIR}/bin/msgfmt.exe")
    message(STATUS "Downloading pre-built gettext binaries...")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/build-data")
    
    file(DOWNLOAD
        "${GETTEXT_URL}"
        "${GETTEXT_ARCHIVE}"
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
        TLS_VERIFY ON
    )
    
    list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
    if(NOT STATUS_CODE EQUAL 0)
        list(GET DOWNLOAD_STATUS 1 ERROR_MESSAGE)
        message(FATAL_ERROR "Failed to download gettext: ${ERROR_MESSAGE}")
    endif()
    
    message(STATUS "Extracting gettext binaries...")
    file(ARCHIVE_EXTRACT
        INPUT "${GETTEXT_ARCHIVE}"
        DESTINATION "${GETTEXT_DIR}"
    )
    
    file(REMOVE "${GETTEXT_ARCHIVE}")
    message(STATUS "gettext installed to: ${GETTEXT_DIR}")
endif()

set(GETTEXT_MSGFMT_BINARY "${GETTEXT_DIR}/bin/msgfmt.exe" CACHE FILEPATH "Path to msgfmt executable" FORCE)

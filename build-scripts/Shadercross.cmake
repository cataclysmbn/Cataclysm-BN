include(FetchContent)

FetchContent_Declare(
    SDL3_SHADERCROSS
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_GetProperties(SDL3_SHADERCROSS)
if (NOT sdl3_shadercross_POPULATED)
    cmake_policy(PUSH)
    if (POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif ()
    FetchContent_Populate(SDL3_SHADERCROSS)
    cmake_policy(POP)
endif ()

set(SHADERCROSS_DXC_ROOT "${sdl3_shadercross_BINARY_DIR}/DirectXShaderCompiler-binaries")
set(DXC_ROOT "${SHADERCROSS_DXC_ROOT}")
include("${sdl3_shadercross_SOURCE_DIR}/build-scripts/download-prebuilt-DirectXShaderCompiler.cmake")
set(DirectXShaderCompiler_ROOT "${SHADERCROSS_DXC_ROOT}")
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    set(DirectXShaderCompiler_INCLUDE_PATH "${SHADERCROSS_DXC_ROOT}/linux/include/dxc")
    set(DirectXShaderCompiler_dxcompiler_LIBRARY "${SHADERCROSS_DXC_ROOT}/linux/lib/libdxcompiler.so")
    set(DirectXShaderCompiler_dxil_LIBRARY "${SHADERCROSS_DXC_ROOT}/linux/lib/libdxil.so")
endif ()

set(CATA_SAVED_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
set(CATA_SAVED_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CATA_SAVED_CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_CXX_FLAGS_DEBUG "")

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(SPIRV_CROSS_SKIP_INSTALL ON)
set(SPIRV_CROSS_CLI OFF)
set(SPIRV_CROSS_ENABLE_TESTS OFF)
set(SPIRV_CROSS_STATIC ON)
set(SPIRV_CROSS_SHARED OFF)
set(SPIRV_CROSS_FORCE_PIC ON)
add_subdirectory(
    "${sdl3_shadercross_SOURCE_DIR}/external/SPIRV-Cross"
    "${sdl3_shadercross_BINARY_DIR}/external/SPIRV-Cross"
    EXCLUDE_FROM_ALL)
if (TARGET spirv-cross-c AND NOT TARGET spirv_cross_c)
    add_library(spirv_cross_c ALIAS spirv-cross-c)
endif ()

set(SDLSHADERCROSS_CLI ON)
set(SDLSHADERCROSS_DXC ON)
set(SDLSHADERCROSS_INSTALL OFF)
set(SDLSHADERCROSS_SHARED ON)
set(SDLSHADERCROSS_STATIC OFF)
set(SDLSHADERCROSS_SPIRVCROSS_SHARED OFF)
set(SDLSHADERCROSS_TESTS OFF)
set(SDLSHADERCROSS_VENDORED OFF)
set(SDLSHADERCROSS_WERROR OFF)

add_subdirectory(
    "${sdl3_shadercross_SOURCE_DIR}"
    "${sdl3_shadercross_BINARY_DIR}"
    EXCLUDE_FROM_ALL)
set(CMAKE_C_FLAGS "${CATA_SAVED_CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CATA_SAVED_CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CATA_SAVED_CMAKE_CXX_FLAGS_DEBUG}")
set_target_properties(shadercross PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${sdl3_shadercross_BINARY_DIR}")

set(SHADERCROSS_EXE $<TARGET_FILE:shadercross>)
set(SHADERCROSS_TARGET shadercross)

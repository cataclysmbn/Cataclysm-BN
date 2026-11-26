cmake_minimum_required(VERSION 3.24)
project(CataShadercross LANGUAGES C CXX)

include(FetchContent)
find_package(SDL3 CONFIG QUIET)
if (NOT SDL3_FOUND)
    set(SDL_SHARED ON)
    set(SDL_STATIC OFF)
    set(SDL_TEST_LIBRARY OFF)
    set(SDL_TESTS OFF)
    set(SDL_INSTALL OFF)
    set(SDL_UNIX_CONSOLE_BUILD ON)
    set(SDL_X11 OFF)
    set(SDL_WAYLAND OFF)
    FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.8.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(SDL3)
endif ()

include("${CMAKE_CURRENT_LIST_DIR}/../Shadercross.cmake")
set_target_properties(shadercross PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

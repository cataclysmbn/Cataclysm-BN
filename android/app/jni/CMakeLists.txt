cmake_minimum_required(VERSION 3.22)

project(CataclysmBNAndroid C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(CATA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(CATA_SRC_DIR "${CATA_SOURCE_DIR}/src")

if(NOT EXISTS "${CATA_SRC_DIR}/version.h")
    string(TIMESTAMP CATA_BUILD_TIMESTAMP "%Y-%m-%d-%H%M")
    file(WRITE "${CATA_SRC_DIR}/version.h" "// NOLINT(cata-header-guard)\n#define VERSION \"unknown\"\n#define BUILD_TIMESTAMP \"${CATA_BUILD_TIMESTAMP}\"\n")
endif()

file(GLOB_RECURSE CATACLYSM_BN_SOURCES CONFIGURE_DEPENDS "${CATA_SRC_DIR}/*.cpp")
list(FILTER CATACLYSM_BN_SOURCES EXCLUDE REGEX "/src/(lua|sol|third-party)/")
file(GLOB LUA_SOURCES CONFIGURE_DEPENDS "${CATA_SRC_DIR}/lua/*.c")
file(GLOB SQLITE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/sqlite3/sqlite3.c")
file(GLOB CATALUA_BINDINGS_SOURCES CONFIGURE_DEPENDS "${CATA_SRC_DIR}/catalua_bindings*.cpp")

foreach(catalua_src IN LISTS CATALUA_BINDINGS_SOURCES)
    set_source_files_properties("${catalua_src}" PROPERTIES COMPILE_OPTIONS "-O0;-g0")
endforeach()

add_library(sqlite3 STATIC ${SQLITE_SOURCES})
target_include_directories(sqlite3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/sqlite3")
target_compile_definitions(sqlite3 PRIVATE
    HAVE_USLEEP=1
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_RTREE
    SQLITE_ENABLE_FTS3
    SQLITE_ENABLE_BATCH_ATOMIC_WRITE
    SQLITE_TEMP_STORE=3
    HAVE_CONFIG_H
    KHTML_NO_EXCEPTIONS
    GKWQ_NO_JAVA
    NO_SUPPORT_JS_BINDING
    QT_NO_WHEELEVENT
    KHTML_NO_XBL
    HAVE_STRCHRNUL=0
    SQLITE_USE_URI=1
    PACKED="")
target_compile_options(sqlite3 PRIVATE
    -Wno-unused-parameter
    -Wno-int-to-pointer-cast
    -Wno-uninitialized
    -Wno-parentheses)

function(auto_import_prebuilt_library target_name library_dir library_name)
    add_library(${target_name} SHARED IMPORTED)
    set_target_properties(${target_name} PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/${library_dir}/${ANDROID_ABI}/${library_name}")
endfunction()

function(auto_import_sdl_library target_name library_dir library_name)
    add_library(${target_name} SHARED IMPORTED)
    set_target_properties(${target_name} PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/${library_dir}/${ANDROID_ABI}/${library_name}")
    target_include_directories(${target_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/${library_dir}/include/")
endfunction()


auto_import_prebuilt_library(SDL3 "SDL3" "libSDL3.so")
auto_import_prebuilt_library(hidapi "libhidapi" "libhidapi.so")
auto_import_prebuilt_library(SDL3_image "SDL3_image" "libSDL3_image.so")
auto_import_prebuilt_library(SDL3_mixer "SDL3_mixer" "libSDL3_mixer.so")
auto_import_prebuilt_library(SDL3_ttf "SDL3_ttf" "libSDL3_ttf.so")

add_library(main SHARED ${CATACLYSM_BN_SOURCES} ${LUA_SOURCES})

target_include_directories(main PUBLIC
    "${CATA_SRC_DIR}"
    "${CATA_SRC_DIR}/lua"
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL3/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL3_image/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL3_mixer/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/SDL3_ttf/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/libhidapi")

target_compile_definitions(main PRIVATE
    TILES=1
    DYNAMIC_ATLAS=1
    SDL_SOUND=1
    LUA=1
    BACKTRACE=1)
target_compile_options(main PRIVATE
    -Wextra
    -Wall
    -Wno-unknown-warning-option
    -fsigned-char
    -ffast-math)
target_link_options(main PRIVATE -fuse-ld=lld)
target_link_libraries(main PRIVATE
    hidapi
    SDL3
    SDL3_image
    SDL3_mixer
    SDL3_ttf
    sqlite3
    GLESv1_CM
    GLESv2
    dl
    log
    z)

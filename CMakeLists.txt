cmake_minimum_required(VERSION 3.24)

project(CataclysmBN C CXX)

list(APPEND CMAKE_MODULE_PATH
        ${CMAKE_SOURCE_DIR}/CMakeModules)
if (NOT VCPKG_MANIFEST_MODE)
    list(APPEND CMAKE_MODULE_PATH
            ${CMAKE_SOURCE_DIR}/CMakeModules/Find)
endif()

include(AstyleFormatSource)

set(CMAKE_TLS_VERIFY ON)

# Build options
include(CMakeDependentOption)
include(ExternalProject)
option(TILES "Build graphical tileset version." "OFF")
cmake_dependent_option(DYNAMIC_ATLAS
    "Enable dynamic atlas generation" ON
    TILES OFF
)
option(CURSES "Build curses version." "ON")
option(SOUND "Support for in-game sounds & music." "OFF")
option(BACKTRACE "Support for printing stack backtraces on crash" "ON")
option(LIBBACKTRACE "Print backtrace with libbacktrace." "OFF")
option(USE_XDG_DIR "Use XDG directories for save and config files." "OFF")
# On Windows, default to current directory for backwards compatibility
# On other platforms, use home directory
if(WIN32)
    set(USE_HOME_DIR_DEFAULT OFF)
else()
    set(USE_HOME_DIR_DEFAULT ON)
endif()
option(USE_HOME_DIR "Use user's home directory for save and config files." ${USE_HOME_DIR_DEFAULT})
cmake_dependent_option(USE_PREFIX_DATA_DIR
    "Use UNIX system directories for game data in release build." ON
    "UNIX" OFF
)
set(LANGUAGES "" CACHE STRING
        "Compile localization files for specified languages. \
        List of language ids separated by semicolon. \
        Set to 'all' or leave empty to compile all, set to 'none' to compile none.")
option(DYNAMIC_LINKING
        "Use dynamic linking. Or use static to remove MinGW dependency instead." "ON")
option(USE_UNITY_BUILD "Use unity build." "OFF")
option(JSON_FORMAT "Build JSON formatter" "OFF")
option(CATA_CCACHE "Try to find and build with ccache" "ON")
option(TESTS "Compile Cata's tests" "ON")
option(CATA_CLANG_TIDY_PLUGIN "Build Cata's custom clang-tidy plugin" "OFF")
option(USE_TRACY "Use Tracy profiler" "OFF")
set(CATA_CLANG_TIDY_INCLUDE_DIR "" CACHE STRING
        "Path to internal clang-tidy headers required for plugin (e.g. ClangTidy.h)")
set(CATA_CHECK_CLANG_TIDY "" CACHE STRING "Path to check_clang_tidy.py for plugin tests")
set(GIT_BINARY "" CACHE STRING "Git binary name or path.")
set(LINKER "" CACHE STRING "Custom Linker to use")
set(GETTEXT_MSGFMT_BINARY "" CACHE FILEPATH "msgfmt binary name or path.")

if (TESTS)
include(CTest)
endif()

message(STATUS "${PROJECT_NAME} build environment --")
message(STATUS "Build realm is: ${CMAKE_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_PROCESSOR}")

if (LINKER)
    add_link_options("-fuse-ld=${LINKER}")
endif ()

# For multi-config generators (e.g. Visual Studio), CMAKE_BUILD_TYPE is not used;
# the configuration is selected at build time via --config flag.
get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT CMAKE_BUILD_TYPE AND NOT _is_multi_config)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

add_definitions(-DCMAKE)

include(GetGitRevisionDescription)
git_describe(GIT_VERSION --tags --always --match "[0-9A-Z]*.[0-9A-Z]*")
if (NOT "${GIT_VERSION}" MATCHES "GIT-NOTFOUND")
    string(REPLACE "-NOTFOUND" "" GIT_VERSION ${GIT_VERSION})
    string(TIMESTAMP _timestamp %Y-%m-%d-%H%M)
    file(WRITE ${CMAKE_SOURCE_DIR}/src/version.h
         "// NOLINT(cata-header-guard)\n\#define VERSION \"${GIT_VERSION}\"\n\#define BUILD_TIMESTAMP \"${_timestamp}\"\n")
    message(STATUS "${PROJECT_NAME} build version is: ${GIT_VERSION}")
    add_definitions(-DGIT_VERSION)

    # get_git_head_revision() does not work with worktrees in Windows
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE _sha1
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    file(WRITE VERSION.txt "\
build type: ${BUILD_PRESET_NAME}\n\
build number: ${_timestamp}\n\
commit sha: ${_sha1}\n\
commit url: https://github.com/cataclysmbn/Cataclysm-BN/commit/${_sha1}"
    )
endif ()

#OS Check Placeholders. Will be used for BINDIST
if ("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
    set(_OS_LINUX_ 1)
endif ()

if ("${CMAKE_SYSTEM_NAME}" MATCHES "FreeBSD")
    set(_OS_FREEBSD_ 1)
endif ()

if ("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
    set(_OS_DARWIN_ 1)
    add_definitions(-DMACOSX)
    if (TILES)
        add_definitions(-DOSX_SDL2_LIBS)
    endif ()
endif ()

include(CheckCXXCompilerFlag)

#FIXME: Add dest build choice: m32 for 32 bit or m64 for 64 bit version
#add_definitions("-m32")
#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
#SET(CMAKE_SHARED_LIBRARY_C_FLAGS "${CMAKE_SHARED_LIBRARY_C_FLAGS} -m32")
#SET(CMAKE_SHARED_LIBRARY_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CXX_FLAGS} -m32")

find_package(PkgConfig)
find_package(SQLite3)
find_package(ZLIB)
if (NOT DYNAMIC_LINKING)
    if(NOT MSVC)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a;.dll.a")
    endif()
    set(BUILD_SHARED_LIBRARIES OFF)
    check_cxx_compiler_flag (-static HAVE_STATIC_FLAG)
    if (HAVE_STATIC_FLAG)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    endif ()
    # Workaround for cmake link library guesser
    set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)       # remove -Wl,-Bdynamic
    set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
    set(CMAKE_SHARED_LIBRARY_C_FLAGS)         # remove -fPIC
    set(CMAKE_SHARED_LIBRARY_CXX_FLAGS)
    set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)    # remove -rdynamic
    set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
endif ()

message(STATUS "${PROJECT_NAME} build options --")

# Preset variables
if (NOT LANGUAGES)
    set(LANGUAGES all)
endif ()

if (GIT_BINARY)
    set(GIT_EXECUTABLE ${GIT_BINARY})
else ()
    find_package(Git)
    if (NOT GIT_FOUND)
        message(WARNING
            "Git binary not found. Build version will be set to NULL. \
            Install Git package or use -DGIT_BINARY to set path to git binary.")
    endif ()
endif ()

# Can't compile curses and tiles build's at same time
if (TILES)
    set(CURSES OFF)
endif ()

# Can't use both home and xdg directories
if (USE_XDG_DIR)
    set(USE_HOME_DIR OFF)
endif ()

# Set build types and display info
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(RELEASE 0)
    message(STATUS "Build ${PROJECT_NAME} in development mode --")
    message(STATUS "Binaries will be located in: " ${CMAKE_SOURCE_DIR})
    set(CMAKE_VERBOSE_MAKEFILE ON)
    # Since CataclysmBN does not respect PREFIX for development builds
    # and has funny path handlers, we should create resulting Binaries
    # in the source directory
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR} CACHE PATH
            "Single Directory for all Executables.")
    set(CMAKE_INSTALL_BINDIR .)
else ()
    set(RELEASE 1)
    add_definitions(-DRELEASE)
    # Use CMAKE_INSTALL_PREFIX as storage of data,gfx, etc.. Useful only on *nix OS.
    if(USE_PREFIX_DATA_DIR)
        if ("${CMAKE_SYSTEM_NAME}" MATCHES "(Linux|FreeBSD|Darwin)")
            set(CMAKE_INSTALL_DATAROOTDIR "${CMAKE_INSTALL_PREFIX}/share/cataclysm-bn")
            add_definitions(-DPREFIX=${CMAKE_INSTALL_PREFIX})
            configure_file(
                "${CMAKE_SOURCE_DIR}/src/prefix.h.in"
                "${CMAKE_SOURCE_DIR}/src/prefix.h"
                @ONLY)
            add_definitions(-DDATA_DIR_PREFIX)
        endif ()
    else()
        set(CMAKE_INSTALL_DATADIR   data)
        set(CMAKE_INSTALL_DOCDIR    .   )
        set(CMAKE_INSTALL_LOCALEDIR lang)
        set(CMAKE_INSTALL_BINDIR    .   )
    endif ()
    message(STATUS "CMAKE_INSTALL_PREFIX          : ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "CMAKE_INSTALL_BINDIR          : ${CMAKE_INSTALL_BINDIR}")
    message(STATUS "DESKTOP_ENTRY_PATH            : ${DESKTOP_ENTRY_PATH}")
    message(STATUS "PIXMAPS_ENTRY_PATH            : ${PIXMAPS_ENTRY_PATH}")
    message(STATUS "PIXMAPS_UNITY_ENTRY_PATH      : ${PIXMAPS_UNITY_ENTRY_PATH}")
    message(STATUS "MANPAGE_ENTRY_PATH            : ${MANPAGE_ENTRY_PATH}")
endif ()

message(STATUS "GIT_BINARY                    : ${GIT_EXECUTABLE}")
message(STATUS "DYNAMIC_LINKING               : ${DYNAMIC_LINKING}")
message(STATUS "TILES                         : ${TILES}")
message(STATUS "DYNAMIC_ATLAS                 : ${DYNAMIC_ATLAS}")
message(STATUS "CURSES                        : ${CURSES}")
message(STATUS "SOUND                         : ${SOUND}")
message(STATUS "BACKTRACE                     : ${BACKTRACE}")
message(STATUS "LIBBACKTRACE                  : ${LIBBACKTRACE}")
message(STATUS "USE_TRACY                     : ${USE_TRACY}")
message(STATUS "USE_XDG_DIR                   : ${USE_XDG_DIR}")
message(STATUS "USE_HOME_DIR                  : ${USE_HOME_DIR}")
message(STATUS "UNITY_BUILD                   : ${USE_UNITY_BUILD}")
message(STATUS "LANGUAGES                     : ${LANGUAGES}")
if (LINKER)
    message(STATUS "LINKER                        : ${LINKER}")
endif ()

message(STATUS "See CMake compiling guide for details and more info --")

if (NOT MSVC)
    set(CATA_WARNINGS
        "-Wall -Wextra \
       -Wformat-signedness \
       -Wlogical-op \
       -Wmissing-declarations \
       -Wmissing-noreturn \
       -Wnon-virtual-dtor \
       -Wold-style-cast \
       -Woverloaded-virtual \
       -Wpedantic \
       -Wsuggest-override \
       -Wunused-macros \
       -Wno-zero-as-null-pointer-constant \
       -Wno-unknown-warning-option \
       -Wno-dangling-reference \
       -Wno-range-loop-analysis")
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
       set(CATA_WARNINGS "${CATA_WARNINGS} -Wno-unknown-warning-option")
    else()
       set(CATA_WARNINGS "${CATA_WARNINGS} -Wno-unknown-warning")
    endif()
    if (NOT "${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
        set(CATA_WARNINGS "${CATA_WARNINGS} -Wredundant-decls")
    endif ()
    set(CATA_OTHER_FLAGS "${CATA_OTHER_FLAGS} -fsigned-char -fno-builtin-std-forward_like -fno-finite-math-only")
    # Add debug symbols for non-release builds or when backtrace is enabled
    if (BACKTRACE OR NOT RELEASE)
        set(CATA_OTHER_FLAGS "${CATA_OTHER_FLAGS} -g1")
        # Use debug prefix map for ccache compatibility with git worktrees
        # see https://ccache.dev/manual/latest.html#_compiling_in_different_directories
        file(RELATIVE_PATH REL_SRC_PATH "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}")
        set(CATA_OTHER_FLAGS "${CATA_OTHER_FLAGS} -ffile-prefix-map=${REL_SRC_PATH}=.")
    endif()
    # Compact the whitespace in the warning string
    string(REGEX REPLACE "[\t ]+" " " CATA_WARNINGS "${CATA_WARNINGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CATA_WARNINGS} ${CATA_OTHER_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "-Og -g2")
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force out-of-source build
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR
            "This project requires an out of source build. \
            Remove the file 'CMakeCache.txt' found in this directory before continuing; \
            create a separate build directory and run 'cmake [options] <srcs>' from there. \
            See CMake compiling guide for details and more info.")
endif ()

#set(THREADS_USE_PTHREADS_WIN32 True)
set(CMAKE_THREAD_PREFER_PTHREAD True)
find_package(Threads REQUIRED)

if (USE_TRACY)
    include(FetchContent)

    if (NOT TRACY_VERSION)
        if("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
            set(TRACY_VERSION 6d1deb5640ed11da01995fb1791115cfebe54dbf)
        else()
            set(TRACY_VERSION v0.10)
        endif()
        message(STATUS "Using tracy version: ${TRACY_VERSION}")
    endif()

    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG "${TRACY_VERSION}"
        GIT_PROGRESS TRUE)

    FetchContent_MakeAvailable(tracy)
    set_target_properties(TracyClient PROPERTIES TRACY_ENABLE ON TRACY_ON_DEMAND ON)

    if(NOT MSVC)
        target_compile_options(TracyClient PRIVATE "-Wno-everything")
    endif()
    if (NOT DYNAMIC_LINKING)
        set_target_properties(TracyClient PROPERTIES TRACY_STATIC ON)
    endif ()
endif ()

# Check for build types and libraries
if (TILES)
    # Find SDL, SDL_ttf & SDL_image for graphical install
    message(STATUS "Searching for SDL2 library --")
    if (NOT CMAKE_FIND_PACKAGE_PREFER_CONFIG)
        set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
        # ^^ Use sdl2-config.cmake provided by the system or VCPKG
        find_package(SDL2)
        if(NOT SDL2_FOUND)
            # Use our CMakeModules/Find/FindSDL2.cmake
            set(CMAKE_FIND_PACKAGE_PREFER_CONFIG OFF)
            find_package(SDL2)
        endif()
        set(CMAKE_FIND_PACKAGE_PREFER_CONFIG OFF)
    endif()
    if (NOT (SDL2_FOUND OR TARGET SDL2::SDL2 OR TARGET SDL2::SDL2-static))
        message(FATAL_ERROR
                "This project requires SDL2 to be installed to compile in graphical mode.  \
                 Please install the SDL2 development libraries, \
                 or try compiling without -DTILES=1 for a text-only compilation. \
                 See CMake compiling guide for details and more info.")
    endif ()

    message(STATUS "Searching for SDL2_TTF library --")
    find_package(SDL2_ttf)
    if (NOT (SDL2_TTF_FOUND OR TARGET SDL2::SDL2_ttf OR TARGET SDL2_ttf::SDL2_ttf-static))
        message(FATAL_ERROR
                "This project requires SDL2_ttf to be installed to compile in graphical mode. \
                Please install the SDL2_ttf development libraries, \
                or try compiling without -DTILES=1 for a text-only compilation. \
                See CMake compiling guide for details and more info.")
    endif ()

    message(STATUS "Searching for SDL2_image library --")
    find_package(SDL2_image)
    if (NOT (SDL2_IMAGE_FOUND OR TARGET SDL2::SDL2_image OR TARGET SDL2_image::SDL2_image-static))
        message(FATAL_ERROR
                "This project requires SDL2_image to be installed to compile in graphical mode. \
                 Please install the SDL2_image development libraries, \
                 or try compiling without -DTILES=1 for a text-only compilation. \
                 See doc/COMPILING/COMPILING-CMAKE.md for details and more info.")
    endif ()
    add_definitions(-DTILES)
endif ()

if (CURSES)
    # Find the ncurses library for a text based compile
    message(STATUS "Searching for Curses library --")
    set(CURSES_NEED_NCURSES TRUE)
    set(CURSES_NEED_WIDE TRUE)
    find_package(Curses)
    if (NOT CURSES_FOUND)
        message(FATAL_ERROR
                "This project requires ncurses to be installed to be compiled in text-only mode. \
                Please install the ncurses development libraries, \
                or try compiling with -DTILES=1 for a graphical compilation. \
                See CMake compiling guide for details and more info.")
    endif ()
    if (NOT DYNAMIC_LINKING)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNCURSES_STATIC")
    endif ()
endif ()

if (SOUND)
    # You need TILES to be able to use SOUND
    if (NOT TILES)
        message(FATAL_ERROR
                "You must enable graphical support with -DTILES=1 \
                to be able to enable sound support. \
                See CMake compiling guide for details and more info.")
    endif ()

    # Sound requires SDL_mixer library
    message(STATUS "Searching for SDL2_mixer library --")
    find_package(SDL2_mixer)
    if (NOT (SDL2_MIXER_FOUND OR TARGET SDL2_mixer::SDL2_mixer
                              OR TARGET SDL2_mixer::SDL2_mixer-static))
        message(FATAL_ERROR
                "You need the SDL2_mixer development library \
                    to be able to compile with sound enabled. \
                    See CMake compiling guide for details and more info.")
    endif()
endif ()

if (BACKTRACE)
    add_definitions(-DBACKTRACE)
    if (LIBBACKTRACE AND NOT MSVC)
        message(STATUS "Integrating libbacktrace build using ExternalProject")

        set(LIBBACKTRACE_GIT_REPO https://github.com/ianlancetaylor/libbacktrace.git)
        set(LIBBACKTRACE_GIT_TAG master)

        set(LIBBACKTRACE_INSTALL_DIR "${CMAKE_BINARY_DIR}/libbacktrace/install")
        SET(LIBBACKTRACE_LIBRARY "${LIBBACKTRACE_INSTALL_DIR}/lib/libbacktrace.a")

        include(ProcessorCount)
        ProcessorCount(NCPU)
        if(NCPU EQUAL 0)
            set(NCPU 1)
        endif()

        # Set up ccache for libbacktrace if available
        if (CCACHE_FOUND AND CATA_CCACHE AND CMAKE_C_COMPILER_LAUNCHER)
            # Join compiler launcher and compiler into a single string for configure
            set(LIBBACKTRACE_CC_LIST "${CMAKE_C_COMPILER_LAUNCHER}" "${CMAKE_C_COMPILER}")
            set(LIBBACKTRACE_CXX_LIST "${CMAKE_CXX_COMPILER_LAUNCHER}" "${CMAKE_CXX_COMPILER}")
            list(JOIN LIBBACKTRACE_CC_LIST " " LIBBACKTRACE_CC)
            list(JOIN LIBBACKTRACE_CXX_LIST " " LIBBACKTRACE_CXX)
        else()
            set(LIBBACKTRACE_CC "${CMAKE_C_COMPILER}")
            set(LIBBACKTRACE_CXX "${CMAKE_CXX_COMPILER}")
        endif()

        ExternalProject_Add(
            libbacktrace_external
            GIT_REPOSITORY ${LIBBACKTRACE_GIT_REPO}
            GIT_TAG ${LIBBACKTRACE_GIT_TAG}
            GIT_SHALLOW TRUE

            SOURCE_DIR "${CMAKE_BINARY_DIR}/libbacktrace/src"
            BINARY_DIR "${CMAKE_BINARY_DIR}/libbacktrace/build"

            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
                CC=${LIBBACKTRACE_CC}
                CXX=${LIBBACKTRACE_CXX}
                <SOURCE_DIR>/configure
                --prefix=${LIBBACKTRACE_INSTALL_DIR}

            BUILD_COMMAND make -j${NCPU}

            INSTALL_COMMAND make install

            INSTALL_DIR ${LIBBACKTRACE_INSTALL_DIR}
            BUILD_BYPRODUCTS ${LIBBACKTRACE_LIBRARY}

            UPDATE_DISCONNECTED 1
        )

        add_library(backtrace SHARED IMPORTED)
        set_target_properties(backtrace
            PROPERTIES IMPORTED_LOCATION ${LIBBACKTRACE_LIBRARY}
        )
        # Hack to make it work, otherwise INTERFACE_INCLUDE_DIRECTORIES will not be propagated
        file(MAKE_DIRECTORY "${LIBBACKTRACE_INSTALL_DIR}/include")
        target_include_directories(backtrace INTERFACE
            "${LIBBACKTRACE_INSTALL_DIR}/include"
        )
        add_dependencies(backtrace libbacktrace_external)
    endif ()
    if (LIBBACKTRACE)
        add_definitions(-DLIBBACKTRACE)
    endif ()
endif ()

if (NOT (LANGUAGES STREQUAL "none") AND "${GETTEXT_MSGFMT_BINARY}" STREQUAL "")
    if(MSVC)
        list(APPEND Gettext_ROOT C:\\msys64\\usr)
        list(APPEND Gettext_ROOT C:\\Program\ Files\\Git\\usr)
    endif(MSVC)
    find_package(Gettext)
endif ()
if (NOT GETTEXT_MSGFMT_EXECUTABLE )
    set(GETTEXT_MSGFMT_EXECUTABLE "${GETTEXT_MSGFMT_BINARY}")
endif()

# Ok. Now create build and install recipes
add_subdirectory(lang)

if (USE_HOME_DIR)
    add_definitions(-DUSE_HOME_DIR)
endif ()

if (USE_XDG_DIR)
    add_definitions(-DUSE_XDG_DIR)
endif ()

find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND AND CATA_CCACHE)
    # Configure ccache for git worktrees
    # Set base_dir to make paths relative, enabling cache sharing across worktrees
    # see https://ccache.dev/manual/latest.html#_configuration_options
    set(CATA_CCACHE_CMD
        "${CMAKE_COMMAND}" -E env
        "CCACHE_BASEDIR=${CMAKE_SOURCE_DIR}"
        "CCACHE_NOHASHDIR=true"
        "${CCACHE_FOUND}"
    )

    set(CMAKE_C_COMPILER_LAUNCHER   ${CATA_CCACHE_CMD})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CATA_CCACHE_CMD})
    set(CMAKE_C_LINKER_LAUNCHER     ${CATA_CCACHE_CMD})
    set(CMAKE_CXX_LINKER_LAUNCHER   ${CATA_CCACHE_CMD})
endif ()

add_subdirectory(src)
add_subdirectory(data)

if(TESTS)
    add_subdirectory(tests)
endif()

if (JSON_FORMAT)
    add_subdirectory(tools/format)
endif()
if (CATA_CLANG_TIDY_PLUGIN)
    add_subdirectory(tools/clang-tidy-plugin)
endif()

if (RELEASE)
    if (USE_PREFIX_DATA_DIR)
        install(DIRECTORY doc TYPE DOC)
        install(DIRECTORY data TYPE DATA
            PATTERN "json/external_tileset/README.md" EXCLUDE
            PATTERN "json/mapgen/lab/README.md" EXCLUDE)
        install(FILES
            ${CMAKE_SOURCE_DIR}/README.md
            ${CMAKE_SOURCE_DIR}/LICENSE.txt
            ${CMAKE_SOURCE_DIR}/LICENSE-OFL-Terminus-Font.txt
            ${CMAKE_SOURCE_DIR}/VERSION.txt
            TYPE DOC)
    else()
        install(DIRECTORY doc DESTINATION .)
        install(DIRECTORY data DESTINATION .
            PATTERN "json/external_tileset/README.md" EXCLUDE
            PATTERN "json/mapgen/lab/README.md" EXCLUDE)
        install(FILES
            ${CMAKE_SOURCE_DIR}/README.md
            ${CMAKE_SOURCE_DIR}/LICENSE.txt
            ${CMAKE_SOURCE_DIR}/LICENSE-OFL-Terminus-Font.txt
            ${CMAKE_SOURCE_DIR}/VERSION.txt
            DESTINATION .)
    endif()
    if (TILES)
        if (USE_PREFIX_DATA_DIR)
            install(DIRECTORY ${CMAKE_SOURCE_DIR}/gfx
                TYPE DATA)
        else()
            install(DIRECTORY ${CMAKE_SOURCE_DIR}/gfx
                DESTINATION .)
        endif()
    endif()
    # Install cataclysm-launcher for portable Linux builds
    if (NOT USE_PREFIX_DATA_DIR AND UNIX AND NOT APPLE)
        install(PROGRAMS ${CMAKE_SOURCE_DIR}/cataclysm-launcher
            DESTINATION .)
    endif()
endif()

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY)

add_custom_target(uninstall
        "${CMAKE_COMMAND}"
        -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

if (USE_UNITY_BUILD)
    set_property(GLOBAL PROPERTY UNITY_BUILD ON)
endif ()

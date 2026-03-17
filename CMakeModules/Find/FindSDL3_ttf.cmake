#.rst:
# FindSDL3_ttf
# -----------
#
# Locate SDL3_ttf library (fallback module — SDL3_ttfConfig.cmake is preferred)
#
# This module defines:
#
#   SDL3_TTF_LIBRARIES, the name of the library to link against
#   SDL3_TTF_INCLUDE_DIRS, where to find the headers
#   SDL3_TTF_FOUND, if false, do not try to link against
#   SDL3_TTF_VERSION_STRING - human-readable string containing the version of SDL3_ttf

find_path(SDL3_TTF_INCLUDE_DIR SDL3/SDL_ttf.h
  HINTS
    ENV SDL3TTFDIR
    ENV SDL3DIR
    ${CMAKE_SOURCE_DIR}/dep/
  PATH_SUFFIXES SDL3
                include/SDL3 include
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(VC_LIB_PATH_SUFFIX lib/x64)
else()
  set(VC_LIB_PATH_SUFFIX lib/x86)
endif()

find_library(SDL3_TTF_LIBRARY
  NAMES SDL3_ttf
  HINTS
    ENV SDL3TTFDIR
    ENV SDL3DIR
    ${CMAKE_SOURCE_DIR}/dep/
  PATH_SUFFIXES lib ${VC_LIB_PATH_SUFFIX}
)

set(SDL3_TTF_LIBRARIES ${SDL3_TTF_LIBRARY})
set(SDL3_TTF_INCLUDE_DIRS ${SDL3_TTF_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SDL3_ttf
                                  REQUIRED_VARS SDL3_TTF_LIBRARIES SDL3_TTF_INCLUDE_DIRS
                                  VERSION_VAR SDL3_TTF_VERSION_STRING)

mark_as_advanced(SDL3_TTF_LIBRARY SDL3_TTF_INCLUDE_DIR)

if (NOT DYNAMIC_LINKING AND PKG_CONFIG_FOUND)
  if (NOT TARGET SDL3_ttf::SDL3_ttf-static)
    add_library(SDL3_ttf::SDL3_ttf-static STATIC IMPORTED)
    set_target_properties(SDL3_ttf::SDL3_ttf-static PROPERTIES
      IMPORTED_LOCATION ${SDL3_TTF_LIBRARY}
      INTERFACE_INCLUDE_DIRECTORIES ${SDL3_TTF_INCLUDE_DIRS}
    )
  endif()
  message(STATUS "Searching for SDL3_ttf deps libraries --")
  find_package(Freetype REQUIRED)
  find_package(Harfbuzz REQUIRED)
  target_link_libraries(SDL3_ttf::SDL3_ttf-static INTERFACE
    Freetype::Freetype
    harfbuzz::harfbuzz
  )
  pkg_check_modules(BROTLI REQUIRED IMPORTED_TARGET libbrotlidec libbrotlicommon)
  target_link_libraries(Freetype::Freetype INTERFACE
    PkgConfig::BROTLI
  )
elseif(NOT TARGET SDL3_ttf::SDL3_ttf)
    add_library(SDL3_ttf::SDL3_ttf UNKNOWN IMPORTED)
    set_target_properties(SDL3_ttf::SDL3_ttf PROPERTIES
      IMPORTED_LOCATION ${SDL3_TTF_LIBRARY}
      INTERFACE_INCLUDE_DIRECTORIES ${SDL3_TTF_INCLUDE_DIRS}
    )
endif()

#.rst:
# FindSDL3_mixer
# -------------
#
# Locate SDL3_mixer library (fallback module — SDL3_mixerConfig.cmake is preferred)
#
# This module defines:
#
#   SDL3_MIXER_LIBRARIES, the name of the library to link against
#   SDL3_MIXER_INCLUDE_DIRS, where to find the headers
#   SDL3_MIXER_FOUND, if false, do not try to link against
#   SDL3_MIXER_VERSION_STRING - human-readable string containing the version of SDL3_mixer

find_path(SDL3_MIXER_INCLUDE_DIR SDL3/SDL_mixer.h
  HINTS
    ENV SDL3MIXERDIR
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

find_library(SDL3_MIXER_LIBRARY
  NAMES SDL3_mixer
  HINTS
    ENV SDL3MIXERDIR
    ENV SDL3DIR
    ${CMAKE_SOURCE_DIR}/dep/
  PATH_SUFFIXES lib ${VC_LIB_PATH_SUFFIX}
)

set(SDL3_MIXER_LIBRARIES ${SDL3_MIXER_LIBRARY})
set(SDL3_MIXER_INCLUDE_DIRS ${SDL3_MIXER_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SDL3_mixer
                                  REQUIRED_VARS SDL3_MIXER_LIBRARIES SDL3_MIXER_INCLUDE_DIRS
                                  VERSION_VAR SDL3_MIXER_VERSION_STRING)

mark_as_advanced(SDL3_MIXER_LIBRARY SDL3_MIXER_INCLUDE_DIR)

if(NOT DYNAMIC_LINKING)
  if (NOT TARGET SDL3_mixer::SDL3_mixer-static)
    add_library(SDL3_mixer::SDL3_mixer-static STATIC IMPORTED)
    set_target_properties(SDL3_mixer::SDL3_mixer-static PROPERTIES
      IMPORTED_LOCATION ${SDL3_MIXER_LIBRARY}
      INTERFACE_INCLUDE_DIRECTORIES ${SDL3_MIXER_INCLUDE_DIRS}
    )
  endif()
elseif(NOT TARGET SDL3_mixer::SDL3_mixer)
    add_library(SDL3_mixer::SDL3_mixer UNKNOWN IMPORTED)
    set_target_properties(SDL3_mixer::SDL3_mixer PROPERTIES
      IMPORTED_LOCATION ${SDL3_MIXER_LIBRARY}
      INTERFACE_INCLUDE_DIRECTORIES ${SDL3_MIXER_INCLUDE_DIRS}
    )
endif()

if(PKG_CONFIG_FOUND)
    message(STATUS "Searching for SDL3_mixer deps libraries --")
    if(TARGET SDL3_mixer::SDL3_mixer-static)
        pkg_check_modules(FLAC REQUIRED IMPORTED_TARGET flac)
        target_link_libraries(SDL3_mixer::SDL3_mixer-static INTERFACE
            PkgConfig::FLAC
        )
    elseif(TARGET SDL3_mixer::SDL3_mixer)
        pkg_check_modules(FLAC REQUIRED IMPORTED_TARGET flac)
        target_link_libraries(SDL3_mixer::SDL3_mixer INTERFACE
            PkgConfig::FLAC
        )
    endif()
endif()

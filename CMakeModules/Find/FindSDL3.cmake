#.rst:
# FindSDL3
# -------
#
# Locate SDL3 library (fallback module — SDL3Config.cmake is preferred)
#
# This module defines:
#
#   SDL3_LIBRARY, the name of the library to link against
#   SDL3_FOUND, if false, do not try to link to SDL3
#   SDL3_INCLUDE_DIR, where to find SDL3/SDL.h
#   SDL3_VERSION_STRING, human-readable string containing the version of SDL3

find_path(SDL3_INCLUDE_DIR SDL3/SDL.h
  HINTS
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

find_library(SDL3_LIBRARY_TEMP
  NAMES SDL3
  HINTS
    ENV SDL3DIR
    ${CMAKE_SOURCE_DIR}/dep/
  PATH_SUFFIXES lib ${VC_LIB_PATH_SUFFIX}
)

if(NOT SDL3_BUILDING_LIBRARY)
  if(NOT SDL3_INCLUDE_DIR MATCHES ".framework")
    find_library(SDL3MAIN_LIBRARY
      NAMES SDL3main
      HINTS
        ENV SDL3DIR
        ${CMAKE_SOURCE_DIR}/dep/
      PATH_SUFFIXES lib ${VC_LIB_PATH_SUFFIX}
    )
  endif()
endif()

if(NOT APPLE)
  find_package(Threads)
endif()

if(SDL3_LIBRARY_TEMP)
  if(SDL3MAIN_LIBRARY AND NOT SDL3_BUILDING_LIBRARY)
    list(FIND SDL3_LIBRARY_TEMP "${SDL3MAIN_LIBRARY}" _SDL3_MAIN_INDEX)
    if(_SDL3_MAIN_INDEX EQUAL -1)
      set(SDL3_LIBRARY_TEMP "${SDL3MAIN_LIBRARY}" ${SDL3_LIBRARY_TEMP})
    endif()
    unset(_SDL3_MAIN_INDEX)
  endif()

  if(APPLE)
    set(SDL3_LIBRARY_TEMP ${SDL3_LIBRARY_TEMP} "-framework Cocoa")
  endif()

  if(NOT APPLE)
    set(SDL3_LIBRARY_TEMP ${SDL3_LIBRARY_TEMP} ${CMAKE_THREAD_LIBS_INIT})
  endif()

  set(SDL3_LIBRARY ${SDL3_LIBRARY_TEMP} CACHE STRING "Where the SDL3 Library can be found")
  set(SDL3_LIBRARY_TEMP "${SDL3_LIBRARY_TEMP}" CACHE INTERNAL "")
endif()

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SDL3
                                  REQUIRED_VARS SDL3_LIBRARY SDL3_INCLUDE_DIR
                                  VERSION_VAR SDL3_VERSION_STRING)

if(SDL3_FOUND AND NOT TARGET SDL3::SDL3)
  if(NOT DYNAMIC_LINKING)
    add_library(SDL3::SDL3-static STATIC IMPORTED)
    set_target_properties(SDL3::SDL3-static PROPERTIES
      IMPORTED_LOCATION ${SDL3_LIBRARY}
      INTERFACE_INCLUDE_DIRECTORIES ${SDL3_INCLUDE_DIR}
    )
  else()
    add_library(SDL3::SDL3 UNKNOWN IMPORTED)
    set_target_properties(SDL3::SDL3 PROPERTIES
      IMPORTED_LOCATION ${SDL3_LIBRARY}
      INTERFACE_INCLUDE_DIRECTORIES ${SDL3_INCLUDE_DIR}
    )
  endif()
endif()

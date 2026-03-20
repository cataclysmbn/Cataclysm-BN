#.rst:
# FindSDL3_image
# -------------
#
# Locate SDL3_image library (fallback module — SDL3_imageConfig.cmake is preferred)
#
# This module defines:
#
#   SDL3_IMAGE_LIBRARIES, the name of the library to link against
#   SDL3_IMAGE_INCLUDE_DIRS, where to find the headers
#   SDL3_IMAGE_FOUND, if false, do not try to link against
#   SDL3_IMAGE_VERSION_STRING - human-readable string containing the version of SDL3_image

find_path(SDL3_IMAGE_INCLUDE_DIR SDL3/SDL_image.h
  HINTS
    ENV SDL3IMAGEDIR
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

find_library(SDL3_IMAGE_LIBRARY
  NAMES SDL3_image
  HINTS
    ENV SDL3IMAGEDIR
    ENV SDL3DIR
    ${CMAKE_SOURCE_DIR}/dep/
  PATH_SUFFIXES lib ${VC_LIB_PATH_SUFFIX}
)

set(SDL3_IMAGE_LIBRARIES ${SDL3_IMAGE_LIBRARY})
set(SDL3_IMAGE_INCLUDE_DIRS ${SDL3_IMAGE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SDL3_image
                                  REQUIRED_VARS SDL3_IMAGE_LIBRARIES SDL3_IMAGE_INCLUDE_DIRS
                                  VERSION_VAR SDL3_IMAGE_VERSION_STRING)

mark_as_advanced(SDL3_IMAGE_LIBRARY SDL3_IMAGE_INCLUDE_DIR)

if(NOT DYNAMIC_LINKING AND PKG_CONFIG_FOUND)
    if (NOT TARGET SDL3_image::SDL3_image-static)
      add_library(SDL3_image::SDL3_image-static STATIC IMPORTED)
      set_property(TARGET SDL3_image::SDL3_image-static
        PROPERTY IMPORTED_LOCATION ${SDL3_IMAGE_LIBRARY}
      )
    endif()
    message(STATUS "Searching for SDL3_image deps libraries --")
    find_package(JPEG REQUIRED)
    find_package(PNG REQUIRED)
    find_package(TIFF REQUIRED)
    find_library(JBIG jbig REQUIRED)
    find_package(LibLZMA REQUIRED)
    target_link_libraries(SDL3_image::SDL3_image-static INTERFACE
      JPEG::JPEG
      PNG::PNG
      TIFF::TIFF
      ${JBIG}
      LibLZMA::LibLZMA
      ${ZSTD}
    )
    pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp)
    pkg_check_modules(ZIP REQUIRED IMPORTED_TARGET libzip)
    pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)
    pkg_check_modules(DEFLATE REQUIRED IMPORTED_TARGET libdeflate)
    target_link_libraries(SDL3_image::SDL3_image-static INTERFACE
      PkgConfig::WEBP
      PkgConfig::ZIP
      PkgConfig::ZSTD
      PkgConfig::DEFLATE
    )
elseif(NOT TARGET SDL3_image::SDL3_image)
      add_library(SDL3_image::SDL3_image UNKNOWN IMPORTED)
      set_target_properties(SDL3_image::SDL3_image PROPERTIES
          IMPORTED_LOCATION ${SDL3_IMAGE_LIBRARY}
          INTERFACE_INCLUDE_DIRECTORIES ${SDL3_IMAGE_INCLUDE_DIRS}
      )
    target_link_libraries(SDL3_image::SDL3_image INTERFACE
      z
    )
endif()

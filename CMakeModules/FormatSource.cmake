find_program(CLANG_FORMAT_EXE clang-format)
find_program(ASTYLE_EXE astyle)

set(FORMAT_COMMANDS "")

if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE CLANG_FORMAT_SOURCES
        "${CMAKE_SOURCE_DIR}/src/*/**/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*/**/*.h"
        "${CMAKE_SOURCE_DIR}/src/*/**/*.hpp"
    )
    # Exclude lua and sol directories
    if(CLANG_FORMAT_SOURCES)
        list(FILTER CLANG_FORMAT_SOURCES EXCLUDE REGEX "/src/(lua|sol)/")
        list(APPEND FORMAT_COMMANDS COMMAND ${CLANG_FORMAT_EXE} -i ${CLANG_FORMAT_SOURCES})
        # exclude src/*.{cpp,h} files to be handled by astyle
        # list(FILTER CLANG_FORMAT_SOURCES EXCLUDE REGEX "/src/[^/]+\.(cpp|h)$")
    endif()
else()
    message(WARNING "clang-format not found - subdirectory formatting will be skipped.")
endif()

if(ASTYLE_EXE)
    set(ASTYLE_OPTIONS_FILE "${CMAKE_SOURCE_DIR}/.astylerc")
    file(GLOB ASTYLE_SOURCES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.h"
        "${CMAKE_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_SOURCE_DIR}/tests/*.h"
        "${CMAKE_SOURCE_DIR}/tools/format/*.cpp"
        "${CMAKE_SOURCE_DIR}/tools/format/*.h"
        "${CMAKE_SOURCE_DIR}/tools/clang-tidy-plugin/*.cpp"
        "${CMAKE_SOURCE_DIR}/tools/clang-tidy-plugin/*.h"
    )

    if(ASTYLE_SOURCES)
        list(APPEND FORMAT_COMMANDS COMMAND ${ASTYLE_EXE} --options=${ASTYLE_OPTIONS_FILE} -n ${ASTYLE_SOURCES})
    endif()
else()
    message(WARNING "astyle not found - standard file formatting will be skipped.")
endif()

if(FORMAT_COMMANDS)
    add_custom_target(format
        ${FORMAT_COMMANDS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format and astyle..."
        VERBATIM
    )

    # Legacy aliases for backwards compatibility
    add_custom_target(astyle DEPENDS format)
    add_custom_target(clang-format-alias DEPENDS format) # Renamed from 'clang-format' to avoid conflict
    add_custom_target(format-source DEPENDS format)
else()
    message(STATUS "Neither clang-format nor astyle found. 'format' target is unavailable.")
endif()

find_program(CLANG_FORMAT_EXECUTABLE clang-format)
find_program(ASTYLE_EXECUTABLE astyle)

set(FORMAT_TARGETS "")

if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE CLANG_FORMAT_SOURCES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.h"
        "${CMAKE_SOURCE_DIR}/src/*.hpp"
    )
    list(FILTER CLANG_FORMAT_SOURCES EXCLUDE REGEX "/src/[^/]+\\.(cpp|h|hpp)$")
    list(FILTER CLANG_FORMAT_SOURCES EXCLUDE REGEX "/src/(lua|sol|third-party)/")

    if(CLANG_FORMAT_SOURCES)
        add_custom_target(clang-format-cpp
            COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${CLANG_FORMAT_SOURCES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Formatting C++ source files in subdirectories with clang-format"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
        list(APPEND FORMAT_TARGETS clang-format-cpp)
    endif()
else()
    message(WARNING "clang-format executable was not found, so subdirectory C++ formatting will be unavailable")
endif()

if(ASTYLE_EXECUTABLE)
    set(ASTYLE_OPTIONS_FILE "${CMAKE_SOURCE_DIR}/.astylerc")
    file(GLOB_RECURSE ASTYLE_SOURCES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.h"
        "${CMAKE_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_SOURCE_DIR}/tests/*.h"
        "${CMAKE_SOURCE_DIR}/tools/format/*.cpp"
        "${CMAKE_SOURCE_DIR}/tools/format/*.h"
        "${CMAKE_SOURCE_DIR}/tools/clang-tidy-plugin/*.cpp"
        "${CMAKE_SOURCE_DIR}/tools/clang-tidy-plugin/*.h"
    )
    list(FILTER ASTYLE_SOURCES EXCLUDE REGEX "/src/[^/]+/.*")

    # AStyle 3.6.13 crashes while parsing these tests, so leave them out of the
    # aggregate formatter target until upstream AStyle can handle them.
    list(REMOVE_ITEM ASTYLE_SOURCES
        "${CMAKE_SOURCE_DIR}/tests/iteminfo_test.cpp"
        "${CMAKE_SOURCE_DIR}/tests/json_test.cpp"
    )

    if(ASTYLE_SOURCES)
        add_custom_target(astyle
            COMMAND ${ASTYLE_EXECUTABLE} --options=${ASTYLE_OPTIONS_FILE} -n ${ASTYLE_SOURCES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Formatting C++ source files with astyle"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
        list(APPEND FORMAT_TARGETS astyle)
    endif()
else()
    message(WARNING "Artistic style executable was not found, so astyle C++ formatting will be unavailable")
endif()

if(FORMAT_TARGETS)
    add_custom_target(format DEPENDS ${FORMAT_TARGETS})
    add_custom_target(format-source DEPENDS format)
else()
    message(STATUS "Neither clang-format nor astyle formatting targets are available")
endif()

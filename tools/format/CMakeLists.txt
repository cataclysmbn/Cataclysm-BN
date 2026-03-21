include(ExternalProject)

add_executable(json_formatter format.cpp ${CMAKE_SOURCE_DIR}/src/json.cpp)

add_definitions(-DCATA_IN_TOOL)

include_directories(${CMAKE_SOURCE_DIR}/src)

if (RELEASE)
    if (USE_PREFIX_DATA_DIR)
        install(TARGETS json_formatter RUNTIME)
    else()
        install(TARGETS json_formatter RUNTIME DESTINATION .)
    endif()
endif()

# Custom targets for JSON formatting - equivalent to Makefile's style-json targets
# Note: These targets only work on Unix-like systems with find, grep, xargs

if (UNIX)
    # Get number of CPUs for parallel execution
    cmake_host_system_information(RESULT NPROC QUERY NUMBER_OF_LOGICAL_CORES)

    add_custom_target(style-json
        COMMAND find ${CMAKE_SOURCE_DIR}/data -name "\"*.json\"" -print0 |
                grep -z -v "^${CMAKE_SOURCE_DIR}/data/names/" |
                xargs -0 -L 1 $<TARGET_FILE:json_formatter>
        DEPENDS json_formatter
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting all JSON files"
    )

    add_custom_target(style-json-parallel
        COMMAND find ${CMAKE_SOURCE_DIR}/data -name "\"*.json\"" -print0 |
                grep -z -v "^${CMAKE_SOURCE_DIR}/data/names/" |
                xargs -0 -L 1 -P ${NPROC} $<TARGET_FILE:json_formatter>
        DEPENDS json_formatter
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting all JSON files in parallel"
    )
endif()

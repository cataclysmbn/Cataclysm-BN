find_program(BASH_EXECUTABLE bash)

set(FORMAT_CPP_SCRIPT "${CMAKE_SOURCE_DIR}/build-scripts/format-cpp.sh")

if(BASH_EXECUTABLE AND EXISTS "${FORMAT_CPP_SCRIPT}")
    add_custom_target(format-cpp
        COMMAND "${BASH_EXECUTABLE}" "${FORMAT_CPP_SCRIPT}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Formatting C++ source files"
        VERBATIM
    )
    add_custom_target(format DEPENDS format-cpp)
    add_custom_target(format-source DEPENDS format)
else()
    message(STATUS "C++ formatter helper requires bash")
endif()

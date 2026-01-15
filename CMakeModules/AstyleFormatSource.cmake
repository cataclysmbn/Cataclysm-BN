#This file contains target 'astyle' which runs astyle to format source files to our code
#style, if astyle was found, otherwise there will be reported warning message about not found astyle
#executable
#If Artistic style executable was found, it will be contained in ASTYLE_EXECUTABLE variable
#and ASTYLE_FOUND variable will be set
find_program(ASTYLE_EXECUTABLE astyle)
if(ASTYLE_EXECUTABLE)
    set(ASTYLE_OPTIONS_FILE ${CMAKE_SOURCE_DIR}/.astylerc)
    set(ASTYLE_FOUND "true")

    # Collect source files to format
    file(GLOB_RECURSE ASTYLE_SOURCES
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.h
        ${CMAKE_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_SOURCE_DIR}/tests/*.h
        ${CMAKE_SOURCE_DIR}/tools/format/*.cpp
        ${CMAKE_SOURCE_DIR}/tools/format/*.h
        ${CMAKE_SOURCE_DIR}/tools/clang-tidy-plugin/*.cpp
        ${CMAKE_SOURCE_DIR}/tools/clang-tidy-plugin/*.h
    )
    list(FILTER ASTYLE_SOURCES EXCLUDE REGEX "/src/(lua|sol)/")

    add_custom_target(astyle
        COMMAND ${ASTYLE_EXECUTABLE} --options=${ASTYLE_OPTIONS_FILE} -n ${ASTYLE_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting C++ source files with astyle"
        VERBATIM
    )

    # Alias for backwards compatibility
    add_custom_target(format-source DEPENDS astyle)
else()
    unset(ASTYLE_FOUND)
    message("Artistic style executable was not found, so automatic code style formatting will be unavailable")
endif()

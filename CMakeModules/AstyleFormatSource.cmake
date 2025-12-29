#This file contains target 'format-source' which runs astyle to format source files to our code
#style, if astyle was found, otherwise there will be reported warning message about not found astyle
#executable
#If Artistic style executable was found, it will be contained in ASTYLE_EXECUTABLE variable
#and ASTYLE_FOUND variable will be set
find_program(ASTYLE_EXECUTABLE astyle)
if(ASTYLE_EXECUTABLE)
    set(ASTYLE_OPTIONS_FILE ${CMAKE_SOURCE_DIR}/.astylerc)
    set(ASTYLE_FOUND "true")
    add_custom_target(format-source
            COMMAND ${ASTYLE_EXECUTABLE}  --options=${ASTYLE_OPTIONS_FILE} -r ${CMAKE_SOURCE_DIR}/src/*.cpp,*.h
            ${CMAKE_SOURCE_DIR}/tests/*.cpp,*.h
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            SOURCES ${ASTYLE_OPTIONS_FILE}
            )
else()
    unset(ASTYLE_FOUND)
    message("Artistic style executable was not found, so automatic code style formating will be unavailable")
endif()

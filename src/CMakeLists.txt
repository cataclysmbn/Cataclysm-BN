set(MAIN_CPP ${CMAKE_SOURCE_DIR}/src/main.cpp)
set(MESSAGES_CPP ${CMAKE_SOURCE_DIR}/src/messages.cpp)
set(RESOURCE_RC ${CMAKE_SOURCE_DIR}/src/resource.rc)

file(GLOB CATACLYSM_BN_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)

list(REMOVE_ITEM CATACLYSM_BN_SOURCES ${MAIN_CPP} ${MESSAGES_CPP})

# Lua binding translation units are template-heavy and primarily run at startup;
# compile them with no optimization to reduce build times.
file(GLOB CATALUA_BINDINGS_SOURCES ${CMAKE_SOURCE_DIR}/src/catalua_bindings*.cpp)
if (MSVC)
    set(CATALUA_BINDINGS_COMPILE_OPTIONS /Od)
else ()
    set(CATALUA_BINDINGS_COMPILE_OPTIONS -O0 -g0)
endif ()
foreach (catalua_src IN LISTS CATALUA_BINDINGS_SOURCES)
    set_source_files_properties(${catalua_src} PROPERTIES COMPILE_OPTIONS "${CATALUA_BINDINGS_COMPILE_OPTIONS}")
endforeach ()

file(GLOB CATACLYSM_BN_HEADERS ${CMAKE_SOURCE_DIR}/src/*.h)

add_subdirectory(lua)
add_subdirectory(sol)

# Get GIT version strings
add_custom_target(
    get_version
    DEPENDS ${CMAKE_SOURCE_DIR}/src/version.h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/src/version.h
    COMMAND ${CMAKE_COMMAND}
            -D SRC=${CMAKE_SOURCE_DIR}/src/version.h.in
            -D DST=${CMAKE_SOURCE_DIR}/src/version.h
            -D GIT_EXECUTABLE=${GIT_EXECUTABLE}
            -P ${CMAKE_SOURCE_DIR}/src/version.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Function to setup common properties for both tiles and curses targets
function(setup_common_target_properties target_name)
    # Add version dependency
    add_dependencies(${target_name} get_version)

    # Add common libraries
    target_link_libraries(${target_name} PUBLIC ZLIB::ZLIB)
    target_link_libraries(${target_name} PUBLIC SQLite::SQLite3)

    # Setup threading
    if (CMAKE_USE_PTHREADS_INIT)
        target_compile_options(${target_name} PUBLIC "-pthread")
    endif ()

    if (CMAKE_THREAD_LIBS_INIT)
        target_link_libraries(${target_name} PUBLIC ${CMAKE_THREAD_LIBS_INIT})
    endif ()

    target_compile_definitions(${target_name} PUBLIC LUA)
    if (UNIX)
        target_compile_definitions(${target_name} PUBLIC LUA_USE_LINUX)
    elseif (APPLE)
        target_compile_definitions(${target_name} PUBLIC LUA_USE_MACOSX)
    endif()
    target_link_libraries(${target_name} PUBLIC libsol)

    # Windows-specific libraries
    if (WIN32)
        target_link_libraries(${target_name} PUBLIC gdi32.lib)
        target_link_libraries(${target_name} PUBLIC winmm.lib)
        target_link_libraries(${target_name} PUBLIC imm32.lib)
        target_link_libraries(${target_name} PUBLIC ole32.lib)
        target_link_libraries(${target_name} PUBLIC oleaut32.lib)
        target_link_libraries(${target_name} PUBLIC version.lib)

        if (BACKTRACE)
            target_link_libraries(${target_name} PUBLIC dbghelp.lib)
            if (LIBBACKTRACE)
                target_link_libraries(${target_name} PUBLIC backtrace)
            endif ()
        endif ()
    elseif (APPLE)
        target_link_libraries(${target_name} PUBLIC "-framework CoreFoundation")
    endif ()

    # Backtrace support
    if (LIBBACKTRACE)
        target_link_libraries(${target_name} PUBLIC backtrace)
    endif ()

    # Tracy profiling support
    if (USE_TRACY)
        target_link_libraries(${target_name} PUBLIC TracyClient)
        target_include_directories(${target_name} SYSTEM PUBLIC ${tracy_SOURCE_DIR}/public)
        target_compile_definitions(${target_name} PUBLIC USE_TRACY)
    endif ()

    # Set up precompiled headers if not exporting compile commands
    if (NOT "${CMAKE_EXPORT_COMPILE_COMMANDS}")
        target_precompile_headers(${target_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/pch/main-pch.hpp)
    endif ()
endfunction()

# Build tiles version if requested
if (TILES)
    add_library(
            cataclysm-bn-tiles-common OBJECT
            ${CATACLYSM_BN_SOURCES}
            ${CATACLYSM_BN_HEADERS})
    target_include_directories(cataclysm-bn-tiles-common INTERFACE ${CMAKE_SOURCE_DIR}/src)

    if (WIN32)
        add_definitions(-DUSE_WINMAIN)
        add_executable(
                cataclysm-bn-tiles WIN32
                ${MAIN_CPP}
                ${MESSAGES_CPP}
                ${RESOURCE_RC})
    else ()
        add_executable(cataclysm-bn-tiles
                ${MAIN_CPP}
                ${MESSAGES_CPP})
    endif ()

    setup_common_target_properties(cataclysm-bn-tiles-common)

    # Tiles-specific setup
    target_compile_definitions(cataclysm-bn-tiles-common PUBLIC TILES)
    if (DYNAMIC_ATLAS)
        target_compile_definitions(cataclysm-bn-tiles-common PUBLIC DYNAMIC_ATLAS)
    endif()
    target_link_libraries(cataclysm-bn-tiles PRIVATE cataclysm-bn-tiles-common)

    # Windows specific tiles libraries
    if (WIN32)
        target_link_libraries(cataclysm-bn-tiles-common PUBLIC setupapi.lib)
        if (SOUND)
            target_link_libraries(cataclysm-bn-tiles-common PUBLIC shlwapi.lib)
        endif()
    endif()

    if (NOT DYNAMIC_LINKING)
        target_link_libraries(cataclysm-bn-tiles-common PUBLIC
            SDL2::SDL2-static
            SDL2_image::SDL2_image-static
            SDL2_ttf::SDL2_ttf-static
        )
    else()
        target_link_libraries(cataclysm-bn-tiles-common PUBLIC
            SDL2::SDL2
            SDL2_image::SDL2_image
            SDL2_ttf::SDL2_ttf
        )
    endif ()

    if (SOUND)
        if (NOT DYNAMIC_LINKING)
            target_link_libraries(cataclysm-bn-tiles-common PUBLIC
                SDL2_mixer::SDL2_mixer-static
            )
        else()
            target_link_libraries(cataclysm-bn-tiles-common PUBLIC
                SDL2_mixer::SDL2_mixer
            )
        endif()
        add_definitions(-DSDL_SOUND)
    endif ()

    if (RELEASE)
        install(TARGETS cataclysm-bn-tiles RUNTIME)
    endif ()

    set_target_properties(cataclysm-bn-tiles PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif ()

# Build curses version if requested
if (CURSES)
    add_library(cataclysm-bn-common OBJECT
            ${CATACLYSM_BN_SOURCES}
            ${CATACLYSM_BN_HEADERS})
    target_include_directories(cataclysm-bn-common INTERFACE ${CMAKE_SOURCE_DIR}/src)

    if (WIN32)
        add_executable(cataclysm-bn
                ${MAIN_CPP}
                ${MESSAGES_CPP}
                ${RESOURCE_RC})
    else ()
        add_executable(cataclysm-bn
                ${MAIN_CPP}
                ${MESSAGES_CPP})
    endif ()

    setup_common_target_properties(cataclysm-bn-common)

    # Curses-specific setup
    target_include_directories(cataclysm-bn-common PUBLIC ${CURSES_INCLUDE_DIR})
    target_link_libraries(cataclysm-bn-common PUBLIC ${CURSES_LIBRARIES})
    target_link_libraries(cataclysm-bn PRIVATE cataclysm-bn-common)

    if (RELEASE)
        install(TARGETS cataclysm-bn RUNTIME)
    endif ()
endif ()

set(MAIN_CPP ${CMAKE_SOURCE_DIR}/src/main.cpp)
set(MESSAGES_CPP ${CMAKE_SOURCE_DIR}/src/messages.cpp)
set(MCP_SERVER_CPP ${CMAKE_SOURCE_DIR}/src/mcp_server.cpp)
set(RESOURCE_RC ${CMAKE_SOURCE_DIR}/src/resource.rc)
set(CLIENT_PLATFORM_CPP
    ${CMAKE_SOURCE_DIR}/src/platform/client_platform.cpp
    ${CMAKE_SOURCE_DIR}/src/platform/client_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/platform/client_entry.cpp)
file(GLOB_RECURSE CATACLYSM_BN_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
list(FILTER CATACLYSM_BN_SOURCES EXCLUDE REGEX "/src/(lua|sol|third-party)/")

list(REMOVE_ITEM CATACLYSM_BN_SOURCES
    ${MAIN_CPP} ${MESSAGES_CPP} ${MCP_SERVER_CPP} ${CLIENT_PLATFORM_CPP})

# Lua binding translation units are template-heavy and primarily run at startup;
# compile them with no optimization to reduce build times.
file(GLOB CATALUA_BINDINGS_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/catalua_bindings*.cpp)
if (MSVC)
    set(CATALUA_BINDINGS_COMPILE_OPTIONS /Od /Ob0)
else ()
    set(CATALUA_BINDINGS_COMPILE_OPTIONS -O0 -g0)
endif ()
foreach (catalua_src IN LISTS CATALUA_BINDINGS_SOURCES)
    set_source_files_properties(${catalua_src} PROPERTIES
        COMPILE_OPTIONS "${CATALUA_BINDINGS_COMPILE_OPTIONS}"
        SKIP_PRECOMPILE_HEADERS ON)
endforeach ()

file(GLOB_RECURSE CATACLYSM_BN_HEADERS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.h)
list(FILTER CATACLYSM_BN_HEADERS EXCLUDE REGEX "/src/(lua|sol|third-party)/")

add_subdirectory(lua)
if (UNIX)
    target_compile_definitions(liblua PUBLIC LUA_USE_LINUX)
elseif (APPLE)
    target_compile_definitions(liblua PUBLIC LUA_USE_MACOSX)
elseif (WIN32)
    target_compile_definitions(liblua PUBLIC LUA_USE_WINDOWS)
endif()

add_subdirectory(sol)

# The protocol adapter is deliberately independent of any renderer.  Keep it
# out of the engine source set so a client that embeds the server cannot get a
# second copy of its JSON-RPC symbols through an OBJECT library.
if (MCP)
    add_library(cataclysm-bn-mcp-protocol STATIC ${MCP_SERVER_CPP})
    target_include_directories(cataclysm-bn-mcp-protocol PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(cataclysm-bn-mcp-protocol PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(cataclysm-bn-mcp-protocol PRIVATE CATA_MCP)
endif ()

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
    target_link_libraries(${target_name} PUBLIC SQLite3::SQLite3)

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
endfunction()

function(link_sdl_core target_name)
    if (NOT DYNAMIC_LINKING)
        target_link_libraries(${target_name} PUBLIC SDL3::SDL3-static)
    else()
        target_link_libraries(${target_name} PUBLIC SDL3::SDL3)
    endif ()
    target_compile_definitions(${target_name} PUBLIC CATA_SDL)
endfunction()

function(link_sdl_mixer target_name)
    if (NOT DYNAMIC_LINKING)
        target_link_libraries(${target_name} PUBLIC SDL3_mixer::SDL3_mixer-static)
    else()
        target_link_libraries(${target_name} PUBLIC SDL3_mixer::SDL3_mixer)
    endif ()
    target_compile_definitions(${target_name} PRIVATE SDL_SOUND)
endfunction()

# Build the game engine exactly once.  Renderer and terminal implementations
# are client libraries below; they must never be added to this source set.
list(REMOVE_ITEM CATACLYSM_BN_SOURCES ${CATA_CLIENT_SOURCES_TO_REMOVE})

add_library(cataclysm-bn-engine STATIC
    ${CATACLYSM_BN_SOURCES}
    ${CATACLYSM_BN_HEADERS})
target_include_directories(cataclysm-bn-engine PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cataclysm-bn-engine PUBLIC nlohmann_json::nlohmann_json)
setup_common_target_properties(cataclysm-bn-engine)
if (CATA_SDL)
    link_sdl_core(cataclysm-bn-engine)
endif ()
if (CATA_SHADERS)
    add_dependencies(cataclysm-bn-engine cata_shaders)
endif ()

function(add_cata_client_library target_name)
    cmake_parse_arguments(CATA_CLIENT "" "" "SOURCES;DEFINITIONS" ${ARGN})
    add_library(${target_name} OBJECT ${CATA_CLIENT_SOURCES})
    target_include_directories(${target_name} PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${target_name} PUBLIC cataclysm-bn-engine)
    target_compile_definitions(${target_name} PRIVATE ${CATA_CLIENT_DEFINITIONS})
endfunction()

add_library(cataclysm-bn-client-common OBJECT ${CATA_CLIENT_COMMON_SOURCES})
target_include_directories(cataclysm-bn-client-common PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cataclysm-bn-client-common PUBLIC cataclysm-bn-engine)
add_library(cataclysm-bn-platform OBJECT ${CLIENT_PLATFORM_CPP})
target_include_directories(cataclysm-bn-platform PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cataclysm-bn-platform PRIVATE cataclysm-bn-engine)
foreach(client TILES CURSES MCP IMGUI)
    if (${client})
        target_compile_definitions(cataclysm-bn-platform PRIVATE CATA_HAS_${client})
    endif ()
endforeach()

add_cata_client_library(cataclysm-bn-client-audio SOURCES ${CATA_AUDIO_SOURCES})
if (SOUND)
    target_compile_definitions(cataclysm-bn-client-audio PRIVATE SDL_SOUND)
    link_sdl_mixer(cataclysm-bn-client-audio)
endif ()

if (TILES)
    add_cata_client_library(cataclysm-bn-tiles-client
        SOURCES ${CATA_TILES_SOURCES}
        DEFINITIONS TILES)
    add_library(cataclysm-bn-tiles-common ALIAS cataclysm-bn-tiles-client)
    target_compile_definitions(cataclysm-bn-tiles-client PRIVATE TILES)
    link_sdl_core(cataclysm-bn-tiles-client)
    if (DYNAMIC_ATLAS)
        target_compile_definitions(cataclysm-bn-tiles-client PRIVATE DYNAMIC_ATLAS)
    endif()
    if (NOT DYNAMIC_LINKING)
        target_link_libraries(cataclysm-bn-tiles-client PUBLIC
            SDL3_image::SDL3_image-static SDL3_ttf::SDL3_ttf-static)
    else()
        target_link_libraries(cataclysm-bn-tiles-client PUBLIC
            SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf)
    endif ()
    if (WIN32)
        target_link_libraries(cataclysm-bn-tiles-client PUBLIC setupapi.lib)
        if (SOUND)
            target_link_libraries(cataclysm-bn-tiles-client PUBLIC shlwapi.lib)
        endif()
    endif()
endif ()

if (CURSES)
    add_cata_client_library(cataclysm-bn-curses-client
        SOURCES ${CATA_CURSES_SOURCES}
        DEFINITIONS CURSES)
    add_library(cataclysm-bn-common ALIAS cataclysm-bn-curses-client)
    target_include_directories(cataclysm-bn-curses-client PUBLIC ${CURSES_INCLUDE_DIR})
    target_link_libraries(cataclysm-bn-curses-client PUBLIC ${CURSES_LIBRARIES})
    if (CATA_SDL)
        link_sdl_core(cataclysm-bn-curses-client)
    endif()
    if (CATA_SHADERS)
        add_dependencies(cataclysm-bn-curses-client cata_shaders)
    endif ()
endif ()

if (IMGUI)
    add_library(cataclysm-bn-imgui-dependency STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp")
    target_include_directories(cataclysm-bn-imgui-dependency SYSTEM PUBLIC
        "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
    target_compile_options(cataclysm-bn-imgui-dependency PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>)
    link_sdl_core(cataclysm-bn-imgui-dependency)
    add_cata_client_library(cataclysm-bn-imgui-client
        SOURCES ${CATA_IMGUI_SOURCES}
        DEFINITIONS CATA_IMGUI)
    target_link_libraries(cataclysm-bn-imgui-client PUBLIC cataclysm-bn-imgui-dependency)
    link_sdl_core(cataclysm-bn-imgui-client)
endif ()

if (MCP)
    add_library(cataclysm-bn-mcp-client OBJECT
        ${CATA_MCP_SOURCES})
    target_include_directories(cataclysm-bn-mcp-client PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(cataclysm-bn-mcp-client PRIVATE CATA_MCP)
    target_link_libraries(cataclysm-bn-mcp-client PRIVATE
        cataclysm-bn-engine cataclysm-bn-mcp-protocol)

endif ()

if (WIN32)
    add_executable(cataclysm-bn WIN32 ${MAIN_CPP} ${MESSAGES_CPP} ${RESOURCE_RC})
else ()
    add_executable(cataclysm-bn ${MAIN_CPP} ${MESSAGES_CPP})
endif ()
target_link_libraries(cataclysm-bn PRIVATE
    cataclysm-bn-engine cataclysm-bn-client-common cataclysm-bn-platform)
if (TILES)
    target_link_libraries(cataclysm-bn PRIVATE cataclysm-bn-tiles-client)
endif ()
if (CURSES)
    target_link_libraries(cataclysm-bn PRIVATE cataclysm-bn-curses-client)
endif ()
if (MCP)
    target_link_libraries(cataclysm-bn PRIVATE
        cataclysm-bn-mcp-client cataclysm-bn-mcp-protocol)
endif ()
if (IMGUI)
    target_link_libraries(cataclysm-bn PRIVATE
        cataclysm-bn-imgui-client cataclysm-bn-imgui-dependency)
endif ()
target_link_libraries(cataclysm-bn PRIVATE cataclysm-bn-client-audio)
if (WIN32)
    target_compile_definitions(cataclysm-bn PRIVATE USE_WINMAIN)
endif ()
if (MSVC AND TILES)
    # Preserve the symbol artifact consumed by the Windows release workflow.
    set_target_properties(cataclysm-bn PROPERTIES PDB_NAME cataclysm-bn-tiles)
endif ()
if (RELEASE)
    install(TARGETS cataclysm-bn RUNTIME)
    set(CATA_CLIENT_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}")
    if (NOT CATA_CLIENT_INSTALL_BINDIR)
        set(CATA_CLIENT_INSTALL_BINDIR bin)
    endif ()
    foreach(client TILES MCP IMGUI)
        if (${client})
            string(TOLOWER "${client}" client_name)
            if (WIN32)
                install(PROGRAMS "$<TARGET_FILE:cataclysm-bn>"
                    DESTINATION "${CATA_CLIENT_INSTALL_BINDIR}"
                    RENAME "cataclysm-bn-${client_name}${CMAKE_EXECUTABLE_SUFFIX}")
            else ()
                install(CODE "file(CREATE_LINK \"cataclysm-bn\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CATA_CLIENT_INSTALL_BINDIR}/cataclysm-bn-${client_name}\" SYMBOLIC)")
            endif ()
        endif ()
    endforeach()
endif ()
set_target_properties(cataclysm-bn PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
if (COMMAND cata_copy_windows_lavapipe_runtime)
    cata_copy_windows_lavapipe_runtime(cataclysm-bn)
endif()
if( LUA_DOCS_ON_BUILD AND DENO_BINARY )
    add_custom_command(TARGET cataclysm-bn POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env --unset=LD_LIBRARY_PATH
            "CATA_EXE=$<TARGET_FILE:cataclysm-bn>" ${DENO_BINARY} task docs:gen
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating Lua reference/annotations (deno task docs:gen)"
        VERBATIM)
endif()

if (TILES)
    add_custom_command(TARGET cataclysm-bn POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:cataclysm-bn>"
            "$<TARGET_FILE_DIR:cataclysm-bn>/cataclysm-bn-tiles${CMAKE_EXECUTABLE_SUFFIX}")
endif ()
if (MCP)
    add_custom_command(TARGET cataclysm-bn POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:cataclysm-bn>"
            "$<TARGET_FILE_DIR:cataclysm-bn>/cataclysm-bn-mcp${CMAKE_EXECUTABLE_SUFFIX}")
endif ()
if (IMGUI)
    add_custom_command(TARGET cataclysm-bn POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:cataclysm-bn>"
            "$<TARGET_FILE_DIR:cataclysm-bn>/cataclysm-bn-imgui${CMAKE_EXECUTABLE_SUFFIX}")
endif ()

if (TILES)
    add_custom_target(cataclysm-bn-tiles DEPENDS cataclysm-bn)
endif ()
if (MCP)
    add_custom_target(cataclysm-bn-mcp DEPENDS cataclysm-bn)
endif ()
if (IMGUI)
    add_custom_target(cataclysm-bn-imgui DEPENDS cataclysm-bn)
endif ()

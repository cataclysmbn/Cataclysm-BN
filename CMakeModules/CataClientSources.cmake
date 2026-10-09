# Source partitions shared by the desktop and Android build graphs.
# CATA_SRC_DIR must point at the repository's src directory.
set(CATA_CLIENT_COMMON_SOURCES
    "${CATA_SRC_DIR}/animation.cpp"
    "${CATA_SRC_DIR}/bionics_ui.cpp"
    "${CATA_SRC_DIR}/crafting_gui.cpp"
    "${CATA_SRC_DIR}/cursesport.cpp"
    "${CATA_SRC_DIR}/character_preview.cpp"
    "${CATA_SRC_DIR}/client_memory.cpp"
    "${CATA_SRC_DIR}/crash.cpp"
    "${CATA_SRC_DIR}/debug.cpp"
    "${CATA_SRC_DIR}/debug_menu.cpp"
    "${CATA_SRC_DIR}/dialogue_win.cpp"
    "${CATA_SRC_DIR}/diary_ui.cpp"
    "${CATA_SRC_DIR}/editmap.cpp"
    "${CATA_SRC_DIR}/examine_item_menu.cpp"
    "${CATA_SRC_DIR}/game_info.cpp"
    "${CATA_SRC_DIR}/game_inventory.cpp"
    "${CATA_SRC_DIR}/game_ui.cpp"
    "${CATA_SRC_DIR}/hsv_color.cpp"
    "${CATA_SRC_DIR}/ime.cpp"
    "${CATA_SRC_DIR}/inventory_ui.cpp"
    "${CATA_SRC_DIR}/loading_ui.cpp"
    "${CATA_SRC_DIR}/lua_action_menu.cpp"
    "${CATA_SRC_DIR}/main_menu.cpp"
    "${CATA_SRC_DIR}/mission_ui.cpp"
    "${CATA_SRC_DIR}/mod_manager_ui.cpp"
    "${CATA_SRC_DIR}/mutation_ui.cpp"
    "${CATA_SRC_DIR}/newcharacter.cpp"
    "${CATA_SRC_DIR}/options.cpp"
    "${CATA_SRC_DIR}/output.cpp"
    "${CATA_SRC_DIR}/overmap/overmap_ui.cpp"
    "${CATA_SRC_DIR}/panels.cpp"
    "${CATA_SRC_DIR}/popup.cpp"
    "${CATA_SRC_DIR}/ranged.cpp"
    "${CATA_SRC_DIR}/safemode_ui.cpp"
    "${CATA_SRC_DIR}/scores_ui.cpp"
    "${CATA_SRC_DIR}/string_editor_window.cpp"
    "${CATA_SRC_DIR}/string_input_popup.cpp"
    "${CATA_SRC_DIR}/trade_win.cpp"
    "${CATA_SRC_DIR}/ui.cpp"
    "${CATA_SRC_DIR}/ui_manager.cpp"
    "${CATA_SRC_DIR}/vehicle/veh_interact.cpp"
    "${CATA_SRC_DIR}/worldfactory.cpp")
file(GLOB CATA_CLIENT_BOUNDARY_SOURCES CONFIGURE_DEPENDS
    "${CATA_SRC_DIR}/client/*.cpp")
set(CATA_AUDIO_SOURCES
    "${CATA_SRC_DIR}/sdlsound.cpp"
    "${CATA_SRC_DIR}/client/sdlsound.cpp"
    "${CATA_SRC_DIR}/client/sounds.cpp")
list(REMOVE_ITEM CATA_CLIENT_BOUNDARY_SOURCES ${CATA_AUDIO_SOURCES})
list(APPEND CATA_CLIENT_COMMON_SOURCES ${CATA_CLIENT_BOUNDARY_SOURCES})
list(REMOVE_DUPLICATES CATA_CLIENT_COMMON_SOURCES)
file(GLOB_RECURSE CATA_TILES_BACKEND_SOURCES CONFIGURE_DEPENDS
    "${CATA_SRC_DIR}/client/tiles/*.cpp")
file(GLOB_RECURSE CATA_CURSES_BACKEND_SOURCES CONFIGURE_DEPENDS
    "${CATA_SRC_DIR}/client/curses/*.cpp")
file(GLOB_RECURSE CATA_MCP_BACKEND_SOURCES CONFIGURE_DEPENDS
    "${CATA_SRC_DIR}/client/mcp/*.cpp")
file(GLOB_RECURSE CATA_IMGUI_BACKEND_SOURCES CONFIGURE_DEPENDS
    "${CATA_SRC_DIR}/client/imgui/*.cpp")

set(CATA_TILES_SOURCES
    "${CATA_SRC_DIR}/cata_tiles.cpp"
    "${CATA_SRC_DIR}/cata_tiles_color.cpp"
    "${CATA_SRC_DIR}/dynamic_atlas.cpp"
    "${CATA_SRC_DIR}/mod_tileset.cpp"
    "${CATA_SRC_DIR}/sdl_font.cpp"
    "${CATA_SRC_DIR}/sdl_geometry.cpp"
    "${CATA_SRC_DIR}/sdl_utils.cpp"
    "${CATA_SRC_DIR}/sdl_wrappers.cpp"
    "${CATA_SRC_DIR}/sdltiles.cpp"
    "${CATA_SRC_DIR}/pixel_minimap.cpp"
    "${CATA_SRC_DIR}/pixel_minimap_projectors.cpp"
    "${CATA_SRC_DIR}/vehicle/vehicle_preview.cpp"
    ${CATA_TILES_BACKEND_SOURCES})
set(CATA_CURSES_SOURCES
    "${CATA_SRC_DIR}/ncurses_def.cpp"
    "${CATA_SRC_DIR}/wincurse.cpp"
    ${CATA_CURSES_BACKEND_SOURCES})
set(CATA_MCP_SOURCES
    "${CATA_SRC_DIR}/mcp_session.cpp"
    ${CATA_MCP_BACKEND_SOURCES})
set(CATA_IMGUI_SOURCES ${CATA_IMGUI_BACKEND_SOURCES})
set(CATA_CLIENT_SOURCES_TO_REMOVE
    ${CATA_CLIENT_COMMON_SOURCES} ${CATA_TILES_SOURCES} ${CATA_CURSES_SOURCES}
    ${CATA_MCP_SOURCES} ${CATA_IMGUI_SOURCES} ${CATA_AUDIO_SOURCES})

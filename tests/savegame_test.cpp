#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "debug.h"
#include "filesystem.h"
#include "game.h"
#include "game_constants.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/mapbuffer_registry.h"
#include "state_helpers.h"
#include "world.h"
#include "worldfactory.h"

#include <filesystem>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

TEST_CASE("failed save load blocks saving over the broken save", "[save][load]") {
    clear_all_state();
    g->clear_failed_load_save_block();
    const auto cleanup = on_out_of_scope([]() {
        clear_all_state();
        g->clear_failed_load_save_block();
    });

    const auto fixture_path = fs::path("tests/data/save/broken_load_save/#QnJva2VuU2F2ZQ==.sav");
    const auto world_path = fs::path(g->get_active_world()->info->folder_path());
    const auto save_path = world_path / "#QnJva2VuU2F2ZQ==.sav";
    CHECK(fs::copy_file(fixture_path, save_path, fs::copy_options::overwrite_existing));

    const auto before_load = read_entire_file(save_path.string());
    auto loaded = true;
    const auto debug_message = capture_debugmsg_during([&]() {
        loaded = g->load(save_t::from_save_id("BrokenSave"));
    });

    CHECK_FALSE(loaded);
    CHECK(debug_message.find("Bad save json") != std::string::npos);
    CHECK_FALSE(g->save(false));
    CHECK(read_entire_file(save_path.string()) == before_load);
}

/// Loading a save used to load the map twice. The save sets the reality bubble size before it
/// reads the player position. Changing the size reloads the map around the player, who still
/// stands at the top-left corner of the old map. Then the load reads the position and loads the
/// map around the player again.
///
/// The test unloads a submap that only the first, wasted reload would bring in: one bubble
/// radius up and left of the map corner. After the load, that submap must still be unloaded.
TEST_CASE("loading a save loads the map only around the saved player position", "[save][load]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });

    const auto player_omt = tripoint_abs_omt(100, 100, 0);
    g->place_player_overmap(player_omt);
    REQUIRE(g->save(false));
    const auto corner = get_map().get_abs_sub();
    const auto wasted_sm =
        tripoint_abs_sm(corner.x() - g_half_mapsize, corner.y() - g_half_mapsize, 0);
    MAPBUFFER.unload_omt(project_to<coords::omt>(wasted_sm), false);
    REQUIRE(MAPBUFFER.lookup_submap_in_memory(wasted_sm) == nullptr);

    REQUIRE(g->load(save_t::from_save_id(get_avatar().get_save_id())));

    CHECK(project_to<coords::omt>(get_avatar().abs_pos()) == player_omt);
    CHECK(MAPBUFFER.lookup_submap_in_memory(wasted_sm) == nullptr);
}

TEST_CASE("manual combat mode is serialized in save data", "[save]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });

    g->manual_combat_mode = true;

    std::ostringstream save_data;
    g->serialize(save_data);

    CHECK(save_data.str().find(R"("manual_combat_mode": true)") != std::string::npos);
}

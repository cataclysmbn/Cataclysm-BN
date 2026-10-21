#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "explosion.h"
#    include "game.h"
#    include "game_constants.h"
#    include "game_session.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "map_perception.h"
#    include "options_helpers.h"
#    include "state_helpers.h"
#    include "weather/weather.h"

#    include <ranges>

namespace {

/// The avatar on open grass at noon with its surroundings acquired once.
auto acquire_on_open_ground() -> map_perception::acquisition_counts {
    clear_all_state();
    get_avatar().clear_map_memory();
    clear_map();
    calendar::turn = calendar::turn_zero + time_duration::from_hours(12);
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    game_session::set_running(true);
    get_avatar().setpos(tripoint_bub_ms(g_half_mapsize_x, g_half_mapsize_y, 0));
    get_map().invalidate_map_cache(0);
    return map_perception::acquire();
}

} // namespace

TEST_CASE(
    "a step rescans only the columns whose visibility changed", "[map_perception][rescan][mcp]") {
    const auto first = acquire_on_open_ground();
    REQUIRE(first.full_rescans == 1);
    REQUIRE(first.candidates > 0);
    get_avatar().setpos(get_avatar().bub_pos() + tripoint(1, 0, 0));
    calendar::turn += 1_turns;
    const auto stepped = map_perception::acquire();
    CHECK(stepped.full_rescans == 1);
    CHECK(stepped.candidates < first.candidates / 2);
}

TEST_CASE(
    "a restricted rescan leaves the same map memory as a complete one",
    "[map_perception][rescan][mcp]") {
    acquire_on_open_ground();
    for (const auto step : std::views::iota(0, 4)) {
        get_avatar().setpos(
            get_avatar().bub_pos() + tripoint(step % 2 == 0 ? 1 : 0, step % 2 == 0 ? 0 : 1, 0));
        calendar::turn += 1_turns;
        map_perception::acquire();
    }
    // A fresh acquisition rescans every column; it finds nothing the restricted ones missed.
    map_perception::reset();
    CHECK(map_perception::acquire().memory_writes == 0);
}

TEST_CASE(
    "a terrain change outside the changed columns is still remembered",
    "[map_perception][rescan][mcp]") {
    acquire_on_open_ground();
    auto& here = get_map();
    const auto wall = get_avatar().bub_pos() + tripoint(5, 3, 0);
    here.ter_set(wall, ter_id("t_wall"));
    map_perception::acquire();
    CHECK(get_avatar().get_terrain_tile(map_local_to_abs(here, wall)).tile == "t_wall");
}

TEST_CASE(
    "a turn remembers how it ends though the avatar does not act",
    "[map_perception][rescan][mcp]") {
    const auto no_autosave = override_option("AUTOSAVE", "false");
    const auto normal_bubble_size =
        override_option("REALITY_BUBBLE_SIZE", std::to_string(g_reality_bubble_size));
    const auto no_mobile_bubble = override_option("ACTIVITY_MOBILE_BUBBLE_SIZE", "0");
    const auto no_idle_bubble = override_option("ACTIVITY_IDLE_BUBBLE_SIZE", "0");
    acquire_on_open_ground();
    auto& here = get_map();
    const auto wall = get_avatar().bub_pos() + tripoint(5, 3, 0);
    here.ter_set(wall, ter_id("t_wall"));
    g->new_game = false;
    get_avatar().set_moves(0);
    CHECK_FALSE(g->do_turn());
    CHECK(get_avatar().get_terrain_tile(map_local_to_abs(here, wall)).tile == "t_wall");
}

TEST_CASE(
    "a turn leaves what changed after its own cache rebuild for the next one",
    "[map_perception][rescan][explosion][mcp]") {
    const auto no_autosave = override_option("AUTOSAVE", "false");
    const auto normal_bubble_size =
        override_option("REALITY_BUBBLE_SIZE", std::to_string(g_reality_bubble_size));
    const auto no_mobile_bubble = override_option("ACTIVITY_MOBILE_BUBBLE_SIZE", "0");
    const auto no_idle_bubble = override_option("ACTIVITY_IDLE_BUBBLE_SIZE", "0");
    const auto no_animation = override_option("ANIMATION_DELAY", "0");
    const auto old_explosions = override_option("OLD_EXPLOSIONS", "false");
    acquire_on_open_ground();
    auto& here = get_map();
    const auto curtains = get_avatar().bub_pos() + tripoint(8, 0, 0);
    here.ter_set(curtains, ter_id("t_curtains"));
    // The turn rebuilds the caches before monsters move; its queued blasts go off after that.
    explosion_handler::
        explosion(curtains + tripoint(1, 0, 0), {.damage = 40, .radius = 2.0f}, nullptr);
    g->new_game = false;
    get_avatar().set_moves(0);
    CHECK_FALSE(g->do_turn());
    REQUIRE(here.ter(curtains) != ter_id("t_curtains"));
    // Gameplay reads the caches as that rebuild made them until the next turn rebuilds them;
    // acquiring knowledge at the end of the turn must not rebuild them early.
    const auto& cache = here.access_cache(0);
    const auto sm = project_to<coords::sm>(curtains);
    CHECK(cache.transparency_cache_dirty[static_cast<size_t>(cache.bidx(sm.x(), sm.y()))]);
}

#endif

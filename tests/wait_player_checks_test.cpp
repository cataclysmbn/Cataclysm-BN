#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "map/field_type.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "map/submap.h"
#include "player_activity.h"
#include "state_helpers.h"
#include "trap.h"
#include "weather/weather.h"

namespace {

const auto act_wait = activity_id("ACT_WAIT");
const auto itype_plank = itype_id("2x4");
const auto tr_firewood_source_id = trap_str_id("tr_firewood_source");

/// Puts the avatar at the north-west corner of a submap at noon and starts a long wait.
auto start_wait() -> tripoint_bub_ms {
    clear_all_state();
    calendar::turn = calendar::turn_zero + 12_hours;
    auto& weather = get_weather();
    weather.weather_id = weather_type_id("clear");
    weather.clear_temp_cache();
    const auto pos = tripoint_bub_ms(SEEX * 5, SEEY * 5, 0);
    g->u.setpos(pos);
    g->reset_light_level();
    // The GPU path builds seen_cache in update_visibility_cache. Without it the avatar keeps the
    // seen_cache of an earlier test, for example a dark vision test, and does not see the tiles
    // here.
    g->m.invalidate_map_cache(pos.z());
    g->m.build_map_cache(pos.z());
    g->m.update_visibility_cache(pos.z());
    g->u.assign_activity(act_wait, to_moves<int>(1_hours), 0);
    REQUIRE(g->u.activity);
    REQUIRE(*g->u.activity);
    return pos;
}

/// Runs one turn of the wait through player_activity::do_turn, as the game loop does.
auto wait_one_turn() -> void {
    g->u.set_moves(100);
    g->u.activity->do_turn(g->u);
    calendar::turn += 1_turns;
    REQUIRE(*g->u.activity);
}

auto planks_at(const tripoint_bub_ms& p) -> int {
    auto count = 0;
    for (const item* it : g->m.i_at(p)) { count += it->typeId() == itype_plank ? 1 : 0; }
    return count;
}

} // namespace

// Checks that a wait feeds a fire that starts mid-wait on the next turn.
// The wait skips the fire search while no fire field is in range, so a new fire must end the skip.
// Setup: a firewood pile 1 tile east with 3 planks, no fire. After 3 turns all planks stay.
// Then a 20 minute old fire starts 1 tile west. On the next turn 1 plank moves onto the fire.
TEST_CASE("wait_feeds_a_fire_that_starts_mid_wait", "[activity][fire]") {
    const auto pos = start_wait();
    const auto pile = pos + point_east;
    const auto fire = pos + point_west;
    g->m.trap_set(pile, tr_firewood_source_id.id());
    for (auto i = 0; i < 3; ++i) { g->m.add_item(pile, item::spawn(itype_plank)); }
    REQUIRE(planks_at(pile) == 3);

    for (auto i = 0; i < 3; ++i) { wait_one_turn(); }
    CHECK(planks_at(pile) == 3);
    CHECK(planks_at(fire) == 0);

    REQUIRE(g->m.add_field(fire, fd_fire, 1, 20_minutes));
    wait_one_turn();
    CHECK(planks_at(pile) == 2);
    CHECK(planks_at(fire) == 1);
    clear_all_state();
}

// Checks that a wait spots a trap that appears mid-wait in the search radius on the next turn.
// The wait skips the trap search while no trap is in the radius, so a new trap must end the skip.
// tr_pit has visibility 0, so an avatar with 8 perception spots it 2 tiles away on every roll.
TEST_CASE("wait_spots_a_trap_that_appears_mid_wait", "[activity][trap]") {
    const auto pos = start_wait();

    SECTION("trap set with trap_set") {
        const auto trap_pos = pos + point(2, 0);
        for (auto i = 0; i < 3; ++i) { wait_one_turn(); }
        CHECK_FALSE(g->u.knows_trap(trap_pos));

        g->m.trap_set(trap_pos, tr_pit);
        wait_one_turn();
        CHECK(g->u.knows_trap(trap_pos));
    }

    SECTION("terrain trap from a whole-submap terrain fill") {
        // Mapgen fills uniform submaps with set_all_ter, for example open air with tr_ledge.
        // The avatar stands on a submap corner, so the tile to the west is on another submap.
        const auto trap_pos = pos + point_west;
        wait_one_turn();
        CHECK_FALSE(g->u.knows_trap(trap_pos));

        submap* sm = g->m.get_submap_at(trap_pos);
        REQUIRE(sm != nullptr);
        REQUIRE(sm != g->m.get_submap_at(pos));
        sm->set_all_ter(ter_str_id("t_pit").id());
        g->m.invalidate_map_cache(pos.z());
        g->m.build_map_cache(pos.z(), true);
        wait_one_turn();
        CHECK(g->u.knows_trap(trap_pos));
    }
    clear_all_state();
}

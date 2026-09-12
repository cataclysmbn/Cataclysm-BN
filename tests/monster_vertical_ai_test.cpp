#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "game_constants.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "monster_action.h"
#include "state_helpers.h"

#include <cfloat>

namespace {

const auto midday = calendar::turn_zero + 12_hours;

auto rebuild_z_caches(int z_min, int z_max) -> void {
    auto& here = get_map();
    for (int z = z_min; z <= z_max; ++z) {
        here.invalidate_map_cache(z);
        here.build_map_cache(z, true);
    }
}

auto setup_open_sky() -> void {
    clear_all_state();
    calendar::turn = midday;
    move_player_out_of_the_way();
    rebuild_z_caches(0, OVERMAP_HEIGHT);
}

} // namespace

TEST_CASE("flying flee does not rocket altitude", "[monster][ai][zlevel]") {
    setup_open_sky();

    auto& wasp = spawn_test_monster("mon_wasp_guard", tripoint_bub_ms(60, 60, 0));
    auto& eyebot = spawn_test_monster("mon_eyebot", tripoint_bub_ms(62, 60, 0));

    SECTION("same-z hop is one level, then altitude holds") {
        CHECK(wasp.flying_flee_altitude(0, 0) == 1);
        CHECK(wasp.flying_flee_altitude(1, 0) == 1);
        CHECK(wasp.flying_flee_altitude(2, 0) == 2);
        CHECK(wasp.flying_flee_altitude(8, 0) == 8);

        auto z = 0;
        for (auto i = 0; i < 4; ++i) {
            z = wasp.flying_flee_altitude(z, 0);
        }
        CHECK(z <= 1);
    }

    SECTION("eyebot sibling uses the same cap") {
        auto z = 0;
        for (auto i = 0; i < 4; ++i) {
            z = eyebot.flying_flee_altitude(z, 0);
        }
        CHECK(z <= 1);
        CHECK(eyebot.flying_flee_altitude(9, 0) == 9);
    }

    SECTION("result stays inside overmap z bounds") {
        CHECK(wasp.flying_flee_altitude(OVERMAP_HEIGHT, OVERMAP_HEIGHT) == OVERMAP_HEIGHT);
        CHECK(wasp.flying_flee_altitude(-OVERMAP_DEPTH, 0) == -OVERMAP_DEPTH);
    }
}

TEST_CASE("crow preferred_z is approached one level at a time", "[monster][ai][zlevel]") {
    setup_open_sky();

    auto& crow = spawn_test_monster("mon_crow", tripoint_bub_ms(60, 60, 0));
    REQUIRE(crow.type->preferred_z);
    CHECK(*crow.type->preferred_z == 3);

    CHECK(crow.flying_flee_altitude(0, 0) == 1);
    CHECK(crow.flying_flee_altitude(1, 0) == 2);
    CHECK(crow.flying_flee_altitude(3, 0) == 3);
    CHECK(crow.flying_flee_altitude(5, 0) == 4);
}

TEST_CASE("ground monsters melee open air at dz 1 but not through floors",
          "[monster][ai][zlevel][melee]") {
    setup_open_sky();

    const auto ground = tripoint_bub_ms(60, 60, 0);
    const auto above = tripoint_bub_ms(60, 60, 1);

    SECTION("open air wasp is a legal upward strike") {
        auto& zed = spawn_test_monster("mon_zombie", ground);
        auto& wasp = spawn_test_monster("mon_wasp_guard", above);
        zed.moves = 100;
        zed.set_dest(wasp.bub_pos());

        const auto action = zed.decide_action();
        CHECK(action.kind == monster_action_kind::attack);
        CHECK(action.target == &wasp);
        CHECK(action.dest == above);
    }

    SECTION("roof floor blocks the upward strike") {
        auto& here = get_map();
        here.ter_set(above, ter_id("t_floor"));
        rebuild_z_caches(0, 1);

        auto& zed = spawn_test_monster("mon_zombie", ground);
        g->place_player(above);
        zed.moves = 100;
        zed.set_dest(get_avatar().bub_pos());

        const auto action = zed.decide_action();
        CHECK(action.kind != monster_action_kind::attack);
    }
}

TEST_CASE("open air altitude penalty ignores roofs and adjacent hover",
          "[monster][ai][zlevel]") {
    setup_open_sky();

    auto& zed = spawn_test_monster("mon_zombie", tripoint_bub_ms(60, 60, 0));
    auto& wasp = spawn_test_monster("mon_wasp_guard", tripoint_bub_ms(60, 60, 1));
    auto& high_bot = spawn_test_monster("mon_eyebot", tripoint_bub_ms(60, 60, 9));

    const auto roof = tripoint_bub_ms(62, 60, 2);
    auto& here = get_map();
    here.ter_set(roof, ter_id("t_floor"));
    rebuild_z_caches(0, 9);
    auto& roof_dummy = spawn_test_monster("debug_mon", roof);

    CHECK(zed.unengageable_altitude_penalty(wasp) == 0);
    CHECK(zed.unengageable_altitude_penalty(roof_dummy) == 0);
    CHECK(zed.unengageable_altitude_penalty(high_bot) > 0);
    CHECK(wasp.unengageable_altitude_penalty(zed) == 0);
}

TEST_CASE("zombies prefer a ground survivor over a stratospheric drone",
          "[monster][ai][zlevel]") {
    setup_open_sky();

    const auto zed_pos = tripoint_bub_ms(60, 60, 0);
    g->place_player(tripoint_bub_ms(60, 75, 0));
    auto& zed = spawn_test_monster("mon_zombie", zed_pos);
    auto& bot = spawn_test_monster("mon_eyebot", tripoint_bub_ms(60, 60, 2));
    rebuild_z_caches(0, 2);

    REQUIRE(zed.sees(get_avatar()));
    REQUIRE(zed.sees(bot));

    const auto player_rating = zed.rate_target(get_avatar(), 100.0f);
    const auto bot_rating = zed.rate_target(bot, 100.0f);
    REQUIRE(player_rating < FLT_MAX);
    REQUIRE(bot_rating < FLT_MAX);
    CHECK(player_rating < bot_rating);

    const auto plan = zed.compute_plan();
    CHECK(plan.goal == get_avatar().bub_pos());
}

TEST_CASE("unreachable open air target is not dropped when it is the only prey",
          "[monster][ai][zlevel]") {
    setup_open_sky();
    put_player_underground();

    auto& zed = spawn_test_monster("mon_zombie", tripoint_bub_ms(60, 60, 0));
    auto& bot = spawn_test_monster("mon_eyebot", tripoint_bub_ms(60, 60, 2));
    rebuild_z_caches(-2, 2);

    REQUIRE(zed.sees(bot));
    REQUIRE(zed.unengageable_altitude_penalty(bot) > 0);

    const auto plan = zed.compute_plan();
    CHECK(plan.goal == bot.bub_pos());
}

#include "../src/map/map.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "map_helpers.h"
#include "monster.h"
#include "mtype.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle/vehicle.h"

TEST_CASE("mps_cmps_round_trip_converges_to_zero", "[vehicle]") {
    constexpr auto max_iterations = 200;

    for (auto v = 1; v <= 50; ++v) {
        auto coll_velocity = v;
        auto iterations = 0;
        while (coll_velocity > 0 && iterations < max_iterations) {
            const auto vel_mps = cmps_to_mps(coll_velocity);
            const auto new_velocity = mps_to_cmps(vel_mps * 0.9);
            coll_velocity = (std::abs(new_velocity) >= std::abs(coll_velocity)) ? 0 : new_velocity;
            ++iterations;
        }
        CAPTURE(v);
        CHECK(coll_velocity == 0);
        CHECK(iterations < max_iterations);
    }
}

TEST_CASE("vehicle_collision_with_wall_terminates", "[vehicle]") {
    clear_all_state();
    auto& here = get_map();
    build_test_map(ter_id("t_pavement"));
    clear_vehicles();

    const auto veh_pos_abs = test_origin;
    const auto wall_pos_abs = veh_pos_abs + tripoint_rel_ms::north();

    auto* veh_ptr =
        here.add_vehicle(vproto_id("bicycle_test"), abs_to_bub(veh_pos_abs), 270_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    REQUIRE(here.ter_set(abs_to_bub(wall_pos_abs), ter_id("t_concrete_wall")));
    here.build_map_cache(0, true);

    CAPTURE(here.ter(abs_to_bub(wall_pos_abs)).id().str());
    CAPTURE(here.move_cost_ter_furn(abs_to_bub(wall_pos_abs)));
    REQUIRE(here.impassable_ter_furn(abs_to_bub(wall_pos_abs)));

    veh_ptr->velocity = 222;
    const auto probe = veh_ptr->part_collision(vehicle_part_collision_options{
        .part = 0,
        .pos = abs_to_bub(wall_pos_abs),
        .just_detect = true,
    });
    REQUIRE(probe.type != veh_coll_nothing);

    veh_ptr->velocity = 222;
    const auto ret = veh_ptr->part_collision(vehicle_part_collision_options{
        .part = 0,
        .pos = abs_to_bub(wall_pos_abs),
    });

    CHECK(ret.type != veh_coll_nothing);
    CHECK(std::abs(veh_ptr->velocity) < 222);
}

TEST_CASE("hallucination_monsters_do_not_shove_vehicles", "[vehicle][monster][hallucination]") {
    clear_all_state();
    move_player_out_of_the_way();
    auto& here = get_map();
    build_test_map(ter_id("t_pavement"));
    clear_vehicles();

    const auto monster_pos_abs = test_origin;
    const auto veh_pos_abs = monster_pos_abs + tripoint_rel_ms::north();
    auto* veh_ptr =
        here.add_vehicle(vproto_id("bicycle_test"), abs_to_bub(veh_pos_abs), 270_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    auto& hallucination =
        spawn_test_monster("mon_zombie_seaweed_brute", abs_to_bub(monster_pos_abs));
    hallucination.hallucination = true;
    REQUIRE(hallucination.is_hallucination());
    REQUIRE(hallucination.has_flag(MF_PUSH_VEH));

    hallucination
        .shove_vehicle(abs_to_bub(veh_pos_abs + tripoint_rel_ms::north()), abs_to_bub(veh_pos_abs));

    CHECK(veh_ptr->velocity == 0);
    CHECK(here.veh_at(abs_to_bub(veh_pos_abs)).has_value());
}

TEST_CASE("vehicle_collision_with_hallucination_terminates", "[vehicle]") {
    clear_all_state();
    auto& here = get_map();
    build_test_map(ter_id("t_pavement"));
    clear_vehicles();

    const auto veh_pos_abs = test_origin;
    const auto hallucination_pos_abs = veh_pos_abs + tripoint_rel_ms::north();

    auto* veh_ptr =
        here.add_vehicle(vproto_id("bicycle_test"), abs_to_bub(veh_pos_abs), 270_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    auto& hallucination = spawn_test_monster("mon_chicken", abs_to_bub(hallucination_pos_abs));
    hallucination.hallucination = true;
    REQUIRE(g->critter_at<monster>(abs_to_bub(hallucination_pos_abs), true) == &hallucination);

    veh_ptr->velocity = 5000;
    const auto ret = veh_ptr->part_collision(vehicle_part_collision_options{
        .part = 0,
        .pos = abs_to_bub(hallucination_pos_abs),
    });

    CHECK(ret.type == veh_coll_body);
    CHECK(ret.imp == 0);
    CHECK(hallucination.is_dead());
    CHECK(veh_ptr->velocity == 5000);
}

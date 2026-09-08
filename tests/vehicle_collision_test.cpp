#include "cache_validation.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "mtype.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle.h"

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

    const auto veh_pos = bub_test_origin();
    const auto wall_pos = veh_pos + point_rel_ms::north();

    auto* veh_ptr = here.add_vehicle(vproto_id("bicycle_test"), veh_pos, 270_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    REQUIRE(here.ter_set(wall_pos, ter_id("t_concrete_wall")));
    here.build_map_cache(0, true);

    CAPTURE(here.ter(wall_pos).id().str());
    CAPTURE(here.move_cost_ter_furn(wall_pos));
    REQUIRE(here.impassable_ter_furn(wall_pos));

    veh_ptr->velocity = 222;
    const auto probe = veh_ptr->part_collision(vehicle_part_collision_options{
        .part = 0,
        .pos = wall_pos,
        .just_detect = true,
    });
    REQUIRE(probe.type != veh_coll_nothing);

    veh_ptr->velocity = 222;
    const auto ret = veh_ptr->part_collision(vehicle_part_collision_options{
        .part = 0,
        .pos = wall_pos,
    });

    CHECK(ret.type != veh_coll_nothing);
    CHECK(std::abs(veh_ptr->velocity) < 222);
}

TEST_CASE("map_vehicle_placement_uses_resident_tiles", "[vehicle][mapbuffer]") {
    clear_all_state();
    auto& here = get_map();
    build_test_map(ter_id("t_pavement"));
    clear_vehicles();

    // A map facade can operate on resident submaps that are not currently
    // receiving simulation ticks.  Placement still needs to validate their
    // terrain and vehicle footprint.
    MAPBUFFER.set_simulated_submaps({});

    const auto vehicle_pos = bub_test_origin();
    auto* const vehicle =
        here.add_vehicle(vproto_id("bicycle_test"), vehicle_pos, 0_degrees, 0, 0, false);
    REQUIRE(vehicle != nullptr);
    CHECK(here.veh_at(vehicle_pos).has_value());

    const auto wall_pos = vehicle_pos + point_rel_ms(10, 0);
    REQUIRE(here.ter_set(wall_pos, ter_id("t_concrete_wall")));

    const auto blocked_vehicle =
        here.add_vehicle(vproto_id("bicycle_test"), wall_pos, 0_degrees, 0, 0, false);
    CHECK(blocked_vehicle == nullptr);
    CHECK_FALSE(MAPBUFFER.veh_at(map_local_to_abs(here, wall_pos)).has_value());
}

TEST_CASE("vehicle_cache_matches_defensive_rebuild", "[vehicle][cache]") {
    clear_all_state();
    auto& here = get_map();
    build_test_map(ter_id("t_pavement"));
    clear_vehicles();

    const auto vehicle_pos = bub_test_origin();
    auto* const vehicle =
        here.add_vehicle(vproto_id("bicycle_test"), vehicle_pos, 270_degrees, 0, 0);
    REQUIRE(vehicle != nullptr);
    REQUIRE(here.veh_at(vehicle_pos).has_value());

    CHECK(test_cache_validation::matches_after_defensive_rebuild(here, vehicle_pos.z(), [&]() {
        return here.veh_at(vehicle_pos).has_value();
    }));
}

TEST_CASE("hallucination_monsters_do_not_shove_vehicles", "[vehicle][monster][hallucination]") {
    clear_all_state();
    move_player_out_of_the_way();
    auto& here = get_map();
    build_test_map(ter_id("t_pavement"));
    clear_vehicles();

    const auto veh_pos = test_origin + point_rel_ms::north();
    auto* veh_ptr =
        here.add_vehicle(vproto_id("bicycle_test"), abs_to_bub(veh_pos), 270_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    auto& hallucination = spawn_test_monster("mon_zombie_seaweed_brute", bub_test_origin());
    hallucination.hallucination = true;
    REQUIRE(hallucination.is_hallucination());
    REQUIRE(hallucination.has_flag(MF_PUSH_VEH));

    hallucination.shove_vehicle(veh_pos + tripoint_north, veh_pos);

    CHECK(veh_ptr->velocity == 0);
    CHECK(here.get_mapbuffer().veh_at(veh_ptr->abs_ms_location()).has_value());
}

TEST_CASE("vehicle_collision_with_hallucination_terminates", "[vehicle]") {
    clear_all_state();
    auto& here = get_map();
    build_test_map(ter_id("t_pavement"));
    clear_vehicles();

    const auto veh_pos = bub_test_origin();
    const auto hallucination_pos = veh_pos + point_rel_ms::north();

    auto* veh_ptr = here.add_vehicle(vproto_id("bicycle_test"), veh_pos, 270_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    auto& hallucination = spawn_test_monster("mon_chicken", hallucination_pos);
    hallucination.hallucination = true;
    REQUIRE(g->critter_at<monster>(hallucination_pos, true) == &hallucination);

    veh_ptr->velocity = 5000;
    const auto ret = veh_ptr->part_collision(vehicle_part_collision_options{
        .part = 0,
        .pos = hallucination_pos,
    });

    CHECK(ret.type == veh_coll_body);
    CHECK(ret.imp == 0);
    CHECK(hallucination.is_dead());
    CHECK(veh_ptr->velocity == 5000);
}

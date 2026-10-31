#include "../src/map/map.h"
#include "../src/vehicle/vehicle_part.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "gun_mode.h"
#include "item.h"
#include "map_helpers.h"
#include "options_helpers.h"
#include "ranged.h"
#include "rng.h"
#include "state_helpers.h"
#include "type_id.h"
#include "units_utility.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"

#include <cmath>
#include <ranges>

TEST_CASE("firing_from_a_vehicle_applies_recoil_to_the_vehicle", "[vehicle][gun]") {
    clear_all_state();
    rng_set_engine_seed(0);

    auto& here = get_map();
    auto& player_character = get_avatar();
    const auto vehicle_origin_abs = test_origin;
    player_character.setpos(vehicle_origin_abs);

    auto* const veh =
        here.add_vehicle(vproto_id("bicycle"), abs_to_bub(vehicle_origin_abs), 0_degrees, 0, 0);
    REQUIRE(veh != nullptr);

    here.board_vehicle(abs_to_bub(vehicle_origin_abs), &player_character);
    REQUIRE(player_character.in_vehicle);

    auto gun = item::spawn(itype_id("m1014"));
    gun->ammo_set(itype_id("shot_bird"));
    player_character.wield(std::move(gun));
    REQUIRE(player_character.primary_weapon().typeId() == itype_id("m1014"));

    REQUIRE(veh->velocity == 0);

    const auto shots_fired = ranged::
        fire_gun(player_character, abs_to_bub(vehicle_origin_abs + tripoint_rel_ms(5, 0, 0)), 1);

    REQUIRE(shots_fired == 1);
    CHECK(veh->velocity != 0);
}

TEST_CASE("vehicle gun recoil scaling factor can disable vehicle thrust", "[vehicle][gun]") {
    clear_all_state();
    rng_set_engine_seed(0);

    override_option vehicle_gun_recoil_factor("VEHICLE_GUN_RECOIL_FACTOR", "0.0");

    auto& here = get_map();
    auto& player_character = get_avatar();
    const auto vehicle_origin_abs = test_origin;
    player_character.setpos(vehicle_origin_abs);

    auto* const veh =
        here.add_vehicle(vproto_id("bicycle"), abs_to_bub(vehicle_origin_abs), 0_degrees, 0, 0);
    REQUIRE(veh != nullptr);

    here.board_vehicle(abs_to_bub(vehicle_origin_abs), &player_character);
    REQUIRE(player_character.in_vehicle);

    auto gun = item::spawn(itype_id("m1014"));
    gun->ammo_set(itype_id("shot_bird"));
    player_character.wield(std::move(gun));
    REQUIRE(player_character.primary_weapon().typeId() == itype_id("m1014"));

    REQUIRE(veh->velocity == 0);

    const auto shots_fired = ranged::
        fire_gun(player_character, abs_to_bub(vehicle_origin_abs + tripoint_rel_ms(5, 0, 0)), 1);

    REQUIRE(shots_fired == 1);
    CHECK(veh->velocity == 0);
}

TEST_CASE("brake hold toggles parked braking drag", "[vehicle][drag]") {
    clear_all_state();

    auto& here = get_map();
    auto* const bicycle =
        here.add_vehicle(vproto_id("bicycle"), bub_test_origin(), 0_degrees, 0, 0);
    auto* const shopping_cart = here.add_vehicle(
        vproto_id("shopping_cart"), abs_to_bub(test_origin + tripoint_rel_ms(10, 0, 0)), 0_degrees,
        0, 0);

    REQUIRE(bicycle != nullptr);
    REQUIRE(shopping_cart != nullptr);

    CHECK(bicycle->static_drag() < bicycle->static_drag(false));

    bicycle->toggle_brake_hold();
    shopping_cart->toggle_brake_hold();

    CHECK(bicycle->static_drag() == bicycle->static_drag(false));
    CHECK(shopping_cart->static_drag() == shopping_cart->static_drag(false));
}

TEST_CASE(
    "single birdshot can move a swivel chair one tile on office floor at 10x recoil",
    "[vehic"
    "le]["
    "gun"
    "]") {
    clear_all_state();
    rng_set_engine_seed(0);

    override_option vehicle_gun_recoil_factor("VEHICLE_GUN_RECOIL_FACTOR", "10.0");

    auto& here = get_map();
    auto& player_character = get_avatar();
    const auto vehicle_origin_abs = test_origin;
    player_character.setpos(vehicle_origin_abs);
    const auto map_origin_abs =
        tripoint_abs_ms(project_to<coords::ms>(here.get_abs_sub()), vehicle_origin_abs.z());

    for (const auto x : std::views::iota(40, 81)) {
        const auto floor_pos = abs_to_bub(
            map_origin_abs + tripoint_rel_ms(x, vehicle_origin_abs.y(), 0));
        here.ter_set(
            floor_pos,
            ter_id("t_"
                   "linoleum_"
                   "white"));
        here.furn_set(
            floor_pos,
            furn_id("f_"
                    "null"));
    }

    auto* const veh = here.add_vehicle(
        vproto_id("swivel_chair"), abs_to_bub(vehicle_origin_abs), 0_degrees, 0, 0);
    REQUIRE(veh != nullptr);

    veh->toggle_brake_hold();

    here.board_vehicle(abs_to_bub(vehicle_origin_abs), &player_character);
    REQUIRE(player_character.in_vehicle);

    auto gun = item::spawn(itype_id("m1014"));
    gun->ammo_set(itype_id("shot_bird"));
    player_character.wield(std::move(gun));
    REQUIRE(player_character.primary_weapon().typeId() == itype_id("m1014"));

    const auto starting_pos = veh->bub_ms_location();

    const auto shots_fired = ranged::
        fire_gun(player_character, abs_to_bub(vehicle_origin_abs + tripoint_rel_ms(5, 0, 0)), 1);

    REQUIRE(shots_fired == 1);
    REQUIRE(veh->velocity != 0);

    for (const auto _ : std::views::iota(0, 20)) {
        (void)_;
        here.vehmove();
        if (veh->bub_ms_location() != starting_pos) { break; }
    }

    CHECK(square_dist(starting_pos, veh->bub_ms_location()) >= 1);
    CHECK(player_character.bub_pos() == veh->bub_ms_location());
}

TEST_CASE(
    "vehicle gun recoil can launch a shopping cart with a mounted M2 Browning near 6 km/h",
    "[vehic"
    "le]["
    "gun"
    "]") {
    clear_all_state();
    rng_set_engine_seed(0);

    auto& here = get_map();
    auto& player_character = get_avatar();
    const auto vehicle_origin_abs = test_origin;
    player_character.setpos(vehicle_origin_abs);
    auto* const veh = here.add_vehicle(
        vproto_id("shopping_cart"), abs_to_bub(vehicle_origin_abs), 0_degrees, 0, 0);
    REQUIRE(veh != nullptr);

    REQUIRE(veh->install_part(tripoint_mnt_veh::zero(), vpart_id("turret_mount_manual_steel"), true)
            >= 0);
    const auto turret_index =
        veh->install_part(tripoint_mnt_veh::zero(), vpart_id("mounted_browning"), true);
    REQUIRE(turret_index >= 0);
    REQUIRE(veh->part(turret_index).ammo_set(itype_id("50bmg")));

    here.board_vehicle(abs_to_bub(vehicle_origin_abs), &player_character);
    REQUIRE(player_character.in_vehicle);

    auto turret = veh->turret_query(veh->part(turret_index));
    REQUIRE(turret);
    REQUIRE(turret.base().gun_set_mode(gun_mode_id("AUTO")));
    REQUIRE(turret.query() == turret_data::status::ready);

    REQUIRE(veh->velocity == 0);

    constexpr auto target_shots = 15;
    constexpr auto max_bursts = 8;
    auto shots_fired = 0;
    for (const auto _ : std::views::iota(0, max_bursts)) {
        (void)_;
        if (shots_fired >= target_shots) { break; }
        const auto target = vehicle_origin_abs + tripoint_rel_ms(10, 0, 0);
        shots_fired += turret.fire(player_character, target);
    }

    REQUIRE(shots_fired >= target_shots);
    CHECK(std::abs(veh->velocity) >= 160);
    CHECK(veh->forward_velocity() < 0.0f);
}

TEST_CASE(
    "perpendicular gun recoil keeps full sideways push on rigid-wheel vehicles",
    "[vehicle]["
    "gun]") {
    const auto vehicle_origin_abs = test_origin;

    override_option vehicle_gun_recoil_factor("VEHICLE_GUN_RECOIL_FACTOR", "1.0");

    struct recoil_result {
        int velocity = 0;
        bool skidding = false;
        units::angle move_dir = 0_degrees;
    };

    const auto fire_recoil =
        [&](const vproto_id& vehicle_type, const units::angle facing,
            const tripoint_abs_ms& target_abs,
            const std::optional<tripoint_abs_ms>& shot_origin_abs) -> recoil_result {
        clear_all_state();
        rng_set_engine_seed(0);

        auto& here = get_map();
        auto& player_character = get_avatar();
        player_character.setpos(vehicle_origin_abs);
        auto* const veh =
            here.add_vehicle(vehicle_type, abs_to_bub(vehicle_origin_abs), facing, 0, 0);
        REQUIRE(veh != nullptr);

        here.board_vehicle(abs_to_bub(vehicle_origin_abs), &player_character);
        REQUIRE(player_character.in_vehicle);

        auto gun = item::spawn(itype_id("m1014"));
        gun->ammo_set(itype_id("shot_00"));
        player_character.wield(std::move(gun));
        REQUIRE(player_character.primary_weapon().typeId() == itype_id("m1014"));

        REQUIRE(veh->velocity == 0);

        const auto target = abs_to_bub(target_abs);
        std::optional<tripoint_bub_ms> shot_origin;
        if (shot_origin_abs) { shot_origin = abs_to_bub(*shot_origin_abs); }

        auto shots_fired = 0;
        for (const auto _ : std::views::iota(0, 5)) {
            (void)_;
            shots_fired += ranged::fire_gun(
                player_character, target, 1, player_character.primary_weapon(), nullptr,
                shot_origin);
        }

        REQUIRE(shots_fired == 5);
        return recoil_result{
            .velocity = std::abs(veh->velocity),
            .skidding = veh->skidding,
            .move_dir = veh->move.dir(),
        };
    };

    const auto forward_result = fire_recoil(
        vproto_id("shopping_cart"), 180_degrees, test_origin + tripoint_rel_ms(-5, 0, 0),
        std::nullopt);
    const auto offset_lateral_result = fire_recoil(
        vproto_id("grocery_cart"), -90_degrees, test_origin + tripoint_rel_ms(-6, 0, 0),
        test_origin + tripoint_rel_ms(-1, 0, 0));

    CHECK(forward_result.skidding);
    CHECK(normalize(forward_result.move_dir) == normalize(0_degrees));
    CHECK(forward_result.velocity > 0);
    CHECK(offset_lateral_result.skidding);
    CHECK(normalize(offset_lateral_result.move_dir) == normalize(0_degrees));
    CHECK(offset_lateral_result.velocity > 0);
}

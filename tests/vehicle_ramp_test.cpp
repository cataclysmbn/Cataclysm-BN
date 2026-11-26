#include "../src/map/map.h"
#include "../src/map/mapdata.h"
#include "../src/vehicle/vehicle_part.h"
#include "../src/vehicle/vpart_position.h"
#include "avatar.h"
#include "avatar_action.h"
#include "bodypart.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "enums.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "itype.h"
#include "line.h"
#include "map_helpers.h"
#include "map_iterator.h"
#include "monster.h"
#include "mtype.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "units.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_grab.h"
#include "vehicle/vpart_range.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

static auto ramp_test_pos_abs(const int x, const int y, const int z) -> tripoint_abs_ms {
    return test_origin + tripoint_rel_ms(x, y, z);
}

static auto set_ramp(const tripoint_abs_ms& transit_pos_abs, const bool use_ramp, const bool up)
    -> void {
    // Set to turn 0 to prevent solars from producing power
    calendar::turn = calendar::turn_zero;
    Character& player_character = get_player_character();
    // Move player somewhere safe
    REQUIRE_FALSE(player_character.in_vehicle);

    map& here = get_map();
    build_test_map(ter_id("t_pavement"));
    const auto map_origin_abs = tripoint_abs_ms(project_to<coords::ms>(here.get_abs_sub()), 0);
    const auto transit_x = abs_to_map_local(here, transit_pos_abs).x();
    const auto map_pos = [map_origin_abs](const int x, const int y, const int z) {
        return abs_to_bub(map_origin_abs + tripoint_rel_ms(x, y, z));
    };
    if (use_ramp) {
        const int upper_zlevel = up ? 1 : 0;
        const int lower_zlevel = up - 1;
        const int highx = transit_x + (up ? 0 : 1);
        const int lowx = transit_x + (up ? 1 : 0);

        // up   z1    ......  rdh  rDl
        //      z0            rUh  rul .................
        // down z0            rDl  rdh .................
        //      z-1   ......  rdl  rUh
        //                 transition and neighboring tile
        for (int y = 0; y < SEEY * MAPSIZE; y++) {
            for (int x = 0; x < transit_x; x++) {
                const int mid = up ? upper_zlevel : lower_zlevel;
                here.ter_set(map_pos(x, y, mid - 2), ter_id("t_rock"));
                here.ter_set(map_pos(x, y, mid - 1), ter_id("t_rock"));
                here.ter_set(map_pos(x, y, mid), ter_id("t_pavement"));
                here.ter_set(map_pos(x, y, mid + 1), ter_id("t_open_air"));
                here.ter_set(map_pos(x, y, mid + 2), ter_id("t_open_air"));
            }
            const auto ramp_up_low = map_pos(lowx, y, lower_zlevel);
            const auto ramp_up_high = map_pos(highx, y, lower_zlevel);
            const auto ramp_down_low = map_pos(lowx, y, upper_zlevel);
            const auto ramp_down_high = map_pos(highx, y, upper_zlevel);
            here.ter_set(ramp_up_low, ter_id("t_ramp_up_low"));
            here.ter_set(ramp_up_high, ter_id("t_ramp_up_high"));
            here.ter_set(ramp_down_low, ter_id("t_ramp_down_low"));
            here.ter_set(ramp_down_high, ter_id("t_ramp_down_high"));
            for (int x = transit_x + 2; x < SEEX * MAPSIZE; x++) {
                here.ter_set(map_pos(x, y, 1), ter_id("t_open_air"));
                here.ter_set(map_pos(x, y, 0), ter_id("t_pavement"));
                here.ter_set(map_pos(x, y, -1), ter_id("t_rock"));
            }
        }
    }
    for (const auto z : std::array{-1, 0, 1}) {
        here.invalidate_map_cache(z);
        here.build_map_cache(z, true);
    }
}

static auto setup_grabbed_shopping_cart(
    const tripoint_abs_ms& player_pos_abs, const tripoint_abs_ms& cart_pos_abs) -> vehicle& {
    auto& here = get_map();
    auto& player_character = get_avatar();
    player_character.setpos(player_pos_abs);
    player_character.str_max = 100;
    player_character.str_cur = 100;

    auto* veh_ptr =
        here.add_vehicle(vproto_id("shopping_cart"), abs_to_bub(cart_pos_abs), 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    for (const auto vp : veh_ptr->get_all_parts()) { veh_ptr->get_items(vp.part_index()).clear(); }

    const auto cart_offset = cart_pos_abs - player_character.abs_pos();
    player_character.grab(OBJECT_VEHICLE, cart_offset);
    REQUIRE(player_character.get_grab_type() == OBJECT_VEHICLE);
    REQUIRE(player_character.grab_point == cart_offset);

    return *veh_ptr;
}

static auto check_avatar_still_grabs(const vehicle& expected_vehicle) -> void {
    auto& player_character = get_avatar();
    CHECK(player_character.get_grab_type() == OBJECT_VEHICLE);
    const auto grabbed_target =
        vehicle_grab_target_at(get_map(), player_character.bub_pos() + player_character.grab_point);
    REQUIRE(grabbed_target);
    CHECK(&grabbed_target->vp.vehicle() == &expected_vehicle);
}

// Algorithm goes as follows:
// Clear map and create a ramp
// Spawn a vehicle
// Drive it over the ramp, and confirm that the vehicle changes z-levels
static auto ramp_transition_angled(
    const vproto_id& veh_id, const units::angle angle, const tripoint_abs_ms& transition_pos_abs,
    const bool use_ramp, const bool up) -> void {
    map& here = get_map();
    set_ramp(transition_pos_abs, use_ramp, up);

    const auto map_starting_point_abs = transition_pos_abs + tripoint_rel_ms(4, 0, 0);
    const auto initial_map_starting_point = abs_to_map_local(here, map_starting_point_abs);
    REQUIRE(here.ter(initial_map_starting_point) == ter_id("t_pavement"));
    if (here.ter(initial_map_starting_point) != ter_id("t_pavement")) { return; }
    vehicle* veh_ptr = here.add_vehicle(veh_id, initial_map_starting_point, angle, 1, 0);

    REQUIRE(veh_ptr != nullptr);
    if (veh_ptr == nullptr) { return; }

    vehicle& veh = *veh_ptr;
    veh.check_falling_or_floating();

    REQUIRE(!veh.is_in_water());

    veh.tags.insert("IN_CONTROL_OVERRIDE");
    veh.engine_on = true;
    Character& player_character = get_player_character();
    get_avatar().setpos(map_starting_point_abs);

    REQUIRE(player_character.abs_pos() == map_starting_point_abs);
    here.board_vehicle(abs_to_bub(map_starting_point_abs), &player_character);
    REQUIRE(player_character.in_vehicle);
    const int transition_cycle = 3;
    veh.cruise_velocity = 0;
    veh.velocity = 0;
    here.vehmove();

    const int target_velocity = 179;
    veh.cruise_velocity = target_velocity;
    veh.velocity = target_velocity;
    CHECK(veh.safe_velocity() > 0);
    int cycles = 0;
    const int target_z = use_ramp ? (up ? 1 : -1) : 0;

    std::set<tripoint_abs_ms> vpts = veh.get_points();
    while (veh.engine_on && veh.safe_velocity() > 0 && cycles < 10) {
        CAPTURE(cycles);
        for (const tripoint_abs_ms& checkpt : vpts) {
            int partnum = 0;
            vehicle* check_veh = here.veh_at_internal(abs_to_map_local(here, checkpt), partnum);
            CHECK(check_veh == veh_ptr);
        }
        vpts.clear();
        here.vehmove();
        CHECK(veh.velocity == target_velocity);
        // If the vehicle starts skidding, the effects become random and test is RUINED
        REQUIRE(!veh.skidding);
        for (const tripoint_abs_ms& pos : veh.get_points()) {
            REQUIRE(here.ter(abs_to_map_local(here, pos)));
        }
        for (const vpart_reference& vp : veh.get_all_parts()) {
            if (vp.info().location != "structure") { continue; }
            const tripoint_mnt_veh& pmount = vp.mount();
            CAPTURE(pmount);
            const tripoint_bub_ms& ppos = vp.pos();
            CAPTURE(ppos);
            if (cycles > (transition_cycle - pmount.x())) { CHECK(ppos.z() == target_z); }
            if (pmount.x() == 0 && pmount.y() == 0) { CHECK(player_character.bub_pos() == ppos); }
        }
        vpts = veh.get_points();
        cycles++;
    }

    const int expected_move = use_ramp ? (up ? 1 : -1) : 0;
    CHECK(veh.abs_ms_location().z() - map_starting_point_abs.z() == expected_move);

    const std::optional<vpart_reference> vp =
        here.veh_at(abs_to_bub(player_character.abs_pos()))
            .part_with_feature(VPFLAG_BOARDABLE, true);
    REQUIRE(vp);
    if (vp) {
        // Regression: get_passenger() must return the correct passenger regardless
        // of the vehicle's z-level.  After a ramp transition bub_ms_location().z is
        // non-zero, but precalc[0].z is always zeroed by precalc_mounts().  The old
        // mount_to_bubble() path returned bub_ms_location().z while precalc[0].z == 0,
        // so get_parts_at() found no BOARDABLE part and get_passenger() returned nullptr.
        // This check is most meaningful for use_ramp == true where z != 0 post-transition.
        CHECK(veh.get_passenger(static_cast<int>(vp->part_index())) != nullptr);
        const int z_change = map_starting_point_abs.z() - player_character.abs_pos().z();
        here.unboard_vehicle(*vp, &player_character, false);
        here.ter_set(abs_to_bub(map_starting_point_abs), ter_id("t_pavement"));
        get_avatar().setpos(map_starting_point_abs);
        if (z_change) { g->vertical_move(z_change, true); }
    }
    here.destroy_vehicle(veh_ptr);
}

TEST_CASE("grabbed_shopping_cart_can_be_pulled_up_ramp", "[vehicle][ramp][grab]") {
    clear_all_state();
    set_ramp(test_origin, true, true);

    auto& here = get_map();
    auto& player_character = get_avatar();
    auto& cart =
        setup_grabbed_shopping_cart(ramp_test_pos_abs(1, 0, 0), ramp_test_pos_abs(2, 0, 0));
    const auto player_start_abs = player_character.abs_pos();
    const auto cart_start_abs = cart.abs_ms_location();

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-1, 0, 1));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-1, 0, 0));
    check_avatar_still_grabs(cart);

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-2, 0, 1));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-2, 0, 1));
    check_avatar_still_grabs(cart);
}

TEST_CASE("grabbed_shopping_cart_can_be_pulled_on_flat_ground", "[vehicle][grab]") {
    clear_all_state();
    set_ramp(test_origin, false, true);

    auto& here = get_map();
    auto& player_character = get_avatar();
    auto& cart =
        setup_grabbed_shopping_cart(ramp_test_pos_abs(1, 0, 0), ramp_test_pos_abs(2, 0, 0));
    const auto player_start_abs = player_character.abs_pos();
    const auto cart_start_abs = cart.abs_ms_location();

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-1, 0, 0));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-1, 0, 0));
    check_avatar_still_grabs(cart);
}

TEST_CASE("grabbed_shopping_cart_can_be_pulled_with_debug_noclip", "[vehicle][grab]") {
    clear_all_state();
    set_ramp(test_origin, false, true);

    auto& here = get_map();
    auto& player_character = get_avatar();
    auto& cart =
        setup_grabbed_shopping_cart(ramp_test_pos_abs(1, 0, 0), ramp_test_pos_abs(2, 0, 0));
    const auto player_start_abs = player_character.abs_pos();
    const auto cart_start_abs = cart.abs_ms_location();
    player_character.set_mutation(trait_id("DEBUG_NOCLIP"));

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-1, 0, 0));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-1, 0, 0));
    check_avatar_still_grabs(cart);
}

TEST_CASE("grabbed_shopping_cart_can_be_pushed_on_flat_ground", "[vehicle][grab]") {
    clear_all_state();
    set_ramp(test_origin, false, true);

    auto& here = get_map();
    auto& player_character = get_avatar();
    auto& cart =
        setup_grabbed_shopping_cart(ramp_test_pos_abs(2, 0, 0), ramp_test_pos_abs(1, 0, 0));
    const auto player_start_abs = player_character.abs_pos();
    const auto cart_start_abs = cart.abs_ms_location();

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-1, 0, 0));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-1, 0, 0));
    check_avatar_still_grabs(cart);
}

TEST_CASE("grabbed_shopping_cart_can_be_pushed_with_debug_noclip", "[vehicle][grab]") {
    clear_all_state();
    set_ramp(test_origin, false, true);

    auto& here = get_map();
    auto& player_character = get_avatar();
    auto& cart =
        setup_grabbed_shopping_cart(ramp_test_pos_abs(2, 0, 0), ramp_test_pos_abs(1, 0, 0));
    const auto player_start_abs = player_character.abs_pos();
    const auto cart_start_abs = cart.abs_ms_location();
    player_character.set_mutation(trait_id("DEBUG_NOCLIP"));

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-1, 0, 0));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-1, 0, 0));
    check_avatar_still_grabs(cart);
}

TEST_CASE("grabbed_shopping_cart_can_be_pushed_up_ramp", "[vehicle][ramp][grab]") {
    clear_all_state();
    set_ramp(test_origin, true, true);

    auto& here = get_map();
    auto& player_character = get_avatar();
    auto& cart =
        setup_grabbed_shopping_cart(ramp_test_pos_abs(2, 0, 0), ramp_test_pos_abs(1, 0, 0));
    const auto player_start_abs = player_character.abs_pos();
    const auto cart_start_abs = cart.abs_ms_location();

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-1, 0, 0));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-1, 0, 1));
    check_avatar_still_grabs(cart);

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-2, 0, 1));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-2, 0, 1));
    check_avatar_still_grabs(cart);
}

TEST_CASE("grabbed_shopping_cart_can_be_pulled_down_ramp", "[vehicle][ramp][grab]") {
    clear_all_state();
    set_ramp(test_origin, true, false);

    auto& here = get_map();
    auto& player_character = get_avatar();
    auto& cart =
        setup_grabbed_shopping_cart(ramp_test_pos_abs(1, 0, 0), ramp_test_pos_abs(2, 0, 0));
    const auto player_start_abs = player_character.abs_pos();
    const auto cart_start_abs = cart.abs_ms_location();

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-1, 0, -1));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-1, 0, 0));
    check_avatar_still_grabs(cart);

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-2, 0, -1));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-2, 0, -1));
    check_avatar_still_grabs(cart);
}

TEST_CASE("grabbed_shopping_cart_can_be_pushed_down_ramp", "[vehicle][ramp][grab]") {
    clear_all_state();
    set_ramp(test_origin, true, false);

    auto& here = get_map();
    auto& player_character = get_avatar();
    auto& cart =
        setup_grabbed_shopping_cart(ramp_test_pos_abs(2, 0, 0), ramp_test_pos_abs(1, 0, 0));
    const auto player_start_abs = player_character.abs_pos();
    const auto cart_start_abs = cart.abs_ms_location();

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-1, 0, 0));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-1, 0, -1));
    check_avatar_still_grabs(cart);

    REQUIRE(avatar_action::move(tripoint_rel_ms::west()));
    CHECK(player_character.abs_pos() == player_start_abs + tripoint_rel_ms(-2, 0, -1));
    CHECK(cart.abs_ms_location() == cart_start_abs + tripoint_rel_ms(-2, 0, -1));
    check_avatar_still_grabs(cart);
}

// I'd like to do this in a single loop, but that doesn't work for some reason
TEST_CASE("vehicle_ramp_test_before_center", "[vehicle][ramp]") {
    clear_all_state();
    ramp_transition_angled(
        vproto_id("motorcycle"), 180_degrees, ramp_test_pos_abs(-1, 0, 0), true, true);
    ramp_transition_angled(
        vproto_id("motorcycle"), 180_degrees, ramp_test_pos_abs(-1, 0, 0), true, false);
}
TEST_CASE("vehicle_ramp_test_at_center", "[vehicle][ramp]") {
    clear_all_state();
    ramp_transition_angled(vproto_id("motorcycle"), 180_degrees, test_origin, true, true);
    ramp_transition_angled(vproto_id("motorcycle"), 180_degrees, test_origin, true, false);
}
TEST_CASE("vehicle_ramp_test_after_center", "[vehicle][ramp]") {
    clear_all_state();
    ramp_transition_angled(
        vproto_id("motorcycle"), 180_degrees, ramp_test_pos_abs(1, 0, 0), true, true);
    ramp_transition_angled(
        vproto_id("motorcycle"), 180_degrees, ramp_test_pos_abs(1, 0, 0), true, false);
}

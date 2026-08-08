#include "avatar.h"
#include "avatar_action.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "creature_tracker.h"
#include "damage.h"
#include "debug.h"
#include "enums.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "json.h"
#include "map.h"
#include "map_helpers.h"
#include "mongroup.h"
#include "monster.h"
#include "options_helpers.h"
#include "overmapbuffer.h"
#include "simulated_island_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vehicle_wait.h"
#include "vpart_position.h"
#include "vpart_range.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

const auto horde_spawn_test_group = mongroup_id("GROUP_ZOMBIE");
const auto horde_spawn_test_monster = mtype_id("mon_zombie");

struct horde_vehicle_spawn_options {
    bool owned = false;
    bool tracked = false;
};

struct horde_vehicle_spawn_fixture {
    std::set<tripoint_abs_ms> vehicle_points;
    mongroup* horde = nullptr;
};

auto point_has_monster(const tripoint_abs_ms& p) -> bool {
    return g->critter_tracker->find(p) != nullptr;
}

auto vehicle_points_contain_monster(const std::set<tripoint_abs_ms>& vehicle_points) -> bool {
    return std::ranges::any_of(vehicle_points, point_has_monster);
}

auto make_horde_vehicle_spawn_fixture(const horde_vehicle_spawn_options& options)
    -> horde_vehicle_spawn_fixture {
    clear_all_state();
    ACTIVE_OVERMAP_BUFFER.clear();

    auto& map = get_map();
    auto& you = get_avatar();
    auto& here = you.get_mapbuffer();
    const auto target_submap = tripoint_abs_sm::zero();
    const auto target_submap_origin = project_to<coords::ms>(target_submap);
    const auto target_submap_end = target_submap_origin + tripoint(SEEX - 1, SEEY - 1, 0);
    const auto vehicle_origin = target_submap_origin + tripoint(SEEX / 2, SEEY / 2, 0);

    you.setpos(target_submap_origin);
    const auto veh = map.add_vehicle(vproto_id("car"), abs_to_bub(vehicle_origin), 0_degrees, 0, 0);
    REQUIRE(veh != nullptr);

    auto group = mongroup(horde_spawn_test_group, target_submap, 1, 0);
    group.horde = true;
    group.interest = 10;
    group.monsters.emplace_back(horde_spawn_test_monster);

    ACTIVE_OVERMAP_BUFFER.get(project_remain<coords::om>(target_submap).quotient);
    auto target_groups = ACTIVE_OVERMAP_BUFFER.groups_at(target_submap);
    std::ranges::for_each(target_groups, [](mongroup* const target_group) {
        target_group->clear();
    });
    ACTIVE_OVERMAP_BUFFER.discard_monster_map(target_submap);

    const auto horde = ACTIVE_OVERMAP_BUFFER.create_horde(group);
    REQUIRE(horde != nullptr);

    if (options.owned) { veh->set_owner(you); }
    if (options.tracked) { veh->toggle_tracking(); }

    const auto vehicle_points = veh->get_points(true);
    const auto horde_spawn_blocking_terrain = ter_id("t_wall");
    for (const auto& handle :
         simulated_tiles_in_rectangle(here, target_submap_origin, target_submap_end)) {
        const auto p = handle.abs_pos();
        if (!vehicle_points.contains(p)) { here.set_ter(p, horde_spawn_blocking_terrain); }
    };
    map.invalidate_map_cache(target_submap.z());
    map.build_map_cache(target_submap.z(), true);

    return horde_vehicle_spawn_fixture{.vehicle_points = vehicle_points, .horde = horde};
}

auto vehicle_with_legacy_pivot_json() -> std::string {
    return R"json(
           {
           "type": "none",
           "posx": 5,
           "posy": 6,
           "om_id": 0,
           "faceDir": 180,
           "moveDir": 180,
           "turn_dir": 180,
           "velocity": 0,
           "falling": false,
           "floating": false,
           "flying": false,
           "cruise_velocity": 0,
           "vertical_velocity": 0,
           "name": "legacy pivot test vehicle",
           "owner": "",
           "old_owner": "",
           "parts": [
           {
           "id": "frame_horizontal",
           "mount_dx": 0,
           "mount_dy": 0,
           "open": false,
           "direction": 0,
           "blood": 0,
           "proxy_part_id": "null",
           "proxy_sym": 0,
           "enabled": false,
           "flags": 0,
           "passenger_id": -1,
           "crew_id": -1,
           "items": [],
           "ammo_pref": "null",
           "part_color": [ 0, 0, 0, 0 ]
       }
           ],
           "pivot": [ -1, 0 ],
           "zones": []
       }
           )json";
}

auto vehicle_with_invalid_part_and_legacy_pivot_json() -> std::string {
    return R"json(
           {
           "type": "none",
           "posx": 5,
           "posy": 6,
           "om_id": 0,
           "faceDir": 180,
           "moveDir": 180,
           "turn_dir": 180,
           "velocity": 0,
           "falling": false,
           "floating": false,
           "flying": false,
           "cruise_velocity": 0,
           "vertical_velocity": 0,
           "name": "legacy pivot invalid part test vehicle",
           "owner": "",
           "old_owner": "",
           "parts": [
           {
           "id": "missing_saved_vehicle_part",
           "mount_dx": 99,
           "mount_dy": 99,
           "open": false,
           "direction": 0,
           "blood": 0,
           "proxy_part_id": "null",
           "proxy_sym": 0,
           "enabled": false,
           "flags": 0,
           "passenger_id": -1,
           "crew_id": -1,
           "items": [],
           "ammo_pref": "null",
           "part_color": [ 0, 0, 0, 0 ]
       },
           {
           "id": "frame_horizontal",
           "mount_dx": 0,
           "mount_dy": 0,
           "open": false,
           "direction": 0,
           "blood": 0,
           "proxy_part_id": "null",
           "proxy_sym": 0,
           "enabled": false,
           "flags": 0,
           "passenger_id": -1,
           "crew_id": -1,
           "items": [],
           "ammo_pref": "null",
           "part_color": [ 0, 0, 0, 0 ]
       }
           ],
           "pivot": [ -1, 0 ],
           "zones": []
       }
           )json";
}

} // namespace

TEST_CASE("vehicle_cargo_uses_full_part_volume", "[vehicle][cargo][volume]") {
    clear_map();

    auto& here = get_map().get_mapbuffer();
    auto* veh_ptr = here.add_vehicle(vproto_id("none"), test_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);
    REQUIRE(veh_ptr->install_part(tripoint_mnt_veh::zero(), vpart_id("frame_vertical"), true) >= 0);

    const auto cargo_index =
        veh_ptr->install_part(tripoint_mnt_veh::zero(), vpart_id("test_large_cargo_space"), true);
    REQUIRE(cargo_index >= 0);

    CHECK(veh_ptr->max_volume(cargo_index) == 3000000_liter);
    CHECK(veh_ptr->free_volume(cargo_index) == 3000000_liter);
}

TEST_CASE("vehicle deserialize accepts legacy two coordinate pivot", "[vehicle][save]") {
    auto json = std::istringstream(vehicle_with_legacy_pivot_json());
    auto jsin = JsonIn(json);
    auto veh = vehicle();

    REQUIRE(jsin.read(veh, true));
    CHECK(veh.mount_to_abs(tripoint_mnt_veh(-1, 0, 0)) == tripoint_abs_ms(5, 6, 0));
    CHECK(veh.mount_to_abs(tripoint_mnt_veh(0, 0, 0)) == tripoint_abs_ms(4, 6, 0));
}

TEST_CASE("vehicle deserialize keeps valid saved parts after an invalid part", "[vehicle][save]") {
    auto json = std::istringstream(vehicle_with_invalid_part_and_legacy_pivot_json());
    auto jsin = JsonIn(json);
    auto veh = vehicle();
    auto loaded = false;

    const auto debug_msg = capture_debugmsg_during([&]() { loaded = jsin.read(veh, true); });

    REQUIRE(loaded);
    CHECK(debug_msg.find("Skipping invalid saved vehicle part") != std::string::npos);
    CHECK(debug_msg.find("missing_saved_vehicle_part") != std::string::npos);
    CHECK(veh.part_count() == 1);
    CHECK(veh.mount_to_abs(tripoint_mnt_veh(0, 0, 0)) == tripoint_abs_ms(4, 6, 0));
}

TEST_CASE("detaching_vehicle_unboards_passengers") {
    clear_all_state();
    map& map = get_map();
    avatar& player_character = get_avatar();
    auto& here = player_character.get_mapbuffer();
    vehicle* veh_ptr = map.add_vehicle(vproto_id("bicycle"), bub_test_origin(), -90_degrees, 0, 0);
    here.board_vehicle(test_origin, player_character);
    REQUIRE(player_character.in_vehicle);
    map.detach_vehicle(veh_ptr);
    REQUIRE(!player_character.in_vehicle);
}

TEST_CASE("detaching_opaque_vehicle_invalidates_transparency_cache", "[vehicle][map_cache]") {
    clear_all_state();
    auto& here = get_map();
    build_test_map(ter_id("t_pavement"));

    const auto origin = bub_test_origin();
    auto* veh_ptr = here.add_vehicle(vproto_id("none"), origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);
    REQUIRE(
        veh_ptr->install_part(tripoint_mnt_veh::zero(), vpart_id("frame_horizontal"), true) >= 0);
    const auto board =
        veh_ptr->install_part(tripoint_mnt_veh::zero(), vpart_id("clothboard_horizontal"), true);
    REQUIRE(board >= 0);

    const auto board_pos = veh_ptr->bub_part_location(board);
    here.add_vehicle_to_cache(veh_ptr);
    here.build_map_cache(board_pos.z(), true);
    REQUIRE_FALSE(here.is_transparent(board_pos));

    here.destroy_vehicle(veh_ptr);
    here.build_map_cache(board_pos.z(), true);

    CHECK(here.is_transparent(board_pos));
}

TEST_CASE("destroy_grabbed_vehicle_section") {
    clear_all_state();
    GIVEN("A vehicle grabbed by the player") {
        map& map = get_map();
        avatar& player_character = get_avatar();
        auto& here = player_character.get_mapbuffer();
        player_character.setpos(test_origin);
        const auto vehicle_origin = test_origin + tripoint_south_east;
        vehicle* veh_ptr =
            map.add_vehicle(vproto_id("bicycle"), abs_to_bub(vehicle_origin), -90_degrees, 0, 0);
        REQUIRE(veh_ptr != nullptr);
        auto grab_point = test_origin + tripoint_rel_ms::east();
        player_character.grab(OBJECT_VEHICLE, tripoint_rel_ms::east());
        REQUIRE(player_character.get_grab_type() != OBJECT_NONE);
        REQUIRE(player_character.grab_point == tripoint_rel_ms::east());
        WHEN("The vehicle section grabbed by the player is destroyed") {
            here.destroy(grab_point);
            REQUIRE(veh_ptr->get_parts_at(grab_point, "", part_status_flag::available).empty());
            THEN("The player's grab is released") {
                CHECK(player_character.get_grab_type() == OBJECT_NONE);
                CHECK(player_character.grab_point == tripoint_rel_ms::zero());
            }
        }
    }
}

TEST_CASE("taking_control_of_vehicle_without_engine", "[vehicle]") {
    clear_all_state();
    auto& player_character = get_avatar();
    player_character.setpos(test_origin);

    auto* veh_ptr = get_map().get_mapbuffer().add_vehicle(
        vproto_id("shopping_cart"), test_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);
    REQUIRE_FALSE(player_character.controlling_vehicle);
    REQUIRE_FALSE(veh_ptr->engine_on);

    veh_ptr->start_engines(true);

    CHECK(player_character.controlling_vehicle);
    CHECK_FALSE(veh_ptr->engine_on);
    CHECK(!player_character.activity);
}

TEST_CASE("moving_flying_vehicle_can_use_wait_menu", "[vehicle][wait]") {
    clear_all_state();

    auto* veh_ptr = get_map().get_mapbuffer().add_vehicle(
        vproto_id("plane_small"), test_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    veh_ptr->velocity = 100;
    CHECK(vehicle_wait::is_wait_blocked_by_movement(*veh_ptr));
    CHECK_FALSE(vehicle_wait::should_offer_flying_wait_durations(*veh_ptr));

    veh_ptr->set_flying(true);
    CHECK_FALSE(vehicle_wait::is_wait_blocked_by_movement(*veh_ptr));
    CHECK(vehicle_wait::should_offer_flying_wait_durations(*veh_ptr));
}

TEST_CASE("vehicle control scale modifies throttle move cost", "[vehicle][speed]") {
    clear_all_state();
    const auto global_scale = override_option("TIME_ACTION_SCALE", "50");
    const auto player_scale = override_option("PLAYER_ACTION_SCALE", "50");
    const auto vehicle_control_scale = override_option("VEHICLE_CONTROL_SCALE", "100");

    auto& you = get_avatar();
    you.set_moves(25);

    auto* veh_ptr =
        get_map().get_mapbuffer().add_vehicle(vproto_id("bicycle"), test_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    veh_ptr->cruise_on = false;
    veh_ptr->pldrive(you, tripoint_rel_veh{0, 1, 0});

    CHECK(you.get_moves() == -25);
}

TEST_CASE("vehicle speed control free in cruise mode", "[vehicle][speed]") {
    clear_all_state();

    auto& you = get_avatar();
    you.set_moves(25);

    auto* veh_ptr =
        get_map().get_mapbuffer().add_vehicle(vproto_id("bicycle"), test_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    veh_ptr->cruise_on = true;
    veh_ptr->pldrive(you, tripoint_rel_veh{0, 1, 0});

    CHECK(you.get_moves() == 25);
}

TEST_CASE("can autodrive", "[vehicle][autodrive]") {
    clear_all_state();
    set_time(calendar::turn_zero + 12_hours);

    auto& map = get_map();
    auto& you = get_avatar();
    auto& here = you.get_mapbuffer();
    you.setpos(test_origin);
    you.clear_map_memory();
    you.set_moves(1000);

    auto* veh_ptr =
        here.add_vehicle(vproto_id("car"), test_origin, 0_degrees, 100, 0, true, false, true);
    REQUIRE(veh_ptr != nullptr);
    here.board_vehicle(test_origin, you);
    veh_ptr->start_engines(true, true);
    veh_ptr->engine_on = true;
    REQUIRE(veh_ptr->player_in_control(you));

    const auto current_omt = project_to<coords::omt>(veh_ptr->abs_ms_location());
    const auto memory_origin = project_to<coords::ms>(current_omt);
    // Keep path planning deterministic; the regression check is the dirty live visibility cache.
    using namespace std::views;
    constexpr auto omt_size = coords::map_squares_per(coords::omt);
    for (const auto x : iota(0, omt_size * 2)) {
        for (const auto y : iota(0, omt_size)) {
            you.memorize_tile(memory_origin + tripoint_rel_ms(x, y, 0), "t_grass", 0, 0);
        }
    }
    you.omt_path = {current_omt + tripoint_rel_omt(1, 0, 0)};
    veh_ptr->is_autodriving = true;

    map.invalidate_visibility_caches();
    REQUIRE(map.visibility_caches_dirty());

    CHECK(veh_ptr->do_autodrive(you) == autodrive_result::ok);
}

TEST_CASE("horde_spawns_skip_owned_vehicle_tiles", "[horde][vehicle][monster]") {
    const auto cleanup = on_out_of_scope([] {
        clear_all_state();
        ACTIVE_OVERMAP_BUFFER.clear();
    });

    SECTION("unowned and untracked vehicle tiles remain valid horde spawn locations") {
        const auto fixture = make_horde_vehicle_spawn_fixture(horde_vehicle_spawn_options{});

        get_map().spawn_monsters(true);

        CHECK(vehicle_points_contain_monster(fixture.vehicle_points));
        CHECK(fixture.horde->empty());
    }

    SECTION("tracked but unowned vehicle tiles remain valid horde spawn locations") {
        const auto fixture = make_horde_vehicle_spawn_fixture(
            horde_vehicle_spawn_options{.tracked = true});

        get_map().spawn_monsters(true);

        CHECK(vehicle_points_contain_monster(fixture.vehicle_points));
        CHECK(fixture.horde->empty());
    }

    SECTION("owned but untracked vehicle tiles are excluded from horde spawn locations") {
        const auto fixture = make_horde_vehicle_spawn_fixture(
            horde_vehicle_spawn_options{.owned = true});

        get_map().spawn_monsters(true);

        CHECK_FALSE(vehicle_points_contain_monster(fixture.vehicle_points));
        CHECK_FALSE(fixture.horde->empty());
    }

    SECTION("owned and tracked vehicle tiles are excluded from horde spawn locations") {
        const auto fixture = make_horde_vehicle_spawn_fixture(
            horde_vehicle_spawn_options{.owned = true, .tracked = true});

        get_map().spawn_monsters(true);

        CHECK_FALSE(vehicle_points_contain_monster(fixture.vehicle_points));
        CHECK_FALSE(fixture.horde->empty());
    }
}

TEST_CASE("add_item_to_broken_vehicle_part") {
    clear_all_state();
    vehicle* veh_ptr =
        get_map().get_mapbuffer().add_vehicle(vproto_id("bicycle"), test_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    const tripoint_bub_ms pos = bub_test_origin() + tripoint_rel_ms::west();
    auto cargo_parts = veh_ptr->get_parts_at(pos, "CARGO", part_status_flag::any);
    REQUIRE(!cargo_parts.empty());
    vehicle_part* cargo_part = cargo_parts.front();
    REQUIRE(cargo_part != nullptr);
    // Must not be broken yet
    REQUIRE(!cargo_part->is_broken());
    // For some reason (0 - cargo_part->hp()) is just not enough to destroy a part
    REQUIRE(veh_ptr->mod_hp(*cargo_part, -(1 + cargo_part->hp()), DT_BASH));
    // Now it must be broken
    REQUIRE(cargo_part->is_broken());
    // Now part is really broken, adding an item should fail
    detached_ptr<item> itm2 = item::spawn("jeans");
    itm2 = veh_ptr->add_item(*cargo_part, std::move(itm2));
    CHECK(itm2);
}

TEST_CASE("damage_vehicle_oob") {
    clear_all_state();
    g->place_player(test_origin);
    const tripoint_rel_ms vehicle_offset(SEEX, 0, 0);
    auto vehicle_origin = test_origin + vehicle_offset;
    vehicle* veh_ptr = get_map().get_mapbuffer().add_vehicle(
        vproto_id("bicycle"), vehicle_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    // Put an item in the vehicle
    const auto cargo_pos = vehicle_origin + tripoint_rel_ms::west();
    auto cargo_parts = veh_ptr->get_parts_at(cargo_pos, "CARGO", part_status_flag::any);
    REQUIRE(!cargo_parts.empty());
    vehicle_part* cargo_part = cargo_parts.front();
    REQUIRE(cargo_part != nullptr);
    REQUIRE(!veh_ptr->add_item(*cargo_part, item::spawn("jeans")));

    // Shift the vehicle half off the map
    g->place_player(test_origin + tripoint_east * SEEX);

    // Check the vehicle is still there.
    optional_vpart_position part_pos = get_map().get_mapbuffer().veh_at(vehicle_origin);
    REQUIRE(part_pos);

    const auto parts = veh_ptr->parts_at_relative(cargo_part->mount, true);
    REQUIRE(!parts.empty());
    for (int part : parts) {
        // We aren't actually smashing each chosen part in turn here
        // it's picking a random one each time, hence why we smash them all
        veh_ptr->damage(part, 10000);
    }
}

static void check_wreckage(int zlevel) {
    const auto origin = tripoint_abs_ms(test_origin.xy(), zlevel);

    g->place_player(origin);
    ensure_simulated_islands_for(g->u.abs_pos());

    vehicle* veh_ptr =
        get_map().get_mapbuffer().add_vehicle(vproto_id("bicycle"), origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    vehicle* veh_ptr2 = get_map().get_mapbuffer().add_vehicle(
        vproto_id("car"), origin + tripoint_north_west, 0_degrees, 0, 0);
    REQUIRE(veh_ptr2 != nullptr);

    INFO(veh_ptr2->name);
    CHECK(veh_ptr2->name == "Wreckage");
}

TEST_CASE("overlapping_vehicles_make_wreck") {
    clear_all_state();
    check_wreckage(0);
    check_wreckage(OVERMAP_HEIGHT);
    check_wreckage(-OVERMAP_DEPTH);
}

static void test_coord_translate(
    units::angle dir, const tripoint_mnt_veh& pivot, const tripoint_mnt_veh& p,
    tripoint_rel_ms& q) {
    tileray tdir(dir);
    tdir.advance(p.x() - pivot.x());
    q.x() = tdir.dx() + tdir.ortho_dx(p.y() - pivot.y());
    q.y() = tdir.dy() + tdir.ortho_dy(p.y() - pivot.y());
}

TEST_CASE("check_vehicle_rotation_against_old", "[.]") {
    clear_all_state();
    vehicle* veh_ptr =
        get_map().get_mapbuffer().add_vehicle(vproto_id("bicycle"), test_origin, 0_degrees, 0, 0);
    const tripoint_mnt_veh pivot;

    for (int dir = 0; dir < 24; dir++) {
        for (int x = -5; x <= 5; x++) {
            for (int y = -5; y <= 5; y++) {
                tripoint_mnt_veh p = {x, y, 0};
                point_rel_ms oldRes;
                veh_ptr->coord_translate(15_degrees * dir, pivot, p, oldRes);

                tripoint_rel_ms newRes;
                test_coord_translate(15_degrees * dir, pivot, p, newRes);

                CHECK(oldRes.x() == newRes.x());
                CHECK(oldRes.y() == newRes.y());
            }
        }
    }
}

TEST_CASE("vehicle_rotation_reverse") {
    clear_all_state();
    vehicle* veh_ptr =
        get_map().get_mapbuffer().add_vehicle(vproto_id("bicycle"), test_origin, 0_degrees, 0, 0);
    const tripoint_mnt_veh pivot;

    for (int dir = 0; dir < 24; dir++) {
        for (int x = -5; x <= 5; x++) {
            for (int y = -5; y <= 5; y++) {
                tripoint_mnt_veh p = {x, y, 0};
                point_rel_ms result;
                veh_ptr->coord_translate(15_degrees * dir, pivot, p, result);

                tripoint_mnt_veh reversed;
                veh_ptr->coord_translate_reverse(
                    15_degrees * dir, pivot, tripoint_rel_ms(result, 0), reversed);

                CHECK(reversed.x() == p.x());
                CHECK(reversed.y() == p.y());
            }
        }
    }
}

TEST_CASE("broken_door_and_lock_can_be_removed", "[vehicle]") {
    clear_all_state();
    auto* veh_ptr = get_map().get_mapbuffer().add_vehicle(
        vproto_id("cross_split_test"), test_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    const auto door_mount = tripoint_mnt_veh(1, 0, 0);
    const auto door_idx = veh_ptr->part_with_feature(door_mount, "OPENABLE", true);
    const auto lock_idx = veh_ptr->part_with_feature(door_mount, "DOOR_LOCKING", true);
    REQUIRE(door_idx >= 0);
    REQUIRE(lock_idx >= 0);

    auto& door_part = veh_ptr->part(door_idx);
    auto& lock_part = veh_ptr->part(lock_idx);
    // DOORS CAN SPAWN OPEN GUYS
    if (door_part.open) { door_part.open = false; }
    REQUIRE_FALSE(door_part.open);

    REQUIRE(veh_ptr->mod_hp(door_part, -(door_part.hp() + 1), DT_BASH));
    REQUIRE(veh_ptr->mod_hp(lock_part, -(lock_part.hp() + 1), DT_BASH));
    REQUIRE(door_part.is_broken());
    REQUIRE(lock_part.is_broken());

    auto door_reason = std::string{};
    auto lock_reason = std::string{};
    CHECK(veh_ptr->can_unmount(door_idx, door_reason));
    CHECK(veh_ptr->can_unmount(lock_idx, lock_reason));
}

TEST_CASE("vehicle_door_movement_respects_door_lock_state", "[vehicle][door][regression]") {
    clear_all_state();

    auto& you = get_avatar();
    auto& here = you.get_mapbuffer();
    you.setpos(test_origin);
    you.moves = 1000;
    build_test_map(ter_id("t_floor"));

    auto* veh_ptr = here.add_vehicle(vproto_id("cross_split_test"), test_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    const auto door_idx = veh_ptr->part_with_feature(tripoint_mnt_veh(1, 0, 0), "OPENABLE", true);
    const auto lock_idx =
        veh_ptr->part_with_feature(tripoint_mnt_veh(1, 0, 0), "DOOR_LOCKING", true);
    REQUIRE(door_idx >= 0);
    REQUIRE(lock_idx >= 0);

    auto& door_part = veh_ptr->part(door_idx);
    auto& lock_part = veh_ptr->part(lock_idx);
    door_part.open = false;
    lock_part.enabled = false;
    veh_ptr->is_locked = true;

    const auto door_pos = veh_ptr->abs_part_location(door_idx);
    REQUIRE(door_pos == test_origin + tripoint_rel_ms::east());
    REQUIRE_FALSE(you.in_vehicle);

    REQUIRE(avatar_action::move(you, tripoint_rel_ms::east()));
    CHECK(door_part.open);
    CHECK(you.abs_pos() == test_origin);

    door_part.open = false;
    lock_part.enabled = true;
    CHECK_FALSE(avatar_action::move(you, tripoint_rel_ms::east()));
    CHECK_FALSE(door_part.open);
    CHECK(you.abs_pos() == test_origin);
}

TEST_CASE("motorcycle_controls_follow_awkward_absolute_movement", "[vehicle][coordinates]") {
    clear_all_state();

    auto& you = get_avatar();
    auto& here = you.get_mapbuffer();
    you.setpos(test_origin);
    you.controlling_vehicle = true;
    build_test_map(ter_id("t_floor"));

    auto* veh_ptr = here.add_vehicle(vproto_id("motorcycle"), test_origin, 0_degrees, 100, 0);
    REQUIRE(veh_ptr != nullptr);
    REQUIRE(here.board_vehicle(test_origin, you));
    REQUIRE(you.in_vehicle);
    veh_ptr->velocity = 100;
    REQUIRE_FALSE(veh_ptr->get_parts_at(test_origin, "CONTROLS", part_status_flag::any).empty());
    REQUIRE(veh_ptr->player_in_control(you));
    const auto controls_part =
        veh_ptr->part_with_feature(tripoint_mnt_veh::zero(), "CONTROLS", false);
    REQUIRE(controls_part >= 0);

    const auto awkward_moves = std::array<tripoint_rel_ms, 4>{
        tripoint_rel_ms::east(),
        tripoint_rel_ms::north(),
        tripoint_rel_ms::west(),
        tripoint_rel_ms::south(),
    };
    for (const auto& delta : awkward_moves) {
        veh_ptr = here.move_vehicle(*veh_ptr, delta, veh_ptr->face);
        REQUIRE(veh_ptr != nullptr);
        you.setpos(veh_ptr->abs_part_location(controls_part));

        CHECK(here.veh_at(you.abs_pos()).has_value());
        CHECK(&here.veh_at(you.abs_pos())->vehicle() == veh_ptr);
        CHECK_FALSE(
            veh_ptr->get_parts_at(you.abs_pos(), "CONTROLS", part_status_flag::any).empty());
        CHECK(veh_ptr->player_in_control(you));
    }
}

TEST_CASE("leaving_blimp_balloon_unboards_passenger", "[vehicle][aircraft][regression]") {
    clear_all_state();

    auto& you = get_avatar();
    auto& here = you.get_mapbuffer();
    you.setpos(test_origin);
    you.moves = 1000;
    build_test_map(ter_id("t_floor"));

    auto* blimp = here.add_vehicle(vproto_id("blimp"), test_origin, 270_degrees, 0, 0, true, false);
    REQUIRE(blimp != nullptr);

    const auto seat_idx = blimp->part_with_feature(tripoint_mnt_veh::zero(), "BOARDABLE", true);
    REQUIRE(seat_idx >= 0);
    const auto seat_pos = blimp->abs_part_location(seat_idx);
    REQUIRE(here.board_vehicle(seat_pos, you));
    REQUIRE(you.in_vehicle);
    REQUIRE(blimp->part(seat_idx).has_flag(vehicle_part::passenger_flag));

    const auto balloon_pos = test_origin + point_rel_ms(3, 1);
    const auto all_parts = blimp->get_all_parts();
    const auto door_pos = balloon_pos + point_rel_ms::west();
    const auto door_part_pos = abs_tile_handle::fetch(here, door_pos)->vehicle_part();
    REQUIRE(door_part_pos);
    const auto door_part = door_part_pos->part_with_feature("OPENABLE", true);
    REQUIRE(door_part);
    const auto door_index = door_part->part_index();
    if (!blimp->is_open(door_index)) { blimp->open(door_index); }
    REQUIRE(blimp->is_open(door_index));
    const auto balloon_part = abs_tile_handle::fetch(here, balloon_pos)->vehicle_part();
    REQUIRE(balloon_part);
    CHECK_FALSE(balloon_part->part_with_feature("BOARDABLE", true));
    REQUIRE(here.veh_at(balloon_pos));
    CHECK(seat_pos == test_origin);
    REQUIRE(avatar_action::move(you, point_rel_ms::south()));
    REQUIRE(avatar_action::move(you, point_rel_ms::east()));
    REQUIRE(avatar_action::move(you, point_rel_ms::east()));
    REQUIRE(avatar_action::move(you, point_rel_ms::east()));

    CHECK(you.abs_pos() == balloon_pos);
    CHECK_FALSE(you.in_vehicle);
    CHECK_FALSE(you.controlling_vehicle);
    CHECK_FALSE(blimp->part(seat_idx).has_flag(vehicle_part::passenger_flag));
}

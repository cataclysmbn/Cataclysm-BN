#include "action.h"
#include "avatar.h"
#include "avatar_action.h"
#include "ballistics.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "character_id.h"
#include "construction.h"
#include "damage.h"
#include "dispersion.h"
#include "enums.h"
#include "faction.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "line.h"
#include "map/legacy_pathfinding.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "map_helpers.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "options_helpers.h"
#include "pathfinding.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "projectile.h"
#include "rng.h"
#include "scent_map.h"
#include "sounds.h"
#include "state_helpers.h"
#include "trap.h"
#include "type_id.h"
#include "units.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"
#include "weather/weather.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <vector>

namespace {

struct runtime_fixture {
    restore_on_out_of_scope<time_point> time{calendar::turn};
    restore_on_out_of_scope<weather_manager> weather{get_weather()};
    restore_on_out_of_scope<cata_default_random_engine> random{rng_get_engine()};
    restore_on_out_of_scope<bool> mapgen{disable_mapgen};
    restore_on_out_of_scope<bool> distance{trigdist};
    restore_on_out_of_scope<safe_mode_type> safe_mode{g->safe_mode};
    restore_on_out_of_scope<bool> manual_combat{g->manual_combat_mode};
    override_option auto_notes{"AUTO_NOTES", "false"};
    override_option auto_features{"AUTO_FEATURES", "false"};
    override_option auto_pickup{"AUTO_PICKUP", "false"};
    override_option warning{"DANGEROUS_TERRAIN_WARNING_PROMPT", "IGNORE"};
    override_option fuel_rate{"SPAWN_RATE_fuel", "1.0"};
    override_option item_rate{"ITEM_SPAWNRATE", "1.0"};
    override_option path_limit{"PATHFINDING_MAX_DIST", "100"};
    character_id old_id{get_avatar().getID()};
    tripoint_abs_ms old_position{get_avatar().abs_pos()};
    point_abs_sm old_window{get_map().get_abs_sub()};
    tripoint_rel_ms old_view{get_avatar().view_offset};
    point old_driving_view{g->driving_view_offset};
    scoped_projectile_animation_suppression animation;
    on_out_of_scope cleanup{[this] {
        auto& here = get_map();
        auto& you = get_avatar();
        if (you.in_vehicle) { here.unboard_vehicle(you.bub_pos()); }
        for (auto& who : g->all_npcs()) {
            if (who.in_vehicle) { here.unboard_vehicle(who.bub_pos()); }
            who.controlling_vehicle = false;
        }
        you.controlling_vehicle = false;
        you.clear_destination();
        clear_npcs();
        if (here.get_abs_sub() != old_window) {
            clear_vehicles();
            here.load(old_window, true, false);
        }
        clear_all_state();
        for (auto z = 1; z <= OVERMAP_HEIGHT; ++z) {
            clear_fields(z);
            clear_items(z);
        }
        sounds::reset_sounds();
        g->scent.reset();
        Pathfinding::clear_d_maps();
        Pathfinding::mark_dirty_z_cache();
        you.setID(old_id, true);
        you.Character::setpos(old_position);
        you.view_offset = old_view;
        g->set_driving_view_offset(old_driving_view);
        g->clear_turn_los_blocker_cache();
    }};

    runtime_fixture() {
        clear_npcs();
        clear_all_state();
        disable_mapgen = true;
        trigdist = false;
        g->safe_mode = SAFE_MODE_OFF;
        g->manual_combat_mode = false;
        get_avatar().clear_destination();
        if (!get_avatar().getID().is_valid()) { get_avatar().setID(g->assign_npc_id()); }
        get_avatar().setpos(tripoint_bub_ms(65, 65, 0));
        set_time(calendar::turn_zero + 12_hours);
        get_weather().weather_id = weather_type_id("clear");
        get_weather().weather_override = weather_type_id("clear");
        get_weather().temperature = 20_c;
        get_weather().clear_temp_cache();
        get_weather().windspeed_override = 0;
        get_weather().nextweather = calendar::turn + 1_days;
        Pathfinding::clear_d_maps();
        Pathfinding::mark_dirty_z_cache();
        sounds::reset_sounds();
        g->scent.reset();
        g->reset_light_level();
        for (auto z = 0; z <= 4; ++z) {
            clear_fields(z);
            clear_items(z);
            for (auto x = 50; x <= 85; ++x) {
                for (auto y = 50; y <= 75; ++y) {
                    get_map().set(tripoint_bub_ms(x, y, z),
                                  ter_id(z == 0 ? "t_pavement" : "t_open_air"), furn_id("f_null"));
                }
            }
        }
    }
};

auto spawn_vehicle(const char* prototype, tripoint_bub_ms pos, units::angle yaw = 0_degrees)
    -> vehicle& { // *NOPAD*
    auto* result = get_map().add_vehicle(vproto_id(prototype), pos, yaw, 100, 0);
    REQUIRE(result != nullptr);
    return *result;
}

auto install(vehicle& veh, tripoint_mnt_veh mount, const char* id = "frame_vertical") -> int {
    const auto index = veh.install_part(mount, vpart_id(id));
    CAPTURE(mount, id);
    REQUIRE(index >= 0);
    // Vehicle activity completion publishes the installed parts to the map cache.
    get_map().add_vehicle_to_cache(&veh);
    return index;
}

auto boardable(const vehicle& veh, tripoint_mnt_veh mount) -> int {
    const auto index = veh.part_with_feature(mount, "BOARDABLE", true);
    REQUIRE(index >= 0);
    return index;
}

auto board(vehicle& veh, Character& who, int index) -> void {
    get_map().board_vehicle(veh.bub_part_location(index), &who);
    REQUIRE(veh.get_passenger(index) == &who);
}

auto check_rider(const vehicle& veh, const Character& who, int index) -> void {
    CHECK(who.abs_pos() == veh.abs_part_location(index));
    CHECK(who.in_vehicle);
    CHECK(veh.get_passenger(index) == &who);
}

auto route(tripoint_abs_ms from, tripoint_abs_ms to, bool legacy) -> std::vector<tripoint_abs_ms> {
    if (legacy) {
        auto settings = pathfinding_settings{};
        settings.max_dist = 100;
        settings.max_length = 1000;
        settings.allow_climb_stairs = true;
        settings.avoid_traps = true;
        return get_map().route(from, to, settings, {});
    }
    const auto settings = PathfindingSettings{.trap_cost = INFINITY, .can_climb_stairs = true};
    const auto limits = RouteSettings{.h_coeff = 0, .max_f_coeff = 10.0f, .max_dist = 100};
    return Pathfinding::route(from, to, settings, limits);
}

} // namespace

TEST_CASE("multiz_core_avatar_passages", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    auto fixture = runtime_fixture{};
    auto& veh = spawn_vehicle("multiz_two_deck_test", tripoint_bub_ms(60, 60, 0));
    auto& you = get_avatar();
    const auto lower = boardable(veh, tripoint_mnt_veh(2, 0, 0));
    const auto upper = boardable(veh, tripoint_mnt_veh(2, 0, 1));
    get_map().ter_set(veh.bub_part_location(lower), ter_id("t_ramp_up_low"));
    get_map().ter_set(veh.bub_part_location(upper), ter_id("t_ramp_down_low"));
    board(veh, you, lower);
    for (const auto destination : {upper, lower}) {
        const auto direction = destination == upper ? 1 : -1;
        const auto path = get_map().route(
            you.bub_pos(), veh.bub_part_location(destination),
            you.get_legacy_pathfinding_settings(), you.get_legacy_path_avoid());
        REQUIRE(path.size() == 1);
        you.set_destination(path);
        REQUIRE(you.get_next_auto_move_direction()
                == (direction > 0 ? ACTION_MOVE_UP : ACTION_MOVE_DOWN));
        you.moves = 1000;
        g->vertical_move(direction, false);
        check_rider(veh, you, destination);
        CHECK(you.moves == 900);
    }
}

TEST_CASE(
    "multiz_core_npc_and_monster_passages", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    const auto actor = GENERATE(0, 1, 2); // NPC, new monster planner, legacy monster planner.
    const auto routing = override_option("USE_LEGACY_PATHFINDING", actor == 2 ? "true" : "false");
    auto fixture = runtime_fixture{};
    auto& veh = spawn_vehicle("multiz_two_deck_test", tripoint_bub_ms(60, 60, 0));
    const auto lower = boardable(veh, tripoint_mnt_veh(2, 0, 0));
    const auto upper = boardable(veh, tripoint_mnt_veh(2, 0, 1));
    if (actor == 0) {
        auto& who = spawn_npc(tripoint_bub_ms(55, 55, 0), "test_talker");
        clear_character(who);
        board(veh, who, lower);
        for (const auto destination : {upper, lower}) {
            who.moves = 1000;
            REQUIRE(who.update_path(veh.bub_part_location(destination)));
            who.move_to_next();
            check_rider(veh, who, destination);
        }
    } else {
        auto type = mtype_id("mon_test_lua_read_bindings").obj();
        type.set_flag(MF_IMMOBILE, false);
        type.set_flag(MF_AVOID_FALL);
        type.legacy_path_settings.max_dist = 20;
        type.legacy_path_settings.max_length = 100;
        type.legacy_path_settings.allow_climb_stairs = true;
        type.path_settings.can_climb_stairs = true;
        type.route_settings.max_dist = 20;
        type.route_settings.alpha = 1;
        auto& who = spawn_test_monster("mon_test_lua_read_bindings", veh.bub_part_location(lower));
        const auto restore_type = restore_on_out_of_scope<const mtype*>(who.type);
        who.type = &type;
        for (const auto destination : {upper, lower}) {
            who.moves = 1000;
            who.set_dest(veh.bub_part_location(destination));
            who.execute_action(
                {.kind = monster_action_kind::idle, .move_cost = 100, .needs_repath = true});
            CHECK(who.abs_pos() == veh.abs_part_location(destination));
        }
        CHECK(veh.boarded_parts().empty());
    }
}

TEST_CASE("multiz_core_connector_routes", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    const auto legacy = GENERATE(false, true);
    auto fixture = runtime_fixture{};
    auto& veh = spawn_vehicle("multiz_two_deck_test", tripoint_bub_ms(60, 60, 0));
    const auto from = veh.abs_part_location(boardable(veh, tripoint_mnt_veh::zero()));
    const auto to = veh.abs_part_location(boardable(veh, tripoint_mnt_veh(0, 0, 1)));
    const auto stair = boardable(veh, tripoint_mnt_veh(2, 0, 0));
    const auto path = route(from, to, legacy);
    REQUIRE_FALSE(path.empty());
    CHECK(path.back() == to);
    CHECK(std::ranges::contains(path, veh.abs_part_location(stair)));
    veh.damage(stair, 10000, DT_TRUE, true, false);
    CHECK(route(from, to, legacy).empty());
    veh.remove_part(stair);
    install(veh, tripoint_mnt_veh(2, 0, 0), "vehicle_stairs");
    veh.part_removal_cleanup();
    CHECK_FALSE(route(from, to, legacy).empty());
}

TEST_CASE(
    "multiz_core_physical_deck_boundaries", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    auto fixture = runtime_fixture{};
    auto& here = get_map();
    auto& veh = spawn_vehicle("none", tripoint_bub_ms(60, 60, 0));
    for (const auto z : {0, 1}) {
        install(veh, tripoint_mnt_veh(0, 0, z));
        install(veh, tripoint_mnt_veh(0, 0, z), "aisle_horizontal");
    }
    const auto floor = boardable(veh, tripoint_mnt_veh(0, 0, 1));
    const auto lower = veh.bub_part_location(boardable(veh, tripoint_mnt_veh::zero()));
    const auto upper = veh.bub_part_location(floor);
    board(veh, get_avatar(), boardable(veh, tripoint_mnt_veh::zero()));
    here.build_map_cache(lower.z());
    here.update_visibility_cache(lower.z());
    CHECK_FALSE(get_avatar().sees(upper));
    auto shot = projectile{};
    shot.speed = 1000;
    shot.range = 10;
    shot.impact = damage_instance(DT_BULLET, 20);
    shot.add_effect(ammo_effect_str_id("NO_OVERSHOOT"));
    const auto attack =
        projectile_attack(shot, lower, upper, dispersion_sources{}, nullptr, nullptr, &veh);
    CHECK(attack.end_point == lower);
    CHECK(here.has_floor(upper));
    CHECK_FALSE(here.has_floor(upper, false, true));
    const auto& empty_tile = construction_str_id("constr_crafting_spot").obj();
    CHECK(can_construct(empty_tile, tripoint_bub_ms(75, 65, 0)));
    CHECK_FALSE(can_construct(empty_tile, upper));
    here.add_item_or_charges(upper, item::spawn(itype_id("rock")));
    here.drop_items(upper);
    CHECK(here.has_items(upper));
    veh.damage(floor, 10000, DT_TRUE, true, false);
    CHECK_FALSE(here.has_floor(upper));
    CHECK(here.tr_at(upper).loadid == tr_ledge);
    here.drop_items(upper);
    CHECK_FALSE(here.has_items(upper));
}

TEST_CASE("multiz_core_whole_vehicle_envelope", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    auto fixture = runtime_fixture{};
    auto& here = get_map();
    const auto origin = tripoint_bub_ms(60, 60, 0);
    here.ter_set(origin + tripoint_rel_ms::above(), ter_id("t_wall"));
    CHECK(here.add_vehicle(vproto_id("multiz_two_deck_test"), origin, 0_degrees, 0, 0, false)
          == nullptr);
    CHECK(here.get_vehicles().empty());
    CHECK(here.ter(origin + tripoint_rel_ms::above()) == ter_id("t_wall"));
    here.ter_set(origin + tripoint_rel_ms::above(), ter_id("t_open_air"));
    auto& veh = spawn_vehicle("multiz_two_deck_test", origin);
    CHECK(veh.bub_part_location(boardable(veh, tripoint_mnt_veh(0, 0, 1)))
          == origin + tripoint_rel_ms::above());
}

TEST_CASE("multiz_core_lowest_deck_lands", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    auto fixture = runtime_fixture{};
    auto& here = get_map();
    here.ter_set(tripoint_bub_ms(60, 60, 0), ter_id("test_t_explosion_floor"));
    auto& veh = spawn_vehicle("none", tripoint_bub_ms(60, 60, 2));
    auto lower = -1;
    for (const auto z : {0, -1}) {
        const auto frame = install(veh, tripoint_mnt_veh(0, 0, z), "test_explosion_frame");
        install(veh, tripoint_mnt_veh(0, 0, z), "test_explosion_interior");
        if (z == -1) { lower = frame; }
    }
    veh.gain_moves();
    REQUIRE(veh.is_falling);
    for (auto step = 0; step < 6; ++step) {
        calendar::turn += 1_turns;
        here.vehmove();
        REQUIRE(veh.abs_part_location(lower).z() >= 0);
        if (!veh.is_falling && veh.vertical_velocity == 0) { break; }
    }
    CHECK(veh.abs_part_location(lower).z() == 0);
    CHECK(veh.abs_ms_location().z() == 1);
    CHECK_FALSE(veh.is_falling);
    CHECK(veh.vertical_velocity == 0);
}

TEST_CASE("multiz_core_followers_cross_decks", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    auto fixture = runtime_fixture{};
    auto& veh = spawn_vehicle("multiz_two_deck_test", tripoint_bub_ms(60, 60, 1));
    const auto destination = boardable(veh, tripoint_mnt_veh(0, 1, 1));
    auto& you = get_avatar();
    board(veh, you, boardable(veh, tripoint_mnt_veh(2, 0, 0)));
    auto& who = spawn_npc(tripoint_bub_ms(55, 55, 0), "test_talker");
    clear_character(who);
    who.set_attitude(NPCATT_FOLLOW);
    who.set_fac(faction_id("your_followers"));
    board(veh, who, boardable(veh, tripoint_mnt_veh(0, 1, 0)));
    REQUIRE(veh.assign_seat(veh.part(destination), who));
    you.moves = 1000;
    g->vertical_move(1, false);
    REQUIRE(you.abs_pos() == veh.abs_part_location(boardable(veh, tripoint_mnt_veh(2, 0, 1))));
    REQUIRE(avatar_action::move(you, get_map(), tripoint_rel_ms::west()));
    REQUIRE(avatar_action::move(you, get_map(), tripoint_rel_ms::west()));
    for (auto turn = 0; turn < 24 && who.abs_pos() != veh.abs_part_location(destination); ++turn) {
        who.moves = 100;
        who.execute_action("npc_follow_embarked");
    }
    check_rider(veh, who, destination);
}

TEST_CASE(
    "multiz_core_driving_turning_and_ramps", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    auto fixture = runtime_fixture{};
    auto& here = get_map();
    constexpr auto ramp_x = 64;
    for (auto y = 50; y <= 75; ++y) {
        for (auto x = 50; x < ramp_x; ++x) {
            here.ter_set(tripoint_bub_ms(x, y, 0), ter_id("t_rock"));
            here.ter_set(tripoint_bub_ms(x, y, 1), ter_id("t_pavement"));
        }
        here.ter_set(tripoint_bub_ms(ramp_x, y, 0), ter_id("t_ramp_up_high"));
        here.ter_set(tripoint_bub_ms(ramp_x + 1, y, 0), ter_id("t_ramp_up_low"));
        here.ter_set(tripoint_bub_ms(ramp_x, y, 1), ter_id("t_ramp_down_high"));
        here.ter_set(tripoint_bub_ms(ramp_x + 1, y, 1), ter_id("t_ramp_down_low"));
    }
    auto& veh = spawn_vehicle("bus_double_decker", tripoint_bub_ms(70, 65, 0), 90_degrees);
    const auto driver = boardable(veh, tripoint_mnt_veh::zero());
    const auto upstairs = boardable(veh, tripoint_mnt_veh(0, 0, 1));
    const auto rear = boardable(veh, tripoint_mnt_veh(-9, 0, 1));
    auto& passenger = spawn_npc(tripoint_bub_ms(65, 55, 0), "test_talker");
    board(veh, get_avatar(), driver);
    board(veh, passenger, upstairs);
    veh.start_engines(true);
    process_activity(get_avatar());
    REQUIRE(veh.engine_on);
    veh.cruise_on = true;
    veh.cruise_velocity = 179;
    veh.thrust(1);
    veh.turn(90_degrees);
    auto facing = tileray(veh.turn_dir);
    facing.advance(1);
    REQUIRE(here.move_vehicle(veh, tripoint_rel_ms(facing.dx(), facing.dy(), 0), facing) == &veh);
    REQUIRE(veh.face.dir() == 180_degrees);
    for (auto turn = 0; turn < 20 && veh.bub_part_location(driver).x() > ramp_x - 1; ++turn) {
        calendar::turn += 1_turns;
        here.vehmove();
    }
    CHECK(veh.bub_part_location(driver).z() == 1);
    check_rider(veh, get_avatar(), driver);
    check_rider(veh, passenger, upstairs);
    REQUIRE(veh.abs_part_location(upstairs).z() != veh.abs_part_location(rear).z());
    veh.stop();
    here.unboard_vehicle(get_avatar().bub_pos());
    here.unboard_vehicle(passenger.bub_pos());
    board(veh, passenger, driver);
    board(veh, get_avatar(), upstairs);
    const auto path = route(get_avatar().abs_pos(), veh.abs_part_location(rear), false);
    REQUIRE_FALSE(path.empty());
    CHECK(std::ranges::all_of(path, [&](const auto& p) {
        const auto vp = here.veh_at(abs_to_map_local(here, p));
        return vp && &vp->vehicle() == &veh && vp->mount().z() == 1;
    }));
    get_avatar().moves = 10000;
    for (const auto& next : path | std::views::drop(1)) {
        REQUIRE(avatar_action::move(
            get_avatar(), here, tripoint_rel_ms(next.xy() - get_avatar().abs_pos().xy(), 0)));
        REQUIRE(get_avatar().abs_pos() == next);
    }
    check_rider(veh, get_avatar(), rear);
}

TEST_CASE("multiz_core_flight_lifecycle", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    auto fixture = runtime_fixture{};
    auto& veh = spawn_vehicle("flight_test_lower_deck", tripoint_bub_ms(60, 60, 1));
    const auto pilot = boardable(veh, tripoint_mnt_veh::zero());
    board(veh, get_avatar(), pilot);
    for (const auto& vp : veh.get_avail_parts("ENGINE")) {
        REQUIRE(vp.part().fuel_set(itype_id("jp8")));
    }
    veh.engine_on = true;
    const auto lower = boardable(veh, tripoint_mnt_veh(0, 0, -1));
    auto& passenger = spawn_npc(tripoint_bub_ms(65, 66, 0), "test_talker");
    board(veh, passenger, lower);
    get_avatar().controlling_vehicle = true;
    REQUIRE(veh.is_aircraft());
    const auto origin = veh.abs_ms_location();
    for (const auto direction : {1, -1}) {
        const auto clear =
            direction > 0
                ? veh.check_heli_ascend(get_avatar())
                : veh.check_heli_descend(get_avatar());
        REQUIRE(clear);
        get_avatar().moves = 100;
        veh.pldrive(get_avatar(), tripoint_rel_veh(0, 0, direction));
        veh.gain_moves();
        REQUIRE(veh.act_on_map() == &veh);
        check_rider(veh, get_avatar(), pilot);
        check_rider(veh, passenger, lower);
        CHECK(veh.is_flying_in_air() == (direction > 0));
        CHECK(veh.abs_ms_location() == origin + tripoint_rel_ms(0, 0, direction > 0 ? 1 : 0));
    }
    CHECK(veh.bub_part_location(lower).z() == 0);
}

TEST_CASE("multiz_core_boating_and_sinking", "[vehicle][vehicle_multiz][vehicle_multiz_core]") {
    auto fixture = runtime_fixture{};
    auto& here = get_map();
    for (auto z = -2; z <= 0; ++z) {
        for (auto x = 54; x <= 80; ++x) {
            for (auto y = 54; y <= 70; ++y) {
                here.ter_set(tripoint_bub_ms(x, y, z),
                             ter_id(z == 0 ? "t_water_dp" : "t_water_cube"));
            }
        }
    }
    auto& veh = spawn_vehicle("boating_test_lower_only", tripoint_bub_ms(60, 60, 1));
    const auto capacity = veh.max_buoyancy();
    for (auto x = -1; x <= 1; ++x) {
        for (auto y = -1; y <= 1; ++y) { install(veh, tripoint_mnt_veh(x, y, 0)); }
    }
    const auto upstairs = install(veh, tripoint_mnt_veh(0, 1, 0), "seat");
    const auto first_cargo = install(veh, tripoint_mnt_veh(0, -1, 0), "cargo_space");
    const auto second_cargo = install(veh, tripoint_mnt_veh(1, -1, 0), "cargo_space");
    const auto driver = boardable(veh, tripoint_mnt_veh(0, 0, -1));
    auto& passenger = spawn_npc(tripoint_bub_ms(65, 71, 0), "test_talker");
    board(veh, get_avatar(), driver);
    board(veh, passenger, upstairs);
    for (const auto& vp : veh.get_all_parts()) {
        if (vp.part().is_engine()) {
            veh.toggle_specific_part(vp.part_index(), true);
            REQUIRE(vp.part().fuel_set(itype_id("battery")));
        }
        if (vp.part().is_battery()) { REQUIRE(vp.part().ammo_set(itype_id("battery")) > 0); }
    }
    veh.gain_moves();
    REQUIRE(veh.is_in_water(true));
    REQUIRE(veh.can_float());
    veh.start_engines(true);
    process_activity(get_avatar());
    REQUIRE(veh.engine_on);
    get_avatar().controlling_vehicle = true;
    veh.cruise_on = true;
    veh.cruise_velocity = 179;
    veh.thrust(1);
    const auto origin = veh.abs_ms_location();
    calendar::turn += 1_turns;
    here.vehmove();
    CHECK(veh.abs_ms_location() == origin + tripoint_rel_ms::east());
    check_rider(veh, get_avatar(), driver);
    check_rider(veh, passenger, upstairs);
    REQUIRE(veh.velocity > 0);
    const auto unit = item::spawn("lead", calendar::turn, 1);
    const auto charges = static_cast<int>(
        units::to_milligram(4000_kilogram) / units::to_milligram(unit->weight()));
    for (const auto cargo : {first_cargo, second_cargo}) {
        REQUIRE_FALSE(veh.add_item(cargo, item::spawn("lead", calendar::turn, charges)));
    }
    CHECK(veh.max_buoyancy() == Approx(capacity));
    REQUIRE_FALSE(veh.can_float());
    calendar::turn += 1_turns;
    here.vehmove();
    // The vehicle reference is invalid after the ordinary whole-boat sink.
    CHECK(here.get_vehicles().empty());
    CHECK_FALSE(get_avatar().in_vehicle);
    CHECK_FALSE(passenger.in_vehicle);
}

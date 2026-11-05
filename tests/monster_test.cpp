#include "../src/map/map.h"
#include "../src/vehicle/vehicle_part.h"
#include "../src/vehicle/vpart_position.h"
#include "action_time_scale.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "line.h"
#include "map/field_type.h"
#include "map/utils/map_functions.h"
#include "map_helpers.h"
#include "monattack.h"
#include "monster.h"
#include "monster_action.h"
#include "monster_hallucination.h"
#include "options.h"
#include "options_helpers.h"
#include "player.h"
#include "state_helpers.h"
#include "test_statistics.h"
#include "type_id.h"
#include "vehicle/vehicle.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <ranges>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using move_statistics = statistics<int>;

namespace {

auto movement_fixture_map_origin_abs() -> tripoint_abs_ms {
    return tripoint_abs_ms(project_to<coords::ms>(get_map().get_abs_sub()), 0);
}

auto count_items_at(const tripoint_bub_ms& pos, const itype_id& type) -> int {
    return std::ranges::count_if(get_map().i_at(pos), [&type](const auto* it) {
        return it->typeId() == type;
    });
}

} // namespace

TEST_CASE("extended monster death drops append to inherited drops", "[monster][death_drops]") {
    clear_all_state();
    const auto global_spawn_rate = override_option("ITEM_SPAWNRATE", "1.0");
    const auto rock_spawn_rate = override_option("SPAWN_RATE_rocks", "1.0");
    const auto wood_spawn_rate = override_option("SPAWN_RATE_scrap_wood", "1.0");
    static_cast<void>(global_spawn_rate);
    static_cast<void>(rock_spawn_rate);
    static_cast<void>(wood_spawn_rate);
    move_player_out_of_the_way();

    auto& here = get_map();
    build_test_map(ter_id("t_floor"));

    const auto monster_pos_abs = test_origin;
    here.i_clear(abs_to_bub(monster_pos_abs));

    auto& test_monster =
        spawn_test_monster("mon_test_death_drops_append", abs_to_bub(monster_pos_abs));
    test_monster.drop_items_on_death();

    CHECK(count_items_at(abs_to_bub(monster_pos_abs), itype_id("rock")) == 1);
    CHECK(count_items_at(abs_to_bub(monster_pos_abs), itype_id("stick")) == 1);
}

TEST_CASE("empty top-level monster death drops replace inherited drops", "[monster][death_drops]") {
    clear_all_state();
    move_player_out_of_the_way();

    auto& here = get_map();
    build_test_map(ter_id("t_floor"));

    const auto monster_pos_abs = test_origin;
    here.i_clear(abs_to_bub(monster_pos_abs));

    auto& test_monster =
        spawn_test_monster("mon_test_death_drops_clear", abs_to_bub(monster_pos_abs));
    test_monster.drop_items_on_death();

    CHECK(here.i_at(abs_to_bub(monster_pos_abs)).empty());
}

TEST_CASE("hallucination_monsters_do_not_open_real_doors", "[monster][hallucination]") {
    clear_all_state();
    move_player_out_of_the_way();
    auto& here = get_map();
    build_test_map(ter_id("t_floor"));

    const auto monster_pos_abs = test_origin;
    const auto door_pos_abs = monster_pos_abs + tripoint_rel_ms::east();
    REQUIRE(here.ter_set(abs_to_bub(door_pos_abs), ter_id("t_door_c")));

    auto& hallucination = spawn_test_monster("mon_zombie_scientist", abs_to_bub(monster_pos_abs));
    hallucination.hallucination = true;
    REQUIRE(hallucination.is_hallucination());
    hallucination.execute_action(
        {.kind = monster_action_kind::open_door, .dest = abs_to_bub(door_pos_abs)});

    CHECK(here.ter(abs_to_bub(door_pos_abs)) == ter_id("t_door_c"));
}

TEST_CASE("hallucination_electric_field_does_not_ignite_items", "[monster][hallucination]") {
    clear_all_state();
    move_player_out_of_the_way();
    auto& here = get_map();
    build_test_map(ter_id("t_pavement"));

    const auto monster_pos_abs = test_origin;
    const auto fuel_pos_abs = monster_pos_abs + tripoint_rel_ms::east();
    here.add_item_or_charges(abs_to_bub(fuel_pos_abs), item::spawn("gasoline"));

    auto& hallucination = spawn_test_monster("mon_zombie_nullfield", abs_to_bub(monster_pos_abs));
    hallucination.hallucination = true;
    REQUIRE(hallucination.is_hallucination());
    hallucination.process_turn();

    CHECK(here.get_field(abs_to_bub(fuel_pos_abs), fd_fire) == nullptr);
}

TEST_CASE(
    "only stalled hallucinations qualify for lifecycle expiry fallback",
    "[monster][hallucination]") {
    auto test_monster = monster(mtype_id("debug_mon"));
    test_monster.set_speed_base(0);
    test_monster.hallucination = true;
    test_monster.set_moves(0);

    REQUIRE(test_monster.get_speed() == 0);
    REQUIRE(test_monster.get_moves() == 0);

    SECTION("a stalled hallucination needs lifecycle expiry") {
        CHECK(monster_hallucination::needs_lifecycle_expiry(test_monster));
    }

    SECTION("real zero-speed monsters do not use hallucination expiry") {
        test_monster.hallucination = false;

        CHECK_FALSE(monster_hallucination::needs_lifecycle_expiry(test_monster));
    }

    SECTION("positive-speed hallucinations keep their action-path expiry") {
        test_monster.set_speed_base(100);

        REQUIRE(test_monster.get_speed() > 0);
        CHECK_FALSE(monster_hallucination::needs_lifecycle_expiry(test_monster));
    }

    SECTION("zero-speed hallucinations with banked moves keep their action-path expiry") {
        test_monster.set_moves(1);

        CHECK_FALSE(monster_hallucination::needs_lifecycle_expiry(test_monster));
    }

    SECTION("dead hallucinations do not need lifecycle expiry") {
        test_monster.set_hp(0);

        REQUIRE(test_monster.is_dead());
        CHECK_FALSE(monster_hallucination::needs_lifecycle_expiry(test_monster));
    }
}

TEST_CASE("MONSTER_SPEED scales monster move credit", "[monster][speed]") {
    clear_all_state();
    move_player_out_of_the_way();

    const auto global_speed_percent = 50;
    const auto monster_speed_percent = 66;
    const auto global_speed_option =
        override_option("TIME_ACTION_SCALE", std::to_string(global_speed_percent));
    const auto monster_speed_option =
        override_option("MONSTER_SPEED", std::to_string(monster_speed_percent));

    auto& test_monster = spawn_test_monster("mon_zombie", bub_test_origin());
    const auto base_speed = test_monster.get_speed_base();
    const auto monster_tick_factor = action_time_scale::monster_tick_action_factor();
    REQUIRE(base_speed == test_monster.type->speed);
    CHECK(
        action_time_scale::monster_action_factor() == global_speed_percent * monster_speed_percent);
    CHECK(test_monster.get_moves()
          == base_speed * monster_tick_factor / action_time_scale::factor_denominator);

    const auto turn_count = 10;
    for (const auto turn : std::views::iota(0, turn_count)) {
        static_cast<void>(turn);
        test_monster.process_turn();
    }

    CHECK(test_monster.get_moves()
          == base_speed * monster_tick_factor * (turn_count + 1)
                 / action_time_scale::factor_denominator);
    CHECK(test_monster.get_speed_base() == base_speed);
    CHECK(test_monster.type->speed == base_speed);
}

TEST_CASE("monster ammo slots accept configured alternate ammo items", "[monster][ammo]") {
    clear_all_state();
    move_player_out_of_the_way();

    auto& test_monster = spawn_test_monster("mon_test_ammo_variants", bub_test_origin());

    CHECK(test_monster.ammo_slot_items(itype_id("9mmfmj"))
          == std::vector<itype_id>({itype_id("9mmfmj"), itype_id("9mm")}));
    CHECK(test_monster.ammo_capacity_for_slot(itype_id("9mmfmj")) == 10);
    CHECK(test_monster.ammo_count_for_slot(itype_id("9mmfmj")) == 10);

    test_monster.ammo[itype_id("9mmfmj")] = 1;
    test_monster.ammo[itype_id("9mm")] = 4;

    CHECK(test_monster.ammo_count_for_slot(itype_id("9mmfmj")) == 5);
    CHECK(test_monster.loaded_ammo_for_slot(itype_id("9mmfmj")) == itype_id("9mm"));
}

TEST_CASE(
    "monster ammo slots derive compatible ammo from the gun when no override is set",
    "[monster][ammo]") {
    clear_all_state();
    move_player_out_of_the_way();

    auto& test_monster = spawn_test_monster("mon_test_gun_ammo_compat", bub_test_origin());
    const auto slot_items = test_monster.ammo_slot_items(itype_id("9mmfmj"));

    CHECK(std::ranges::contains(slot_items, itype_id("9mmfmj")));
    CHECK(std::ranges::contains(slot_items, itype_id("9mm")));
    CHECK(slot_items.size() > 1);
}

static auto moves_to_destination(
    const std::string& monster_type, const tripoint_abs_ms& start_abs,
    const tripoint_abs_ms& end_abs) -> int {
    clear_creatures();
    REQUIRE(g->num_creatures() == 1); // the player
    monster& test_monster = spawn_test_monster(monster_type, abs_to_bub(start_abs));
    // Get it riled up and give it a goal.
    test_monster.anger = 100;
    test_monster.set_dest(abs_to_bub(end_abs));
    test_monster.set_moves(0);
    const int monster_speed = test_monster.get_speed();
    int moves_spent = 0;
    for (int turn = 0; turn < 1000; ++turn) {
        test_monster.mod_moves(monster_speed);
        while (test_monster.moves >= 0) {
            test_monster.anger = 100;
            const int moves_before = test_monster.moves;
            test_monster.move();
            moves_spent += moves_before - test_monster.moves;
            if (test_monster.bub_pos() == test_monster.move_target()) {
                g->remove_zombie(test_monster);
                return moves_spent;
            }
        }
    }
    g->remove_zombie(test_monster);
    // Return an unreasonably high number.
    return 100000;
}

struct track {
    char participant;
    int moves;
    int distance;
    tripoint_abs_ms location;
};

static auto operator<<(std::ostream& os, track const& value) -> std::ostream& { // *NOPAD*
    os << value.participant << " l:" << value.location << " d:" << value.distance
       << " m:" << value.moves;
    return os;
}

static auto operator<<(std::ostream& os, const std::vector<track>& vec)
    -> std::ostream& { // *NOPAD*
    for (auto& track_instance : vec) { os << track_instance << " "; }
    return os;
}

/**
 * Simulate a player running from the monster, checking if it can catch up.
 **/
static auto can_catch_player(
    const std::string& monster_type, const tripoint_rel_ms& direction_of_flight) -> int {
    clear_map();
    REQUIRE(g->num_creatures() == 1); // the player
    player& test_player = get_avatar();
    // Strip off any potentially encumbering clothing.
    std::vector<detached_ptr<item>> temp;
    while (test_player.takeoff(test_player.i_at(-2), &temp));

    const auto center_abs = movement_fixture_map_origin_abs() + tripoint_rel_ms(65, 65, 0);
    test_player.setpos(center_abs);
    test_player.set_moves(0);
    // Give the player a head start.
    const auto monster_start_abs = center_abs + (direction_of_flight * -10);
    monster& test_monster = spawn_test_monster(monster_type, abs_to_bub(monster_start_abs));
    // Get it riled up and give it a goal.
    test_monster.anger = 100;
    test_monster.set_dest(test_player.bub_pos());
    test_monster.set_moves(0);
    const int monster_speed = test_monster.get_speed();
    const int target_speed = 100;

    std::vector<track> tracker;
    for (int turn = 0; turn < 1000; ++turn) {
        test_player.mod_moves(target_speed);
        while (test_player.moves >= 0) {
            test_player.setpos(test_player.abs_pos() + direction_of_flight);
            if (test_player.bub_pos().x() < SEEX * int(MAPSIZE / 2)
                || test_player.bub_pos().y() < SEEY * int(MAPSIZE / 2)
                || test_player.bub_pos().x() >= SEEX * (1 + int(MAPSIZE / 2))
                || test_player.bub_pos().y() >= SEEY * (1 + int(MAPSIZE / 2))) {
                const auto offset = center_abs - test_player.abs_pos();
                test_player.setpos(center_abs);
                test_monster.setpos(test_monster.abs_pos() + offset);
                // Verify that only the player and one monster are present.
                REQUIRE(g->num_creatures() == 2);
            }
            const int move_cost = get_map().combined_movecost(
                test_player.bub_pos(), test_player.bub_pos() + direction_of_flight, nullptr, 0);
            tracker.push_back(
                {'p', move_cost, rl_dist(test_monster.abs_pos(), test_player.abs_pos()),
                 test_player.abs_pos()});
            test_player.mod_moves(-move_cost);
        }
        get_map().clear_traps();
        test_monster.set_dest(test_player.bub_pos());
        test_monster.mod_moves(monster_speed);
        while (test_monster.moves >= 0) {
            const int moves_before = test_monster.moves;
            test_monster.move();
            tracker.push_back(
                {'m', moves_before - test_monster.moves,
                 rl_dist(test_monster.abs_pos(), test_player.abs_pos()), test_monster.abs_pos()});
            if (rl_dist(test_monster.bub_pos(), test_player.bub_pos()) == 1) {
                INFO(tracker);
                clear_map();
                return turn;
            } else if (rl_dist(test_monster.bub_pos(), test_player.bub_pos()) > 20) {
                INFO(tracker);
                clear_map();
                return -turn;
            }
        }
    }
    WARN(tracker);
    return -1000;
}

// Verify that the named monster has the expected effective speed, not reduced
// due to wasted motion from shambling.
static void check_shamble_speed(
    const std::string& monster_type, const tripoint_rel_ms& destination_offset) {
    // Scale the scaling factor based on the ratio of diagonal to cardinal steps.
    const float slope = get_normalized_angle(point_zero, destination_offset.raw().xy());
    const float diagonal_multiplier = 1.0 + (get_option<bool>("CIRCLEDIST") ? (slope * 0.41) : 0.0);
    INFO(monster_type << " " << destination_offset);
    // Wandering makes things nondeterministic, so look at the distribution rather than a target
    // number.
    move_statistics move_stats;
    const auto origin_abs = movement_fixture_map_origin_abs();
    for (int i = 0; i < 10; ++i) {
        move_stats.add(
            moves_to_destination(monster_type, origin_abs, origin_abs + destination_offset));
        if ((move_stats.avg() / (10000.0 * diagonal_multiplier)) == Approx(1.0).epsilon(0.02)) {
            break;
        }
    }
    CAPTURE(slope);
    CAPTURE(move_stats.avg());
    INFO(diagonal_multiplier);
    CHECK((move_stats.avg() / (10000.0 * diagonal_multiplier)) == Approx(1.0).epsilon(0.02));
}

static void test_moves_to_squares(const std::string& monster_type, const bool write_data = false) {
    std::map<int, move_statistics> turns_at_distance;
    std::map<int, move_statistics> turns_at_slope;
    std::map<int, move_statistics> turns_at_angle;
    // For the regression test we want just enough samples, for data we want a lot more.
    const int required_samples = write_data ? 100 : 20;
    const int sampling_resolution = write_data ? 1 : 20;
    bool not_enough_samples = true;
    while (not_enough_samples) {
        not_enough_samples = false;
        for (int x = 0; x <= 100; x += sampling_resolution) {
            for (int y = 0; y <= 100; y += sampling_resolution) {
                const int distance = square_dist({50, 50, 0}, {x, y, 0});
                if (distance <= 5) {
                    // Very short ranged tests are squirrely.
                    continue;
                }
                const int rise = 50 - y;
                const int run = 50 - x;
                const float angle = atan2(run, rise);
                // Bail out if we already have enough samples for this angle.
                if (turns_at_angle[angle * 100].n() >= required_samples) { continue; }
                // Scale the scaling factor based on the ratio of diagonal to cardinal steps.
                const float slope = get_normalized_angle({50, 50}, {x, y});
                const float diagonal_multiplier =
                    1.0 + (get_option<bool>("CIRCLEDIST") ? (slope * 0.41) : 0.0);
                turns_at_angle[angle * 100].new_type();
                turns_at_slope[slope].new_type();
                for (int i = 0; i < 5; ++i) {
                    const int moves = moves_to_destination(
                        monster_type,
                        movement_fixture_map_origin_abs() + tripoint_rel_ms(50, 50, 0),
                        movement_fixture_map_origin_abs() + tripoint_rel_ms(x, y, 0));
                    const int adjusted_moves = moves / (diagonal_multiplier * distance);
                    turns_at_distance[distance].add(adjusted_moves);
                    turns_at_angle[angle * 100].add(adjusted_moves);
                    turns_at_slope[slope].add(adjusted_moves);
                }
                if (turns_at_angle[angle * 100].n() < required_samples) {
                    not_enough_samples = true;
                }
            }
        }
    }
    for (const auto& stat_pair : turns_at_distance) {
        INFO("Monster:" << monster_type << " Dist: " << stat_pair.first
                        << " moves: " << stat_pair.second.avg());
        CHECK(stat_pair.second.avg() == Approx(100.0).epsilon(0.1));
    }
    for (const auto& stat_pair : turns_at_slope) {
        INFO("Monster:" << monster_type << " Slope: " << stat_pair.first << " moves: "
                        << stat_pair.second.avg() << " types: " << stat_pair.second.types());
        CHECK(stat_pair.second.avg() == Approx(100.0).epsilon(0.1));
    }
    for (auto& stat_pair : turns_at_angle) {
        std::stringstream sample_string;
        for (auto sample : stat_pair.second.get_samples()) { sample_string << sample << ", "; }
        INFO("Monster:" << monster_type << " Angle: " << stat_pair.first << " moves: "
                        << stat_pair.second.avg() << " types: " << stat_pair.second.types()
                        << " samples: " << sample_string.str());
        CHECK(stat_pair.second.avg() == Approx(100.0).epsilon(0.1));
    }

    if (write_data) {
        std::ofstream data;
        data.open(
            "slope_test_data_" + std::string((trigdist ? "trig_" : "square_")) + monster_type);
        for (const auto& stat_pair : turns_at_angle) {
            data << stat_pair.first << " " << stat_pair.second.avg() << "\n";
        }
        data.close();
    }
}

static void monster_check() {
    const float diagonal_multiplier = (get_option<bool>("CIRCLEDIST") ? 1.41 : 1.0);
    // Have a monster walk some distance in a direction and measure how long it takes.
    const auto origin_abs = movement_fixture_map_origin_abs();
    float vert_move =
        moves_to_destination("mon_pig", origin_abs, origin_abs + tripoint_rel_ms(100, 0, 0));
    CHECK((vert_move / 10000.0) == Approx(1.0));
    int horiz_move =
        moves_to_destination("mon_pig", origin_abs, origin_abs + tripoint_rel_ms(0, 100, 0));
    CHECK((horiz_move / 10000.0) == Approx(1.0));
    int diag_move =
        moves_to_destination("mon_pig", origin_abs, origin_abs + tripoint_rel_ms(100, 100, 0));
    CHECK((diag_move / (10000.0 * diagonal_multiplier)) == Approx(1.0).epsilon(0.05));

    check_shamble_speed("mon_pig", {100, 0, 0});
    check_shamble_speed("mon_pig", {0, 100, 0});
    check_shamble_speed("mon_pig", {100, 100, 0});
    check_shamble_speed("mon_zombie", {100, 0, 0});
    check_shamble_speed("mon_zombie", {0, 100, 0});
    check_shamble_speed("mon_zombie", {100, 100, 0});
    check_shamble_speed("mon_zombie_dog", {100, 0, 0});
    check_shamble_speed("mon_zombie_dog", {0, 100, 0});
    check_shamble_speed("mon_zombie_dog", {100, 100, 0});
    check_shamble_speed("mon_zombear", {100, 0, 0});
    check_shamble_speed("mon_zombear", {0, 100, 0});
    check_shamble_speed("mon_zombear", {100, 100, 0});
    check_shamble_speed("mon_jabberwock", {100, 0, 0});
    check_shamble_speed("mon_jabberwock", {0, 100, 0});
    check_shamble_speed("mon_jabberwock", {100, 100, 0});

    // Check moves to all squares relative to monster start within 50 squares.
    test_moves_to_squares("mon_zombie_dog");
    test_moves_to_squares("mon_pig");

    // Verify that a walking player can escape from a zombie, but is caught by a zombie dog.
    INFO("Trigdist is " << (get_option<bool>("CIRCLEDIST") ? "on" : "off"));
    CHECK(can_catch_player("mon_zombie", tripoint_rel_ms::east()) < 0);
    CHECK(can_catch_player("mon_zombie", tripoint_rel_ms::south_east()) < 0);
    CHECK(can_catch_player("mon_zombie_dog", tripoint_rel_ms::east()) > 0);
    CHECK(can_catch_player("mon_zombie_dog", tripoint_rel_ms::south_east()) > 0);
}

// Write out a map of slope at which monster is moving to time required to reach their destination.
TEST_CASE("write_slope_to_speed_map_trig", "[.][!mayfail]") {
    clear_all_state();
    move_player_out_of_the_way();
    override_option opt("CIRCLEDIST", "true");
    trigdist = true;
    test_moves_to_squares("mon_zombie_dog", true);
    test_moves_to_squares("mon_pig", true);
}

TEST_CASE("write_slope_to_speed_map_square", "[.][!mayfail]") {
    clear_all_state();
    move_player_out_of_the_way();
    override_option opt("CIRCLEDIST", "false");
    trigdist = false;
    test_moves_to_squares("mon_zombie_dog", true);
    test_moves_to_squares("mon_pig", true);
}

// Characterization test for monster movement speed.
// It's not necessarally the one true speed for monsters, we just want notice if it changes.
TEST_CASE("monster_speed_square", "[speed][.][!mayfail]") {
    clear_all_state();
    move_player_out_of_the_way();
    override_option opt("CIRCLEDIST", "false");
    trigdist = false;
    monster_check();
}

// TODO: Figure out why this sometimes fails, seems to be RNG-dependent
TEST_CASE("monster_speed_trig", "[speed][.][!mayfail]") {
    clear_all_state();
    move_player_out_of_the_way();
    override_option opt("CIRCLEDIST", "true");
    trigdist = true;
    monster_check();
}

TEST_CASE("monster_move_through_vehicle_holes", "[.][monster]") {
    clear_all_state();
    move_player_out_of_the_way();
    const auto origin_abs = test_origin;

    get_map().add_vehicle(vproto_id("apc"), abs_to_bub(origin_abs), -45_degrees, 0, 0);

    const auto mon_origin_abs = origin_abs + tripoint_rel_ms(-2, 1, 0);
    monster& zombie = spawn_test_monster("mon_zombie", abs_to_bub(mon_origin_abs));
    zombie.move_to(abs_to_bub(mon_origin_abs + tripoint_rel_ms::north_west()), false, false, 0.0f);

    const monster* m = g->critter_at<monster>(abs_to_bub(mon_origin_abs));
    CHECK(m != nullptr);

    const monster* m2 = g->critter_at<monster>(
        abs_to_bub(mon_origin_abs + tripoint_rel_ms::north_west()));
    CHECK(m2 == nullptr);
}

TEST_CASE("monster_vertical_melee_respects_floors", "[monster][z-level]") {
    clear_all_state();
    clear_map();

    avatar& you = get_avatar();
    auto& here = get_map();
    const auto avatar_abs = test_origin_at_z(2);
    here.ter_set(abs_to_map_local(here, avatar_abs), ter_id("t_floor"));
    here.ter_set(abs_to_map_local(here, avatar_abs + tripoint_rel_ms::below()), ter_id("t_floor"));
    you.setpos(avatar_abs);
    const auto zombie_abs = avatar_abs + tripoint_rel_ms::below();
    here.ter_set(abs_to_bub(avatar_abs), ter_id("t_open_air"));

    monster& grabber = spawn_test_monster("mon_zombie_grabber", abs_to_bub(zombie_abs));

    SECTION("open air does not block vertical melee") {
        CHECK_FALSE(here.floor_between(abs_to_bub(zombie_abs), abs_to_bub(avatar_abs)));
        CHECK(grabber.attack_at(you.bub_pos()));
    }

    SECTION("terrain floors block vertical melee") {
        here.ter_set(abs_to_bub(avatar_abs), ter_id("t_floor"));

        CHECK(here.floor_between(abs_to_bub(zombie_abs), abs_to_bub(avatar_abs)));
        CHECK_FALSE(grabber.attack_at(you.bub_pos()));
    }

    SECTION("vehicle floors block vertical melee") {
        const vpart_id vpart_frame_vertical("frame_vertical");
        const vpart_id vpart_seat("seat");
        auto* veh = here.add_vehicle(vproto_id("none"), abs_to_bub(avatar_abs), 0_degrees, 0, 0);

        REQUIRE(veh != nullptr);
        veh->install_part(tripoint_mnt_veh::zero(), vpart_frame_vertical);
        veh->install_part(tripoint_mnt_veh::zero(), vpart_seat);
        here.add_vehicle_to_cache(veh);

        CHECK_FALSE(grabber.attack_at(you.bub_pos()));
    }

    SECTION("grabber below player on blimp cannot attack through the floor") {
        auto* blimp = here.add_vehicle(vproto_id("blimp"), abs_to_bub(avatar_abs), 0_degrees, 0, 0);

        REQUIRE(blimp != nullptr);
        const auto cockpit_part =
            blimp->part_with_feature(tripoint_mnt_veh::zero(), "BOARDABLE", true);
        REQUIRE(cockpit_part != -1);

        const auto blimp_tile_abs = blimp->abs_part_location(cockpit_part);
        you.setpos(blimp_tile_abs);
        grabber.setpos(blimp_tile_abs + tripoint_rel_ms::below());

        CHECK(here.veh_at(you.bub_pos()).part_with_feature("BOARDABLE", true).has_value());
        CHECK_FALSE(grabber.attack_at(you.bub_pos()));
    }
}

TEST_CASE("physical_clear_path_respects_vehicle_floors", "[map][z-level]") {
    clear_all_state();
    clear_map();

    auto& here = get_map();
    const auto lower_pos_abs = test_origin_at_z(1);
    const auto upper_pos_abs = test_origin_at_z(2);

    SECTION("open air does not block physical paths") {
        CHECK(map_funcs::physical_clear_path({
            .m = here,
            .from = abs_to_bub(lower_pos_abs),
            .to = abs_to_bub(upper_pos_abs),
            .require_clear_path = false,
        }));
    }

    SECTION("terrain floors block physical paths") {
        here.ter_set(abs_to_bub(upper_pos_abs), ter_id("t_floor"));

        CHECK_FALSE(map_funcs::physical_clear_path({
            .m = here,
            .from = abs_to_bub(lower_pos_abs),
            .to = abs_to_bub(upper_pos_abs),
            .require_clear_path = false,
        }));
    }

    SECTION("vehicle floors block physical paths") {
        auto* veh = here.add_vehicle(vproto_id("none"), abs_to_bub(upper_pos_abs), 0_degrees, 0, 0);

        REQUIRE(veh != nullptr);
        veh->install_part(tripoint_mnt_veh::zero(), vpart_id("frame_vertical"));
        veh->install_part(tripoint_mnt_veh::zero(), vpart_id("seat"));
        here.add_vehicle_to_cache(veh);

        CHECK_FALSE(map_funcs::physical_clear_path({
            .m = here,
            .from = abs_to_bub(lower_pos_abs),
            .to = abs_to_bub(upper_pos_abs),
            .require_clear_path = false,
        }));
    }

    SECTION("vehicle obstacles block physical paths") {
        const auto obstacle_pos_abs = test_origin_at_z(2) + tripoint_rel_ms::east();
        auto* veh =
            here.add_vehicle(vproto_id("none"), abs_to_bub(obstacle_pos_abs), 0_degrees, 0, 0);

        REQUIRE(veh != nullptr);
        veh->install_part(tripoint_mnt_veh::zero(), vpart_id("frame_vertical"));
        veh->install_part(tripoint_mnt_veh::zero(), vpart_id("windshield"));
        here.add_vehicle_to_cache(veh);

        CHECK(here.veh_at(abs_to_bub(obstacle_pos_abs)).obstacle_at_part().has_value());
        CHECK_FALSE(map_funcs::physical_clear_path({
            .m = here,
            .from = abs_to_bub(obstacle_pos_abs + tripoint_rel_ms::west()),
            .to = abs_to_bub(obstacle_pos_abs + tripoint_rel_ms::east()),
            .require_clear_path = false,
        }));
    }

    SECTION("lower-z paths cannot enter helicopter cabins through open air") {
        const auto helicopter_pos_abs = test_origin_at_z(2);
        auto* const helicopter = here.add_vehicle(
            vproto_id("2seater2"), abs_to_bub(helicopter_pos_abs), 0_degrees, 0, 0);

        REQUIRE(helicopter != nullptr);
        const auto seat_part =
            helicopter->part_with_feature(tripoint_mnt_veh::zero(), "SEAT", true);
        REQUIRE(seat_part != -1);
        const auto seat_pos_abs = helicopter->abs_part_location(seat_part);

        CHECK(here.veh_at(abs_to_bub(seat_pos_abs))->is_inside());
        CHECK_FALSE(map_funcs::physical_clear_path({
            .m = here,
            .from = abs_to_bub(
                seat_pos_abs + tripoint_rel_ms::below() + tripoint_rel_ms::east() * 4),
            .to = abs_to_bub(seat_pos_abs),
            .require_clear_path = false,
        }));
    }
}

TEST_CASE("zombie_technician_pull_through_helicopter_windshield_is_blocked", "[monster][vehicle]") {
    clear_all_state();
    clear_map();

    avatar& you = get_avatar();
    auto& here = get_map();
    const auto helicopter_abs = test_origin_at_z(2);
    const auto pipe_id = itype_id("pipe");
    auto* const helicopter = here.add_vehicle(
        vproto_id("2seater2"), abs_to_map_local(here, helicopter_abs), 0_degrees, 0, 0);

    REQUIRE(helicopter != nullptr);
    const auto seat_part = helicopter->part_with_feature(tripoint_mnt_veh::zero(), "SEAT", true);
    REQUIRE(seat_part != -1);
    const auto seat_abs = helicopter->abs_part_location(seat_part);

    const auto windshield_pos = abs_to_map_local(here, seat_abs + tripoint_rel_ms::east());
    REQUIRE(here.veh_at(windshield_pos).obstacle_at_part().has_value());
    you.setpos(seat_abs);
    const auto technician_pos_abs = seat_abs + tripoint_rel_ms(2, 0, 0);
    you.str_cur = 4;
    you.remove_primary_weapon();
    you.wield(item::spawn(pipe_id));
    REQUIRE(you.primary_weapon().typeId() == pipe_id);

    monster& technician =
        spawn_test_monster("mon_zombie_technician", abs_to_bub(technician_pos_abs));
    technician.set_goal(abs_to_bub(seat_abs));
    REQUIRE(technician.sees(you));

    CHECK_FALSE(mattack::pull_metal_weapon(&technician));
    CHECK(you.primary_weapon().typeId() == pipe_id);
}

TEST_CASE("zombie_technician_pull_weakens_with_distance", "[monster][balance]") {
    CHECK(mattack::pull_metal_weapon_success_chance(100, 0) == 100);
    CHECK(mattack::pull_metal_weapon_success_chance(100, 1) == 100);
    CHECK(mattack::pull_metal_weapon_success_chance(100, 2) == 100);
    CHECK(mattack::pull_metal_weapon_success_chance(100, 4) == 25);
    CHECK(mattack::pull_metal_weapon_success_chance(100, 12) == 2);
    CHECK(mattack::pull_metal_weapon_success_chance(0, 1) == 0);
}

TEST_CASE("zombie_technician_pull_uses_physical_clear_path", "[monster][z-level]") {
    clear_all_state();
    clear_map();

    avatar& you = get_avatar();
    auto& here = get_map();
    const auto target_abs = test_origin_at_z(2);
    const auto flaregun_id = itype_id("flaregun");

    here.ter_set(abs_to_map_local(here, target_abs), ter_id("t_floor"));
    you.setpos(target_abs);
    here.ter_set(abs_to_bub(target_abs), ter_id("t_open_air"));
    const auto technician_pos_abs = target_abs + tripoint_rel_ms::below();
    here.ter_set(abs_to_bub(technician_pos_abs), ter_id("t_floor"));
    you.str_cur = 4;
    you.remove_primary_weapon();
    you.wield(item::spawn(flaregun_id));
    REQUIRE(you.primary_weapon().typeId() == flaregun_id);

    monster& technician =
        spawn_test_monster("mon_zombie_technician", abs_to_bub(technician_pos_abs));
    technician.set_goal(abs_to_bub(target_abs));

    SECTION("open air allows metal pulls") {
        CHECK(mattack::pull_metal_weapon(&technician));
        CHECK(you.primary_weapon().is_null());
    }

    SECTION("vehicle floors block metal pulls") {
        auto* veh = here.add_vehicle(vproto_id("none"), abs_to_bub(target_abs), 0_degrees, 0, 0);

        REQUIRE(veh != nullptr);
        veh->install_part(tripoint_mnt_veh::zero(), vpart_id("frame_vertical"));
        veh->install_part(tripoint_mnt_veh::zero(), vpart_id("seat"));
        here.add_vehicle_to_cache(veh);

        CHECK_FALSE(mattack::pull_metal_weapon(&technician));
        CHECK(you.primary_weapon().typeId() == flaregun_id);
    }
}

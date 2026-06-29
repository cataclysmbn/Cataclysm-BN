#include "avatar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "item.h"
#include "melee.h"
#include "monster.h"
#include "player_helpers.h"
#include "ret_val.h"
#include "rng.h"
#include "sounds.h"
#include "state_helpers.h"
#include "type_id.h"

#include <algorithm>
#include <cstdlib>
#include <memory>

struct itype;

static constexpr auto deterministic_dps_seed = 0U;

static auto reset_dps_rng() -> void { rng_set_engine_seed(deterministic_dps_seed); }

// Run a large number of trials of a player attacking a monster with a given weapon,
// and return the average damage done per second.
static double weapon_dps_trials(avatar& attacker, monster& defender, item& weapon) {
    constexpr auto trials = 250;

    int total_damage = 0;
    int total_moves = 0;

    clear_character(attacker);
    REQUIRE(attacker.can_wield(weapon).success());

    melee::clear_stats();
    melee_statistic_data melee_stats = melee::get_stats();
    // Rerun the trials in groups until enough crits occur for a stable comparison.
    for (auto i = 0; i < 6 && melee_stats.actual_crit_count < 30;
         ++i, melee_stats = melee::get_stats()) {
        for (auto j = 0; j < trials; ++j) {
            // Reset and re-wield weapon before each attack to prevent skill-up during trials
            clear_character(attacker);
            attacker.wield(item::spawn(weapon));
            // Verify that wielding worked (and not e.g. using martial arts instead)
            REQUIRE(attacker.primary_weapon().type == weapon.type);

            int before_moves = attacker.get_moves();

            // Keep the defender at maximum health
            const int starting_hp = defender.get_hp_max();
            defender.set_hp(starting_hp);

            // Attack once
            attacker.melee_attack(defender, false);

            // Tally total damage and moves
            total_damage += std::max(0, starting_hp - defender.get_hp());
            total_moves += std::abs(attacker.get_moves() - before_moves);

            // Every hit or miss enqueues a new sound
            // Ideally, we'd have sound vector get cleared after every test, but it's not easy
            sounds::reset_sounds();
        }
    }

    // Scale damage-per-move to damage-per-second
    // 1 second == 100 moves
    return 100.0f * total_damage / total_moves;
}

// Compare actual DPS with estimated effective DPS for an attacker/defender/weapon combo.
static auto check_actual_dps(avatar& attacker, monster& defender, item& weapon) -> void {
    reset_dps_rng();
    clear_character(attacker);
    const auto expect_dps = weapon.effective_dps(attacker, defender);
    reset_dps_rng();
    const auto actual_dps = weapon_dps_trials(attacker, defender, weapon);
    CHECK(actual_dps == Approx(expect_dps).epsilon(0.35f));
}

static auto check_accuracy_dps(
    avatar& attacker, monster& defender, item& wpn1, item& wpn2, item& wpn3) -> void {
    clear_character(attacker);
    reset_dps_rng();
    melee::clear_stats();
    const auto dps_wpn1 = weapon_dps_trials(attacker, defender, wpn1);
    const auto wpn1_stats = melee::get_stats();
    reset_dps_rng();
    melee::clear_stats();
    const auto dps_wpn2 = weapon_dps_trials(attacker, defender, wpn2);
    const auto wpn2_stats = melee::get_stats();
    reset_dps_rng();
    melee::clear_stats();
    const auto dps_wpn3 = weapon_dps_trials(attacker, defender, wpn3);
    const auto wpn3_stats = melee::get_stats();
    REQUIRE(wpn1_stats.hit_count > 0);
    REQUIRE(wpn2_stats.hit_count > 0);
    REQUIRE(wpn3_stats.hit_count > 0);
    CAPTURE(wpn1_stats.attack_count);
    CAPTURE(wpn2_stats.attack_count);
    CAPTURE(wpn3_stats.attack_count);
    double wpn1_hit_rate = static_cast<double>(wpn1_stats.hit_count) / wpn1_stats.attack_count;
    double wpn2_hit_rate = static_cast<double>(wpn2_stats.hit_count) / wpn2_stats.attack_count;
    double wpn3_hit_rate = static_cast<double>(wpn3_stats.hit_count) / wpn3_stats.attack_count;
    CHECK(wpn2_hit_rate > wpn1_hit_rate);
    CHECK(wpn3_hit_rate > wpn2_hit_rate);
    CHECK(dps_wpn2 > dps_wpn1);
    CHECK(dps_wpn3 > dps_wpn2);
}
TEST_CASE("effective damage per second", "[effective][dps]") {
    clear_all_state();
    rng_set_engine_seed(0);
    avatar& dummy = g->u;
    clear_character(dummy);

    item& clumsy_sword = *item::spawn_temporary("test_clumsy_sword");
    item& normal_sword = *item::spawn_temporary("test_normal_sword");
    item& good_sword = *item::spawn_temporary("test_balanced_sword");

    SECTION("against a debug monster with no armor or dodge") {
        reset_dps_rng();
        monster debug_unarmored(mtype_id("debug_mon"));

        CHECK(clumsy_sword.effective_dps(dummy, debug_unarmored) == Approx(29.5f).epsilon(0.15f));
        CHECK(good_sword.effective_dps(dummy, debug_unarmored) == Approx(45.0f).epsilon(0.15f));
    }

    SECTION("against an debug agile target") {
        reset_dps_rng();
        monster debug_agile(mtype_id("debug_mon_agile"));
        REQUIRE(debug_agile.get_dodge() >= 4);

        CHECK(clumsy_sword.effective_dps(dummy, debug_agile) == Approx(10.5f).epsilon(0.15f));
        CHECK(good_sword.effective_dps(dummy, debug_agile) == Approx(30.0f).epsilon(0.15f));
    }

    SECTION("against an armored target") {
        reset_dps_rng();
        monster debug_mon_armored(mtype_id("debug_mon_armored"));

        CHECK(clumsy_sword.effective_dps(dummy, debug_mon_armored) == Approx(5.7f).epsilon(0.15f));
        CHECK(good_sword.effective_dps(dummy, debug_mon_armored) == Approx(11.9f).epsilon(0.15f));
    }

    SECTION("effect of STR and DEX on damage per second") {
        monster mummy(mtype_id("debug_mon"));

        SECTION("STR 6, DEX 6") {
            reset_dps_rng();
            dummy.str_max = 6;
            dummy.dex_max = 6;

            CHECK(clumsy_sword.effective_dps(dummy, mummy) == Approx(23.5f).epsilon(0.15f));
            CHECK(normal_sword.effective_dps(dummy, mummy) == Approx(31.5f).epsilon(0.15f));
            CHECK(good_sword.effective_dps(dummy, mummy) == Approx(38.75f).epsilon(0.15f));
        }

        SECTION("STR 8, DEX 10") {
            reset_dps_rng();
            dummy.str_max = 8;
            dummy.dex_max = 10;

            CHECK(clumsy_sword.effective_dps(dummy, mummy) == Approx(29.5f).epsilon(0.15f));
            CHECK(normal_sword.effective_dps(dummy, mummy) == Approx(37.75f).epsilon(0.15f));
            CHECK(good_sword.effective_dps(dummy, mummy) == Approx(45.0f).epsilon(0.15f));
        }

        SECTION("STR 10, DEX 10") {
            reset_dps_rng();
            dummy.str_max = 10;
            dummy.dex_max = 10;

            CHECK(clumsy_sword.effective_dps(dummy, mummy) == Approx(31.0f).epsilon(0.15f));
            CHECK(normal_sword.effective_dps(dummy, mummy) == Approx(40.0f).epsilon(0.15f));
            CHECK(good_sword.effective_dps(dummy, mummy) == Approx(47.5f).epsilon(0.15f));
        }
    }
}

TEST_CASE("effective vs actual damage per second", "[actual][dps]") {
    clear_all_state();
    rng_set_engine_seed(0);
    avatar& dummy = g->u;
    clear_character(dummy);

    // WE HOPE AND PRAY THIS NEVER BREAKS
    // IF IT DOES DELETE IT
    monster soldier(mtype_id("mon_zombie_soldier"));
    monster smoker(mtype_id("mon_zombie_smoker"));
    monster survivor(mtype_id("mon_zombie_survivor"));

    item& clumsy_sword = *item::spawn_temporary("test_clumsy_sword");
    item& normal_sword = *item::spawn_temporary("test_normal_sword");
    item& good_sword = *item::spawn_temporary("test_balanced_sword");

    SECTION("soldier zombie") {
        check_actual_dps(dummy, soldier, clumsy_sword);
        check_actual_dps(dummy, soldier, normal_sword);
        check_actual_dps(dummy, soldier, good_sword);
    }

    SECTION("smoker zombie") {
        check_actual_dps(dummy, smoker, clumsy_sword);
        check_actual_dps(dummy, smoker, normal_sword);
        check_actual_dps(dummy, smoker, good_sword);
    }

    SECTION("survivor zombie") {
        check_actual_dps(dummy, survivor, clumsy_sword);
        check_actual_dps(dummy, survivor, normal_sword);
        check_actual_dps(dummy, survivor, good_sword);
    }
}

TEST_CASE("accuracy increases success", "[accuracy][dps]") {
    clear_all_state();
    rng_set_engine_seed(0);
    avatar& dummy = g->u;
    clear_character(dummy);

    monster soldier(mtype_id("mon_zombie_soldier"));
    monster smoker(mtype_id("mon_zombie_smoker"));
    monster survivor(mtype_id("mon_zombie_survivor"));

    item& clumsy_sword = *item::spawn_temporary("test_clumsy_sword");
    item& normal_sword = *item::spawn_temporary("test_normal_sword");
    item& good_sword = *item::spawn_temporary("test_balanced_sword");

    SECTION("soldier zombie") {
        check_accuracy_dps(dummy, soldier, clumsy_sword, normal_sword, good_sword);
    }

    SECTION("smoker zombie") {
        check_accuracy_dps(dummy, smoker, clumsy_sword, normal_sword, good_sword);
    }

    SECTION("survivor zombie") {
        check_accuracy_dps(dummy, survivor, clumsy_sword, normal_sword, good_sword);
    }
}

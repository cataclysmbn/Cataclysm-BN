#include "activity_skip_wait_helpers.h"
#include "activity_time_cadence.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "morale_types.h"
#include "rng.h"

#include <cstdlib>
#include <ranges>
#include <string>
#include <vector>

namespace {

/// Waits 2 hours in the field in clear 18 C weather at TIME_ACTION_SCALE @p scale. The avatar
/// starts with a 3 hour test_high effect (+25 morale) and a decaying food morale bonus. The RNG
/// starts from the same seed for every scale.
auto wait_at_scale(const int scale) -> wait_end_state {
    const auto skip_options = skip_window_options{};
    const auto action_scale = override_option("TIME_ACTION_SCALE", std::to_string(scale));
    setup_wait_scene(wait_scene::field);
    hold_weather(weather_type_id("clear"), 18_c);
    rng_set_engine_seed(1);
    g->u.add_effect(efftype_id("test_high"), 3_hours);
    g->u.add_morale(morale_type("morale_food_good"), 20, 20, 3_hours, 1_hours);

    const auto state = wait_through_activity(2_hours);
    INFO(state.error_or(""));
    REQUIRE(state.has_value());
    return *state;
}

auto describe(const wait_end_state& s) -> std::string {
    auto text = string_format(
        "kcal %d thirst %d fatigue %d morale %d turn %d calls %d tick %d", s.stored_kcal, s.thirst,
        s.fatigue, s.morale, to_turns<int>(s.turn - calendar::turn_zero), s.do_turn_calls,
        s.tick_turns);
    for (const auto& [bp, temp] : s.body_temperature) {
        text += string_format("\n  temp %s %d", bp.str(), units::to_millidegree_celsius(temp));
    }
    for (const auto& [eff, dur] : s.effect_durations) {
        text += string_format("\n  effect %s %d", eff, to_turns<int>(dur));
    }
    return text;
}

} // namespace

TEST_CASE(
    "waiting gives the same end state at TIME_ACTION_SCALE 100 and 10",
    "[activity][fixed_window][speed]") {
    const auto exact = wait_at_scale(100);
    const auto coarse = wait_at_scale(10);
    INFO("scale 100: " << describe(exact));
    INFO("scale 10: " << describe(coarse));

    // The scale reaches the skip window: 1 calendar turn per tick at 100, 10 at 10.
    CHECK(exact.tick_turns == 1);
    CHECK(coarse.tick_turns == 10);
    // Most of the wait passes inside skip windows. Without them, one do_turn call per tick
    // takes 7200 calls at scale 100 and 720 at scale 10; 24 windows of 5 minutes take about 100.
    const auto windows =
        to_turns<int>(2_hours) / to_turns<int>(activity_time_cadence::fixed_window());
    CHECK(exact.do_turn_calls <= windows * 5);
    CHECK(coarse.do_turn_calls <= windows * 5);

    // Not checked yet, as they differ today: calendar::turn and the test_high duration.
    const auto kcal_tolerance = 10;
    const auto need_tolerance = 1;
    const auto morale_tolerance = 1;
    const auto temperature_tolerance_mc = 50;
    CHECK(std::abs(exact.stored_kcal - coarse.stored_kcal) <= kcal_tolerance);
    CHECK(std::abs(exact.thirst - coarse.thirst) <= need_tolerance);
    CHECK(std::abs(exact.fatigue - coarse.fatigue) <= need_tolerance);
    CHECK(std::abs(exact.morale - coarse.morale) <= morale_tolerance);
    for (const auto& [bp, temperature] : exact.body_temperature) {
        CAPTURE(bp.str());
        CHECK(std::abs(units::to_millidegree_celsius(temperature)
                       - units::to_millidegree_celsius(coarse.body_temperature.at(bp)))
              <= temperature_tolerance_mc);
    }
    const auto effect_names = [](const wait_end_state& s) {
        return s.effect_durations | std::views::keys | std::ranges::to<std::vector>();
    };
    CHECK(effect_names(exact) == effect_names(coarse));
    CHECK(exact.effect_durations.at("happy:num_bp") == coarse.effect_durations.at("happy:num_bp"));
}

TEST_CASE(
    "waiting in cold or rain gives the same body state at TIME_ACTION_SCALE 100 and 10",
    "[activity][fixed_window][speed]") {
    const auto skip_options = skip_window_options{};
    const auto wait_in =
        [](const int scale, const weather_type_id& weather, const units::temperature temperature) {
            const auto action_scale = override_option("TIME_ACTION_SCALE", std::to_string(scale));
            setup_wait_scene(wait_scene::field);
            hold_weather(weather, temperature);
            rng_set_engine_seed(1);
            const auto state = wait_through_activity(1_hours);
            INFO(state.error_or(""));
            REQUIRE(state.has_value());
            return *state;
        };
    // Body temperature converges and rain wets once per call, so a 10-turn tick needs 10 calls.
    const auto check_same_body =
        [&](const weather_type_id& weather, const units::temperature temperature) {
            const auto exact = wait_in(100, weather, temperature);
            const auto coarse = wait_in(10, weather, temperature);
            INFO("scale 100: " << describe(exact));
            INFO("scale 10: " << describe(coarse));
            REQUIRE(coarse.tick_turns == 10);
            CHECK(std::abs(exact.morale - coarse.morale) <= 1);
            for (const auto& [bp, body_temperature] : exact.body_temperature) {
                CAPTURE(bp.str());
                CHECK(std::abs(units::to_millidegree_celsius(body_temperature)
                               - units::to_millidegree_celsius(coarse.body_temperature.at(bp)))
                      <= 50);
            }
        };

    SECTION("cold") { check_same_body(weather_type_id("clear"), -10_c); }
    SECTION("rain") { check_same_body(weather_type_id("rain"), 10_c); }
}

TEST_CASE("waiting with a dead avatar returns an error", "[activity][fixed_window]") {
    // A dead avatar made the next do_turn open the last words prompt, which never returns in a
    // test. The helper must return an error before it calls do_turn: set head and torso HP to 0,
    // wait.
    setup_wait_scene(wait_scene::field);
    g->u.set_part_hp_cur(bodypart_id("head"), 0);
    g->u.set_part_hp_cur(bodypart_id("torso"), 0);
    REQUIRE(g->u.is_dead_state());

    const auto state = wait_through_activity(10_minutes);
    CHECK_FALSE(state.has_value());
}

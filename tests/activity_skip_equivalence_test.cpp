#include "../src/map/map.h"
#include "activity_skip_wait_helpers.h"
#include "activity_time_cadence.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "map/field.h"
#include "map/field_type.h"
#include "morale_types.h"
#include "rng.h"
#include "timed_event.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

namespace {

/// Weather and length of one wait.
struct wait_conditions {
    std::string weather = "clear";
    units::temperature temperature = 18_c;
    time_duration duration = 2_hours;
    bool skip_windows = true;
};

/// Waits in the field under @p conditions at TIME_ACTION_SCALE @p scale. The avatar starts with
/// a 3 hour test_high effect (+25 morale) and a decaying food morale bonus. The RNG starts from
/// the same seed for every scale.
auto wait_at_scale(const int scale, const wait_conditions& conditions = {}) -> wait_end_state {
    const auto skip_options = skip_window_options{};
    const auto action_scale = override_option("TIME_ACTION_SCALE", std::to_string(scale));
    setup_wait_scene(wait_scene::field);
    hold_weather(weather_type_id(conditions.weather), conditions.temperature);
    // A timed event in the next 5 minutes blocks the skip window.
    for (auto t = 5_minutes; !conditions.skip_windows && t <= conditions.duration; t += 5_minutes) {
        g->timed_events.add(TIMED_EVENT_NULL, calendar::turn + t);
    }
    rng_set_engine_seed(1);
    g->u.add_effect(efftype_id("test_high"), 3_hours);
    g->u.add_morale(morale_type("morale_food_good"), 20, 20, 3_hours, 1_hours);

    const auto state = wait_through_activity(conditions.duration);
    INFO(state.error_or(""));
    REQUIRE(state.has_value());
    return *state;
}

/// Checks morale within 1 and the temperature of each body part within 50 millidegrees.
auto check_same_body(const wait_end_state& exact, const wait_end_state& coarse) -> void {
    CHECK(exact.morale == Approx(coarse.morale).margin(1));
    for (const auto& [bp, temperature] : exact.body_temperature) {
        CAPTURE(bp.str());
        CHECK(units::to_millidegree_celsius(temperature)
              == Approx(units::to_millidegree_celsius(coarse.body_temperature.at(bp))).margin(50));
    }
}

} // namespace

TEST_CASE(
    "waiting gives the same end state at TIME_ACTION_SCALE 100 and 10",
    "[activity][fixed_window][speed]") {
    const auto exact = wait_at_scale(100);
    const auto coarse = wait_at_scale(10);

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
    CHECK(exact.stored_kcal == Approx(coarse.stored_kcal).margin(10));
    CHECK(exact.thirst == Approx(coarse.thirst).margin(1));
    CHECK(exact.fatigue == Approx(coarse.fatigue).margin(1));
    check_same_body(exact, coarse);
    const auto effect_names = [](const wait_end_state& s) {
        return s.effect_durations | std::views::keys | std::ranges::to<std::vector>();
    };
    REQUIRE(effect_names(exact) == effect_names(coarse));
    CHECK(exact.effect_durations.at("happy:num_bp") == coarse.effect_durations.at("happy:num_bp"));
}

/// A chicken appears at the first spawn check of a 10 minute wait. The wait still ends after
/// exactly 10 minutes.
TEST_CASE("wait ends on time when a monster appears", "[activity][fixed_window]") {
    const auto skip_options = skip_window_options{};
    const auto action_scale = override_option("TIME_ACTION_SCALE", "100");
    setup_wait_scene(wait_scene::field);
    hold_weather(weather_type_id("clear"), 18_c);
    g->m.add_spawn(mtype_id("mon_chicken"), 1, tripoint_bub_ms(60, 30, 0));
    const auto end_turn = calendar::turn + 10_minutes;

    const auto state = wait_through_activity(10_minutes);
    INFO(state.error_or(""));
    REQUIRE(state.has_value());
    CHECK(g->num_creatures() == 2);
    CHECK(to_turn<int>(state->turn) == to_turn<int>(end_turn));
}

// Body temperature converges and rain wets once per call, so a 10-turn tick needs 10 calls, inside
// and outside skip windows.
TEST_CASE(
    "waiting in cold or rain gives the same body state at TIME_ACTION_SCALE 100 and 10",
    "[activity][fixed_window][speed]") {
    const auto [weather, temperature] = GENERATE(table<std::string, units::temperature>({
        {"clear", -10_c},
        {"rain", 10_c},
    }));
    const auto skip_windows = GENERATE(true, false);
    CAPTURE(weather, skip_windows);
    const auto conditions = wait_conditions{
        .weather = weather,
        .temperature = temperature,
        // Without skip windows scale 100 takes 1 do_turn call per turn.
        .duration = skip_windows ? 1_hours : 10_minutes,
        .skip_windows = skip_windows,
    };
    const auto exact = wait_at_scale(100, conditions);
    const auto coarse = wait_at_scale(10, conditions);
    REQUIRE(coarse.tick_turns == 10);
    // Without skip windows each 10-turn tick is one do_turn call.
    if (!skip_windows) { REQUIRE(coarse.do_turn_calls == to_turns<int>(10_minutes) / 10); }
    check_same_body(exact, coarse);
}

// Issue 11046: the avatar refuels the fireplace from a firewood source, so the fire burns for the
// whole hour. The wait still skips time in 5-minute windows.
TEST_CASE("waiting next to a refueled fireplace skips time", "[activity][fixed_window]") {
    const auto skip_options = skip_window_options{};
    const auto action_scale = override_option("TIME_ACTION_SCALE", "100");
    setup_wait_scene(wait_scene::fireplace);
    hold_weather(weather_type_id("clear"), 18_c);

    const auto state = wait_through_activity(1_hours);
    INFO(state.error_or(""));
    REQUIRE(state.has_value());
    CHECK(g->m.get_field(g->u.bub_pos() + fireplace_fire, fd_fire) != nullptr);
    CHECK(g->m.i_at(g->u.bub_pos() + fireplace_pile).size() < 30);
    const auto windows =
        to_turns<int>(1_hours) / to_turns<int>(activity_time_cadence::fixed_window());
    CHECK(state->do_turn_calls <= windows * 5);
}

// A fire that can spread does not stop the skip window. The fire starts on the last tile of a
// submap and burns a line of 24 webs through the next 2 submaps within one 5-minute window. Blood
// in the last submap ages the full 5 minutes, also the turns before the fire reached it.
TEST_CASE("waiting near a spreading fire skips time", "[activity][fixed_window]") {
    const auto skip_options = skip_window_options{};
    const auto action_scale = override_option("TIME_ACTION_SCALE", "100");
    setup_wait_scene(wait_scene::field);
    hold_weather(weather_type_id("clear"), 18_c);
    // The avatar stands 6 tiles east of a submap border, so x + 5 is the last tile of a submap.
    const auto fire = g->u.bub_pos() + point(5, 24);
    g->m.add_field(fire, fd_fire, 2);
    for (auto i = 1; i <= 24; ++i) { g->m.add_field(fire + point(i, 0), fd_web, 1); }
    const auto blood = fire + point(24, 1);
    g->m.add_field(blood, fd_blood, 1);

    const auto state = wait_through_activity(5_minutes);
    INFO(state.error_or(""));
    REQUIRE(state.has_value());
    // Fire removes a web in the tile it spreads to.
    const auto webs_left = std::ranges::count_if(std::views::iota(1, 25), [&](int i) {
        return g->m.get_field(fire + point(i, 0), fd_web) != nullptr;
    });
    CHECK(webs_left == 0);
    const auto* blood_field = g->m.get_field(blood, fd_blood);
    REQUIRE(blood_field != nullptr);
    CHECK(blood_field->get_field_age() == 5_minutes);
    CHECK(state->do_turn_calls <= 5);
}

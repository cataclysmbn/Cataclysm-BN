#include "action_time_scale.h"
#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "player_helpers.h"
#include "rng.h"
#include "state_helpers.h"

#include <cmath>
#include <optional>
#include <vector>

static const efftype_id effect_accumulated_mutagen("accumulated_mutagen");
static const efftype_id effect_pblue("pblue");
static const bionic_id bio_leaky("bio_leaky");

namespace {

/// Expected events of a rate test. Each test runs long enough for 100 expected events.
constexpr auto expected_events = 100;
/// 4 standard deviations of a Poisson count with mean 100.
constexpr auto event_tolerance = 40;

/// Advances `guy` by `turns` calendar turns in ticks of `tick_length` turns.
/// It works like game::advance_time_action_tick: set the tick length, add it to calendar::turn,
/// then process the tick. It calls `after_tick` after each process_turn().
template <typename Fn>
auto run_ticks(avatar& guy, const int tick_length, const int turns, Fn&& after_tick) -> void {
    const auto tick_scope = action_time_scale::scoped_calendar_turns_this_tick(tick_length);
    for (auto elapsed = 0; elapsed < turns; elapsed += tick_length) {
        calendar::turn += time_duration::from_turns(tick_length);
        guy.process_turn();
        guy.set_moves(0);
        after_tick();
    }
}

auto prepare_avatar() -> avatar& { // *NOPAD*
    clear_all_state();
    auto& guy = get_avatar();
    clear_character(guy, true);
    calendar::turn = calendar::turn_zero + 1_hours;
    // The rate tests check a 4 sigma bound, so a fixed seed keeps them from failing at random.
    rng_set_engine_seed(1);
    return guy;
}

} // namespace

TEST_CASE("Tick turn helper calls once and keeps the turn at tick length 1", "[speed]") {
    clear_all_state();
    calendar::turn = calendar::turn_zero + 1_hours;
    const auto tick_scope = action_time_scale::scoped_calendar_turns_this_tick(1);

    auto seen = std::vector<time_point>{};
    action_time_scale::for_each_calendar_turn_this_tick([&] {
        seen.push_back(calendar::turn);
        return true;
    });

    CHECK(seen == std::vector{calendar::turn_zero + 1_hours});
    CHECK(calendar::turn == calendar::turn_zero + 1_hours);
    CHECK(action_time_scale::calendar_turns_this_tick() == 1);
}

TEST_CASE("Tick turn helper visits each turn of a multi-turn tick", "[speed]") {
    clear_all_state();
    const auto last_turn = calendar::turn_zero + 1_hours;
    calendar::turn = last_turn;
    const auto tick_scope = action_time_scale::scoped_calendar_turns_this_tick(3);

    SECTION("all turns, oldest first, each with tick length 1") {
        auto seen = std::vector<time_point>{};
        auto lengths = std::vector<int>{};
        action_time_scale::for_each_calendar_turn_this_tick([&] {
            seen.push_back(calendar::turn);
            lengths.push_back(action_time_scale::calendar_turns_this_tick());
            return true;
        });

        CHECK(seen == std::vector{last_turn - 2_turns, last_turn - 1_turns, last_turn});
        CHECK(lengths == std::vector{1, 1, 1});
    }

    SECTION("stops when the callable returns false") {
        auto calls = 0;
        action_time_scale::for_each_calendar_turn_this_tick([&] { return ++calls < 2; });

        CHECK(calls == 2);
    }

    // The helper restores the turn and the tick length in both sections.
    CHECK(calendar::turn == last_turn);
    CHECK(action_time_scale::calendar_turns_this_tick() == 3);
}

// pblue has "rad_chance": 100 and "rad_min": -1, so it removes 1 rad in 1 of 100 turns.
// 10000 turns give 100 expected procs at tick length 1 and 10.
// Rad is set back to 40 after each tick, so each tick counts its own procs.
// 40 rad is below the 50 rad threshold of radiation vomiting.
TEST_CASE("Effect proc chance keeps its calendar rate in multi-turn ticks", "[effect][speed]") {
    const auto tick_length = GENERATE(1, 10);
    CAPTURE(tick_length);
    auto& guy = prepare_avatar();
    constexpr auto start_rad = 40;
    guy.add_effect(effect_pblue, 1_days);
    guy.set_rad(start_rad);

    auto procs = 0;
    run_ticks(guy, tick_length, 10000, [&] {
        procs += start_rad - guy.get_rad();
        guy.set_rad(start_rad);
    });

    CHECK(procs >= expected_events - event_tolerance);
    CHECK(procs <= expected_events + event_tolerance);
}

// bio_leaky lowers the healthy mod by 1 in 1 of 360 turns (one_turn_in( 6_minutes ) in suffer()).
// 36000 turns give 100 expected procs at tick length 1 and 10.
// The healthy mod is set back to 0 after each tick, so each tick counts its own procs.
TEST_CASE(
    "Bionic side effect roll keeps its calendar rate in multi-turn ticks", "[bionics][speed]") {
    const auto tick_length = GENERATE(1, 10);
    CAPTURE(tick_length);
    auto& guy = prepare_avatar();
    guy.add_bionic(bio_leaky);
    guy.set_healthy_mod(0);

    auto procs = 0;
    run_ticks(guy, tick_length, 36000, [&] {
        procs += static_cast<int>(std::lround(-guy.get_healthy_mod()));
        guy.set_healthy_mod(0);
    });

    CHECK(procs >= expected_events - event_tolerance);
    CHECK(procs <= expected_events + event_tolerance);
}

// An effect added with 99 turns left at turn T loses 1 turn per calendar turn.
// It reaches 0 at T + 99 and goes away at T + 100, a tick end for tick length 1 and 10.
// After 10 turns it has 89 turns left. A tick of 10 turns must not take 10 turns per turn.
TEST_CASE("Effect ends on the same calendar turn in multi-turn ticks", "[effect][speed]") {
    const auto tick_length = GENERATE(1, 10);
    CAPTURE(tick_length);
    auto& guy = prepare_avatar();
    const auto start = calendar::turn;
    guy.add_effect(effect_pblue, 99_turns);

    run_ticks(guy, tick_length, 10, [] {});
    CHECK(guy.get_effect_dur(effect_pblue) == 89_turns);

    auto end_turn = std::optional<time_point>{};
    run_ticks(guy, tick_length, 200, [&] {
        if (!end_turn && !guy.has_effect(effect_pblue)) { end_turn = calendar::turn; }
    });

    CHECK(end_turn == start + 100_turns);
}

// suffer() adds 1 hour of accumulated_mutagen once per hour (once_every_this_tick( 1_hours )).
// 40 turns that cross one hour boundary must add it once, also when the turns run in ticks of 10.
TEST_CASE("Hourly suffer event happens once per hour in multi-turn ticks", "[mutations][speed]") {
    const auto tick_length = GENERATE(1, 10);
    CAPTURE(tick_length);
    auto& guy = prepare_avatar();
    calendar::turn = calendar::turn_zero + 2_hours - 20_turns;
    guy.remove_effect(effect_accumulated_mutagen);

    run_ticks(guy, tick_length, 40, [] {});

    CHECK(guy.get_effect_dur(effect_accumulated_mutagen) == 1_hours);
}

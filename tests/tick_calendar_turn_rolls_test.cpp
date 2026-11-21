#include "action_time_scale.h"
#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "player_helpers.h"
#include "rng.h"
#include "state_helpers.h"

#include <cmath>
#include <optional>

static const efftype_id effect_accumulated_mutagen("accumulated_mutagen");
static const efftype_id effect_pblue("pblue");
static const bionic_id bio_leaky("bio_leaky");

namespace {

/// Advances `guy` by `turns` calendar turns in ticks of `tick_length` turns, as
/// game::advance_time_action_tick does, and calls `after_tick` after each process_turn().
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

// Each roll gives 100 expected procs at tick length 1 and 10, within 40 (4 sigma of a Poisson count
// of mean 100). Each tick sets the tested value back, so it counts only its own procs.
TEST_CASE(
    "Per-turn rolls keep their calendar rate in multi-turn ticks", "[effect][bionics][speed]") {
    const auto tick_length = GENERATE(1, 10);
    CAPTURE(tick_length);
    auto& guy = prepare_avatar();
    auto procs = 0;

    SECTION("pblue removes 1 rad in 1 of 100 turns") {
        // 40 rad is below the 50 rad threshold of radiation vomiting.
        guy.add_effect(effect_pblue, 1_days);
        guy.set_rad(40);
        run_ticks(guy, tick_length, 10000, [&] {
            procs += 40 - guy.get_rad();
            guy.set_rad(40);
        });
    }
    SECTION("bio_leaky lowers the healthy mod by 1 in 1 of 360 turns") {
        guy.add_bionic(bio_leaky);
        guy.set_healthy_mod(0);
        run_ticks(guy, tick_length, 36000, [&] {
            procs += static_cast<int>(std::lround(-guy.get_healthy_mod()));
            guy.set_healthy_mod(0);
        });
    }

    CHECK(procs >= 100 - 40);
    CHECK(procs <= 100 + 40);
}

// An effect added with 99 turns left at turn T has 89 left after 10 turns, also in 10-turn ticks.
// It goes away at T + 100, a tick end for tick length 1 and 10.
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

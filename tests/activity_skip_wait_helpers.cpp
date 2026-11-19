#include "activity_skip_wait_helpers.h"

#include "../src/map/map.h"
#include "action_time_scale.h"
#include "avatar.h"
#include "effect.h"
#include "game.h"
#include "map_helpers.h"
#include "player_activity.h"
#include "state_helpers.h"
#include "timed_event.h"
#include "weather/weather.h"

namespace {

auto build_town() -> void {
    auto& here = get_map();
    const auto wall = ter_id("t_brick_wall");
    const auto floor = ter_id("t_floor");
    const auto roof = ter_id("t_flat_roof");
    for (auto x = 0; x < here.getmapsize() * SEEX; ++x) {
        for (const auto side : {-1, 1}) {
            for (auto d = 3; d <= 10; ++d) {
                for (auto z = 0; z <= 2; ++z) {
                    const auto edge = d == 3 || d == 10 || x % 12 == 0;
                    const auto door = d == 3 && x % 12 == 6 && z == 0;
                    here.ter_set(
                        tripoint_bub_ms(x, 66 + side * d, z),
                        z == 2 ? roof
                        : edge && !door
                            ? wall
                            : floor);
                }
            }
        }
    }
    for (auto i = 0; i < 12; ++i) {
        spawn_test_monster("mon_cow", tripoint_bub_ms(20 + i * 8, 66 + (i % 2 ? 2 : -2), 0));
    }
}

} // namespace

auto setup_wait_scene(const wait_scene scene) -> void {
    clear_all_state();
    g->place_player(tripoint_bub_ms(30, 66, 0));
    calendar::turn = calendar::turn_zero + 12_hours;
    if (scene == wait_scene::town) { build_town(); }

    // A wait in an earlier test leaves the next skip window check at its end turn.
    g->reset_activity_fixed_window();
    g->timed_events = timed_event_manager{};

    auto& weather = get_weather();
    weather.weather_id = weather_type_id("clear");
    weather.nextweather = calendar::turn + 10_minutes;
    weather.temperature = 18_c;
    weather.clear_temp_cache();

    g->reset_light_level();
    g->m.invalidate_map_cache(g->get_levz());
    g->m.build_map_cache(g->get_levz(), true);
    g->new_game = false;
}

auto hold_weather(const weather_type_id& weather, const units::temperature temperature) -> void {
    auto& w = get_weather();
    w.weather_id = weather;
    w.temperature = temperature;
    w.nextweather = calendar::turn + 3_hours;
    // A weather update also runs when the player is in a different submap than at the last one.
    w.last_weather_position = g->u.abs_pos();
    w.clear_temp_cache();
}

auto wait_through_activity(const time_duration& duration)
    -> std::expected<wait_end_state, std::string> {
    auto& u = g->u;
    u.set_moves(100);
    u.assign_activity(activity_id("ACT_WAIT"), to_moves<int>(duration), 0);

    const auto end_turn = calendar::turn + duration;
    // do_turn on a dead avatar opens the last words prompt, which never returns in a test.
    const auto is_over = [&] { return u.is_dead_state() || g->uquit != QUIT_NO; };
    const auto died = [&] {
        const auto* killer = u.get_killer();
        return std::unexpected(string_format(
            "avatar died after %d turns, killer: %s",
            to_turns<int>(calendar::turn - end_turn + duration),
            killer ? killer->disp_name() : "none"));
    };
    if (is_over()) { return died(); }
    auto calls = 0;
    // Stop one minute late at most if the activity does not end on its own.
    while (u.activity && *u.activity && calendar::turn < end_turn + 1_minutes) {
        ++calls;
        if (g->do_turn() || is_over()) { return died(); }
    }

    auto state = wait_end_state{
        .stored_kcal = u.get_stored_kcal(),
        .thirst = u.get_thirst(),
        .fatigue = u.get_fatigue(),
        .body_temperature = {},
        .morale = u.get_morale_level(),
        .effect_durations = {},
        .turn = calendar::turn,
        .do_turn_calls = calls,
        .tick_turns = action_time_scale::calendar_turns_this_tick(),
    };
    for (const auto& bp : u.get_all_body_parts()) {
        state.body_temperature[bp.id()] = u.get_part_temp_cur(bp);
    }
    for (const auto& [type, per_part] : u.get_effects()) {
        for (const auto& [bp, eff] : per_part) {
            state.effect_durations[type.str() + ":" + bp.str()] = eff.get_duration();
        }
    }
    return state;
}

#pragma once
#ifndef CATA_TESTS_ACTIVITY_SKIP_WAIT_HELPERS_H
#    define CATA_TESTS_ACTIVITY_SKIP_WAIT_HELPERS_H

#    include "bodypart.h"
#    include "calendar.h"
#    include "game_constants.h"
#    include "options_helpers.h"
#    include "type_id.h"
#    include "units_temperature.h"

#    include <expected>
#    include <map>
#    include <string>

enum class wait_scene : int {
    /// Daylight field with nothing near the avatar.
    field,
    /// Field with two-storey brick houses on both sides of the street and 12 cows.
    town,
};

/// Options that let the skip window run in a test map. Keep an instance alive during the wait.
struct skip_window_options {
    override_option no_autosave{"AUTOSAVE", "false"};
    override_option
        normal_bubble_size{"REALITY_BUBBLE_SIZE", std::to_string(g_reality_bubble_size)};
    override_option no_mobile_bubble{"ACTIVITY_MOBILE_BUBBLE_SIZE", "0"};
    override_option no_idle_bubble{"ACTIVITY_IDLE_BUBBLE_SIZE", "0"};
    override_option no_underground_bubble{"UNDERGROUND_BUBBLE_SIZE", "0"};
    override_option no_vehicle_bubble{"VEHICLE_BUBBLE_SIZE", "0"};
    override_option no_combat_bubble{"COMBAT_BUBBLE_SIZE", "0"};
};

/// Avatar and calendar state after a wait.
struct wait_end_state {
    int stored_kcal = 0;
    int thirst = 0;
    int fatigue = 0;
    std::map<bodypart_str_id, units::temperature> body_temperature;
    int morale = 0;
    /// Remaining duration per effect, keyed by "effect_id:body_part".
    std::map<std::string, time_duration> effect_durations;
    time_point turn;
    /// Number of game::do_turn calls the wait took.
    int do_turn_calls = 0;
    /// Calendar turns of one tick when the wait ended.
    int tick_turns = 0;
};

/// Clears the game state and sets up @p scene at noon of the first day, 18 C, clear weather.
auto setup_wait_scene(wait_scene scene) -> void;

/// Sets @p weather and @p temperature and keeps them for 3 hours: no weather update replaces them.
auto hold_weather(const weather_type_id& weather, units::temperature temperature) -> void;

/// Waits @p duration through ACT_WAIT and game::do_turn, as the wait menu does, so the skip
/// window runs. Fails with a message when the avatar dies.
auto wait_through_activity(const time_duration& duration)
    -> std::expected<wait_end_state, std::string>;

#endif // CATA_TESTS_ACTIVITY_SKIP_WAIT_HELPERS_H

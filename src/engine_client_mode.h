#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

struct input_event;

namespace engine_client
{

/// One value a mode can be set to: a language-neutral id and its localized label.
struct mode_choice {
    std::string id = {};
    std::string label = {};
    auto operator<=>( const mode_choice & ) const = default; // *NOPAD*
};
/// A setting of the game or the avatar that native keys toggle or cycle, with the value it has now and the values
/// it takes. `set_mode` sets it to a named value, so a client never has to read the current value first.
struct mode_value {
    std::string id = {};
    std::string label = {};
    std::string value = {};
    std::vector<mode_choice> values = {};
    auto operator<=>( const mode_value & ) const = default; // *NOPAD*
};
/// Sets the mode `mode` to the value `value`, both by id.
struct mode_operation {
    std::string mode = {};
    std::string value = {};
};

/// Every mode the avatar can set now, in a stable order, with the values they hold now.
auto engine_modes() -> std::vector<mode_value>;
/// Why `operation` names no mode or no value of one, localized; the success value when it names both.
auto mode_operation_check( const mode_operation &operation ) -> std::expected<void, std::string>;
/// The input the map view reads for a mode operation and carries out (`game::handle_action`); replay records and
/// plays it back like any other input.
auto mode_operation_event( const mode_operation &operation ) -> input_event;
/// The mode operation `event` carries; nullopt for any other input.
auto mode_operation_of( const input_event &event ) -> std::optional<mode_operation>;
/// Sets the mode as the native key does when it lands on that value (same messages and side effects, never an
/// intermediate value); a mode that already holds the value is left alone. False when the check fails.
auto run_mode_operation( const mode_operation &operation ) -> bool;

} // namespace engine_client

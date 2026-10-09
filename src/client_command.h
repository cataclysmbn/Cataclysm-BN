#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "client_interaction_data.h"
#include "input.h"
#include "point.h"

namespace game_client
{

/// A client-neutral input request, resolved against the live interaction context.
struct input_command {
    std::string action;
    std::string key;
    std::vector<std::string> modifiers;
    std::string text;
    std::optional<point> mouse_position;
    std::string mouse_button;
    /// Reject this command if the observed input boundary is no longer current.
    std::optional<std::uint64_t> input_id;
    std::optional<interaction_command> interaction;
};

struct input_action {
    std::string id;
    std::string name;
    std::vector<input_event> bindings;
};

/// Called once at each native or replay input read, including raw-key prompts.
auto begin_input_boundary() -> void;
auto current_input_id() -> std::uint64_t;
auto available_input_actions() -> std::vector<input_action>;
auto resolve_input_command( const input_command &command, point screen_size )
-> std::expected<input_event, std::string>;

} // namespace game_client

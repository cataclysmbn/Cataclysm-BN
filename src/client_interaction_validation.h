#pragma once

#include "client_interaction_data.h"

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace game_client
{

enum class interaction_rejection { stale_boundary, stale_schema, invalid };
struct interaction_validation_error {
    interaction_rejection kind = interaction_rejection::invalid;
    std::string message;
};
struct interaction_validation_request {
    const interaction_command &command;
    std::optional<std::string_view> expected_schema = std::nullopt;
    std::optional<std::string_view> target_space = std::nullopt;
};

/// Capture the live native provider once, then check its identity and native operation rules.
/// Runs synchronously on the game thread; never executes a gameplay callback.
auto resolve_checked_interaction( const interaction_validation_request &request )
-> std::expected<interaction_event, interaction_validation_error>;

} // namespace game_client

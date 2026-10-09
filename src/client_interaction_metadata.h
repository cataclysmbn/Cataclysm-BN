#pragma once

#include "client_interaction.h"

namespace game_client
{

/// Normalize owned metadata without publishing an input boundary or changing live work counters.
auto normalize_interaction_metadata( const input_context &context, interaction_snapshot snapshot )
-> interaction_snapshot;

/// Use the native semantic validator against owned, complete metadata, without a live provider.
/// Selectable is intentionally not synonymous with enabled (the existing native contract).
auto validate_interaction_metadata( const interaction_snapshot &snapshot,
                                    const interaction_event &event )
-> std::expected<void, std::string>;

} // namespace game_client

#pragma once

namespace game_client
{

/// Capabilities and invalidation hooks supplied by the linked display client.
auto has_tiles() -> bool;
/// A protocol client draws the map itself, so it also zooms it itself.
auto has_protocol_client() -> bool;
auto reset_minimap() -> void;
auto on_options_changed() -> void;

} // namespace game_client

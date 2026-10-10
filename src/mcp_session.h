#pragma once

namespace bn::mcp
{

/// Starts the MCP stdio transport and installs the memory client's input provider.
/// Protocol requests are serviced synchronously at the game's input boundaries.
auto start_session() -> void;

/// Publishes the state between two steps of a native auto-move to a subscribed client.
auto publish_step() -> void;

/// Requests input providers to stop waiting. The process may call this during orderly shutdown.
auto request_stop() -> void;

/// Releases the memory client callbacks after the game has finished its normal shutdown.
auto finish_session() -> void;

} // namespace bn::mcp

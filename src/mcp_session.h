#pragma once

namespace bn::mcp
{

/// Starts the MCP stdio transport and installs the memory client's input provider.
/// Protocol requests are serviced synchronously at the game's input boundaries.
auto start_session() -> void;

/// Publishes the state between two steps of a native auto-move to a subscribed client.
auto publish_step() -> void;
/// Publishes what a running activity changed, to a client that negotiated credit delivery.
auto publish_activity() -> void;

/// How long the screen input goes to waits before it moves on by itself (a timed mini-game), 0 when it
/// waits for the client.
auto refresh_wait_ms() -> int;
/// Requests input providers to stop waiting. The process may call this during orderly shutdown.
auto request_stop() -> void;

/// Releases the memory client callbacks after the game has finished its normal shutdown.
auto finish_session() -> void;

} // namespace bn::mcp

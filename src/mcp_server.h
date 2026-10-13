#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "client_command.h"
#include "engine_client_jsonrpc.h"
#include "engine_client_session.h"
#include "loading_ui_client.h"

namespace bn::mcp
{

/// MCP transports the same commands used by other clients.
using key_event = game_client::input_command;

/// A complete, render-independent view of the current game client.
/// `json` must be a JSON object and is embedded in the tool result as structured content.
struct screen_snapshot {
    std::string json;
    std::string text;
};

struct mcp_host {
    /// Capture the complete current screen after all pending redraws are flushed.
    std::function < auto() -> screen_snapshot > observe;
    /// Capture structured player-visible and player-known game state.
    std::function < auto() -> screen_snapshot > state;
    /// Deliver events through the active game input pipeline, then redraw the client.
    std::function < auto( const std::vector<key_event> & ) -> bool > submit;
    /// Reports whether submit has events waiting at the game input boundary.
    std::function < auto() -> bool > has_input;
    /// Waits up to this many milliseconds for a request frame; false when none arrived.
    /// Optional: unset means a timed wait is never cut short.
    std::function < auto( int ) -> bool > wait_input;
    /// Describe the active context and its registered actions/bindings as a JSON object.
    /// This is optional so clients can still operate with raw keys during startup screens.
    std::function < auto() -> std::string > actions;
    /// Describe the current structured interaction, with bounded choice pagination.
    std::function < auto( std::size_t, std::size_t ) -> screen_snapshot > interaction;
    /// Resize the terrain window to this many map cells, as a native window resize would.
    /// Optional: unset means the client's viewport is ignored.
    std::function < auto( int, int ) -> void > resize_viewport;
    /// Production defaults to the process authority; tests may bind an isolated real session.
    engine_client::session *contract_session = nullptr;
    /// Called after the server publishes a boundary, so the other connections can be sent it.
    /// Optional.
    std::function < auto() -> void > published;
};

/// A small MCP stdio server that adapts the game client to the standard JSON-RPC protocol.
/// It intentionally knows nothing about game state or widgets: all gameplay remains behind
/// the host callbacks, so menus and dialogs use exactly the same input path as a human client.
class server
{
    public:
        /// MCP stdio frames are limited to 1 MiB, excluding the terminating newline.
        static constexpr auto default_max_frame_bytes = std::size_t { 1024 * 1024 };

        struct options {
            std::string name = "cataclysm-bright-nights";
            std::string version = "development";
            std::string protocol_version = "2025-11-25";
            std::size_t max_frame_bytes = default_max_frame_bytes;
            /// The wall clock that paces activity publication; unset means the steady clock.
            std::function < auto() -> std::chrono::steady_clock::time_point > now;
            /// A spectator reads the game but never commands it: it publishes nothing, interrupts
            /// nothing and leaves the viewport and tile settings to the controller.
            bool spectator = false;
        };

        explicit server( mcp_host host );
        server( mcp_host host, options opts );

        /// Serve newline-delimited JSON-RPC messages until stdin closes or shutdown is requested.
        auto run( std::istream &in, std::ostream &out, std::ostream &err ) -> int;

        /// Process protocol messages on the game thread until submit has queued input, or, with a
        /// nonzero `refresh_ms`, until that long has passed without a request: it then returns
        /// true with nothing queued, so a screen the engine redraws itself can move on.
        auto pump_until_input( std::istream &in, std::ostream &out, std::ostream &err,
                               int refresh_ms = 0 ) -> bool;

        /// Serve one request frame that is already waiting while an activity runs, without
        /// awaiting input or completing the command. False when input ended or failed.
        auto poll_input( std::istream &in, std::ostream &out, std::ostream &err ) -> bool;

        /// Tell a client that said hello the engine is alive while an activity keeps it from
        /// reaching an input boundary. The caller decides how often.
        auto heartbeat( std::ostream &out ) -> void;

        /// Publish the state between two steps of an auto-move, which reads no input.
        auto publish_step( std::ostream &out ) -> bool;

        /// Publish what a running activity changed, for a client that negotiated credit delivery
        /// and reads as it goes; any other client sees the activity's result at the next boundary.
        /// Publishing a state costs a capture of the world, so the turns in between wait until the
        /// publications have taken a fifth of the time; the next publication carries their facts.
        auto publish_activity( std::ostream &out ) -> bool;

        /// Complete a deferred bn.press response using the latest published screen.
        auto finish_pending( std::ostream &out, std::ostream &err ) -> void;

        /// Reject the remainder of a batch whose next input is invalid in the current context.
        auto reject_pending( std::string error = {} ) -> void;

        /// Interrupt what is outstanding and write the notifications that caused. For shutdown.
        auto finish( std::ostream &out ) -> bool;

        /// Write a `bn.loading` notification now, from the game thread, while the game is busy
        /// loading. Only a client that completed `bn.hello` receives it.
        auto notify_loading( const game_client::loading_progress &progress, std::ostream &out ) -> void;

        /// Write what the session published since this server last wrote, without publishing.
        auto flush_pushed( std::ostream &out ) -> bool;

        /// True when input ended because of an invalid or unreadable stdio frame.
        auto failed() const -> bool;
        /// Why input ended, for the line the engine writes before it exits.
        auto end_reason() const -> std::string;

        /// What credit delivery is holding for a client that has no credits left.
        auto held_frames() const -> std::size_t;
        auto held_bytes() const -> std::size_t;

        struct deferred_response {
            engine_client::jsonrpc::request_id id;
            bool interaction = false;
            std::string error;
        };

    private:
        mcp_host host_;
        options options_;
        bool initialize_requested_ = false;
        bool initialized_ = false;
        bool pump_mode_ = false;
        bool polling_ = false;
        bool eof_ = false;
        bool failed_ = false;
        std::optional<deferred_response> pending_response_;
        engine_client::session &session_;
        engine_client::push_reader push_ = session_.open_push();
        bool hello_ = false;
        /// The BN language id the connection asked its text in; empty: the engine's.
        std::string locale_;
        /// What the last hello negotiated; a 0.1 connection is served as it always was.
        engine_client::wire_version version_ = engine_client::wire_version::v0_1;
        /// Events are pushed from `bn.subscribe` until a resync, `bn.unsubscribe` or `bn.hello`.
        bool subscribed_ = false;
        /// Command stages are pushed from `bn.subscribe` until `bn.unsubscribe` or `bn.hello`: no
        /// snapshot carries them, so a resync does not stop them.
        bool commands_ = false;
        /// Complete notification frames waiting to be written after the current response.
        std::vector<std::string> notifications_;
        /// Pushed frames a client with credit delivery has yet to be paid for, oldest first.
        struct held_frame {
            std::string bytes;
            bool credited = true;
            /// A `bn.command` stage, which a resync or a new snapshot does not replace.
            bool command = false;
            /// Newest event sequence the frame carries.
            engine_client::counter through = 0;
        };
        /// Pushed frames the client can still be sent; set once hello asked for credit delivery.
        std::optional<std::size_t> credits_;
        std::deque<held_frame> held_;
        std::size_t held_bytes_ = 0;
        /// The shortest wall time between two publications of a running activity the client asked for.
        std::chrono::steady_clock::duration activity_period_{};
        /// The earliest time a running activity is published again.
        std::chrono::steady_clock::time_point next_activity_publish_{};
        /// Newest event sequence written to the client since its snapshot.
        engine_client::counter delivered_through_ = 0;
        std::optional<engine_client::jsonrpc::response_frame> deferred_frame_;
        std::optional<engine_client::jsonrpc::envelope_cursor> pending_requests_;

        auto publish_boundary( std::ostream &out, bool settles = true ) -> bool;
        /// Interrupt the open command. A spectator has none, and never ends the controller's.
        auto interrupt( engine_client::error reason ) -> void;
        /// Turn what the session published into notifications and write them, in order.
        auto flush_push( std::ostream &out ) -> bool;
        /// Turn what the session published into notifications written after the current response.
        auto queue_push() -> void;
        auto write_notifications( std::ostream &out ) -> bool;
        /// Write the held frames that cost nothing or that credits pay for, in order.
        auto write_held( std::ostream &out ) -> bool;
        /// Drop the held frames a snapshot replaces: all but the command stages.
        auto drop_held_events() -> void;
        /// Whether the connection reads a language other than the one the engine is in.
        auto foreign_locale() const -> bool;
        auto deliver_input() -> bool;
        auto direct( const engine_client::jsonrpc::request &request ) ->
        std::expected<std::optional<engine_client::jsonrpc::response>, engine_client::jsonrpc::output_error>;
        auto legacy_dispatch( const engine_client::jsonrpc::request &request,
                              std::ostream &err ) ->
        std::expected<std::optional<engine_client::jsonrpc::response>, engine_client::jsonrpc::output_error>;
        auto process_requests( std::ostream &out, std::ostream &err ) -> bool;
        auto dispatch( std::string_view line, std::ostream &out, std::ostream &err ) -> bool;
};

} // namespace bn::mcp

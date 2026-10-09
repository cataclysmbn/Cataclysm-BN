#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "client_command.h"
#include "engine_client_jsonrpc.h"
#include "engine_client_session.h"

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
    /// Describe the active context and its registered actions/bindings as a JSON object.
    /// This is optional so clients can still operate with raw keys during startup screens.
    std::function < auto() -> std::string > actions;
    /// Describe the current structured interaction, with bounded choice pagination.
    std::function < auto( std::size_t, std::size_t ) -> screen_snapshot > interaction;
    /// Production defaults to the process authority; tests may bind an isolated real session.
    engine_client::session *contract_session = nullptr;
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
        };

        explicit server( mcp_host host );
        server( mcp_host host, options opts );

        /// Serve newline-delimited JSON-RPC messages until stdin closes or shutdown is requested.
        auto run( std::istream &in, std::ostream &out, std::ostream &err ) -> int;

        /// Process protocol messages on the game thread until submit has queued input.
        auto pump_until_input( std::istream &in, std::ostream &out, std::ostream &err ) -> bool;

        /// Complete a deferred bn.press response using the latest published screen.
        auto finish_pending( std::ostream &out, std::ostream &err ) -> void;

        /// Reject the remainder of a batch whose next input is invalid in the current context.
        auto reject_pending( std::string error = {} ) -> void;

        /// True when input ended because of an invalid or unreadable stdio frame.
        auto failed() const -> bool;

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
        bool eof_ = false;
        bool failed_ = false;
        std::optional<deferred_response> pending_response_;
        engine_client::session &session_;
        std::optional<engine_client::negotiated_contract> negotiated_;
        engine_client::projection projection_;
        engine_client::event_batch completion_events_;
        std::optional<std::string> completion_command_;
        std::optional<engine_client::jsonrpc::response_frame> deferred_frame_;
        std::optional<engine_client::jsonrpc::envelope_cursor> pending_requests_;

        auto publish_boundary() -> bool;
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

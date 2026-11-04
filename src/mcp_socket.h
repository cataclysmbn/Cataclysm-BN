#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <iosfwd>
#include <streambuf>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "mcp_server.h"

namespace bn::mcp
{

#if defined(_WIN32)
/// A stream buffer that writes `fd`, a pipe, and closes it when destroyed. A write that cannot be
/// completed fails the stream. Elsewhere stdio is served by `socket_hub::attach_stdio`.
auto make_fd_output( int fd ) -> std::unique_ptr<std::streambuf>;
#endif

struct socket_options {
    /// The most bytes of unsent output a connection may hold before it is dropped.
    std::size_t output_bound = std::size_t { 16 * 1024 * 1024 };
};

/// Serves the protocol on a local stream socket (a Unix domain socket; on Windows 10 and later,
/// AF_UNIX) to several connections at once, each with the newline-delimited JSON-RPC framing of
/// stdio and its own `server`: its own hello, credits, held frames and subscription.
///
/// The first connection, and any connection made while there is none, is the controller: it
/// alone may command the game. Every other connection is a spectator and gets the read-only
/// role. A connection that fails or closes is dropped without touching the others.
///
/// Stdio is served the same way, as one controller connection on a pair of pipes: a client that
/// stops reading cannot keep the engine from reading its requests, and one whose unread output
/// passes the bound is dropped as a socket is. Stdio has no next controller, so that ends the
/// session; the caller keeps the game, since the client did not choose to leave.
///
/// Sockets never hold the game: output goes to a bounded per-connection buffer written without
/// blocking, and a connection that lets it fill up is dropped. A spectator's frame never waits; the
/// controller's waits at most 10 ms for its reader to take it, so a frame larger than the pipe or
/// socket buffer reaches a reading client at once and not with the next one. The controller's input
/// is the only read the game waits for, as with stdio; while it waits, the others are served.
class socket_hub
{
    public:
        /// The most connections served at once; further ones are closed at once.
        static constexpr auto maximum_connections = std::size_t { 8 };

        /// Bind `path`, replacing a stale socket file that nothing answers on.
        static auto listen( const std::string &path, const mcp_host &host, socket_options options = {} ) ->
        std::expected<std::unique_ptr<socket_hub>, std::string>;
#if !defined(_WIN32)
        /// Serve the protocol on the descriptors `input` and `output` (stdio's pipes, left in their
        /// blocking mode) as the controller, which never leaves; the hub closes `output` only.
        static auto attach_stdio( int input, int output, const mcp_host &host, socket_options options = {} )
        ->
        std::unique_ptr<socket_hub>;
#endif
        ~socket_hub();
        socket_hub( const socket_hub & ) = delete;
        auto operator=( const socket_hub & ) -> socket_hub & = delete; // *NOPAD*

        /// The controller's server and streams.
        struct endpoint {
            server &protocol;
            std::istream &in;
            std::ostream &out;
        };
        auto controller() -> std::optional<endpoint>;
        /// Whether serving the controller would not wait: a whole frame, or its end, has arrived.
        auto controller_ready() -> bool;
        /// Wait up to `wait_ms` for the controller's input, serving the others meanwhile; false when
        /// none came.
        auto wait_controller_ready( int wait_ms ) -> bool;
        /// Drop the controller after its server ended or failed; its open command is interrupted.
        auto drop_controller() -> void;
        /// Whether the controller's output failed because it left more than the bound unread.
        auto controller_fell_behind() const -> bool;

        /// Accept connections, serve the spectators' frames and send pending output, waiting up to
        /// `wait_ms` (negative: until something happens) for any of that to be possible.
        auto service( int wait_ms ) -> void;
        /// Send what the session published to every connection but the controller.
        auto flush_spectators() -> void;
        /// Tell every connection that said hello the engine is alive.
        auto heartbeat() -> void;
        auto notify_loading( const game_client::loading_progress &progress ) -> void;
        /// Interrupt what is outstanding and write the notifications that caused, for shutdown.
        auto finish() -> void;

        auto connections() const -> std::size_t;
        /// Whether the hub accepts connections (a socket) rather than serves stdio.
        auto listening() const -> bool;

    private:
        struct connection;
        socket_hub( std::intptr_t listener, std::string path, mcp_host host, socket_options options );

        std::intptr_t listener_;
        std::string path_;
        mcp_host host_;
        socket_options options_;
        std::vector<std::unique_ptr<connection>> connections_;

        auto accept_all() -> void;
        auto serve( connection &spectator ) -> void;
        auto reap() -> void;
        auto wait_for_controller_input() -> bool;
};

} // namespace bn::mcp

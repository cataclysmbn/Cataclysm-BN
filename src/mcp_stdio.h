#pragma once

#include <cstddef>
#include <iosfwd>
#include <streambuf>
#include <memory>
#include <optional>

#include "mcp_server.h"

namespace bn::mcp
{

/// The connection that commands the game: its server and streams.
struct controller_endpoint {
    server &protocol;
    std::istream &in;
    std::ostream &out;
};

#if defined(_WIN32)
/// A stream buffer that writes `fd`, a pipe, and closes it when destroyed. A write that cannot be
/// completed fails the stream. Elsewhere stdio is served by `stdio_hub`.
auto make_fd_output( int fd ) -> std::unique_ptr<std::streambuf>;
#else
struct stdio_options {
    /// The most bytes of unsent output the client may leave unread before it is dropped.
    std::size_t output_bound = std::size_t { 16 * 1024 * 1024 };
};

/// Serves the protocol on a pair of pipes, stdio, with the newline-delimited JSON-RPC framing: a
/// client that stops reading cannot keep the engine from reading its requests, and one whose
/// unread output passes the bound is dropped. Stdio has no next client, so that ends the session;
/// the caller keeps the game, since the client did not choose to leave.
///
/// Output goes to a bounded buffer written without blocking. A frame waits at most 10 ms for its
/// reader to take it, so a frame larger than the pipe buffer reaches a reading client at once and
/// not with the next one. The client's input is the only read the game waits for.
class stdio_hub
{
    public:
        /// Serve the protocol on the descriptors `input` and `output` (stdio's pipes, left in their
        /// blocking mode); the hub closes `output` only.
        stdio_hub( int input, int output, const mcp_host &host, stdio_options options = {} );
        ~stdio_hub();
        stdio_hub( const stdio_hub & ) = delete;
        auto operator=( const stdio_hub & ) -> stdio_hub & = delete; // *NOPAD*

        auto controller() -> std::optional<controller_endpoint>;
        /// Whether serving the client would not wait: a whole frame, or its end, has arrived.
        auto controller_ready() -> bool;
        /// Wait up to `wait_ms` for the client's input; false when none came.
        auto wait_controller_ready( int wait_ms ) -> bool;
        /// Whether the output failed because the client left more than the bound unread.
        auto controller_fell_behind() const -> bool;

        /// Send pending output, waiting up to `wait_ms` (negative: until something happens) for the
        /// client's input or for the pipe to take output.
        auto service( int wait_ms ) -> void;
        /// Interrupt what is outstanding and write the notifications that caused, for shutdown.
        auto finish() -> void;

    private:
        struct connection;
        std::unique_ptr<connection> connection_;
};
#endif

} // namespace bn::mcp

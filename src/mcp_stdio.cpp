#include "mcp_stdio.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <climits>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <streambuf>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace bn::mcp
{
#if defined(_WIN32)
namespace
{

/// Writes a pipe, the protocol's stdout, whole and in order. A pipe that is full blocks the write,
/// as on any blocking pipe; only one that failed or whose reader left fails the stream. Windows
/// has no hub for stdio: its pipes cannot be polled.
class fd_output_buffer final : public std::streambuf
{
    public:
        explicit fd_output_buffer( const int fd ) : fd_( fd ) {}
        ~fd_output_buffer() override { _close( fd_ ); }

    protected:
        auto xsputn( const char *data, const std::streamsize size ) -> std::streamsize override {
            auto remaining = static_cast<size_t>( size );
            while( remaining > 0 ) {
                const auto written = _write( fd_, data, static_cast<unsigned int>( remaining ) );
                if( written < 0 && errno == EINTR ) { continue; }
                if( written <= 0 ) { return size - static_cast<std::streamsize>( remaining ); }
                data += written;
                remaining -= static_cast<size_t>( written );
            }
            return size;
        }

        auto overflow( const int_type character ) -> int_type override {
            if( character == traits_type::eof() ) { return 0; }
            const auto byte = static_cast<char>( character );
            return xsputn( &byte, 1 ) == 1 ? character : traits_type::eof();
        }

    private:
        int fd_;
};

} // namespace

auto make_fd_output( const int fd ) -> std::unique_ptr<std::streambuf>
{
    return std::make_unique<fd_output_buffer>( fd );
}
#else
namespace
{

/// How long shutdown waits for a reader to take the output still pending.
constexpr auto final_drain = std::chrono::seconds( 2 );
/// The longest a flush of a frame waits for its reader to take it before the rest is left for later.
constexpr auto frame_wait = std::chrono::milliseconds( 10 );

/// Whether `fd` is ready for `events` now; when it is not, errno says so for `would_block`.
auto ready_now( const int fd, const short events ) -> bool
{
    auto entry = pollfd{ .fd = fd, .events = events, .revents = 0 };
    if( poll( &entry, 1, 0 ) > 0 ) { return true; }
    errno = EAGAIN;
    return false;
}
auto would_block() -> bool { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
/// A pipe may be blocking, and the client's end may be shared with other processes, so it is not
/// made non-blocking: it is read only when readable, and written at most PIPE_BUF bytes, which a
/// writable pipe takes at once.
auto receive( const int fd, char *data, const std::size_t size ) -> std::ptrdiff_t
{
    return ready_now( fd, POLLIN ) ? read( fd, data, size ) : -1;
}
auto transmit( const int fd, const char *data, const std::size_t size ) -> std::ptrdiff_t
{
    return ready_now( fd, POLLOUT ) ? write( fd, data, std::min<std::size_t>( size, PIPE_BUF ) ) : -1;
}

/// Whether `fd` can take more output within `wait`.
auto writable_within( const int fd, const std::chrono::milliseconds wait ) -> bool
{
    auto entry = pollfd{ .fd = fd, .events = POLLOUT, .revents = 0 };
    return poll( &entry, 1, static_cast<int>( wait.count() ) ) > 0;
}

/// Reads a pipe without blocking. The stream waits through `wait`, which sends pending output
/// meanwhile.
class input_buffer final : public std::streambuf
{
    public:
        std::function < auto() -> bool > wait;

        input_buffer( const int fd, const std::size_t max_line ) : fd_( fd ), max_line_( max_line ) {}

        /// Take what the socket has now, up to one frame past the bound.
        auto fill() -> void {
            data_.erase( 0, static_cast<std::size_t>( gptr() - eback() ) );
            auto chunk = std::array<char, 64 * 1024> {};
            while( !ended_ && data_.size() <= max_line_ ) {
                const auto got = receive( fd_, chunk.data(), chunk.size() );
                if( got > 0 ) {
                    data_.append( chunk.data(), static_cast<std::size_t>( got ) );
                    continue;
                }
                ended_ = got == 0 || !would_block();
                break;
            }
            setg( data_.data(), data_.data(), data_.data() + data_.size() );
        }
        auto has_line() const -> bool { return std::memchr( gptr(), '\n', egptr() - gptr() ) != nullptr; }
        /// More than a frame has arrived without ending one.
        auto overlong() const -> bool { return !has_line() && static_cast<std::size_t>( egptr() - gptr() ) > max_line_; }
        auto ended() const -> bool { return ended_; }

    protected:
        auto underflow() -> int_type override {
            while( gptr() == egptr() && !ended_ ) {
                fill();
                if( gptr() == egptr() && ( !wait || !wait() ) ) { break; }
            }
            return gptr() == egptr() ? traits_type::eof() : traits_type::to_int_type( *gptr() );
        }

    private:
        int fd_;
        std::size_t max_line_;
        std::string data_;
        bool ended_ = false;
};

struct output_options {
    int fd;
    /// The most unsent bytes before the stream fails.
    std::size_t bound;
    /// The longest a flush waits for the reader to take what is left; zero never waits.
    std::chrono::milliseconds wait;
};

/// Writes a pipe without blocking: what the pipe does not take stays here, up to a bound. A
/// write that leaves more than the bound unsent, or one after the pipe failed, fails the stream,
/// so the connection ends. A flush waits at most `wait` while the reader takes the rest; a reader
/// that took nothing meanwhile is not waited for again until it takes some.
class output_buffer final : public std::streambuf
{
    public:
        explicit output_buffer( const output_options &options ) :
            fd_( options.fd ), bound_( options.bound ), wait_( options.wait ) {}

        auto drain() -> void {
            while( !broken_ && sent_ < pending_.size() ) {
                const auto wrote = transmit( fd_, pending_.data() + sent_, pending_.size() - sent_ );
                if( wrote > 0 ) {
                    sent_ += static_cast<std::size_t>( wrote );
                    stalled_ = false;
                    continue;
                }
                broken_ = !would_block();
                break;
            }
            if( sent_ == pending_.size() ) {
                pending_.clear();
                sent_ = 0;
            }
        }
        /// What is left to send; a broken connection sends nothing more.
        auto pending() const -> std::size_t { return broken_ ? 0 : pending_.size() - sent_; }
        /// The stream failed because its reader left more than the bound unread, not because the
        /// connection failed.
        auto fell_behind() const -> bool { return behind_; }

    protected:
        auto xsputn( const char *data, const std::streamsize size ) -> std::streamsize override {
            if( broken_ ) { return 0; }
            pending_.append( data, static_cast<std::size_t>( size ) );
            // A frame is judged after the pipe took what it could, so only a reader that leaves
            // more than the bound unread ends the connection, however large one frame is.
            if( pending() > bound_ ) { drain(); }
            behind_ = behind_ || pending() > bound_;
            broken_ = broken_ || behind_;
            return broken_ ? 0 : size;
        }
        auto overflow( const int_type character ) -> int_type override {
            if( character == traits_type::eof() ) { return 0; }
            const auto byte = static_cast<char>( character );
            return xsputn( &byte, 1 ) == 1 ? character : traits_type::eof();
        }
        auto sync() -> int override {
            drain();
            // A frame larger than the pipe buffer reaches a reader that takes it now, not with
            // the next frame, which the game may compute for a long while; a slow reader holds the game no
            // longer than `wait_` per flush.
            const auto until = std::chrono::steady_clock::now() + wait_;
            while( !stalled_ && pending() > 0 ) {
                const auto rest = std::chrono::ceil<std::chrono::milliseconds>( until -
                                  std::chrono::steady_clock::now() );
                if( rest.count() <= 0 ) { break; }
                const auto left = pending();
                if( writable_within( fd_, rest ) ) { drain(); }
                stalled_ = pending() == left;
            }
            return broken_ ? -1 : 0;
        }

    private:
        int fd_;
        std::size_t bound_;
        std::chrono::milliseconds wait_;
        std::string pending_;
        std::size_t sent_ = 0;
        bool broken_ = false;
        bool behind_ = false;
        bool stalled_ = false;
};

} // namespace

struct stdio_hub::connection {
    /// The client's input is the process's own stdin; only `out_fd` is ours to close.
    int in_fd;
    int out_fd;
    input_buffer input;
    output_buffer buffered;
    std::istream in{ &input };
    std::ostream out{ &buffered };
    std::unique_ptr<server> protocol;

    connection( const int input_fd, const int output_fd, const mcp_host &host,
                const stdio_options &options ) :
        in_fd( input_fd ), out_fd( output_fd ),
        input( input_fd, server::default_max_frame_bytes ),
        buffered( { .fd = output_fd, .bound = options.output_bound, .wait = frame_wait } ),
    protocol( std::make_unique<server>( host ) ) {}
    ~connection() { close( out_fd ); }
    connection( const connection & ) = delete;
    auto operator=( const connection & ) -> connection & = delete; // *NOPAD*
};

stdio_hub::stdio_hub( std::unique_ptr<connection> opened ) : connection_( std::move( opened ) ) {}

stdio_hub::~stdio_hub() = default;

auto stdio_hub::attach( const int input, const int output, const mcp_host &host,
                        const stdio_options options ) -> std::unique_ptr<stdio_hub>
{
    auto hub = std::unique_ptr<stdio_hub>( new stdio_hub( std::make_unique<connection>( input, output,
                                           host,
                                           options ) ) );
    hub->connection_->input.wait = [raw = hub.get()] { return raw->wait_for_controller_input(); };
    return hub;
}

auto stdio_hub::controller() -> std::optional<controller_endpoint>
{
    return controller_endpoint{ *connection_->protocol, connection_->in, connection_->out };
}

auto stdio_hub::controller_ready() -> bool
{
    auto &leader = *connection_;
    leader.input.fill();
    return leader.protocol->failed() || leader.input.has_line() || leader.input.ended() ||
           leader.input.overlong();
}

auto stdio_hub::wait_controller_ready( const int wait_ms ) -> bool
{
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds( wait_ms );
    while( !controller_ready() ) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                              until - std::chrono::steady_clock::now() ).count();
        if( left <= 0 ) { return false; }
        service( static_cast<int>( left ) );
    }
    return true;
}

auto stdio_hub::controller_fell_behind() const -> bool { return connection_->buffered.fell_behind(); }

auto stdio_hub::service( const int wait_ms ) -> void
{
    auto &item = *connection_;
    auto entries = std::array{
        pollfd{ .fd = item.in_fd, .events = POLLIN, .revents = 0 },
        // poll skips a negative descriptor.
        pollfd{ .fd = item.buffered.pending() > 0 ? item.out_fd : -1, .events = POLLOUT, .revents = 0 } };
    if( poll( entries.data(), entries.size(), wait_ms ) > 0 && entries[1].revents != 0 ) { item.buffered.drain(); }
}

auto stdio_hub::wait_for_controller_input() -> bool
{
    service( 1000 );
    return true;
}

auto stdio_hub::finish() -> void
{
    auto &item = *connection_;
    static_cast<void>( item.protocol->finish( item.out ) );
    // A reader that is slow gets a moment to take the last frames; one that stalls loses them.
    const auto until = std::chrono::steady_clock::now() + final_drain;
    while( item.buffered.pending() > 0 && std::chrono::steady_clock::now() < until ) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                              until - std::chrono::steady_clock::now() ).count();
        writable_within( item.out_fd, std::chrono::milliseconds( left ) );
        item.buffered.drain();
    }
}
#endif

} // namespace bn::mcp

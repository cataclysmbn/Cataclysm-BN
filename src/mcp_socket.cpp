#include "mcp_socket.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <climits>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <ranges>
#include <streambuf>
#include <utility>

#if defined(_WIN32)
#include <winsock2.h>
#include <afunix.h>
#include <io.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace bn::mcp
{
namespace
{

using handle = std::intptr_t;
using native_socket = decltype( socket( 0, 0, 0 ) );
constexpr auto invalid_handle = handle { -1 };
/// How long shutdown waits for a reader to take the output still pending.
constexpr auto final_drain = std::chrono::seconds( 2 );
/// The longest a flush of the controller's frame waits for its reader to take it before the rest is left
/// for later.
constexpr auto controller_wait = std::chrono::milliseconds( 10 );

#if defined(_WIN32)
using poll_entry = WSAPOLLFD;
auto native( const handle fd ) -> SOCKET { return static_cast<SOCKET>( fd ); }
auto close_handle( const handle fd ) -> void { closesocket( native( fd ) ); }
auto set_nonblocking( const handle fd ) -> bool
{
    auto enabled = u_long{ 1 };
    return ioctlsocket( native( fd ), FIONBIO, &enabled ) == 0;
}
// Only sockets are served on Windows: there is no stdio hub, so `pipe` is never set.
auto receive( const handle fd, char *data, const std::size_t size,
              const bool /*pipe*/ ) -> std::ptrdiff_t
{
    return recv( native( fd ), data, static_cast<int>( size ), 0 );
}
auto transmit( const handle fd, const char *data, const std::size_t size,
               const bool /*pipe*/ ) -> std::ptrdiff_t
{
    return send( native( fd ), data, static_cast<int>( size ), 0 );
}
auto would_block() -> bool { return WSAGetLastError() == WSAEWOULDBLOCK; }
auto wait_for( std::vector<poll_entry> &entries, const int wait_ms ) -> int
{
    return WSAPoll( entries.data(), static_cast<ULONG>( entries.size() ), wait_ms );
}
auto start_sockets() -> bool
{
    static const auto started = []() {
        auto data = WSADATA{};
        return WSAStartup( MAKEWORD( 2, 2 ), &data ) == 0;
    }
    ();
    return started;
}
#else
using poll_entry = pollfd;
auto close_handle( const handle fd ) -> void { close( static_cast<int>( fd ) ); }
auto set_nonblocking( const handle fd ) -> bool
{
    const auto flags = fcntl( static_cast<int>( fd ), F_GETFL );
    return flags >= 0 && fcntl( static_cast<int>( fd ), F_SETFL, flags | O_NONBLOCK ) == 0 &&
           fcntl( static_cast<int>( fd ), F_SETFD, FD_CLOEXEC ) == 0;
}
auto would_block() -> bool { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
/// Whether `fd` is ready for `events` now; when it is not, errno says so for `would_block`.
auto ready_now( const handle fd, const short events ) -> bool
{
    auto entry = pollfd{ .fd = static_cast<int>( fd ), .events = events, .revents = 0 };
    if( poll( &entry, 1, 0 ) > 0 ) { return true; }
    errno = EAGAIN;
    return false;
}
/// A pipe may be blocking, and the client's end may be shared with other processes, so it is not
/// made non-blocking: it is read only when readable, and written at most PIPE_BUF bytes, which a
/// writable pipe takes at once.
auto receive( const handle fd, char *data, const std::size_t size,
              const bool pipe ) -> std::ptrdiff_t
{
    if( !pipe ) { return recv( static_cast<int>( fd ), data, size, 0 ); }
    return ready_now( fd, POLLIN ) ? read( static_cast<int>( fd ), data, size ) : -1;
}
auto transmit( const handle fd, const char *data, const std::size_t size,
               const bool pipe ) -> std::ptrdiff_t
{
    if( pipe ) {
        return ready_now( fd, POLLOUT ) ? write( static_cast<int>( fd ), data, std::min<std::size_t>( size,
                PIPE_BUF ) ) : -1;
    }
#if defined(MSG_NOSIGNAL)
    return send( static_cast<int>( fd ), data, size, MSG_NOSIGNAL );
#else
    return send( static_cast<int>( fd ), data, size, 0 );
#endif
}
auto wait_for( std::vector<poll_entry> &entries, const int wait_ms ) -> int
{
    return poll( entries.data(), entries.size(), wait_ms );
}
auto start_sockets() -> bool { return true; }
#endif

/// Whether `fd` can take more output within `wait`.
auto writable_within( const handle fd, const std::chrono::milliseconds wait ) -> bool
{
    auto entries = std::vector{ poll_entry{ .fd = static_cast<decltype( poll_entry::fd )>( fd ), .events = POLLOUT, .revents = 0 } };
    return wait_for( entries, static_cast<int>( wait.count() ) ) > 0;
}

auto open_stream() -> handle
{
    if( !start_sockets() ) { return invalid_handle; }
    return static_cast<handle>( socket( AF_UNIX, SOCK_STREAM, 0 ) );
}

auto address_of( const std::string &path ) -> sockaddr_un
{
    auto address = sockaddr_un{};
    address.sun_family = AF_UNIX;
    std::ranges::copy( path, address.sun_path );
    return address;
}

/// Whether something accepts connections on `path`.
auto answers( const std::string &path ) -> bool
{
    const auto probe = open_stream();
    if( probe == invalid_handle ) { return false; }
    const auto address = address_of( path );
    const auto connected = connect( static_cast<native_socket>( probe ),
                                    reinterpret_cast<const sockaddr *>( &address ), sizeof( address ) ) == 0;
    close_handle( probe );
    return connected;
}

/// Reads a socket without blocking. The controller's stream waits through `wait`, which serves
/// the other connections meanwhile; a spectator's never waits, since it is read only when a whole
/// frame has arrived.
class input_buffer final : public std::streambuf
{
    public:
        std::function < auto() -> bool > wait;

        input_buffer( const handle fd, const bool pipe, const std::size_t max_line ) :
            fd_( fd ), pipe_( pipe ), max_line_( max_line ) {}

        /// Take what the socket has now, up to one frame past the bound.
        auto fill() -> void {
            data_.erase( 0, static_cast<std::size_t>( gptr() - eback() ) );
            auto chunk = std::array<char, 64 * 1024> {};
            while( !ended_ && data_.size() <= max_line_ ) {
                const auto got = receive( fd_, chunk.data(), chunk.size(), pipe_ );
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
        handle fd_;
        bool pipe_;
        std::size_t max_line_;
        std::string data_;
        bool ended_ = false;
};

struct output_options {
    handle fd;
    bool pipe;
    /// The most unsent bytes before the stream fails.
    std::size_t bound;
    /// The longest a flush waits for the reader to take what is left; zero never waits.
    std::chrono::milliseconds wait;
};

/// Writes a socket without blocking: what the socket does not take stays here, up to a bound. A
/// write that leaves more than the bound unsent, or one after the socket failed, fails the stream,
/// so the connection ends. A flush waits at most `wait` while the reader takes the rest; a reader
/// that took nothing meanwhile is not waited for again until it takes some.
class output_buffer final : public std::streambuf
{
    public:
        explicit output_buffer( const output_options &options ) :
            fd_( options.fd ), pipe_( options.pipe ), bound_( options.bound ), wait_( options.wait ) {}

        auto drain() -> void {
            while( !broken_ && sent_ < pending_.size() ) {
                const auto wrote = transmit( fd_, pending_.data() + sent_, pending_.size() - sent_, pipe_ );
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
            // A frame is judged after the socket took what it could, so only a reader that leaves
            // more than the bound unread ends its connection, however large one frame is.
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
            // A frame larger than the pipe or socket buffer reaches a reader that takes it now, not with
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
        handle fd_;
        bool pipe_;
        std::size_t bound_;
        std::chrono::milliseconds wait_;
        std::string pending_;
        std::size_t sent_ = 0;
        bool broken_ = false;
        bool behind_ = false;
        bool stalled_ = false;
};

#if defined(_WIN32)
/// Writes a pipe, the protocol's stdout, whole and in order. A pipe that is full blocks the write,
/// as on any blocking pipe; only one that failed or whose reader left fails the stream. Windows
/// has no hub for stdio: its pipes cannot be polled with the sockets'.
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
#endif

} // namespace

#if defined(_WIN32)
auto make_fd_output( const int fd ) -> std::unique_ptr<std::streambuf>
{
    return std::make_unique<fd_output_buffer>( fd );
}
#endif

namespace
{
struct connection_options {
    handle in;
    handle out;
    bool controller;
    /// `in` and `out` are pipes, read and written by file descriptor, not sockets.
    bool pipe = false;
};
} // namespace

struct socket_hub::connection {
    handle fd;
    /// Where output goes: `fd`, unless stdio's output is another descriptor than its input.
    handle out_fd;
    bool controller;
    input_buffer input;
    output_buffer buffered;
    std::istream in{ &input };
    std::ostream out{ &buffered };
    std::unique_ptr<server> protocol;
    bool dead = false;

    connection( const connection_options &opened, const mcp_host &host,
                const socket_options &options ) :
        fd( opened.in ), out_fd( opened.out ), controller( opened.controller ),
        input( opened.in, opened.pipe, server::default_max_frame_bytes ),
        buffered( { .fd = opened.out, .pipe = opened.pipe, .bound = options.output_bound,
                    .wait = opened.controller ? controller_wait : std::chrono::milliseconds::zero() } ),
    protocol( std::make_unique<server>( host, server::options{ .spectator = !opened.controller } ) ) {}
    ~connection() {
        // Stdio's input is the process's own stdin; only its output descriptor is ours to close.
        if( out_fd != fd ) { close_handle( out_fd ); }
        else { close_handle( fd ); }
    }
    connection( const connection & ) = delete;
    auto operator=( const connection & ) -> connection & = delete; // *NOPAD*
};

socket_hub::socket_hub( const std::intptr_t listener, std::string path, mcp_host host,
                        socket_options options ) :
    listener_( listener ), path_( std::move( path ) ), host_( std::move( host ) ), options_( options )
{
    host_.published = [this] { flush_spectators(); };
}

socket_hub::~socket_hub()
{
    connections_.clear();
    if( listening() ) {
        close_handle( listener_ );
        auto ignored = std::error_code{};
        std::filesystem::remove( path_, ignored );
    }
}

auto socket_hub::listen( const std::string &path, const mcp_host &host,
                         const socket_options options ) ->
std::expected<std::unique_ptr<socket_hub>, std::string>
{
    auto address = sockaddr_un{};
    if( path.empty() || path.size() >= sizeof( address.sun_path ) ) {
        return std::unexpected( "--listen path must be 1 to " + std::to_string( sizeof(
                                    address.sun_path ) - 1 ) +
                                " bytes" );
    }
    auto ignored = std::error_code{};
    if( std::filesystem::exists( std::filesystem::symlink_status( path, ignored ) ) ) {
        if( answers( path ) ) { return std::unexpected( "--listen path is in use: " + path ); }
#if !defined(_WIN32)
        if( !std::filesystem::is_socket( std::filesystem::symlink_status( path, ignored ) ) ) {
            return std::unexpected( "--listen path exists and is not a socket: " + path );
        }
#endif
        std::filesystem::remove( path, ignored );
    }
    const auto fd = open_stream();
    if( fd == invalid_handle ) { return std::unexpected( "unable to create a socket" ); }
    address = address_of( path );
#if !defined(_WIN32)
    // Whoever connects controls the game: only the user may.
    const auto previous = umask( 0177 );
#endif
    const auto bound = bind( static_cast<native_socket>( fd ),
                             reinterpret_cast<const sockaddr *>( &address ), sizeof( address ) ) == 0;
#if !defined(_WIN32)
    umask( previous );
#endif
    if( !bound || ::listen( static_cast<native_socket>( fd ), 8 ) != 0 || !set_nonblocking( fd ) ) {
        close_handle( fd );
        return std::unexpected( "unable to listen on " + path );
    }
    return std::unique_ptr<socket_hub>( new socket_hub( fd, path, host, options ) );
}

#if !defined(_WIN32)
auto socket_hub::attach_stdio( const int input, const int output, const mcp_host &host,
                               const socket_options options ) -> std::unique_ptr<socket_hub>
{
    auto hub = std::unique_ptr<socket_hub>( new socket_hub( invalid_handle, {}, host, options ) );
    hub->connections_.push_back( std::make_unique<connection>( connection_options{
        .in = input, .out = output, .controller = true, .pipe = true }, hub->host_, hub->options_ ) );
    hub->connections_.back()->input.wait = [raw = hub.get()] { return raw->wait_for_controller_input(); };
    return hub;
}
#endif

auto socket_hub::listening() const -> bool { return listener_ != invalid_handle; }

auto socket_hub::connections() const -> std::size_t { return connections_.size(); }

auto socket_hub::controller() -> std::optional<endpoint>
{
    const auto found = std::ranges::find_if( connections_, &connection::controller );
    if( found == connections_.end() ) { return std::nullopt; }
    return endpoint{ *( *found )->protocol, ( *found )->in, ( *found )->out };
}

auto socket_hub::controller_ready() -> bool
{
    const auto found = std::ranges::find_if( connections_, &connection::controller );
    if( found == connections_.end() ) { return false; }
    auto &leader = **found;
    leader.input.fill();
    return leader.protocol->failed() || leader.input.has_line() || leader.input.ended() ||
           leader.input.overlong();
}

auto socket_hub::wait_controller_ready( const int wait_ms ) -> bool
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

auto socket_hub::controller_fell_behind() const -> bool
{
    const auto found = std::ranges::find_if( connections_, &connection::controller );
    return found != connections_.end() && ( *found )->buffered.fell_behind();
}

auto socket_hub::drop_controller() -> void
{
    const auto found = std::ranges::find_if( connections_, &connection::controller );
    if( found == connections_.end() ) { return; }
    // The command it left open is interrupted; telling the dead socket is best effort.
    static_cast<void>( ( *found )->protocol->finish( ( *found )->out ) );
    std::cerr << "mcp: the controller disconnected: " << ( ( *found )->buffered.fell_behind() ?
              "it left more than " + std::to_string( options_.output_bound ) + " bytes unread" :
              ( *found )->protocol->end_reason() ) << std::endl;
    connections_.erase( found );
}

auto socket_hub::accept_all() -> void
{
    while( true ) {
        const auto fd = static_cast<handle>( accept( static_cast<native_socket>( listener_ ), nullptr,
                                             nullptr ) );
        if( fd == invalid_handle ) { return; }
        if( connections_.size() >= maximum_connections || !set_nonblocking( fd ) ) {
            close_handle( fd );
            continue;
        }
        const auto leads = std::ranges::none_of( connections_, &connection::controller );
        auto &added = *connections_.emplace_back( std::make_unique<connection>( connection_options{
            .in = fd, .out = fd, .controller = leads }, host_, options_ ) );
        // The game waits on the controller's input; the other connections are served meanwhile.
        if( leads ) { added.input.wait = [this] { return wait_for_controller_input(); }; }
    }
}

auto socket_hub::serve( connection &spectator ) -> void
{
    spectator.input.fill();
    while( !spectator.dead && spectator.input.has_line() ) {
        spectator.dead = !spectator.protocol->poll_input( spectator.in, spectator.out, std::cerr );
    }
    if( spectator.input.overlong() ) {
        std::cerr << "mcp: a spectator sent a frame past the limit; closing it" << std::endl;
        spectator.dead = true;
    }
    spectator.dead = spectator.dead || spectator.input.ended() || spectator.protocol->failed();
}

auto socket_hub::reap() -> void
{
    std::erase_if( connections_, []( const auto & item ) { return item->dead && !item->controller; } );
}

auto socket_hub::flush_spectators() -> void
{
    for( const auto &item : connections_ ) {
        if( !item->controller && !item->dead ) {
            item->dead = !item->protocol->flush_pushed( item->out );
        }
    }
}

auto socket_hub::service( const int wait_ms ) -> void
{
    flush_spectators();
    reap();
    auto entries = std::vector<poll_entry> {};
    // Entries: the listener, then each connection's input and output. Where one descriptor does
    // both, the output entry is left empty, and poll skips a negative descriptor.
    const auto entry = []( const handle fd, const short events ) {
        return poll_entry{ .fd = static_cast<decltype( poll_entry::fd )>( fd ), .events = events, .revents = 0 };
    };
    entries.push_back( entry( listener_, POLLIN ) );
    for( const auto &item : connections_ ) {
        const auto shared = item->out_fd == item->fd;
        const auto pending = item->buffered.pending() > 0;
        entries.push_back( entry( item->fd, static_cast<short>( POLLIN | ( shared &&
                                  pending ? POLLOUT : 0 ) ) ) );
        entries.push_back( entry( !shared && pending ? item->out_fd : invalid_handle, POLLOUT ) );
    }
    if( wait_for( entries, wait_ms ) <= 0 ) { return; }
    const auto known = connections_.size();
    if( entries.front().revents != 0 ) { accept_all(); }
    for( const auto index : std::views::iota( std::size_t{ 0 }, known ) ) {
        auto &item = *connections_[index];
        const auto ready = entries[2 * index + 1].revents;
        const auto writable = entries[2 * index + 2].revents;
        if( ( ready & POLLOUT ) != 0 || writable != 0 ) { item.buffered.drain(); }
        if( !item.controller && ( ready & ( POLLIN | POLLHUP | POLLERR ) ) != 0 ) { serve( item ); }
    }
}

auto socket_hub::wait_for_controller_input() -> bool
{
    if( !controller() ) { return false; }
    service( 1000 );
    return true;
}

auto socket_hub::heartbeat() -> void
{
    for( const auto &item : connections_ ) { item->protocol->heartbeat( item->out ); }
}

auto socket_hub::notify_loading( const game_client::loading_progress &progress ) -> void
{
    for( const auto &item : connections_ ) { item->protocol->notify_loading( progress, item->out ); }
}

auto socket_hub::finish() -> void
{
    for( const auto &item : connections_ ) { static_cast<void>( item->protocol->finish( item->out ) ); }
    // A reader that is slow gets a moment to take the last frames; one that stalls loses them.
    const auto until = std::chrono::steady_clock::now() + final_drain;
    while( std::chrono::steady_clock::now() < until ) {
        auto entries = std::vector<poll_entry> {};
        for( const auto &item : connections_ ) {
            if( item->buffered.pending() > 0 ) {
                entries.push_back( { .fd = static_cast<decltype( poll_entry::fd )>( item->out_fd ), .events = POLLOUT, .revents = 0 } );
            }
        }
        if( entries.empty() ) { return; }
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                              until - std::chrono::steady_clock::now() ).count();
        wait_for( entries, static_cast<int>( left ) );
        for( const auto &item : connections_ ) { item->buffered.drain(); }
    }
}

} // namespace bn::mcp

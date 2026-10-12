#include "mcp_socket.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <ranges>
#include <streambuf>
#include <utility>

#if defined(_WIN32)
#include <winsock2.h>
#include <afunix.h>
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

#if defined(_WIN32)
using poll_entry = WSAPOLLFD;
auto native( const handle fd ) -> SOCKET { return static_cast<SOCKET>( fd ); }
auto close_handle( const handle fd ) -> void { closesocket( native( fd ) ); }
auto set_nonblocking( const handle fd ) -> bool
{
    auto enabled = u_long{ 1 };
    return ioctlsocket( native( fd ), FIONBIO, &enabled ) == 0;
}
auto receive( const handle fd, char *data, const std::size_t size ) -> std::ptrdiff_t
{
    return recv( native( fd ), data, static_cast<int>( size ), 0 );
}
auto transmit( const handle fd, const char *data, const std::size_t size ) -> std::ptrdiff_t
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
auto receive( const handle fd, char *data, const std::size_t size ) -> std::ptrdiff_t
{
    return recv( static_cast<int>( fd ), data, size, 0 );
}
auto transmit( const handle fd, const char *data, const std::size_t size ) -> std::ptrdiff_t
{
#if defined(MSG_NOSIGNAL)
    return send( static_cast<int>( fd ), data, size, MSG_NOSIGNAL );
#else
    return send( static_cast<int>( fd ), data, size, 0 );
#endif
}
auto would_block() -> bool { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
auto wait_for( std::vector<poll_entry> &entries, const int wait_ms ) -> int
{
    return poll( entries.data(), entries.size(), wait_ms );
}
auto start_sockets() -> bool { return true; }
#endif

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

        input_buffer( const handle fd, const std::size_t max_line ) : fd_( fd ), max_line_( max_line ) {}

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
        handle fd_;
        std::size_t max_line_;
        std::string data_;
        bool ended_ = false;
};

/// Writes a socket without waiting: what the socket does not take stays here, up to a bound. A
/// write that leaves more than the bound unsent, or one after the socket failed, fails the stream,
/// so the connection ends.
class output_buffer final : public std::streambuf
{
    public:
        output_buffer( const handle fd, const std::size_t bound ) : fd_( fd ), bound_( bound ) {}

        auto drain() -> void {
            while( !broken_ && sent_ < pending_.size() ) {
                const auto wrote = transmit( fd_, pending_.data() + sent_, pending_.size() - sent_ );
                if( wrote > 0 ) {
                    sent_ += static_cast<std::size_t>( wrote );
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
        auto pending() const -> std::size_t { return pending_.size() - sent_; }

    protected:
        auto xsputn( const char *data, const std::streamsize size ) -> std::streamsize override {
            if( broken_ ) { return 0; }
            pending_.append( data, static_cast<std::size_t>( size ) );
            // A frame is judged after the socket took what it could, so only a reader that leaves
            // more than the bound unread ends its connection, however large one frame is.
            if( pending() > bound_ ) { drain(); }
            broken_ = broken_ || pending() > bound_;
            return broken_ ? 0 : size;
        }
        auto overflow( const int_type character ) -> int_type override {
            if( character == traits_type::eof() ) { return 0; }
            const auto byte = static_cast<char>( character );
            return xsputn( &byte, 1 ) == 1 ? character : traits_type::eof();
        }
        auto sync() -> int override {
            drain();
            return broken_ ? -1 : 0;
        }

    private:
        handle fd_;
        std::size_t bound_;
        std::string pending_;
        std::size_t sent_ = 0;
        bool broken_ = false;
};

} // namespace

struct socket_hub::connection {
    handle fd;
    bool controller;
    input_buffer input;
    output_buffer buffered;
    std::istream in{ &input };
    std::ostream out{ &buffered };
    std::unique_ptr<server> protocol;
    bool dead = false;

    connection( const handle socket_fd, const bool is_controller, const mcp_host &host,
                const socket_options &options ) :
        fd( socket_fd ), controller( is_controller ), input( socket_fd, server::default_max_frame_bytes ),
        buffered( socket_fd, options.output_bound ),
        protocol( std::make_unique<server>( host, server::options{ .spectator = !is_controller } ) ) {}
    ~connection() { close_handle( fd ); }
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
    close_handle( listener_ );
    auto ignored = std::error_code{};
    std::filesystem::remove( path_, ignored );
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

auto socket_hub::drop_controller() -> void
{
    const auto found = std::ranges::find_if( connections_, &connection::controller );
    if( found == connections_.end() ) { return; }
    // The command it left open is interrupted; telling the dead socket is best effort.
    static_cast<void>( ( *found )->protocol->finish( ( *found )->out ) );
    std::cerr << "mcp: the controller disconnected: " << ( *found )->protocol->end_reason() <<
              std::endl;
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
        auto &added = *connections_.emplace_back( std::make_unique<connection>( fd, leads, host_,
                      options_ ) );
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
    entries.push_back( { .fd = static_cast<decltype( poll_entry::fd )>( listener_ ), .events = POLLIN, .revents = 0 } );
    for( const auto &item : connections_ ) {
        entries.push_back( { .fd = static_cast<decltype( poll_entry::fd )>( item->fd ),
                             .events = static_cast<short>( POLLIN | ( item->buffered.pending() > 0 ? POLLOUT : 0 ) ),
                             .revents = 0 } );
    }
    if( wait_for( entries, wait_ms ) <= 0 ) { return; }
    const auto known = connections_.size();
    if( entries.front().revents != 0 ) { accept_all(); }
    for( const auto index : std::views::iota( std::size_t{ 0 }, known ) ) {
        auto &item = *connections_[index];
        const auto ready = entries[index + 1].revents;
        if( ( ready & POLLOUT ) != 0 ) { item.buffered.drain(); }
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
}

} // namespace bn::mcp

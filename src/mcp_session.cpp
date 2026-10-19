#include "mcp_session.h"
#include "game_observation.h"

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <cstdio>
#include <deque>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>

#if defined(_WIN32)
#include <io.h>
#include "platform_win.h"
#else
#include <unistd.h>
#endif

#include "catacharset.h"
#include "client_command.h"
#include "client_input.h"
#include "client_interaction.h"
#include "client_memory.h"
#include "client_snapshot.h"
#include "engine_client_component.h"
#include "engine_client_contract.h"
#include "game_ui.h"
#include "input.h"
#include "client_backend.h"
#include "game.h"
#include "game_session.h"
#include "mcp_server.h"
#include "mcp_socket.h"
#include "json.h"
#include "output.h"
#include "runtime_handlers.h"

namespace bn::mcp
{
namespace
{

constexpr size_t max_pending_events = 256;

struct session_state {
    std::deque<key_event> events;
    screen_snapshot screen{ .json = "{\"width\":0,\"height\":0,\"cells\":[]}" };
    std::unique_ptr<server> transport;
    std::unique_ptr<std::streambuf> output_buffer;
    std::unique_ptr<std::ostream> output;
    /// Serves the protocol on a socket instead of stdio; then `transport` and `output` are unset.
    std::unique_ptr<socket_hub> sockets;
    /// The game has told the spectators where it waits for a controller to connect.
    bool published_unled = false;
    bool started = false;
    bool stop = false;
};

auto state() -> session_state &
{
    static auto result = session_state{};
    return result;
}

auto close_fd( const int fd ) -> void
{
#if defined(_WIN32)
    _close( fd );
#else
    close( fd );
#endif
}

auto duplicate_stdout() -> int
{
#if defined(_WIN32)
    return _dup( _fileno( stdout ) );
#else
    return dup( fileno( stdout ) );
#endif
}

auto redirect_stdout_to_stderr() -> bool
{
#if defined(_WIN32)
    return _dup2( _fileno( stderr ), _fileno( stdout ) ) == 0;
#else
    return dup2( fileno( stderr ), fileno( stdout ) ) >= 0;
#endif
}

auto serialize_actions() -> std::string
{
    const auto active = game_client::active_input_context();
    auto output = std::ostringstream{};
    auto json = JsonOut( output );
    json.start_object();
    json.member( "input_id", game_client::current_input_id() );
    json.member( "category", std::string( active.category ) );
    json.member( "timeout_ms", active.timeout_ms );
    json.member( "actions" );
    json.start_array();
    for( const auto &action : game_client::available_input_actions() ) {
        json.start_object();
        json.member( "id", action.id );
        json.member( "name", action.name );
        json.member( "bindings" );
        json.start_array();
        for( const auto &binding : action.bindings ) {
            json.start_object();
            json.member( "input_method", binding.type == input_event_t::keyboard ? "keyboard" : "other" );
            json.member( "sequence" );
            json.start_array();
            for( const auto key : binding.sequence ) {
                json.write( inp_mngr.get_keyname( key, binding.type, true ) );
            }
            json.end_array();
            json.end_object();
        }
        json.end_array();
        json.end_object();
    }
    json.end_array();
    json.end_object();
    return output.str();
}

auto has_input() -> bool
{
    return !state().events.empty();
}

auto submit_events( const std::vector<key_event> &requests ) -> bool
{
    if( requests.empty() || requests.size() > max_pending_events ||
        state().events.size() + requests.size() > max_pending_events ) {
        return false;
    }
    for( const auto &request : requests ) {
        state().events.push_back( request );
    }
    return true;
}

auto observe() -> screen_snapshot
{
    return state().screen;
}

auto actions() -> std::string
{
    return serialize_actions();
}

auto structured_state() -> screen_snapshot
{
    const auto current = game_observation::capture();
    return { .json = current.json, .text = current.text };
}

auto resize_viewport( const int cols, const int rows ) -> void
{
    game_ui::resize_terrain_window( { cols, rows } );
}

auto interaction( const std::size_t offset, const std::size_t limit ) -> screen_snapshot
{
    const auto current = game_client::current_interaction( { .offset = offset, .limit = limit } );
    const auto text = !current.message.empty() ? current.message : current.title;
    return { .json = game_client::serialize_interaction( current ), .text = text };
}

constexpr auto heartbeat_interval = std::chrono::seconds( 2 );

#if defined(_WIN32)
/// Whether a request frame can be read, waiting up to `wait_ms` for one.
auto stdin_ready( const int wait_ms = 0 ) -> bool
{
    // Only a pipe (or a redirected file) can be tested; a console would need its own event queue.
    const auto ready = []() {
        const auto handle = GetStdHandle( STD_INPUT_HANDLE );
        const auto type = GetFileType( handle );
        if( std::cin.rdbuf()->in_avail() > 0 || type == FILE_TYPE_DISK ) {
            return true;
        }
        if( type != FILE_TYPE_PIPE ) {
            return false;
        }
        auto available = DWORD{ 0 };
        // A closed pipe fails the peek; report it ready so the read observes the end of input.
        return !PeekNamedPipe( handle, nullptr, 0, nullptr, &available, nullptr ) || available > 0;
    };
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds( wait_ms );
    while( !ready() && std::chrono::steady_clock::now() < until ) {
        Sleep( 5 );
    }
    return ready();
}
#endif

#if !defined(_WIN32)
/// Says which signal ended the engine, then dies of it like any other process.
extern "C" auto name_the_signal( const int sig ) -> void
{
    const auto text = sig == SIGTERM ? "mcp: the engine is exiting: received SIGTERM\n" :
                      "mcp: the engine is exiting: received SIGINT\n";
    static_cast<void>( write( STDERR_FILENO, text, strlen( text ) ) );
    std::signal( sig, SIG_DFL );
    std::raise( sig );
}
#endif

/// Leaves the process with the reason on stderr, which the client shows on its error screen.
[[noreturn]] auto leave( const std::string &why ) -> void
{
    auto &session = state();
    const auto leader = session.sockets ? session.sockets->controller() : std::nullopt;
    const auto protocol = leader ? &leader->protocol : session.transport.get();
    std::cerr << "mcp: the engine is exiting: "
              << ( session.stop ? "a stop was requested" : protocol ? protocol->end_reason() : why ) << std::endl;
    exit_handler( protocol && protocol->failed() ? 1 : 0 );
}

/// The connection that commands the game: stdio, or the socket that holds the controller role.
auto controller() -> std::optional<socket_hub::endpoint>
{
    auto &session = state();
    if( session.sockets ) { return session.sockets->controller(); }
#if defined(_WIN32)
    if( session.transport && session.output ) {
        return socket_hub::endpoint{ *session.transport, std::cin, *session.output };
    }
#endif
    return std::nullopt;
}

auto controller_ready() -> bool
{
#if defined(_WIN32)
    return state().sockets ? state().sockets->controller_ready() : stdin_ready();
#else
    return state().sockets->controller_ready();
#endif
}

/// Saves the game being played, as the native save and quit does. Saving may happen inside a
/// native screen or a running activity, as the Android quicksave on losing focus does.
auto save_game() -> void
{
    // A failed save shows a popup, whose input wait ends here again through the lost controller.
    static auto saving = false;
    if( std::exchange( saving, true ) || !game_session::running() || g->uquit != QUIT_NO ) { return; }
    std::cerr << ( g->save( true ) ? "mcp: the game was saved" : "mcp: the game could not be saved" ) <<
              std::endl;
}

/// The controller is gone: a socket's game waits for the next one, stdio's ends with it. A stdio
/// client dropped for falling behind did not choose to leave, so its game is saved first.
auto lose_controller( const std::string &why ) -> void
{
    auto &session = state();
    if( session.sockets && session.sockets->listening() ) {
        session.sockets->drop_controller();
        return;
    }
    if( session.sockets && session.sockets->controller_fell_behind() ) {
        std::cerr << "mcp: the client left more output unread than the engine keeps" << std::endl;
        save_game();
    }
    leave( why );
}

/// Without a controller the game waits at its input boundary, as it does for a player who has not
/// pressed a key: the spectators are told where it stands, and a new connection takes the role.
auto wait_for_controller() -> void
{
    auto &session = state();
    if( !std::exchange( session.published_unled, true ) ) {
        static_cast<void>( engine_client::process_session().publish_boundary() );
        session.sockets->flush_spectators();
    }
    session.sockets->service( 1000 );
}

/// Answers the requests that already wait while an activity runs, which reaches no input boundary.
auto serve_waiting_requests() -> void
{
    auto &session = state();
    // The heartbeat tells the client the activity is alive.
    static auto last_beat = std::chrono::steady_clock::time_point{};
    const auto now = std::chrono::steady_clock::now();
    if( now - last_beat >= heartbeat_interval ) {
        last_beat = now;
        if( session.sockets ) {
            session.sockets->heartbeat();
        } else if( session.transport ) {
            session.transport->heartbeat( *session.output );
        }
    }
    // A socket's controller may leave mid-activity: the activity runs on, and the game waits
    // for a controller at its next input boundary.
    if( session.sockets ) { session.sockets->service( 0 ); }
    while( session.events.empty() && controller() && controller_ready() ) {
        const auto leader = controller();
        if( leader->protocol.poll_input( leader->in, leader->out, std::cerr ) ) { continue; }
        lose_controller( "input ended while an activity was running" );
    }
}

auto provide_input( const int timeout_ms ) -> input_event
{
    auto &session = state();
    // A screen the engine redraws itself (a timed mini-game) moves on when the client sends nothing
    // within the native wait, as the native input wait would.
    const auto refreshing = timeout_ms > 0 && refresh_wait_ms() > 0;
    while( !session.stop ) {
        if( !session.events.empty() ) {
            const auto request = std::move( session.events.front() );
            session.events.pop_front();
            if( const auto event = game_client::resolve_input_command( request,
                                   game_client::memory::screen_size() ) ) {
                return *event;
            } else {
                session.events.clear();
                if( const auto leader = controller() ) { leader->protocol.reject_pending( event.error() ); }
            }
        }
        // A zero-timeout poll is a running activity checking for interruption, not input being
        // awaited: the command stays open and the activity continues to its end. Requests that
        // are already waiting are served, so the client can interrupt it.
        if( timeout_ms == 0 ) {
            serve_waiting_requests();
            if( !session.events.empty() ) {
                continue;
            }
            auto polled = input_event{};
            polled.type = input_event_t::timeout;
            return polled;
        }
        const auto leader = controller();
        if( !leader && session.sockets ) {
            wait_for_controller();
            continue;
        }
        session.published_unled = false;
        if( !leader || !leader->protocol.pump_until_input( leader->in, leader->out, std::cerr,
                refreshing ? timeout_ms : 0 ) ) {
            if( !leader ) { break; }
            lose_controller( "input ended" );
            continue;
        }
        if( refreshing && session.events.empty() ) {
            auto lapsed = input_event{};
            lapsed.type = input_event_t::timeout;
            return lapsed;
        }
    }
    leave( "no transport" );
}

auto report_loading( const game_client::loading_progress &progress ) -> void
{
    auto &session = state();
    if( session.sockets ) {
        session.sockets->service( 0 );
        session.sockets->notify_loading( progress );
    } else if( session.transport && session.output ) {
        session.transport->notify_loading( progress, *session.output );
    }
}

auto serialize_screen( const game_client::screen_snapshot &screen ) -> screen_snapshot
{
    auto output = std::ostringstream{};
    auto json = JsonOut( output );
    json.start_object();
    json.member( "width", screen.width );
    json.member( "height", screen.height );
    json.member( "cursor" );
    json.start_object();
    json.member( "x", screen.cursor.x );
    json.member( "y", screen.cursor.y );
    json.member( "visible", screen.cursor_visible );
    json.end_object();
    json.member( "rows" );
    json.start_array();
    for( int y = 0; y < screen.height; ++y ) {
        auto row = std::string{};
        for( int x = 0; x < screen.width; ++x ) {
            row += screen.cells[static_cast<size_t>( y * screen.width + x )].text;
        }
        json.write( row );
    }
    json.end_array();
    json.member( "styles" );
    json.start_array();
    for( int y = 0; y < screen.height; ++y ) {
        auto run_start = 0;
        while( run_start < screen.width ) {
            const auto &first = screen.cells[static_cast<size_t>( y * screen.width + run_start )];
            auto run_length = 1;
            while( run_start + run_length < screen.width ) {
                const auto &next = screen.cells[
                                       static_cast<size_t>( y * screen.width + run_start + run_length )];
                if( next.foreground != first.foreground || next.background != first.background ) {
                    break;
                }
                ++run_length;
            }
            json.start_array();
            json.write( y * screen.width + run_start );
            json.write( run_length );
            json.write( first.foreground );
            json.write( first.background );
            json.end_array();
            run_start += run_length;
        }
    }
    json.end_array();
    json.end_object();
    return {.json = output.str(), .text = screen.text};
}

auto publish_screen( const game_client::screen_snapshot &screen ) -> void
{
    state().screen = serialize_screen( screen );
}

} // namespace

auto refresh_wait_ms() -> int
{
    const auto boundary = engine_client::capture_boundary( { .ready = { .phase = "waiting_for_input" } } );
    return boundary ? engine_client::top_screen( *boundary ).refresh_ms : 0;
}

auto start_session() -> void
{
    auto &session = state();
    if( session.started ) {
        return;
    }
#if !defined(_WIN32)
    // A consumer that closes the pipe must surface as a write error, not terminate the game.
    std::signal( SIGPIPE, SIG_IGN );
    // SDL would turn SIGTERM and SIGINT into a quit event that ends the engine as if its client had
    // closed stdin; ours name the signal, from the first moment, even while the game loads.
    setenv( "SDL_NO_SIGNAL_HANDLERS", "1", 1 );
    std::signal( SIGTERM, name_the_signal );
    std::signal( SIGINT, name_the_signal );
#endif
    const auto protocol_fd = duplicate_stdout();
    if( protocol_fd < 0 || !redirect_stdout_to_stderr() ) {
        if( protocol_fd >= 0 ) {
            close_fd( protocol_fd );
        }
        throw std::runtime_error( "Unable to isolate MCP stdio" );
    }
    auto host = mcp_host{
        .observe = observe,
        .state = structured_state,
        .submit = submit_events,
        .has_input = has_input,
        .wait_input = []( const int wait_ms ) { return state().sockets->wait_controller_ready( wait_ms ); },
        .actions = actions,
        .interaction = interaction,
        .resize_viewport = resize_viewport
    };
    if( const auto &path = game_client::listen_endpoint(); !path.empty() ) {
        close_fd( protocol_fd );
        auto listening = socket_hub::listen( path, host );
        if( !listening ) { throw std::runtime_error( listening.error() ); }
        session.sockets = std::move( *listening );
    } else {
#if defined(_WIN32)
        host.wait_input = []( const int wait_ms ) { return stdin_ready( wait_ms ); };
        session.output_buffer = make_fd_output( protocol_fd );
        session.output = std::make_unique<std::ostream>( session.output_buffer.get() );
        session.transport = std::make_unique<server>( std::move( host ) );
#else
        session.sockets = socket_hub::attach_stdio( STDIN_FILENO, protocol_fd, host );
#endif
    }
    session.started = true;
    game_client::memory::set_input_provider( provide_input );
    game_client::memory::set_present_callback( publish_screen );
    game_client::set_loading_observer( report_loading );
}

auto publish_step() -> void
{
    if( const auto leader = controller() ) { static_cast<void>( leader->protocol.publish_step( leader->out ) ); }
}

auto publish_activity() -> void
{
    // The native game reads keys only ten times a second during an activity; a client's requests
    // are answered after every step of it.
    serve_waiting_requests();
    if( const auto leader = controller() ) {
        static_cast<void>( leader->protocol.publish_activity( leader->out ) );
    }
}

auto request_stop() -> void
{
    state().stop = true;
}

auto finish_session() -> void
{
    auto &session = state();
    if( !session.started ) {
        return;
    }
    if( const auto leader = controller() ) {
        leader->protocol.finish_pending( leader->out, std::cerr );
        static_cast<void>( leader->protocol.finish( leader->out ) );
        leader->out.flush();
    }
    if( session.sockets ) { session.sockets->finish(); }
    request_stop();
    game_client::memory::set_input_provider( {} );
    game_client::memory::set_present_callback( {} );
    game_client::set_loading_observer( nullptr );
    session.output.reset();
    session.output_buffer.reset();
    session.transport.reset();
    session.sockets.reset();
    session.events.clear();
    session.started = false;
}

} // namespace bn::mcp

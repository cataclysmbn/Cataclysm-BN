#include "mcp_session.h"
#include "game_observation.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
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
#else
#include <unistd.h>
#endif

#include "catacharset.h"
#include "client_command.h"
#include "client_input.h"
#include "client_interaction.h"
#include "client_memory.h"
#include "client_snapshot.h"
#include "input.h"
#include "mcp_server.h"
#include "json.h"
#include "output.h"
#include "runtime_handlers.h"

namespace bn::mcp
{
namespace
{

constexpr size_t max_pending_events = 256;

auto close_fd( int fd ) -> void;

struct fd_output_buffer final : std::streambuf {
        explicit fd_output_buffer( const int fd ) : fd_( fd ) {}
        ~fd_output_buffer() override { close_fd( fd_ ); }

    protected:
        auto xsputn( const char *data, const std::streamsize size ) -> std::streamsize override {
            auto remaining = static_cast<size_t>( size );
            while( remaining > 0 ) {
#if defined(_WIN32)
                const auto written = _write( fd_, data, static_cast<unsigned int>( remaining ) );
#else
                const auto written = ::write( fd_, data, remaining );
#endif
                if( written < 0 && errno == EINTR ) {
                    continue;
                }
                if( written <= 0 ) {
                    return size - static_cast<std::streamsize>( remaining );
                }
                data += written;
                remaining -= static_cast<size_t>( written );
            }
            return size;
        }

        auto overflow( const int_type character ) -> int_type override {
            if( character == traits_type::eof() ) {
                return 0;
            }
            const auto byte = static_cast<char>( character );
            return xsputn( &byte, 1 ) == 1 ? character : traits_type::eof();
        }

    private:
        int fd_;
};

struct session_state {
    std::deque<key_event> events;
    std::string actions_json = "{\"actions\":[]}";
    screen_snapshot screen{ .json = "{\"width\":0,\"height\":0,\"cells\":[]}" };
    std::unique_ptr<server> transport;
    std::unique_ptr<fd_output_buffer> output_buffer;
    std::unique_ptr<std::ostream> output;
    bool started = false;
    bool stop = false;
    bool closed = false;
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

auto update_actions() -> void
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
    state().actions_json = output.str();
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
    return state().actions_json;
}

auto structured_state() -> screen_snapshot
{
    const auto current = game_observation::capture();
    return { .json = current.json, .text = current.text };
}

auto interaction( const std::size_t offset, const std::size_t limit ) -> screen_snapshot
{
    const auto current = game_client::current_interaction( { .offset = offset, .limit = limit } );
    const auto text = !current.message.empty() ? current.message : current.title;
    return { .json = game_client::serialize_interaction( current ), .text = text };
}

auto provide_input( const int /*timeout_ms*/ ) -> input_event
{
    auto &session = state();
    while( !session.stop ) {
        update_actions();
        if( !session.events.empty() ) {
            const auto request = std::move( session.events.front() );
            session.events.pop_front();
            if( const auto event = game_client::resolve_input_command( request,
                                   game_client::memory::screen_size() ) ) {
                return *event;
            } else {
                session.events.clear();
                session.transport->reject_pending( event.error() );
            }
        }
        if( !session.transport ||
            !session.transport->pump_until_input( std::cin, *session.output, std::cerr ) ) {
            session.closed = true;
            break;
        }
    }
    exit_handler( session.transport && session.transport->failed() ? 1 : 0 );
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

auto start_session() -> void
{
    auto &session = state();
    if( session.started ) {
        return;
    }
#if !defined(_WIN32)
    // A consumer that closes the pipe must surface as a write error, not terminate the game.
    std::signal( SIGPIPE, SIG_IGN );
#endif
    const auto protocol_fd = duplicate_stdout();
    if( protocol_fd < 0 || !redirect_stdout_to_stderr() ) {
        if( protocol_fd >= 0 ) {
            close_fd( protocol_fd );
        }
        throw std::runtime_error( "Unable to isolate MCP stdio" );
    }
    session.output_buffer = std::make_unique<fd_output_buffer>( protocol_fd );
    session.output = std::make_unique<std::ostream>( session.output_buffer.get() );
    session.transport = std::make_unique<server>( mcp_host{
        .observe = observe,
        .state = structured_state,
        .submit = submit_events,
        .has_input = has_input,
        .actions = actions,
        .interaction = interaction
    } );
    session.started = true;
    game_client::memory::set_input_provider( provide_input );
    game_client::memory::set_present_callback( publish_screen );
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
    if( session.transport && session.output ) {
        session.transport->finish_pending( *session.output, std::cerr );
        session.output->flush();
    }
    request_stop();
    game_client::memory::set_input_provider( {} );
    game_client::memory::set_present_callback( {} );
    session.output.reset();
    session.output_buffer.reset();
    session.transport.reset();
    session.events.clear();
    session.started = false;
}

auto should_stop() -> bool
{
    return state().stop;
}

} // namespace bn::mcp

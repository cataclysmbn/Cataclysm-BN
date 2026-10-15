#include "mcp_server.h"
#include "engine_client_component.h"
#include "engine_client_schema.h"
#include "engine_client_world.h"

#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "json.h"
#include "client_memory.h"
#include "game_constants.h"
#include "language.h"
#include "locale_scope.h"
#include "overmap/overmap_ui.h"
#include <ranges>

namespace bn::mcp
{
namespace
{

namespace rpc = engine_client::jsonrpc;
using frame_status = rpc::read_status;

auto frame_error( const frame_status status, const std::size_t max_bytes ) -> std::string
{
    if( status == frame_status::too_large ) {
        return "MCP: stdio frame exceeds " + std::to_string( std::min( max_bytes,
                rpc::maximum_frame_bytes ) ) +
               " bytes; closing connection";
    }
    if( status == frame_status::partial_eof ) {
        return "MCP: unterminated stdio frame; closing connection";
    }
    return "MCP: failed to read stdio frame; closing connection";
}

auto write_tool_schema( JsonOut &json, const std::string &name,
                        const std::string &description ) -> void
{
    json.start_object();
    json.member( "name", name );
    json.member( "description", description );
    json.member( "inputSchema" );
    json.start_object();
    json.member( "type", "object" );
    if( name == "bn.press" ) {
        json.member( "properties" );
        json.start_object();
        json.member( "keys" );
        json.start_object();
        json.member( "type", "array" );
        json.member( "minItems", 1 );
        json.member( "maxItems", 256 );
        json.member( "description", "Logical keys delivered in order through the active input context." );
        json.member( "items" );
        json.start_object();
        json.member( "type", "object" );
        json.member( "properties" );
        json.start_object();
        json.member( "input_id" );
        json.start_object();
        json.member( "type", "integer" );
        json.member( "minimum", 0 );
        json.member( "description", "Input boundary from bn.actions; stale commands are rejected." );
        json.end_object();
        json.member( "key" );
        json.start_object();
        json.member( "type", "string" );
        json.member( "description",
                     "Key name, character, named key such as ENTER or ESCAPE, TIMEOUT, or IDLE. IDLE is accepted only at a nonblocking input boundary." );
        json.end_object();
        json.member( "action" );
        json.start_object();
        json.member( "type", "string" );
        json.member( "description", "Registered action id in the active input context; the host resolves "
                     "its binding." );
        json.end_object();
        json.member( "modifiers" );
        json.start_object();
        json.member( "type", "array" );
        json.member( "items" );
        json.start_object();
        json.member( "type", "string" );
        json.end_object();
        json.end_object();
        json.member( "text" );
        json.start_object();
        json.member( "type", "string" );
        json.member( "description",
                     "UTF-8 text input; it may be supplied without key for text-only entry." );
        json.end_object();
        json.member( "mouse" );
        json.start_object();
        json.member( "type", "object" );
        json.member( "properties" );
        json.start_object();
        json.member( "x" );
        json.start_object();
        json.member( "type", "integer" );
        json.end_object();
        json.member( "y" );
        json.start_object();
        json.member( "type", "integer" );
        json.end_object();
        json.member( "button" );
        json.start_object();
        json.member( "type", "string" );
        json.member( "enum" );
        json.start_array();
        json.write( "left" );
        json.write( "right" );
        json.write( "scroll_up" );
        json.write( "scroll_down" );
        json.write( "move" );
        json.end_array();
        json.end_object();
        json.end_object();
        json.member( "required" );
        json.start_array();
        json.write( "x" );
        json.write( "y" );
        json.write( "button" );
        json.end_array();
        json.end_object();
        json.end_object();
        json.member( "required" );
        json.start_array();
        json.end_array();
        json.end_object();
        json.end_object();
        json.end_object();
        json.member( "required" );
        json.start_array();
        json.write( "keys" );
        json.end_array();
    } else if( name == "bn.interaction" ) {
        json.member( "properties" );
        json.start_object();
        for( const auto &property : { std::string( "offset" ), std::string( "limit" ) } ) {
            json.member( property );
            json.start_object();
            json.member( "type", "integer" );
            json.member( "minimum", 0 );
            json.end_object();
        }
        json.end_object();
    } else if( name == "bn.interact" ) {
        json.member( "properties" );
        json.start_object();
        json.member( "input_id" );
        json.start_object();
        json.member( "type", "integer" );
        json.member( "minimum", 0 );
        json.end_object();
        json.member( "operation" );
        json.start_object();
        json.member( "type", "string" );
        json.member( "enum" );
        json.start_array();
        json.write( "choose" );
        json.write( "fill" );
        json.write( "set_count" );
        json.write( "set_target" );
        json.write( "cancel" );
        json.end_array();
        json.end_object();
        for( const auto &property : {
                 std::string( "choice_id" ), std::string( "field_id" ),
                 std::string( "candidate_id" ), std::string( "value" )
             } ) {
            json.member( property );
            json.start_object();
            json.member( "type", "string" );
            json.end_object();
        }
        json.member( "submit" );
        json.start_object();
        json.member( "type", "boolean" );
        json.end_object();
        json.member( "count" );
        json.start_object();
        json.member( "type", "integer" );
        json.member( "minimum", 0 );
        json.end_object();
        json.member( "position" );
        json.start_object();
        json.member( "type", "object" );
        json.member( "properties" );
        json.start_object();
        for( const auto &coordinate : { std::string( "x" ), std::string( "y" ), std::string( "z" ) } ) {
            json.member( coordinate );
            json.start_object();
            json.member( "type", "integer" );
            json.end_object();
        }
        json.end_object();
        json.member( "required" );
        json.start_array();
        json.write( "x" );
        json.write( "y" );
        json.write( "z" );
        json.end_array();
        json.end_object();
        json.end_object();
        json.member( "required" );
        json.start_array();
        json.write( "input_id" );
        json.write( "operation" );
        json.end_array();
    }
    json.end_object();
    json.end_object();
}

auto write_tools( JsonOut &json ) -> void
{
    json.member( "tools" );
    json.start_array();
    write_tool_schema( json, "bn.observe",
                       "Capture the complete current BN client screen, including menus, dialogs, map, sidebar, "
                       "and messages." );
    write_tool_schema( json, "bn.state",
                       "Capture structured player-visible and player-known BN state, including avatar needs, "
                       "equipment, inventory, visible entities, items, and known overmap terrain." );
    write_tool_schema( json, "bn.press",
                       "Deliver logical keyboard events through the active BN input context and return the "
                       "resulting screen." );
    write_tool_schema( json, "bn.actions",
                       "List the active BN input context, polling timeout, registered actions, and exact key bindings." );
    write_tool_schema( json, "bn.interaction",
                       "Describe the live client-neutral choices or field at the current input boundary." );
    write_tool_schema( json, "bn.interact",
                       "Apply one semantic choose, fill, or cancel operation and return the next interaction." );
    json.end_array();
}

auto read_nonnegative_integer( const JsonObject &object, const std::string &name ) -> std::uint64_t
{
    if( !object.has_member( name ) ) {
        object.throw_error( name + " is required" );
    }
    auto *const value = object.get_raw( name );
    if( !value->test_number() ) {
        object.throw_error( name + " must be an integer" );
    }
    const auto start = value->tell();
    value->skip_number();
    if( value->substr( start, value->tell() - start ).find_first_of( ".eE" ) != std::string::npos ) {
        object.throw_error( name + " must be an integer" );
    }
    const auto result = object.get_member( name ).get_int64();
    if( result < 0 ) {
        object.throw_error( name + " must not be negative" );
    }
    return static_cast<std::uint64_t>( result );
}

auto read_integer( const JsonObject &object, const std::string &name ) -> int
{
    if( !object.has_member( name ) ) {
        object.throw_error( name + " is required" );
    }
    auto *const value = object.get_raw( name );
    if( !value->test_number() ) {
        object.throw_error( name + " must be an integer" );
    }
    const auto start = value->tell();
    value->skip_number();
    if( value->substr( start, value->tell() - start ).find_first_of( ".eE" ) != std::string::npos ) {
        object.throw_error( name + " must be an integer" );
    }
    const auto result = object.get_member( name ).get_int64();
    if( result < std::numeric_limits<int>::min() || result > std::numeric_limits<int>::max() ) {
        object.throw_error( name + " is outside the supported coordinate range" );
    }
    return static_cast<int>( result );
}

auto read_keys( const JsonObject &arguments ) -> std::vector<key_event>
{
    if( !arguments.has_member( "keys" ) ) {
        arguments.throw_error( "bn.press requires a keys array" );
    }
    auto result = std::vector<key_event> {};
    for( const JsonObject event : arguments.get_array( "keys" ) ) {
        event.allow_omitted_members();
        auto key = key_event{ .action = event.get_string( "action", "" ), .key = event.get_string( "key", "" ) };
        key.text = event.get_string( "text", "" );
        if( event.has_member( "input_id" ) ) {
            key.input_id = read_nonnegative_integer( event, "input_id" );
        }
        if( event.has_member( "mouse" ) ) {
            const auto mouse = event.get_object( "mouse" );
            mouse.allow_omitted_members();
            key.mouse_position = point{ read_integer( mouse, "x" ), read_integer( mouse, "y" ) };
            key.mouse_button = mouse.get_string( "button" );
            if( key.mouse_position->x < 0 || key.mouse_position->y < 0 ) {
                event.throw_error( "Mouse coordinates must be non-negative" );
            }
            if( key.mouse_button != "left" && key.mouse_button != "right" &&
                key.mouse_button != "scroll_up" && key.mouse_button != "scroll_down" &&
                key.mouse_button != "move" ) {
                event.throw_error( "Unknown mouse button" );
            }
        }
        const auto has_mouse = key.mouse_position.has_value();
        if( ( key.action.empty() && key.key.empty() && key.text.empty() && !has_mouse ) ||
            ( !key.action.empty() && ( !key.key.empty() || !key.text.empty() ) ) ||
            ( has_mouse && ( !key.action.empty() || !key.key.empty() || !key.text.empty() ) ) ) {
            event.throw_error( "Provide exactly one input mode: action, key with optional text, text, or mouse" );
        }
        key.modifiers = event.get_string_array( "modifiers" );
        if( !key.modifiers.empty() && key.key.empty() ) {
            event.throw_error( "Modifiers require a key event" );
        }
        result.push_back( std::move( key ) );
    }
    return result;
}

auto read_interaction_command( const JsonObject &arguments ) -> key_event
{
    arguments.allow_omitted_members();
    const auto operation_name = arguments.get_string( "operation" );
    const auto operation = game_client::parse_interaction_operation( operation_name );
    if( !operation ) {
        arguments.throw_error( "operation must be choose, fill, set_count, set_target, or cancel" );
    }
    auto semantic = game_client::interaction_command{
        .input_id = read_nonnegative_integer( arguments, "input_id" ),
        .operation = *operation,
    };
    if( *operation == game_client::interaction_operation::choose ) {
        semantic.target_id = arguments.get_string( "choice_id" );
        if( semantic.target_id.empty() || arguments.has_member( "field_id" ) ||
            arguments.has_member( "candidate_id" ) || arguments.has_member( "value" ) ||
            arguments.has_member( "submit" ) || arguments.has_member( "count" ) ||
            arguments.has_member( "position" ) ) {
            arguments.throw_error( "choose requires only input_id, operation, and choice_id" );
        }
    } else if( *operation == game_client::interaction_operation::fill ) {
        if( !arguments.has_member( "value" ) || !arguments.has_member( "submit" ) ) {
            arguments.throw_error( "fill requires field_id, value, and explicit submit" );
        }
        semantic.target_id = arguments.get_string( "field_id" );
        semantic.value = arguments.get_string( "value" );
        semantic.submit = arguments.get_bool( "submit" );
        if( semantic.target_id.empty() || arguments.has_member( "choice_id" ) ||
            arguments.has_member( "candidate_id" ) || arguments.has_member( "count" ) ||
            arguments.has_member( "position" ) ) {
            arguments.throw_error( "fill requires only input_id, operation, field_id, value, and submit" );
        }
    } else if( *operation == game_client::interaction_operation::set_count ) {
        semantic.target_id = arguments.get_string( "choice_id" );
        semantic.count = read_nonnegative_integer( arguments, "count" );
        if( semantic.target_id.empty() || arguments.has_member( "field_id" ) ||
            arguments.has_member( "candidate_id" ) || arguments.has_member( "value" ) ||
            arguments.has_member( "submit" ) || arguments.has_member( "position" ) ) {
            arguments.throw_error( "set_count requires only input_id, operation, choice_id, and count" );
        }
    } else if( *operation == game_client::interaction_operation::set_target ) {
        if( !arguments.has_member( "position" ) ) {
            arguments.throw_error( "set_target requires position" );
        }
        semantic.target_id = arguments.get_string( "candidate_id", "" );
        auto position = arguments.get_object( "position" );
        semantic.position = game_client::interaction_position{
            .x = read_integer( position, "x" ),
            .y = read_integer( position, "y" ),
            .z = read_integer( position, "z" ),
        };
        position.finish();
        if( arguments.has_member( "choice_id" ) || arguments.has_member( "field_id" ) ||
            arguments.has_member( "value" ) || arguments.has_member( "submit" ) ||
            arguments.has_member( "count" ) ) {
            arguments.throw_error( "set_target requires only input_id, operation, position, and optional candidate_id" );
        }
    } else if( arguments.has_member( "choice_id" ) || arguments.has_member( "field_id" ) ||
               arguments.has_member( "candidate_id" ) || arguments.has_member( "value" ) ||
               arguments.has_member( "submit" ) || arguments.has_member( "count" ) ||
               arguments.has_member( "position" ) ) {
        arguments.throw_error( "cancel does not accept an operation payload" );
    }
    auto result = key_event{};
    result.interaction = std::move( semantic );
    return result;
}

auto write_tool_result( JsonOut &json, const screen_snapshot &screen,
const std::string &error = {} ) -> void {
    json.member( "content" );
    json.start_array();
    json.start_object();
    json.member( "type", "text" );
    json.member( "text", screen.text.empty() ? screen.json : screen.text );
    json.end_object();
    if( !error.empty() )
    {
        json.start_object();
        json.member( "type", "text" );
        json.member( "text", error );
        json.end_object();
    }
    json.end_array();
    json.member( "structuredContent" );
    auto *stream = json.get_stream();
    *stream << screen.json;
    json.set_need_separator();
    if( !error.empty() )
    {
        json.member( "isError", true );
    }
}

} // namespace

server::server( mcp_host host ) : server( std::move( host ), {} )
{
}

server::server( mcp_host host, options opts ) : host_( std::move( host ) ),
    options_( std::move( opts ) ),
    session_( host_.contract_session ? * host_.contract_session : engine_client::process_session() )
{
    if( !host_.observe || !host_.submit ) {
        throw std::invalid_argument( "MCP host must provide observe and submit callbacks" );
    }
    if( options_.max_frame_bytes == 0 ) {
        throw std::invalid_argument( "MCP maximum frame size must be positive" );
    }
}

auto server::run( std::istream &in, std::ostream &out, std::ostream &err ) -> int
{
    if( !publish_boundary( out ) ) { failed_ = true; return 1; }
    while( true ) {
        auto frame = rpc::read_frame( in, options_.max_frame_bytes );
        if( frame.status == frame_status::eof ) {
            eof_ = true;
            return 0;
        }
        if( frame.status != frame_status::complete ) {
            failed_ = true;
            eof_ = true;
            err << frame_error( frame.status, options_.max_frame_bytes ) << '\n';
            return 1;
        }
        if( !dispatch( frame.bytes, out, err ) ) {
            failed_ = true;
            return 1;
        }
    }
}

auto server::pump_until_input( std::istream &in, std::ostream &out, std::ostream &err,
                               const int refresh_ms ) -> bool
{
    if( failed_ || eof_ ) { return false; }
    if( host_.has_input && host_.has_input() ) { return true; }
    if( !publish_boundary( out ) ) { failed_ = true; eof_ = true; return false; }
    pump_mode_ = true;
    finish_pending( out, err );
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( refresh_ms );
    while( !failed_ ) {
        if( pending_requests_ && !pending_response_ && !process_requests( out, err ) ) {
            failed_ = true;
            break;
        }
        // A direct command is not queued until its complete bounded receipt frame is written.
        if( session_.has_received() && !deferred_frame_ ) {
            // A rejected command hands over to the one that waited behind it: go on until one is queued
            // or none is left, for no request may arrive to move the queue.
            auto queued = false;
            while( !queued && session_.has_received() ) { queued = deliver_input(); }
            if( !flush_push( out ) ) { failed_ = true; break; }
        }
        if( host_.has_input && host_.has_input() ) {
            pump_mode_ = false;
            return true;
        }
        if( walk_ && !pending_response_ && step_walk() ) {
            pump_mode_ = false;
            return true;
        }
        if( pending_response_ ) {
            finish_pending( out, err );
            continue;
        }
        if( refresh_ms > 0 && host_.wait_input ) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  deadline - std::chrono::steady_clock::now() ).count();
            if( left <= 0 || !host_.wait_input( static_cast<int>( left ) ) ) {
                pump_mode_ = false;
                return true;
            }
        }
        const auto frame = rpc::read_frame( in, options_.max_frame_bytes );
        if( frame.status == frame_status::eof ) { break; }
        if( frame.status != frame_status::complete ) {
            failed_ = true;
            err << frame_error( frame.status, options_.max_frame_bytes ) << '\n';
            break;
        }
        if( !dispatch( frame.bytes, out, err ) ) { failed_ = true; }
    }
    pump_mode_ = false;
    eof_ = true;
    static_cast<void>( finish( out ) );
    return false;
}

auto server::poll_input( std::istream &in, std::ostream &out, std::ostream &err ) -> bool
{
    if( failed_ || eof_ ) { return false; }
    polling_ = true;
    const auto frame = rpc::read_frame( in, options_.max_frame_bytes );
    if( frame.status == frame_status::eof ) {
        eof_ = true;
    } else if( frame.status != frame_status::complete ) {
        failed_ = true;
        err << frame_error( frame.status, options_.max_frame_bytes ) << '\n';
    } else if( !dispatch( frame.bytes, out, err ) ) {
        failed_ = true;
    }
    polling_ = false;
    return !failed_ && !eof_;
}

auto server::publish_step( std::ostream &out ) -> bool
{
    if( failed_ || eof_ ) { return false; }
    // The walk goes on after this step: its command completes at the boundary where input is awaited.
    if( !publish_boundary( out, false ) ) { failed_ = true; eof_ = true; return false; }
    return true;
}

auto server::publish_activity( std::ostream &out ) -> bool
{
    if( failed_ || eof_ ) { return false; }
    if( !credits_ ) { return true; }
    const auto now = options_.now ? options_.now : []() { return std::chrono::steady_clock::now(); };
    const auto began = now();
    if( began < next_activity_publish_ ) { return true; }
    if( !publish_boundary( out, false ) ) { failed_ = true; eof_ = true; return false; }
    // Publishing may take a fifth of the activity's time, so a long one costs a client little more than none;
    // a client that capped its frame rate gets no more than that, whichever is sparser.
    const auto ended = now();
    next_activity_publish_ = std::max( ended + 4 * ( ended - began ), began + activity_period_ );
    return true;
}

auto server::finish( std::ostream &out ) -> bool
{
    interrupt( engine_client::error::not_ready );
    return flush_push( out );
}

auto server::notify_loading( const game_client::loading_progress &progress,
                             std::ostream &out ) -> void
{
    if( !hello_ || failed_ ) { return; }
    notifications_.push_back( "{\"jsonrpc\":\"2.0\",\"method\":\"bn.loading\",\"params\":" +
                              engine_client::serialize_loading( session_.epoch(), progress ) + "}" );
    if( !write_notifications( out ) ) { failed_ = true; }
}

auto server::heartbeat( std::ostream &out ) -> void
{
    if( !hello_ || failed_ ) { return; }
    notifications_.push_back( "{\"jsonrpc\":\"2.0\",\"method\":\"bn.progress\",\"params\":{\"epoch\":\""
                              +
                              session_.epoch() + "\"}}" );
    if( !write_notifications( out ) ) { failed_ = true; }
}

auto server::held_frames() const -> std::size_t { return held_.size(); }

auto server::held_bytes() const -> std::size_t { return held_bytes_; }

auto server::failed() const -> bool
{
    return failed_;
}

auto server::end_reason() const -> std::string
{
    if( failed_ ) { return "an invalid or unwritable protocol frame"; }
    if( eof_ ) { return "the client closed its end of stdin"; }
    return "input ended without a reason";
}

auto server::reject_pending( std::string error ) -> void
{
    interrupt( engine_client::error::validation_failed );
    if( pending_response_ ) {
        pending_response_->error = error.empty() ? "Input was rejected in the active context" :
                                   std::move( error );
    }
}

auto server::finish_pending( std::ostream &out, std::ostream &err ) -> void
{
    namespace rpc = engine_client::jsonrpc;
    if( !pending_response_ ) { return; }
    const auto pending = std::exchange( pending_response_, std::nullopt );
    auto value = std::ostringstream{};
    auto json = JsonOut( value );
    json.start_object();
    const auto snapshot = pending->interaction && host_.interaction ? host_.interaction( 0, 100 ) :
                          host_.observe();
    write_tool_result( json, snapshot, pending->error );
    json.end_object();
    const auto request = rpc::request{ .id = pending->id, .method = "tools/call" };
    const auto response = rpc::make_result( request, value.str() );
    if( !response || !deferred_frame_ || !deferred_frame_->append( *response ) ) {
        failed_ = true;
        interrupt( engine_client::error::not_ready );
        return;
    }
    // Shutdown may finish an already-terminal frame, but must not dispatch later members.
    if( pending_requests_ && pending_requests_->remaining() == 0 && !process_requests( out, err ) ) {
        failed_ = true;
    }
}

auto server::interrupt( const engine_client::error reason ) -> void
{
    if( !options_.spectator ) { session_.interrupt( reason ); }
}

auto server::publish_boundary( std::ostream &out, const bool settles ) -> bool
{
    if( !options_.spectator ) {
        if( !session_.publish_boundary( settles ) ) { interrupt( engine_client::error::not_ready ); return false; }
        if( host_.published ) { host_.published(); }
    }
    return flush_push( out );
}

auto server::flush_pushed( std::ostream &out ) -> bool
{
    if( failed_ ) { return false; }
    failed_ = !flush_push( out );
    return !failed_;
}

auto server::write_notifications( std::ostream &out ) -> bool
{
    for( const auto &frame : std::exchange( notifications_, {} ) ) {
        if( !rpc::write_frame( out, frame ) ) { return false; }
    }
    if( !write_held( out ) ) { return false; }
    if( held_.size() <= engine_client::maximum_held_frames &&
        held_bytes_ <= engine_client::maximum_held_bytes ) { return true; }
    // A client without credits does not get an ever longer queue: it starts over from a snapshot.
    auto resync = "{\"jsonrpc\":\"2.0\",\"method\":\"bn.resync\",\"params\":" +
                  engine_client::serialize_resync( session_.epoch(), "backpressure", delivered_through_ ) + "}";
    drop_held_events();
    // Only a client that left more stages unpaid than the bounds hold loses the oldest of them.
    while( !held_.empty() && ( held_.size() >= engine_client::maximum_held_frames ||
                               held_bytes_ + resync.size() > engine_client::maximum_held_bytes ) ) {
        held_bytes_ -= held_.front().bytes.size();
        held_.pop_front();
    }
    subscribed_ = false;
    held_bytes_ += resync.size();
    // The resync costs nothing, so it goes ahead of the stages that wait for credits.
    held_.push_front( { .bytes = std::move( resync ), .credited = false } );
    return write_held( out );
}

auto server::write_held( std::ostream &out ) -> bool
{
    while( !held_.empty() && ( !held_.front().credited || credits_.value_or( 0 ) > 0 ) ) {
        const auto &next = held_.front();
        if( !rpc::write_frame( out, next.bytes ) ) { return false; }
        if( next.credited ) { --*credits_; }
        delivered_through_ = std::max( delivered_through_, next.through );
        held_bytes_ -= next.bytes.size();
        held_.pop_front();
    }
    return true;
}

auto server::drop_held_events() -> void
{
    using namespace std::views;
    std::erase_if( held_, []( const auto & frame ) { return !frame.command; } );
    const auto size = []( const auto & frame ) { return frame.bytes.size(); };
    held_bytes_ = std::ranges::fold_left( held_ | transform( size ), std::size_t{ 0 }, std::plus<> {} );
}

auto server::flush_push( std::ostream &out ) -> bool
{
    queue_push();
    return write_notifications( out );
}

auto server::queue_push() -> void
{
    using namespace engine_client;
    // Without credit delivery a frame is written at once; with it, when the client can be paid.
    const auto notify = [this]( const std::string_view method, const std::string & params,
    const counter through = 0 ) {
        auto frame = "{\"jsonrpc\":\"2.0\",\"method\":\"" + std::string{method} +
                     "\",\"params\":" + params + "}";
        if( !credits_ ) { notifications_.push_back( std::move( frame ) ); return; }
        held_bytes_ += frame.size();
        held_.push_back( { .bytes = std::move( frame ), .credited = method != "bn.resync",
                           .command = method == "bn.command", .through = through } );
    };
    auto batch = event_batch{};
    auto batch_bytes = std::size_t{0};
    /// The params of the batch's first event, which are the whole batch's while it holds no other.
    auto first_params = std::string{};
    const auto flush_batch = [&]() {
        if( batch.events.empty() ) { return; }
        const auto through = batch.events.back().value().sequence;
        notify( "bn.events", batch.events.size() == 1 ? first_params : serialize_events( batch ), through );
        batch.events.clear();
        first_params.clear();
        batch_bytes = 0;
    };
    // A resync ends the events that follow it, not the command stages: no snapshot carries them.
    for( auto &item : push_.take() ) {
        if( !( std::holds_alternative<command_result>( item ) ? commands_ : subscribed_ ) ) { continue; }
        if( const auto *pushed = std::get_if<event_push>( &item ) ) {
            // A line in the engine's language says so to a connection that reads another.
            auto marked = std::optional<public_event> {};
            if( foreign_locale() && pushed->event.value().message ) {
                auto value = pushed->event.value();
                value.message->lang = get_language().id;
                marked.emplace( std::move( value ) );
            }
            const auto &event = marked ? *marked : pushed->event;
            // Cheap for an event the stream sealed: its serialized value is taken as it is.
            auto params = serialize_events( {.epoch = pushed->epoch, .events = {event}} );
            const auto bytes = params.size();
            if( batch.epoch != pushed->epoch || batch_bytes + bytes > maximum_inline_bytes ) { flush_batch(); }
            batch.epoch = pushed->epoch;
            if( batch.events.empty() ) { first_params = std::move( params ); }
            batch.events.push_back( event );
            batch_bytes += bytes;
            continue;
        }
        flush_batch();
        if( const auto *command = std::get_if<command_result>( &item ) ) {
            notify( "bn.command", serialize_command_result( *command ) );
        } else {
            const auto &notice = std::get<resync_notice>( item );
            notify( "bn.resync", serialize_resync( notice.epoch, notice.reason, notice.lost_after ) );
            subscribed_ = false;
        }
    }
    flush_batch();
}

auto server::foreign_locale() const -> bool { return !locale_.empty() && locale_ != get_language().id; }

auto server::step_walk() -> bool
{
    const auto state = session_.boundary();
    if( !state || engine_client::top_screen( *state ).screen_id != walk_->screen_id ) { walk_.reset(); return false; }
    if( walk_->taken && ( walk_->released || *walk_->taken == session_.standing() ) ) { walk_.reset(); return false; }
    // A request the client has already sent is read before the next step: it may be the release.
    if( walk_->taken && host_.wait_input && host_.wait_input( 0 ) ) { return false; }
    walk_->taken = session_.standing();
    if( host_.submit( { key_event{ .action = walk_->action, .input_id = game_client::current_input_id() } } ) ) { return true; }
    walk_.reset();
    return false;
}

auto server::deliver_input() -> bool
{
    const auto input = session_.prepare_input( game_client::memory::screen_size() );
    if( !input ) { return false; }
    if( !host_.submit( {*input} ) ) { interrupt( engine_client::error::not_ready ); return false; }
    return true;
}

auto server::direct( const engine_client::jsonrpc::request &request ) ->
std::expected<std::optional<engine_client::jsonrpc::response>, engine_client::jsonrpc::output_error>
{
    using namespace engine_client;
    namespace rpc = jsonrpc;
    // Direct notifications are deliberately effect-free, including hello and reads.
    if( !request.id.json ) { return std::nullopt; }
    const auto fail = [&]( const error reason, std::optional<schema_hashes> schemas = std::nullopt ) {
        auto data = application_error{ .kind = reason, .at = session_.at(), .schemas = std::move( schemas ) };
        switch( reason ) {
            case error::negotiation_failed:
                data.action = required_action::hello;
                break;
            case error::stale_epoch:
            case error::stale_revision:
            case error::stale_boundary:
            case error::stale_interaction_schema:
            case error::resync_required:
                data.action = required_action::subscribe;
                break;
            case error::not_ready:
                data.action = required_action::retry;
                break;
            default:
                break;
        }
        const auto encoded = serialize_application_error( data );
        if( !encoded ) { return rpc::make_error( { .id = request.id } ); }
        return rpc::make_error( { .id = request.id, .code = rpc::error_code::application_error,
                                  .data = *encoded } );
    };
    const auto invalid = [&]() {
        return rpc::make_error( { .id = request.id, .code = rpc::error_code::invalid_params } );
    };
    const auto params = request.params ? std::string_view{*request.params} :
                        std::string_view{};
    if( request.method == "bn.hello" ) {
        const auto decoded = decode_hello_request( params );
        if( !decoded ) { return invalid(); }
        if( !std::ranges::contains( decoded->versions, contract_version ) ) {
            return fail( error::negotiation_failed );
        }
        // The decoder requires the schema of a hello that offers the version.
        if( *decoded->schema != engine_schema_hash ) {
            return fail( error::negotiation_failed, schema_hashes{ .engine = engine_schema_hash, .client = *decoded->schema } );
        }
        // The language is the connection's own to ask for, and the engine must be able to word it.
        if( decoded->locale && !localization::locale_available( *decoded->locale ) ) { return fail( error::negotiation_failed ); }
        // A legacy batch still waiting for native delivery cannot transfer authority mid-flight.
        if( !options_.spectator && ( pending_response_ || ( host_.has_input && host_.has_input() ) ) ) {
            return fail( error::command_busy );
        }
        hello_ = true;
        walk_.reset();
        locale_ = decoded->locale.value_or( std::string{} );
        push_.use_locale( locale_ );
        // At an input boundary the live game is what was published; during an activity a subscribe publishes one.
        if( !polling_ ) { session_.prepare_locale( locale_ ); }
        credits_ = decoded->credits;
        // A new negotiation drops what was held for the old one, so its stream restarts at a snapshot.
        subscribed_ = false;
        commands_ = false;
        activity_period_ = decoded->max_hz ?
                           std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                               std::chrono::nanoseconds{ std::chrono::seconds{ 1 } } / static_cast<std::int64_t>
                               ( *decoded->max_hz ) ) :
                           std::chrono::steady_clock::duration{};
        held_.clear();
        held_bytes_ = 0;
        // The controller decides how the game is drawn for everyone.
        if( !options_.spectator ) {
            game_client::set_overmap_tiles( decoded->tiles );
            engine_client::world::set_tint_ids( { decoded->tint_ids.begin(), decoded->tint_ids.end() } );
            if( decoded->view && host_.resize_viewport ) {
                host_.resize_viewport( decoded->view->cols, decoded->view->rows );
            }
        }
        const auto grant = credits_ ? std::optional{ delivery_grant{ .credits = *credits_, .max_hz = decoded->max_hz } } :
                           std::nullopt;
        auto reply = hello_reply{ .epoch = session_.epoch(), .engine = describe_engine(), .delivery = grant };
        reply.identity = { .schema = engine_schema_hash, .locale = locale_.empty() ? reply.engine.locale : locale_,
                           .engine_locale = reply.engine.locale,
                           .role = options_.spectator ? "spectator" : "controller"
                         };
        return rpc::make_result( request, serialize_hello( reply ) );
    }
    if( !hello_ ) { return fail( error::negotiation_failed ); }
    if( options_.spectator && ( request.method == "bn.viewport" || request.method == "bn.interrupt" ||
                                request.method == "bn.command.submit" || request.method == "bn.hold" ||
                                request.method == "bn.release" ) ) {
        return fail( error::read_only );
    }
    if( request.method == "bn.viewport" ) {
        const auto decoded = decode_viewport_request( params );
        if( !decoded ) { return invalid(); }
        if( host_.resize_viewport ) { host_.resize_viewport( decoded->cols, decoded->rows ); }
        // The new view is part of what the client reads, so it is published like any other change.
        // A running activity is no boundary: its command is not settled by a resize.
        if( const auto published = session_.publish_boundary( !polling_ ); !published ) { return fail( published.error() ); }
        if( host_.published ) { host_.published(); }
        queue_push();
        return rpc::make_result( request, "{}" );
    }
    if( request.method == "bn.interrupt" ) {
        if( !decode_empty_request( params ) ) { return invalid(); }
        // Only an activity that is running can be interrupted; otherwise input is awaited and the
        // client answers it with a command.
        if( !polling_ ) { return fail( error::not_ready ); }
        // The native interrupt key; the activity asks for confirmation as in Tiles.
        auto key = game_client::input_command{};
        key.action = "pause";
        if( !host_.submit( {key} ) ) { return fail( error::not_ready ); }
        return rpc::make_result( request, "{}" );
    }
    if( request.method == "bn.subscribe" ) {
        if( !decode_empty_request( params ) ) { return invalid(); }
        auto current = session_.current( locale_ );
        // During an activity no publication has worded the language yet: publish one, as a resize does.
        if( !current && polling_ && foreign_locale() && session_.publish_boundary( false ) ) {
            if( host_.published ) { host_.published(); }
            queue_push();
            current = session_.current( locale_ );
        }
        if( !current ) { return fail( current.error() ); }
        if( foreign_locale() ) { for( auto &line : current->log ) { line.lang = get_language().id; } }
        const auto wire = serialize_snapshot( *current );
        if( !wire ) { return fail( wire.error() ); }
        // The parts follow the response frame, before any event after `current->at`.
        for( const auto &part : wire->parts ) {
            notifications_.push_back( "{\"jsonrpc\":\"2.0\",\"method\":\"bn.snapshot.part\",\"params\":" +
                                      part + "}" );
        }
        // The snapshot supersedes the events still waiting for credits, not the command stages.
        drop_held_events();
        delivered_through_ = current->at.sequence;
        subscribed_ = true;
        commands_ = true;
        return rpc::make_result( request, wire->header );
    }
    if( request.method == "bn.unsubscribe" ) {
        if( !decode_empty_request( params ) ) { return invalid(); }
        subscribed_ = false;
        commands_ = false;
        held_.clear();
        held_bytes_ = 0;
        return rpc::make_result( request, "{}" );
    }
    if( request.method == "bn.credit" ) {
        const auto decoded = decode_credit_request( params );
        if( !decoded ) { return invalid(); }
        if( !credits_ ) { return fail( error::negotiation_failed ); }
        // The held frames follow the response, in the order they were published.
        credits_ = std::min( *credits_ + decoded->frames, maximum_credits );
        return rpc::make_result( request, "{\"credits\":" + std::to_string( *credits_ ) + "}" );
    }
    if( request.method == "bn.interaction.choices" ) {
        const auto words = localization::locale_scope{ locale_ };
        const auto decoded = decode_choices_request( params );
        if( !decoded ) { return invalid(); }
        if( decoded->component_id ) {
            // A list of the top screen.
            // The rows are read from the live screen, so the boundary must be the live one.
            if( decoded->boundary_id != live_boundary_of( session_.epoch() ) ) { return fail( error::stale_boundary ); }
            const auto state = session_.boundary();
            if( !state ) { return fail( state.error() ); }
            const auto rows = read_rows( { .epoch = decoded->epoch, .boundary_id = decoded->boundary_id,
                                           .component_id = *decoded->component_id, .offset = decoded->offset, .limit = decoded->limit },
                                         *state );
            if( !rows ) { return fail( rows.error() ); }
            // A page that cannot be sent is recoverable: the client asks for fewer rows.
            auto wire = serialize_rows( *rows );
            if( wire.size() > maximum_inline_bytes ) { return fail( error::resource_limit ); }
            return rpc::make_result( request, wire );
        }
        const auto page = read_choices( *decoded, session_.epoch() );
        if( !page ) { return fail( page.error() ); }
        return rpc::make_result( request, serialize_choices( *page ) );
    }
    if( request.method == "bn.world.describe" ) {
        const auto words = localization::locale_scope{ locale_ };
        const auto decoded = decode_describe_request( params );
        if( !decoded ) { return invalid(); }
        const auto text = describe_tile( *decoded, session_.epoch() );
        if( !text ) { return fail( text.error() ); }
        return rpc::make_result( request, serialize_description( *text ) );
    }
    if( request.method == "bn.world.cells" ) {
        const auto decoded = decode_cells_request( params );
        if( !decoded ) { return invalid(); }
        const auto current = session_.current();
        if( !current ) { return fail( current.error() ); }
        const auto page = read_cells( *decoded, *current );
        if( !page ) { return fail( page.error() ); }
        return rpc::make_result( request, serialize_cells( *page ) );
    }
    if( request.method == "bn.overmap.cells" ) {
        const auto decoded = decode_cells_request( params );
        if( !decoded ) { return invalid(); }
        const auto current = session_.current();
        if( !current ) { return fail( current.error() ); }
        if( decoded->epoch != current->at.epoch ) { return fail( error::stale_epoch ); }
        // A spectator pans through this read; the controller's cursor is not what it reads.
        constexpr auto bound = 1 << 24;
        const auto &low = decoded->min;
        const auto &high = decoded->max;
        const auto inside = [&]( const position & at ) {
            return at.x >= -bound && at.x <= bound && at.y >= -bound && at.y <= bound &&
                   at.z >= -OVERMAP_DEPTH && at.z <= OVERMAP_HEIGHT;
        };
        if( !low.dim.empty() || !high.dim.empty() || low.z != high.z || low.x > high.x || low.y > high.y ||
            !inside( low ) || !inside( high ) ) { return fail( error::validation_failed ); }
        const auto squares = ( std::int64_t{high.x} - low.x + 1 ) * ( std::int64_t{high.y} - low.y + 1 );
        if( squares > static_cast<std::int64_t>( cells_per_query ) ) { return fail( error::resource_limit ); }
        const auto wire = serialize_overmap( overmap_ui::known_squares(
                tripoint_abs_omt( low.x, low.y, low.z ), tripoint_abs_omt( high.x, high.y, high.z ),
                game_client::overmap_tiles() ) );
        if( wire.size() > maximum_inline_bytes ) { return fail( error::resource_limit ); }
        return rpc::make_result( request, wire );
    }
    if( request.method == "bn.command.submit" ) {
        const auto decoded = decode_command_request( params );
        if( !decoded ) { return invalid(); }
        if( !pump_mode_ ) { return fail( error::not_ready ); }
        if( decoded->epoch != session_.epoch() ) { return fail( error::stale_epoch ); }
        const auto received = session_.submit( *decoded );
        if( !received ) { return fail( received.error() ); }
        const auto encoded = serialize_receipt( *received );
        if( !encoded ) { interrupt( engine_client::error::not_ready ); return fail( encoded.error() ); }
        return rpc::make_result( request, *encoded );
    }
    if( request.method == "bn.hold" ) {
        const auto decoded = decode_hold_request( params );
        if( !decoded ) { return invalid(); }
        if( !pump_mode_ ) { return fail( error::not_ready ); }
        if( decoded->epoch != session_.epoch() ) { return fail( error::stale_epoch ); }
        const auto state = session_.boundary();
        if( !state ) { return fail( state.error() ); }
        const auto top = top_screen( *state );
        if( decoded->screen_id != top.screen_id ) { return fail( error::stale_screen ); }
        const auto found = std::ranges::find( top.affordances, decoded->affordance_id, &affordance::id );
        if( found == top.affordances.end() ) { return fail( error::unknown_affordance ); }
        if( !found->holdable ) { return fail( error::invalid_arguments ); }
        walk_ = held_walk{ .screen_id = top.screen_id, .action = found->target };
        return rpc::make_result( request, "{}" );
    }
    if( request.method == "bn.release" ) {
        if( !decode_empty_request( params ) ) { return invalid(); }
        if( walk_ ) { walk_->released = true; }
        return rpc::make_result( request, "{}" );
    }
    if( request.method == "bn.command.result" ) {
        const auto decoded = decode_result_request( params );
        if( !decoded ) { return invalid(); }
        const auto current = session_.result( *decoded );
        if( !current ) { return fail( current.error() ); }
        return rpc::make_result( request, serialize_command_result( *current ) );
    }
    return rpc::make_error( { .id = request.id, .code = rpc::error_code::method_not_found } );
}

auto server::dispatch( const std::string_view line, std::ostream &out, std::ostream &err ) -> bool
{
    if( line.empty() ) { return true; } // Historical blank-line tolerance.
    const auto inspected = rpc::inspect_frame( line );
    if( !inspected || inspected->size() == 0 ) {
        auto frame = rpc::response_frame{ false };
        const auto code = !inspected && inspected.error() == rpc::parse_error::invalid_json ?
                          rpc::error_code::parse_error : rpc::error_code::invalid_request;
        const auto response = rpc::make_error( { .id = { .json = "null" }, .code = code } );
        if( !response || !frame.append( *response ) ) { interrupt( engine_client::error::not_ready ); return false; }
        const auto bytes = std::move( frame ).finish();
        if( !bytes || !rpc::write_frame( out, *bytes ) ) { interrupt( engine_client::error::not_ready ); return false; }
        return true;
    }
    // The cursor pins the complete validated input, including across native widget calls.
    pending_requests_ = inspected->cursor();
    deferred_frame_.emplace( inspected->batch );
    return process_requests( out, err );
}

auto server::process_requests( std::ostream &out, std::ostream &err ) -> bool
{
    const auto terminal = [&]() {
        interrupt( engine_client::error::not_ready );
        pending_requests_.reset();
        deferred_frame_.reset();
        err << "MCP: protocol output failed; closing connection\n";
        return false;
    };
    if( !pending_requests_ || !deferred_frame_ ) { return terminal(); }
    while( const auto envelope = pending_requests_->next() ) {
        const auto method = rpc::method_of( *envelope );
        // The `bn.` namespace is the strict protocol; an unknown name in it is method_not_found, not a legacy error.
        const auto is_direct = method.starts_with( "bn." );
        auto response = std::expected<std::optional<rpc::response>, rpc::output_error> {std::nullopt};
        if( is_direct ) {
            const auto entry = rpc::apply_strict_policy( *envelope );
            if( const auto request = std::get_if<rpc::request>( &entry ) ) {
                response = direct( *request );
            } else {
                response = rpc::make_error( { .id = { .json = "null" }, .code = std::get<rpc::error_code>( entry ) } );
            }
        } else {
            const auto entry = rpc::apply_legacy_policy( *envelope );
            if( const auto request = std::get_if<rpc::request>( &entry ) ) {
                response = legacy_dispatch( *request, err );
            } else {
                response = rpc::make_legacy_error( std::get<rpc::legacy_error>( entry ) );
            }
        }
        if( !response || !deferred_frame_->append( *response ) ) { return terminal(); }
        // Never process a later batch member by reentering the current native widget.
        // Its cursor and bounded response prefix survive until the next real input boundary.
        if( pending_response_ ) { return true; }
    }
    pending_requests_.reset();
    auto frame = std::move( *deferred_frame_ );
    deferred_frame_.reset();
    const auto bytes = std::move( frame ).finish();
    if( !bytes || !rpc::write_frame( out, *bytes ) || !write_notifications( out ) ) { return terminal(); }
    return true;
}

auto server::legacy_dispatch( const rpc::request &request, std::ostream &err ) ->
std::expected<std::optional<rpc::response>, rpc::output_error>
{
    const auto fail = [&]( const rpc::legacy_reason reason ) {
        return rpc::make_legacy_error( { .id = request.id, .reason = reason } );
    };
    const auto reply = [&]( const auto & write ) {
        auto value = std::ostringstream{};
        auto json = JsonOut{value};
        json.start_object();
        write( json );
        json.end_object();
        return rpc::make_result( request, value.str() );
    };
    const auto tool = [&]( const screen_snapshot & snapshot, const std::string &error = std::string{} ) {
        return reply( [&]( auto & json ) { write_tool_result( json, snapshot, error ); } );
    };
    const auto notification = !request.id.json;
    const auto &method = request.method;
    try {
        // Only historical legacy application params use JsonIn. Direct params never do.
        auto bytes = std::istringstream{request.params.value_or( "null" )};
        auto reader = JsonIn{bytes};
        if( method == "notifications/initialized" ) {
            initialized_ = initialize_requested_;
            return std::nullopt;
        }
        if( method == "notifications/cancelled" || method == "notifications/progress" ) {
            return std::nullopt;
        }
        if( method == "initialize" ) {
            const auto params = reader.get_object();
            params.allow_omitted_members();
            const auto requested = params.get_string( "protocolVersion", options_.protocol_version );
            const auto protocol = requested == "2025-06-18" ? requested : options_.protocol_version;
            if( notification ) { return std::nullopt; }
            auto response = reply( [&]( auto & json ) {
                json.member( "protocolVersion", protocol );
                json.member( "capabilities" );
                json.start_object();
                json.member( "tools" );
                json.start_object();
                json.member( "listChanged", false );
                json.end_object();
                json.member( "resources" );
                json.start_object();
                json.end_object();
                json.member( "prompts" );
                json.start_object();
                json.end_object();
                json.end_object();
                json.member( "serverInfo" );
                json.start_object();
                json.member( "name", options_.name );
                json.member( "version", options_.version );
                json.end_object();
                json.member( "instructions",
                             "Use bn.observe and bn.press. bn.press always follows the active BN UI input context." );
            } );
            if( response ) { initialize_requested_ = true; }
            return response;
        }
        if( !initialized_ && method != "ping" ) { return fail( rpc::legacy_reason::not_initialized ); }
        if( method == "ping" ) { return reply( []( auto & /*json*/ ) {} ); }
        if( method == "tools/list" ) { return reply( write_tools ); }
        if( method == "resources/list" ) {
            return reply( []( auto & json ) {
                json.member( "resources" );
                json.start_array();
                json.start_object();
                json.member( "uri", "bn://screen" );
                json.member( "name", "screen" );
                json.member( "description", "The complete current BN client screen." );
                json.member( "mimeType", "application/json" );
                json.end_object();
                json.start_object();
                json.member( "uri", "bn://state" );
                json.member( "name", "state" );
                json.member( "description", "Structured player-visible and player-known BN state." );
                json.member( "mimeType", "application/json" );
                json.end_object();
                json.end_array();
            } );
        }
        if( method == "resources/read" ) {
            const auto params = reader.get_object();
            params.allow_omitted_members();
            const auto uri = params.get_string( "uri", "" );
            if( uri != "bn://screen" && uri != "bn://state" ) { return fail( rpc::legacy_reason::unknown_resource ); }
            if( uri == "bn://state" && !host_.state ) { return fail( rpc::legacy_reason::structured_state_unavailable ); }
            if( notification ) { return std::nullopt; }
            return reply( [&]( auto & json ) {
                json.member( "contents" );
                json.start_array();
                json.start_object();
                json.member( "uri", uri );
                json.member( "mimeType", "application/json" );
                json.member( "text", uri == "bn://state" ? host_.state().json : host_.observe().json );
                json.end_object();
                json.end_array();
            } );
        }
        if( method == "prompts/list" ) {
            return reply( []( auto & json ) { json.member( "prompts" ); json.start_array(); json.end_array(); } );
        }
        if( method == "resources/subscribe" || method == "notifications/resources/list_changed" ) {
            return fail( rpc::legacy_reason::subscriptions_not_supported );
        }
        if( method == "tools/call" ) {
            const auto params = reader.get_object();
            params.allow_omitted_members();
            const auto name = params.get_string( "name" );
            if( notification ) { return std::nullopt; }
            if( name == "bn.observe" ) { return tool( host_.observe() ); }
            if( name == "bn.state" ) {
                if( !host_.state ) { return fail( rpc::legacy_reason::structured_state_unavailable ); }
                return tool( host_.state() );
            }
            if( ( hello_ || options_.spectator ) && ( name == "bn.press" || name == "bn.interact" ) ) {
                const auto data = engine_client::serialize_application_error( {
                    .kind = options_.spectator ? engine_client::error::read_only :
                    engine_client::error::invalid_lifecycle,
                } );
                if( !data ) { return std::unexpected( rpc::output_error::invalid_value ); }
                return rpc::make_error( { .id = request.id, .code = rpc::error_code::application_error, .data = *data } );
            }
            if( name == "bn.press" ) {
                const auto arguments = params.get_object( "arguments" );
                arguments.allow_omitted_members();
                const auto accepted = host_.submit( read_keys( arguments ) );
                if( pump_mode_ ) {
                    pending_response_ = deferred_response{
                        .id = request.id,
.error = accepted ? std::string{} : "Input batch was rejected"
                        ,
                    };
                    return std::nullopt;
                }
                return tool( host_.observe(), accepted ? std::string{} : "Input batch was rejected" );
            }
            if( name == "bn.interaction" ) {
                if( !host_.interaction ) { return fail( rpc::legacy_reason::structured_interaction_unavailable ); }
                const auto arguments = params.get_object( "arguments" );
                arguments.allow_omitted_members();
                const auto offset = arguments.has_member( "offset" ) ? read_nonnegative_integer( arguments,
                                    "offset" ) : 0;
                const auto limit = arguments.has_member( "limit" ) ? read_nonnegative_integer( arguments,
                                   "limit" ) : 100;
                return tool( host_.interaction( static_cast<std::size_t>( offset ),
                                                static_cast<std::size_t>( limit ) ) );
            }
            if( name == "bn.interact" ) {
                if( !host_.interaction ) { return fail( rpc::legacy_reason::structured_interaction_unavailable ); }
                const auto arguments = params.get_object( "arguments" );
                const auto accepted = host_.submit( { read_interaction_command( arguments ) } );
                if( pump_mode_ ) {
                    pending_response_ = deferred_response{
                        .id = request.id,
                        .interaction = true,
.error = accepted ? std::string{} : "Semantic input was rejected"
                        ,
                    };
                    return std::nullopt;
                }
                return tool( host_.interaction( 0, 100 ), accepted ? std::string{} :
                             "Semantic input was rejected" );
            }
            if( name == "bn.actions" ) {
                const auto actions = host_.actions ? host_.actions() : "{\"actions\":[]}";
                return tool( { .json = actions }, host_.actions ? std::string{} :
                             "Registered actions are unavailable" );
            }
            return fail( rpc::legacy_reason::unknown_tool );
        }
        return fail( rpc::legacy_reason::method_not_found );
    } catch( const std::exception & /*error*/ ) {
        if( notification ) { return std::nullopt; }
        err << "MCP: Invalid parameters\n";
        return fail( rpc::legacy_reason::invalid_parameters );
    }
}

} // namespace bn::mcp

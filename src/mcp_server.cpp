#include "mcp_server.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "json.h"
#include "client_memory.h"
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
            key.mouse_position = point{ mouse.get_int( "x" ), mouse.get_int( "y" ) };
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
    if( !publish_boundary() ) { failed_ = true; return 1; }
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

auto server::pump_until_input( std::istream &in, std::ostream &out, std::ostream &err ) -> bool
{
    if( failed_ || eof_ ) { return false; }
    if( host_.has_input && host_.has_input() ) { return true; }
    if( !publish_boundary() ) { failed_ = true; eof_ = true; return false; }
    pump_mode_ = true;
    finish_pending( out, err );
    while( !failed_ ) {
        if( pending_requests_ && !pending_response_ && !process_requests( out, err ) ) {
            failed_ = true;
            break;
        }
        // A direct command is not queued until its complete bounded receipt frame is written.
        if( session_.has_received() && !deferred_frame_ ) { static_cast<void>( deliver_input() ); }
        if( host_.has_input && host_.has_input() ) {
            pump_mode_ = false;
            return true;
        }
        if( pending_response_ ) {
            finish_pending( out, err );
            continue;
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
    session_.interrupt();
    return false;
}

auto server::failed() const -> bool
{
    return failed_;
}

auto server::reject_pending( std::string error ) -> void
{
    session_.interrupt();
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
    const auto request = rpc::request{ .id = { .json = pending->id.raw_json }, .method = "tools/call" };
    const auto response = rpc::make_result( request, value.str() );
    if( !response || !deferred_frame_ || !deferred_frame_->append( *response ) ) {
        failed_ = true;
        session_.interrupt();
        return;
    }
    // Shutdown may finish an already-terminal frame, but must not dispatch later members.
    if( pending_requests_ && pending_requests_->remaining() == 0 && !process_requests( out, err ) ) {
        failed_ = true;
    }
}

auto server::publish_boundary() -> bool
{
    using namespace engine_client;
    if( !session_.publish_boundary() ) { session_.interrupt(); return false; }
    if( completion_command_ && session_.latest_event() ) {
        const auto &event = *session_.latest_event();
        if( event.value().payload.command_id == completion_command_ ) {
            completion_events_ = {};
            const auto view = session_.read_snapshot( projection_ );
            if( view ) {
                const auto projected = project_event( event, view->state );
                if( projected ) {
                    completion_events_ = { .first_sequence = event.value().public_sequence,
                                           .last_sequence = event.value().public_sequence,
                                           .events = {*projected}
                                         };
                }
            }
        }
    }
    return true;
}

auto server::deliver_input() -> bool
{
    const auto input = session_.prepare_input( game_client::memory::screen_size() );
    if( !input ) { return false; }
    if( !host_.submit( {*input} ) ) { session_.interrupt(); return false; }
    return true;
}

auto server::direct( const engine_client::jsonrpc::request &request ) ->
std::expected<std::optional<engine_client::jsonrpc::response>, engine_client::jsonrpc::output_error>
{
    using namespace engine_client;
    namespace rpc = jsonrpc;
    // Direct notifications are deliberately effect-free, including negotiation and reads.
    if( !request.id.json ) { return std::nullopt; }
    const auto fail = [&]( const error reason, const application_error_stage stage ) {
        auto data = application_error{ .kind = reason, .stage = stage };
        if( reason == error::negotiation_failed || reason == error::unsupported_capability ||
            reason == error::stale_epoch ) {
            data.action = required_action::negotiate;
        } else if( reason == error::stale_revision || reason == error::stale_boundary ||
                   reason == error::stale_interaction_schema ||
                   reason == error::resync_required ) {
            data.action = required_action::read_snapshot;
        }
        if( const auto current = session_.read_snapshot( projection_ ) ) {
            data.current = error_current{ .session_epoch = current->session_epoch,
                                          .state_revision = current->state_revision,
                                          .through_public_sequence = current->through_public_sequence };
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
    const auto supports = [&]( const std::string & name ) {
        return negotiated_ &&
               std::ranges::find( negotiated_->capabilities, name ) != negotiated_->capabilities.end();
    };
    if( request.method == "bn.contract.negotiate" ) {
        const auto decoded = decode_negotiation_request( params );
        if( !decoded ) { return invalid(); }
        const auto selected = negotiate( *decoded );
        if( !selected ) { return fail( selected.error(), application_error_stage::negotiation ); }
        // A legacy batch still waiting for native delivery cannot transfer authority mid-flight.
        if( pending_response_ || ( host_.has_input && host_.has_input() ) ) {
            return fail( error::command_busy, application_error_stage::negotiation );
        }
        negotiated_ = *selected;
        return rpc::make_result( request, serialize_negotiation( *selected, session_.epoch() ) );
    }
    if( !negotiated_ ) { return fail( error::negotiation_failed, application_error_stage::negotiation ); }
    if( request.method == "bn.snapshot.get" ) {
        const auto decoded = decode_snapshot_request( params );
        if( !decoded ) { return invalid(); }
        if( !supports( "snapshot.readiness" ) || !supports( "snapshot.actions" ) ||
            !supports( "snapshot.interaction" ) ) {
            return fail( error::unsupported_capability, application_error_stage::validation );
        }
        if( decoded->session_epoch != session_.epoch() ) {
            return fail( error::stale_epoch, application_error_stage::validation );
        }
        const auto current = session_.read_snapshot( decoded->page );
        if( !current ) { return fail( current.error(), application_error_stage::validation ); }
        projection_ = decoded->page;
        return rpc::make_result( request, serialize_snapshot( *current ) );
    }
    if( request.method == "bn.command.submit" ) {
        const auto decoded = decode_command_request( params );
        if( !decoded ) { return invalid(); }
        const auto capability = std::holds_alternative<semantic_operation>( decoded->operation ) ?
                                "command.semantic_interaction" : "command.registered_action";
        if( !supports( capability ) ) {
            return fail( error::unsupported_capability, application_error_stage::receipt );
        }
        if( !pump_mode_ ) { return fail( error::not_ready, application_error_stage::receipt ); }
        const auto received = session_.submit( *decoded );
        if( !received ) { return fail( received.error(), application_error_stage::receipt ); }
        completion_command_ = received->command_id;
        completion_events_ = {};
        const auto encoded = serialize_receipt( *received );
        if( !encoded ) { session_.interrupt(); return fail( encoded.error(), application_error_stage::receipt ); }
        return rpc::make_result( request, *encoded );
    }
    if( request.method == "bn.command.result" ) {
        const auto decoded = decode_result_request( params );
        if( !decoded ) { return invalid(); }
        if( !supports( "delivery.inline_completion" ) ) {
            return fail( error::unsupported_capability, application_error_stage::completion );
        }
        auto current = session_.result( *decoded );
        if( !current ) { return fail( current.error(), application_error_stage::completion ); }
        auto batch = supports( "events.interaction_replaced" ) &&
                     completion_command_ == current->command_id ?
                     completion_events_ : event_batch{};
        if( current->completed && batch.events.empty() ) { current->completed->resync_required = true; }
        auto encoded = serialize_command_response( *current, batch );
        if( !encoded ) {
            if( current->completed ) { current->completed->resync_required = true; }
            encoded = serialize_command_response( *current, {} );
        }
        if( !encoded ) { return fail( encoded.error(), application_error_stage::completion ); }
        return rpc::make_result( request, *encoded );
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
        if( !response || !frame.append( *response ) ) { session_.interrupt(); return false; }
        const auto bytes = std::move( frame ).finish();
        if( !bytes || !rpc::write_frame( out, *bytes ) ) { session_.interrupt(); return false; }
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
        session_.interrupt();
        pending_requests_.reset();
        deferred_frame_.reset();
        err << "MCP: protocol output failed; closing connection\n";
        return false;
    };
    if( !pending_requests_ || !deferred_frame_ ) { return terminal(); }
    while( const auto envelope = pending_requests_->next() ) {
        const auto &method = envelope->method.decoded;
        const auto is_direct = method == "bn.contract.negotiate" || method == "bn.snapshot.get" ||
                               method == "bn.command.submit" || method == "bn.command.result";
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
    if( !bytes || !rpc::write_frame( out, *bytes ) ) { return terminal(); }
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
            if( negotiated_ && ( name == "bn.press" || name == "bn.interact" ) ) {
                const auto data = engine_client::serialize_application_error( {
                    .kind = engine_client::error::invalid_lifecycle,
                    .stage = engine_client::application_error_stage::receipt,
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
                        .id = { .type = request_id::kind::null_value, .raw_json = request.id.json },
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
                        .id = { .type = request_id::kind::null_value, .raw_json = request.id.json },
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

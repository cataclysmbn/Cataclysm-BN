#include "mcp_server.h"
#include "engine_client_component.h"
#include "engine_client_world.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <chrono>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

#include "json.h"
#include "client_input.h"
#include "client_memory.h"
#include "engine_client_wire.h"
#include "engine_client_wire_json.h"
#include "input.h"
#include "game_constants.h"
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

/// Each input schema states what the handler of its tool reads.
constexpr auto tool_list = R"json({"tools":[
{"name":"bn.observe","description":"Capture the complete current BN client screen, including menus, dialogs, map, sidebar, and messages.",
 "inputSchema":{"type":"object"}},
{"name":"bn.state","description":"Capture the world state the engine publishes to its clients: what the avatar sees and knows. The header counts the parts that hold the map cells; part reads one of them.",
 "inputSchema":{"type":"object","properties":{"part":{"type":"integer","minimum":0}}}},
{"name":"bn.press","description":"Deliver logical keyboard events through the active BN input context and return the resulting screen.",
 "inputSchema":{"type":"object","required":["keys"],"properties":{"keys":{
  "type":"array","minItems":1,"maxItems":256,"description":"Logical keys delivered in order through the active input context.",
  "items":{"type":"object","properties":{
   "input_id":{"type":"integer","minimum":0,"description":"Input boundary from bn.actions; stale commands are rejected."},
   "key":{"type":"string","description":"Key name, character, named key such as ENTER or ESCAPE, TIMEOUT, or IDLE. IDLE is accepted only at a nonblocking input boundary."},
   "action":{"type":"string","description":"Registered action id in the active input context; the host resolves its binding."},
   "modifiers":{"type":"array","items":{"type":"string"}},
   "text":{"type":"string","description":"UTF-8 text input; it may be supplied without key for text-only entry."},
   "mouse":{"type":"object","required":["x","y","button"],"properties":{
    "x":{"type":"integer","minimum":0},"y":{"type":"integer","minimum":0},
    "button":{"type":"string","enum":["left","right","scroll_up","scroll_down","move"]}}}}}}}}},
{"name":"bn.actions","description":"List the active BN input context, polling timeout, registered actions, and exact key bindings.",
 "inputSchema":{"type":"object"}},
{"name":"bn.interaction","description":"Describe the top screen at the current input boundary: its components and the affordances bn.interact invokes.",
 "inputSchema":{"type":"object"}},
{"name":"bn.interact","description":"Invoke one affordance of the top screen, as bn.command.submit does, and return the next top screen.",
 "inputSchema":{"type":"object","required":["affordance_id"],"properties":{
  "affordance_id":{"type":"string"},
  "args":{"type":"object","description":"The arguments the affordance states: row, count, text, pos, or pane."}}}}
]})json";

constexpr auto resource_list = R"json({"resources":[
{"uri":"bn://screen","name":"screen","description":"The complete current BN client screen.","mimeType":"application/json"},
{"uri":"bn://state","name":"state","description":"The header of the world state the engine publishes to its clients.","mimeType":"application/json"}
]})json";

using shown = std::expected<screen_snapshot, engine_client::error>;

/// The composed screen: its rows, and the color runs of each row as `[offset, length, fg, bg]`.
auto observe() -> screen_snapshot
{
    const auto screen = game_client::memory::snapshot();
    const auto same = []( const auto & a, const auto & b ) { return a.foreground == b.foreground && a.background == b.background; };
    auto rows = nlohmann::ordered_json::array();
    auto styles = nlohmann::ordered_json::array();
    for( auto y = 0; y < screen.height; ++y ) {
        auto row = std::string{};
        for( auto x = 0; x < screen.width; ++x ) {
            const auto offset = static_cast<std::size_t>( y * screen.width + x );
            const auto &cell = screen.cells[offset];
            row += cell.text;
            if( x > 0 && same( cell, screen.cells[offset - 1] ) ) {
                styles.back()[1] = styles.back()[1].get<int>() + 1;
            } else {
                styles.push_back( { offset, 1, cell.foreground, cell.background } );
            }
        }
        rows.push_back( row );
    }
    const auto cursor = nlohmann::ordered_json{ { "x", screen.cursor.x }, { "y", screen.cursor.y }, { "visible", screen.cursor_visible } };
    return { .json = engine_client::to_wire( { { "width", screen.width }, { "height", screen.height }, { "cursor", cursor }, { "rows", rows }, { "styles", styles } } ),
        .text = screen.text };
}

/// The context waiting for input, its registered actions, and the keys bound to each.
auto serialize_actions() -> std::string
{
    namespace ranges = std::ranges;
    using namespace std::views;
    const auto active = game_client::active_input_context();
    auto listed = nlohmann::ordered_json::array();
    for( const auto &action : game_client::available_input_actions() ) {
        auto bindings = nlohmann::ordered_json::array();
        for( const auto &binding : action.bindings ) {
            const auto name = [&]( const int key ) { return inp_mngr.get_keyname( key, binding.type, true ); };
            bindings.push_back( { { "input_method", binding.type == input_event_t::keyboard ? "keyboard" : "other" },
                { "sequence", binding.sequence | transform( name ) | ranges::to<std::vector>() } } );
        }
        listed.push_back( { { "id", action.id }, { "name", action.name }, { "bindings", bindings } } );
    }
    return engine_client::to_wire( { { "input_id", game_client::current_input_id() }, { "category", active.category },
        { "timeout_ms", active.timeout_ms }, { "actions", listed } } );
}

/// The top screen of the published boundary, as the `stack` of the contract holds it.
auto top_screen_of( const engine_client::session &authority ) -> shown
{
    const auto state = authority.boundary();
    if( !state ) { return std::unexpected( state.error() ); }
    const auto boundary = nlohmann::ordered_json::parse( engine_client::serialize_boundary( *state ) );
    return screen_snapshot{ .json = engine_client::to_wire( boundary.at( "stack" ).back() ) };
}

/// The published world state: its header, or with `part` the part of that index.
auto world_of( const engine_client::session &authority,
               const std::optional<std::int64_t> part ) -> shown
{
    const auto current = authority.current();
    if( !current ) { return std::unexpected( current.error() ); }
    const auto wire = engine_client::serialize_snapshot( *current );
    if( !wire ) { return std::unexpected( wire.error() ); }
    if( !part ) { return screen_snapshot{ .json = wire->header }; }
    if( *part >= static_cast<std::int64_t>( wire->parts.size() ) ) { return std::unexpected( engine_client::error::invalid_arguments ); }
    return screen_snapshot{ .json = wire->parts[*part] };
}

/// The keys of `bn.press`. Throws `decode_error` or a JSON type error for a key outside the schema.
auto read_keys( const nlohmann::json &arguments ) -> std::vector<key_event>
{
    using engine_client::integer;
    using engine_client::require;
    engine_client::members( arguments, {"keys"} );
    require( arguments.at( "keys" ).is_array() );
    auto result = std::vector<key_event> {};
    for( const auto &event : arguments.at( "keys" ) ) {
        engine_client::members( event, {}, {"input_id", "key", "action", "modifiers", "text", "mouse"} );
        auto key = key_event{ .action = event.value( "action", "" ), .key = event.value( "key", "" ),
                              .modifiers = event.value( "modifiers", std::vector<std::string> {} ), .text = event.value( "text", "" ) };
        if( event.contains( "input_id" ) ) {
            key.input_id = integer( event, "input_id", 0,
                                    static_cast<std::int64_t>( engine_client::maximum_safe_integer ) );
        }
        if( event.contains( "mouse" ) ) {
            const auto &mouse = event.at( "mouse" );
            engine_client::members( mouse, {"x", "y", "button"} );
            constexpr auto most = std::int64_t{ std::numeric_limits<int>::max() };
            key.mouse_position = point( static_cast<int>( integer( mouse, "x", 0, most ) ),
                                        static_cast<int>( integer( mouse, "y", 0, most ) ) );
            key.mouse_button = mouse.at( "button" ).get<std::string>();
            require( std::ranges::contains( std::array{ "left", "right", "scroll_up", "scroll_down", "move" },
                                            key.mouse_button ) );
        }
        // Exactly one input mode: an action, a key with optional text, text, or a mouse event.
        const auto modes = std::array{ !key.action.empty(), !key.key.empty() || !key.text.empty(), key.mouse_position.has_value() };
        require( std::ranges::count( modes, true ) == 1 );
        require( key.modifiers.empty() || !key.key.empty() );
        result.push_back( std::move( key ) );
    }
    return result;
}

/// A tool result. One too large for a frame is a tool error the agent can act on, not the end of the session.
auto tool_response( const rpc::request &request, const shown &value,
const std::string &error = {} ) -> std::expected<std::optional<rpc::response>, rpc::output_error> {
    const auto problem = value ? error : engine_client::error_text( value.error() );
    const auto screen = value.value_or( screen_snapshot{ .json = "{}" } );
    const auto text = []( const std::string & said ) { return nlohmann::ordered_json{ { "type", "text" }, { "text", said } }; };
    auto content = nlohmann::ordered_json::array( { text( screen.text.empty() ? screen.json : screen.text ) } );
    if( !problem.empty() ) { content.push_back( text( problem ) ); }
    const auto response = rpc::make_result( request, R"({"content":)" + engine_client::to_wire( content ) +
                                            R"(,"structuredContent":)" + screen.json + ( problem.empty() ? "}" : R"(,"isError":true})" ) );
    if( response || response.error() != rpc::output_error::resource_limit ) { return response; }
    return tool_response( request, screen_snapshot{ .json = "{}" }, "The result exceeds the 1 MiB stdio frame" );
}

/// Add the response to `request` to `frame`; `request` is null for an invalid entry. An MCP result the frame
/// has no room left for fails only its request: a tool call is a tool error, any other request -32603.
auto append_fitting( rpc::response_frame &frame, const rpc::request *request,
                     const std::optional<rpc::response> &response ) -> bool
{
    if( !response || !request || request->method.starts_with( "bn." ) || frame.fits( *response ) ) {
        return frame.append( response ).has_value();
    }
    const auto refused = request->method == "tools/call" ?
                         tool_response( *request, screen_snapshot{ .json = "{}" },
                                        "The result exceeds what is left of the 1 MiB stdio frame" ) :
                         rpc::make_error( { .id = request->id, .code = rpc::error_code::internal_error } );
    return refused && frame.append( *refused ).has_value();
}

} // namespace

server::server( mcp_host host ) : server( std::move( host ), {} )
{
}

server::server( mcp_host host, options opts ) : host_( std::move( host ) ),
    options_( std::move( opts ) ),
    session_( host_.contract_session ? * host_.contract_session : engine_client::process_session() )
{
    if( !host_.submit ) {
        throw std::invalid_argument( "MCP host must provide a submit callback" );
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
            while( !queued && session_.has_received() ) { queued = deliver_input().has_value(); }
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
    session_.interrupt( engine_client::error::not_ready );
    return flush_push( out );
}

auto server::notify_loading( const game_client::loading_progress &progress,
                             std::ostream &out ) -> void
{
    if( !hello_ || failed_ ) { return; }
    notifications_.push_back( rpc::make_notification( "bn.loading",
                              engine_client::serialize_loading( session_.epoch(), progress ) ) );
    if( !write_notifications( out ) ) { failed_ = true; }
}

auto server::heartbeat( std::ostream &out ) -> void
{
    if( !hello_ || failed_ ) { return; }
    notifications_.push_back( rpc::make_notification( "bn.progress",
    nlohmann::json{ { "epoch", session_.epoch() } }.dump() ) );
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
    session_.interrupt( engine_client::error::validation_failed );
    if( pending_response_ ) {
        pending_response_->error = error.empty() ? "Input was rejected in the active context" : std::move(
                                       error );
    }
}

auto server::finish_pending( std::ostream &out, std::ostream &err ) -> void
{
    if( !pending_response_ ) { return; }
    const auto pending = *std::exchange( pending_response_, std::nullopt );
    auto error = pending.error;
    // A command that was rejected or interrupted on its way to this boundary says why.
    if( pending.command && error.empty() ) {
        const auto ended = session_.result( *pending.command );
        if( !ended || ended->failure ) {
            error = engine_client::error_text( ended ? *ended->failure : ended.error() );
            if( ended && !ended->message.empty() ) { error += ": " + ended->message; }
        }
    }
    const auto request = rpc::request{ .id = pending.id, .method = "tools/call" };
    const auto response = tool_response( request, pending.command ? top_screen_of( session_ ) : shown{ observe() },
                                         error );
    if( !response || !deferred_frame_ || !append_fitting( *deferred_frame_, &request, *response ) ) {
        failed_ = true;
        session_.interrupt( engine_client::error::not_ready );
        return;
    }
    // Shutdown dispatches no later member: each is refused, so the frame is written with the answered ones.
    while( !pump_mode_ && pending_requests_ && pending_requests_->remaining() > 0 ) {
        const auto entry = rpc::apply_strict_policy( *pending_requests_->next() );
        const auto *unread = std::get_if<rpc::request>( &entry );
        const auto refused = rpc::make_error( { .id = unread ? unread->id : rpc::request_id{ .json = "null" },
                                                .code = unread ? rpc::error_code::internal_error : std::get<rpc::error_code>( entry ) } );
        if( !refused || !deferred_frame_->append( *refused ) ) {
            failed_ = true;
            return;
        }
    }
    if( pending_requests_ && pending_requests_->remaining() == 0 && !process_requests( out, err ) ) {
        failed_ = true;
    }
}

auto server::publish_boundary( std::ostream &out, const bool settles ) -> bool
{
    if( !session_.publish_boundary( settles ) ) { session_.interrupt( engine_client::error::not_ready ); return false; }
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
    auto resync = rpc::make_notification( "bn.resync",
                                          engine_client::serialize_resync( session_.epoch(), "backpressure", delivered_through_ ) );
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
        auto frame = jsonrpc::make_notification( method, params );
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
            // Cheap for an event the stream sealed: its serialized value is taken as it is.
            // The event is copied once: an initializer list would copy it twice.
            auto single = event_batch{ .epoch = pushed->epoch };
            single.events.push_back( pushed->event );
            auto params = serialize_events( single );
            const auto bytes = params.size();
            if( batch.epoch != pushed->epoch || batch_bytes + bytes > maximum_inline_bytes ) { flush_batch(); }
            batch.epoch = pushed->epoch;
            if( batch.events.empty() ) { first_params = std::move( params ); }
            batch.events.push_back( std::move( single.events.front() ) );
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

auto server::deliver_input() -> std::expected<void, engine_client::error>
{
    const auto input = session_.prepare_input( game_client::memory::screen_size() );
    if( !input ) { return std::unexpected( input.error() ); }
    if( !host_.submit( {*input} ) ) {
        session_.interrupt( engine_client::error::not_ready );
        return std::unexpected( engine_client::error::not_ready );
    }
    return {};
}

auto server::direct( const engine_client::jsonrpc::request &request ) ->
std::expected<std::optional<engine_client::jsonrpc::response>, engine_client::jsonrpc::output_error>
{
    using namespace engine_client;
    namespace rpc = jsonrpc;
    // Direct notifications are deliberately effect-free, including hello and reads.
    if( !request.id.json ) { return std::nullopt; }
    const auto fail = [&]( const error reason ) {
        auto data = application_error{ .kind = reason, .at = session_.at() };
        switch( reason ) {
            case error::negotiation_failed:
                data.action = required_action::hello;
                break;
            case error::stale_epoch:
            case error::stale_revision:
            case error::stale_boundary:
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
        // Input still waiting for native delivery or its deferred MCP response cannot transfer authority mid-flight.
        if( pending_response_ || ( host_.has_input && host_.has_input() ) ) {
            return fail( error::command_busy );
        }
        hello_ = true;
        walk_.reset();
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
        game_client::set_overmap_tiles( decoded->tiles );
        engine_client::world::set_tint_ids( { decoded->tint_ids.begin(), decoded->tint_ids.end() } );
        if( decoded->view && host_.resize_viewport ) {
            host_.resize_viewport( decoded->view->cols, decoded->view->rows );
        }
        const auto grant = credits_ ? std::optional{ delivery_grant{ .credits = *credits_, .max_hz = decoded->max_hz } } :
                           std::nullopt;
        auto reply = hello_reply{ .epoch = session_.epoch(), .engine = describe_engine(), .delivery = grant };
        // The engine words all its text in its own language, whatever the hello asked for.
        reply.identity = { .locale = reply.engine.locale, .engine_locale = reply.engine.locale };
        return rpc::make_result( request, serialize_hello( reply ) );
    }
    if( !hello_ ) { return fail( error::negotiation_failed ); }
    if( request.method == "bn.viewport" ) {
        const auto decoded = decode_viewport_request( params );
        if( !decoded ) { return invalid(); }
        if( host_.resize_viewport ) { host_.resize_viewport( decoded->cols, decoded->rows ); }
        // The new view is part of what the client reads, so it is published like any other change.
        // A running activity is no boundary: its command is not settled by a resize.
        if( const auto published = session_.publish_boundary( !polling_ ); !published ) { return fail( published.error() ); }
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
        const auto current = session_.current();
        if( !current ) { return fail( current.error() ); }
        const auto wire = serialize_snapshot( *current );
        if( !wire ) { return fail( wire.error() ); }
        // The parts follow the response frame, before any event after `current->at`.
        for( const auto &part : wire->parts ) {
            notifications_.push_back( rpc::make_notification( "bn.snapshot.part", part ) );
        }
        // The snapshot supersedes the events still waiting for credits, not the command stages.
        drop_held_events();
        delivered_through_ = current->at.sequence;
        subscribed_ = true;
        commands_ = true;
        return rpc::make_result( request, wire->header );
    }
    if( request.method == "bn.credit" ) {
        const auto decoded = decode_credit_request( params );
        if( !decoded ) { return invalid(); }
        if( !credits_ ) { return fail( error::negotiation_failed ); }
        // The held frames follow the response, in the order they were published.
        credits_ = std::min( *credits_ + decoded->frames, maximum_credits );
        return rpc::make_result( request, nlohmann::json{ { "credits", *credits_ } }.dump() );
    }
    if( request.method == "bn.interaction.choices" ) {
        const auto decoded = decode_choices_request( params );
        if( !decoded ) { return invalid(); }
        // The rows of a list of the top screen are read from the live screen, so the boundary must be the live one.
        if( decoded->boundary_id != live_boundary_of( session_.epoch() ) ) { return fail( error::stale_boundary ); }
        const auto state = session_.boundary();
        if( !state ) { return fail( state.error() ); }
        const auto rows = read_rows( *decoded, *state );
        if( !rows ) { return fail( rows.error() ); }
        // A page that cannot be sent is recoverable: the client asks for fewer rows.
        auto wire = serialize_rows( *rows );
        if( wire.size() > maximum_inline_bytes ) { return fail( error::resource_limit ); }
        return rpc::make_result( request, wire );
    }
    if( request.method == "bn.world.describe" ) {
        const auto decoded = decode_describe_request( params );
        if( !decoded ) { return invalid(); }
        const auto text = describe_tile( *decoded, session_.epoch() );
        if( !text ) { return fail( text.error() ); }
        return rpc::make_result( request, serialize_description( *text ) );
    }
    if( request.method == "bn.overmap.cells" ) {
        const auto decoded = decode_cells_request( params );
        if( !decoded ) { return invalid(); }
        const auto current = session_.current();
        if( !current ) { return fail( current.error() ); }
        if( decoded->epoch != current->at.epoch ) { return fail( error::stale_epoch ); }
        // The read names its own area; the controller's cursor is not what it reads.
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
        if( !encoded ) { session_.interrupt( engine_client::error::not_ready ); return fail( encoded.error() ); }
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
        if( !response || !frame.append( *response ) ) { session_.interrupt( engine_client::error::not_ready ); return false; }
        const auto bytes = std::move( frame ).finish();
        if( !bytes || !rpc::write_frame( out, *bytes ) ) { session_.interrupt( engine_client::error::not_ready ); return false; }
        return true;
    }
    // The cursor pins the complete validated input, including across native widget calls.
    // An activity answers the frames it reads at once; a frame that waits for its deferred
    // response keeps its place until the next input boundary.
    auto waiting_requests = std::exchange( pending_requests_, inspected->cursor() );
    auto waiting_frame = std::exchange( deferred_frame_, rpc::response_frame{ inspected->batch } );
    const auto answered = process_requests( out, err );
    if( waiting_frame ) {
        pending_requests_ = std::move( waiting_requests );
        deferred_frame_ = std::move( waiting_frame );
    }
    return answered;
}

auto server::process_requests( std::ostream &out, std::ostream &err ) -> bool
{
    const auto terminal = [&]() {
        session_.interrupt( engine_client::error::not_ready );
        pending_requests_.reset();
        deferred_frame_.reset();
        err << "MCP: protocol output failed; closing connection\n";
        return false;
    };
    if( !pending_requests_ || !deferred_frame_ ) { return terminal(); }
    while( const auto envelope = pending_requests_->next() ) {
        const auto waiting = pending_response_.has_value();
        const auto entry = rpc::apply_strict_policy( *envelope );
        const auto request = std::get_if<rpc::request>( &entry );
        // The `bn.` namespace is the engine contract; every other method is MCP.
        const auto response = !request ?
                              rpc::make_error( { .id = { .json = "null" }, .code = std::get<rpc::error_code>( entry ) } ) :
                              request->method.starts_with( "bn." ) ? direct( *request ) : mcp_dispatch( *request );
        if( !response || !append_fitting( *deferred_frame_, request, *response ) ) { return terminal(); }
        // A later batch member is never read inside the native widget the deferred input opens: its cursor and
        // response prefix wait for the next input boundary.
        if( !waiting && pending_response_ ) { return true; }
    }
    pending_requests_.reset();
    auto frame = std::move( *deferred_frame_ );
    deferred_frame_.reset();
    const auto bytes = std::move( frame ).finish();
    if( !bytes || !rpc::write_frame( out, *bytes ) || !write_notifications( out ) ) { return terminal(); }
    return true;
}

auto server::mcp_dispatch( const rpc::request &request ) ->
std::expected<std::optional<rpc::response>, rpc::output_error>
{
    using namespace engine_client;
    const auto fail = [&]( const rpc::error_code code ) { return rpc::make_error( { .id = request.id, .code = code } ); };
    const auto &method = request.method;
    // MCP notifications have no effect but completing the handshake.
    if( !request.id.json ) {
        if( method == "notifications/initialized" ) { initialized_ = initialize_requested_; }
        return std::nullopt;
    }
    // An unknown method is unknown before the handshake too.
    constexpr auto methods = std::array{ "initialize", "ping", "tools/list", "resources/list", "resources/read", "tools/call" };
    if( !std::ranges::contains( methods, method ) ) { return fail( rpc::error_code::method_not_found ); }
    auto calling_tool = false;
    try {
        const auto params = nlohmann::json::parse( request.params.value_or( "{}" ) );
        if( method == "initialize" ) {
            const auto requested = params.value( "protocolVersion", options_.protocol_version );
            const auto result = nlohmann::ordered_json{
                { "protocolVersion", requested == "2025-06-18" ? requested : options_.protocol_version },
                { "capabilities", { { "tools", { { "listChanged", false } } }, { "resources", nlohmann::ordered_json::object() } } },
                { "serverInfo", { { "name", options_.name }, { "version", options_.version } } },
                { "instructions", "Read the screen with bn.observe and play with bn.press, or read bn.interaction and play with bn.interact." } };
            const auto response = rpc::make_result( request, to_wire( result ) );
            if( response ) { initialize_requested_ = true; }
            return response;
        }
        if( !initialized_ && method != "ping" ) { return fail( rpc::error_code::not_initialized ); }
        if( method == "ping" ) { return rpc::make_result( request, "{}" ); }
        if( method == "tools/list" ) { return rpc::make_result( request, tool_list ); }
        if( method == "resources/list" ) { return rpc::make_result( request, resource_list ); }
        if( method == "resources/read" ) {
            members( params, {"uri"}, {"_meta"} );
            const auto uri = params.at( "uri" ).get<std::string>();
            if( uri != "bn://screen" && uri != "bn://state" ) { return fail( rpc::error_code::invalid_params ); }
            const auto value = uri == "bn://state" ? world_of( session_, std::nullopt ) : shown{ observe() };
            if( !value ) { return fail( rpc::error_code::internal_error ); }
            const auto content = nlohmann::ordered_json{ { "uri", uri }, { "mimeType", "application/json" }, { "text", value->json } };
            const auto response = rpc::make_result( request, to_wire( { { "contents", nlohmann::ordered_json::array( { content } ) } } ) );
            // A result too large for one frame fails its request, not the session.
            if( !response && response.error() == rpc::output_error::resource_limit ) { return fail( rpc::error_code::internal_error ); }
            return response;
        }
        members( params, {"name"}, {"arguments", "_meta"} );
        const auto name = params.at( "name" ).get<std::string>();
        const auto arguments = params.value( "arguments", nlohmann::json::object() );
        calling_tool = true;
        const auto tool = [&]( const shown & value, const std::string &error = {} ) { return tool_response( request, value, error ); };
        if( name == "bn.observe" ) { members( arguments, {} ); return tool( observe() ); }
        if( name == "bn.actions" ) { members( arguments, {} ); return tool( screen_snapshot{ .json = serialize_actions() } ); }
        if( name == "bn.interaction" ) { members( arguments, {} ); return tool( top_screen_of( session_ ) ); }
        if( name == "bn.state" ) {
            members( arguments, {}, {"part"} );
            const auto part = arguments.contains( "part" ) ?
                              std::optional{ integer( arguments, "part", 0, std::numeric_limits<std::int32_t>::max() ) } :
                              std::nullopt;
            return tool( world_of( session_, part ) );
        }
        if( name != "bn.press" && name != "bn.interact" ) { return fail( rpc::error_code::invalid_params ); }
        // A client of the engine contract owns the input.
        if( hello_ ) {
            const auto data = serialize_application_error( { .kind = error::invalid_lifecycle } );
            if( !data ) { return std::unexpected( rpc::output_error::invalid_value ); }
            return rpc::make_error( { .id = request.id, .code = rpc::error_code::application_error, .data = *data } );
        }
        // One input at a time: a rejected key could not tell which waiting call it belongs to.
        if( pending_response_ ) { return tool( std::unexpected( error::command_busy ) ); }
        // Input answers at the next input boundary, with the screen it leads to.
        const auto defer = [&]( const std::optional<result_request> &command, const std::string & error ) ->
        std::expected<std::optional<rpc::response>, rpc::output_error> {
            if( !pump_mode_ ) { return tool( command ? top_screen_of( session_ ) : shown{ observe() }, error ); }
            pending_response_ = deferred_response{ .id = request.id, .command = command, .error = error };
            return std::nullopt;
        };
        if( name == "bn.press" ) { return defer( std::nullopt, host_.submit( read_keys( arguments ) ) ? "" : "Input batch was rejected" ); }
        members( arguments, {"affordance_id"}, {"args"} );
        const auto state = session_.boundary();
        const auto at = session_.at();
        if( !pump_mode_ || !state || !at ) { return tool( std::unexpected( error::not_ready ) ); }
        const auto &affordances = top_screen( *state ).affordances;
        const auto found = std::ranges::find( affordances,
                                              arguments.at( "affordance_id" ).get<std::string>(), &affordance::id );
        const auto expect = found != affordances.end() && scope_of( *found ) == expect_scope::boundary ?
nlohmann::json{ { "boundary_id", state->id }, { "revision", std::to_string( at->revision ) } } :
        nlohmann::json{ { "screen_id", top_screen( *state ).screen_id } };
        auto operation = arguments;
        operation["kind"] = "invoke";
        const auto command = decode_command_request( nlohmann::json{ { "epoch", session_.epoch() }, { "expect", expect }, { "operation", operation } }.dump() );
        if( !command ) { throw command.error(); }
        const auto received = session_.submit( *command );
        if( !received ) { return tool( top_screen_of( session_ ), error_text( received.error() ) ); }
        const auto delivered = deliver_input();
        if( !delivered ) { return tool( top_screen_of( session_ ), error_text( delivered.error() ) ); }
        return defer( result_request{ .epoch = received->epoch, .command_id = received->command_id }, {} );
    } catch( const decode_error & ) {
    } catch( const nlohmann::json::exception & ) {
    }
    // A tool's own failure, such as an argument outside its schema, is a result the agent reads.
    if( calling_tool ) { return tool_response( request, screen_snapshot{ .json = "{}" }, "The arguments do not match the tool's input schema" ); }
    return fail( rpc::error_code::invalid_params );
}

} // namespace bn::mcp

#include "mcp_server.h"
#include "engine_client_component.h"
#include "engine_client_world.h"

#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "json.h"
#include "client_memory.h"
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
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( refresh_ms );
    while( !failed_ ) {
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
        if( walk_ && step_walk() ) {
            pump_mode_ = false;
            return true;
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

auto server::reject_pending() -> void
{
    session_.interrupt( engine_client::error::validation_failed );
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

auto server::deliver_input() -> bool
{
    const auto input = session_.prepare_input( game_client::memory::screen_size() );
    if( !input ) { return false; }
    if( !host_.submit( {*input} ) ) { session_.interrupt( engine_client::error::not_ready ); return false; }
    return true;
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
        // Input still waiting for native delivery cannot transfer authority mid-flight.
        if( host_.has_input && host_.has_input() ) {
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
    pending_requests_ = inspected->cursor();
    deferred_frame_.emplace( inspected->batch );
    return process_requests( out, err );
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
        const auto entry = rpc::apply_strict_policy( *envelope );
        const auto request = std::get_if<rpc::request>( &entry );
        const auto response = request ? direct( *request ) :
                              rpc::make_error( { .id = { .json = "null" }, .code = std::get<rpc::error_code>( entry ) } );
        if( !response || !deferred_frame_->append( *response ) ) { return terminal(); }
    }
    pending_requests_.reset();
    auto frame = std::move( *deferred_frame_ );
    deferred_frame_.reset();
    const auto bytes = std::move( frame ).finish();
    if( !bytes || !rpc::write_frame( out, *bytes ) || !write_notifications( out ) ) { return terminal(); }
    return true;
}

} // namespace bn::mcp

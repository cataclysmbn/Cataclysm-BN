#include "engine_client_session.h"

#include <stdexcept>
#include <utility>

#include "game_session.h"
#include "get_version.h"
#include "world.h"
#include "worldfactory.h"

namespace engine_client
{

session::session()
{
    const auto created = new_session_epoch();
    if( !created ) { throw std::runtime_error( "Engine session unavailable" ); }
    epoch_ = *created;
    lifecycle_ = std::make_unique<command_lifecycle>( epoch_, *this );
}

auto session::epoch() const -> const std::string & { return epoch_; } // *NOPAD*

auto session::current_permissions() const -> command_permissions
{
    const auto stable = phase_ == "menu" || phase_ == "waiting_for_input" || phase_ == "loading";
    return { .accepts_interaction_commands = stable, .accepts_registered_actions = stable };
}

auto session::set_phase( std::string phase ) -> void { phase_ = std::move( phase ); }

auto session::set_world_capture( const world_capture_fn capture ) -> void { world_capture_ = capture; }

auto session::refresh_result() -> void
{
    if( !active_ ) { return; }
    const auto current = lifecycle_->result( active_->command_id );
    if( !current ) { return; }
    if( current->stage != active_->stage ) { push_.emplace_back( *current ); }
    active_ = *current;
}

auto session::interrupt() -> void
{
    if( active_ ) {
        static_cast<void>( lifecycle_->interrupt() );
        refresh_result();
    }
    input_.reset();
    validated_input_.reset();
}

auto session::restart_epoch( const std::string_view reason ) -> void
{
    interrupt();
    retired_ = active_;
    if( stream_ ) {
        push_.emplace_back( resync_notice{ .epoch = epoch_, .reason = std::string{reason},
                                           .lost_after = stream_->current().at.sequence } );
    }
    const auto created = new_session_epoch();
    if( !created ) { throw std::runtime_error( "Engine session unavailable" ); }
    epoch_ = *created;
    lifecycle_ = std::make_unique<command_lifecycle>( epoch_, *this );
    active_.reset();
    stream_.reset();
}

auto session::replace_world() -> void { restart_epoch( "world_replaced" ); }

auto session::capture() const -> std::expected<state_value, error>
{
    const auto permissions = current_permissions();
    auto boundary = capture_boundary( {
        .epoch = epoch_,
        .ready = {
            .phase = phase_, .game_ready = game_session::running(),
            .accepts_interaction_commands = permissions.accepts_interaction_commands,
            .accepts_registered_actions = permissions.accepts_registered_actions
        },
    } );
    if( !boundary ) { return std::unexpected( boundary.error() ); }
    return state_value{ .interaction = std::move( *boundary ),
                        .world = world_capture_ ? world_capture_() : world_state{} };
}

auto session::publish_boundary() -> std::expected<void, error>
{
    // Startup language/debug prompts are genuine native input boundaries, not engine readiness.
    if( phase_ == "starting" ) { phase_ = "menu"; }
    auto candidate = capture();
    if( !candidate ) { return std::unexpected( candidate.error() ); }
    if( !stream_ ) {
        auto created = event_stream::create( epoch_, std::move( *candidate ) );
        if( !created ) { return std::unexpected( created.error() ); }
        stream_ = std::move( *created );
        return {};
    }
    auto request = publish_request{ .next = *candidate };
    if( active_ && active_->stage == command_stage::executing ) { request.command = active_->command_id; }
    auto published = stream_->publish( std::move( request ) );
    if( published ) {
        if( *published ) { push_.emplace_back( event_push{ .epoch = epoch_, .event = **published } ); }
    } else if( published.error() == error::resource_limit ||
               published.error() == error::resync_required ) {
        // The change does not fit one event or cannot be expressed: adopt the state and make
        // subscribers start over.
        push_.emplace_back( resync_notice{ .epoch = epoch_,
                                           .reason = published.error() == error::resource_limit ? "overflow" : "state_removed",
                                           .lost_after = stream_->current().at.sequence } );
        if( const auto rebased = stream_->rebase( std::move( *candidate ) ); !rebased ) { return rebased; }
    } else { return std::unexpected( published.error() ); }
    if( active_ && active_->stage == command_stage::executing ) {
        const auto &now = stream_->current();
        const auto completed = lifecycle_->complete_at_boundary( now.at, now.value.interaction );
        if( !completed && completed.error() != error::invalid_lifecycle ) {
            interrupt();
            return std::unexpected( completed.error() );
        }
        refresh_result();
    }
    return {};
}

auto session::current() const -> std::expected<snapshot, error>
{
    if( !stream_ ) { return std::unexpected( error::not_ready ); }
    return stream_->current();
}

auto session::at() const -> std::optional<clock_point>
{
    if( !stream_ ) { return std::nullopt; }
    return stream_->current().at;
}

auto session::submit( command_request request ) -> std::expected<receipt, error>
{
    auto input = request;
    const auto received = lifecycle_->submit( std::move( request ) );
    if( !received ) { return std::unexpected( received.error() ); }
    active_ = *received;
    input_ = std::move( input );
    retired_.reset();
    return receipt{ .epoch = received->epoch, .command_id = received->command_id };
}

auto session::has_received() const -> bool
{
    return active_ && active_->stage == command_stage::received;
}

auto session::prepare_input( const point screen_size ) ->
std::expected<game_client::input_command, error>
{
    if( !stream_ || !input_ ) { return std::unexpected( error::not_ready ); }
    const auto &now = stream_->current();
    const auto validated = lifecycle_->validate( now.at, now.value.interaction, screen_size );
    refresh_result();
    if( !validated ) { input_.reset(); return std::unexpected( validated.error() ); }
    validated_input_ = *validated;
    auto native = game_client::input_command{ .input_id = game_client::current_input_id() };
    if( const auto semantic = std::get_if<semantic_operation>( &input_->operation ) ) {
        // Semantic commands carry their boundary inside the operation, not the raw-input field.
        native.input_id.reset();
        native.interaction = semantic->command;
        native.interaction->input_id = game_client::current_input_id();
        if( semantic->target ) {
            native.interaction->position = to_bubble( *semantic->target, current_bubble_frame() );
        }
    } else {
        native.action = std::get<registered_action>( input_->operation ).id;
    }
    input_.reset();
    return native;
}

auto session::delivered( input_event fallback ) -> input_event
{
    if( active_ && active_->stage == command_stage::validated && validated_input_ ) {
        if( lifecycle_->execution_started() ) {
            refresh_result();
            auto validated = std::exchange( validated_input_, std::nullopt );
            return std::move( *validated );
        }
        interrupt();
    }
    return fallback;
}

auto session::result( const result_request &request ) const -> std::expected<command_result, error>
{
    if( retired_ && request.epoch == retired_->epoch &&
        request.command_id == retired_->command_id ) { return *retired_; }
    if( request.epoch != epoch_ ) { return std::unexpected( error::stale_epoch ); }
    return lifecycle_->result( request.command_id );
}

auto session::take_push() -> std::vector<push_item> { return std::exchange( push_, {} ); }

auto describe_engine() -> engine_info
{
    auto result = engine_info{ .build = getVersionString() };
    if( world_generator && world_generator->active_world && world_generator->active_world->info ) {
        for( const auto &mod : world_generator->active_world->info->active_mod_order ) {
            result.mods.push_back( mod.str() );
        }
    }
    return result;
}

auto process_session() -> session & // *NOPAD*
{
    static auto value = session{};
    return value;
}

} // namespace engine_client

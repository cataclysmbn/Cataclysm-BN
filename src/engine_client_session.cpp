#include "engine_client_session.h"

#include <stdexcept>
#include <utility>

#include "game_session.h"

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

auto session::refresh_result() -> void
{
    if( active_ ) {
        if( const auto current = lifecycle_->result( active_->command_id ) ) { active_ = *current; }
    }
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

auto session::replace_world() -> void
{
    interrupt();
    retired_ = active_;
    const auto created = new_session_epoch();
    if( !created ) { throw std::runtime_error( "Engine session unavailable" ); }
    epoch_ = *created;
    lifecycle_ = std::make_unique<command_lifecycle>( epoch_, *this );
    active_.reset();
    stream_.reset();
    event_.reset();
}

auto session::capture( const projection page ) const -> std::expected<state_value, error>
{
    const auto permissions = current_permissions();
    return capture_state( {
        .session_epoch = epoch_,
        .ready = {
            .phase = phase_, .game_ready = game_session::running(),
            .accepts_interaction_commands = permissions.accepts_interaction_commands,
            .accepts_registered_actions = permissions.accepts_registered_actions
        },
        .page = { .offset = page.offset, .limit = page.limit },
    } );
}

auto session::publish_boundary() -> std::expected<void, error>
{
    // Startup language/debug prompts are genuine native input boundaries, not engine readiness.
    if( phase_ == "starting" ) { phase_ = "menu"; }
    const auto candidate = capture( {} );
    if( !candidate ) { return std::unexpected( candidate.error() ); }
    event_.reset();
    if( !stream_ ) {
        auto created = event_stream::create( { .session_epoch = epoch_, .state = *candidate } );
        if( !created ) { return std::unexpected( created.error() ); }
        stream_ = std::move( *created );
    } else {
        auto sink = null_event_sink{};
        auto replacement = engine_client::replacement{ .state = *candidate };
        if( active_ && active_->stage == command_stage::executing ) {
            replacement.command_id = active_->command_id;
        }
        auto published = stream_->replace( disclosure::publish, std::move( replacement ), sink );
        if( !published ) { return std::unexpected( published.error() ); }
        event_ = std::move( *published );
    }
    if( active_ && active_->stage == command_stage::executing ) {
        const auto completed = lifecycle_->complete_at_boundary( stream_->current_snapshot() );
        if( !completed && completed.error() != error::invalid_lifecycle ) {
            interrupt();
            return std::unexpected( completed.error() );
        }
        refresh_result();
    }
    return {};
}

auto session::read_snapshot( const projection page ) const -> std::expected<snapshot, error>
{
    if( !stream_ ) { return std::unexpected( error::not_ready ); }
    const auto candidate = capture( page );
    if( !candidate ) { return std::unexpected( candidate.error() ); }
    return stream_->project_snapshot( *candidate );
}

auto session::submit( command_request request ) -> std::expected<receipt, error>
{
    auto input = request;
    const auto received = lifecycle_->submit( std::move( request ) );
    if( !received ) { return std::unexpected( received.error() ); }
    active_ = *received;
    input_ = std::move( input );
    retired_.reset();
    return receipt{ .session_epoch = received->session_epoch, .command_id = received->command_id };
}

auto session::has_received() const -> bool
{
    return active_ && active_->stage == command_stage::received;
}

auto session::prepare_input( const point screen_size ) ->
std::expected<game_client::input_command, error>
{
    if( !stream_ || !input_ ) { return std::unexpected( error::not_ready ); }
    const auto validated = lifecycle_->validate( stream_->current_snapshot(), screen_size );
    refresh_result();
    if( !validated ) { input_.reset(); return std::unexpected( validated.error() ); }
    validated_input_ = *validated;
    auto native = game_client::input_command{ .input_id = game_client::current_input_id() };
    if( const auto semantic = std::get_if<semantic_operation>( &input_->operation ) ) {
        // Semantic commands carry their boundary inside the operation, not the raw-input field.
        native.input_id.reset();
        native.interaction = semantic->command;
        native.interaction->input_id = game_client::current_input_id();
        if( semantic->target ) { native.interaction->position = semantic->target->position; }
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
    if( retired_ && request.session_epoch == retired_->session_epoch &&
        request.command_id == retired_->command_id ) { return *retired_; }
    if( request.session_epoch != epoch_ ) { return std::unexpected( error::stale_epoch ); }
    return lifecycle_->result( request.command_id );
}

auto session::latest_event() const -> const std::optional<public_event> & { return event_; } // *NOPAD*

auto process_session() -> session & // *NOPAD*
{
    static auto value = session{};
    return value;
}

} // namespace engine_client

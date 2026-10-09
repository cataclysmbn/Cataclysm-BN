#include "engine_client_contract.h"
#include "client_interaction_validation.h"

#include <algorithm>
#include <array>
#include <limits>
#include <random>
#include <ranges>
#include <type_traits>
#include <utility>

namespace engine_client
{
namespace
{
static_assert( std::is_nothrow_assignable_v < std::optional<command_request> &,
               command_request && > );
static_assert( std::is_nothrow_assignable_v < std::optional<command_result> &,
               command_result && > );
static_assert( std::is_nothrow_constructible_v < std::expected<command_result, error>,
               command_result && > );
static_assert( std::is_nothrow_constructible_v < std::expected<input_event, error>,
               input_event && > );

constexpr auto capabilities = std::array
{
    "snapshot.readiness", "snapshot.actions", "snapshot.interaction",
    "command.semantic_interaction", "command.registered_action",
    "events.interaction_replaced", "delivery.inline_completion"
};
auto supported( const std::string &name ) -> bool
{
    return std::ranges::find( capabilities, name ) != capabilities.end();
}
auto terminal( const command_stage stage ) -> bool
{
    return stage == command_stage::rejected || stage == command_stage::completed ||
           stage == command_stage::interrupted;
}
auto valid_id( const std::string &value ) -> bool
{
    return !value.empty() && value.size() <= maximum_id_bytes;
}
auto validate_shape( const command_request &request ) -> bool
{
    if( !valid_id( request.session_epoch ) || !valid_id( request.input_boundary_id ) ||
        request.interaction_schema_id.size() > maximum_id_bytes ) { return false; }
    const auto semantic = std::get_if<semantic_operation>( &request.operation );
    if( semantic == nullptr ) { return valid_id( std::get<registered_action>( request.operation ).id ); }
    if( !valid_id( request.interaction_schema_id ) ) { return false; }
    const auto &command = semantic->command;
    if( command.target_id.size() > maximum_id_bytes ||
        ( command.count && *command.count > maximum_safe_integer ) ) { return false; }
    switch( command.operation ) {
        case game_client::interaction_operation::choose:
            return !command.target_id.empty() && command.value.empty() && !command.submit &&
                   !command.count && !command.position && !semantic->target;
        case game_client::interaction_operation::fill:
            return !command.target_id.empty() && command.submit.has_value() && !command.count &&
                   !command.position && !semantic->target;
        case game_client::interaction_operation::set_count:
            return !command.target_id.empty() && command.count.has_value() && command.value.empty() &&
                   !command.submit && !command.position && !semantic->target;
        case game_client::interaction_operation::set_target:
            return command.value.empty() && !command.submit && !command.count && !command.position &&
                   semantic->target && semantic->target->space == "reality_bubble_map_square" &&
                   valid_id( semantic->target->frame_id ) && semantic->target->dimension_id.empty();
        case game_client::interaction_operation::cancel:
            return command.target_id.empty() && command.value.empty() && !command.submit &&
                   !command.count && !command.position && !semantic->target;
    }
    return false;
}
} // namespace

auto error_name( const error value ) -> std::string
{
    switch( value ) {
        case error::negotiation_failed:
            return "negotiation_failed";
        case error::unsupported_capability:
            return "unsupported_capability";
        case error::not_ready:
            return "not_ready";
        case error::stale_epoch:
            return "stale_epoch";
        case error::stale_revision:
            return "stale_revision";
        case error::stale_boundary:
            return "stale_boundary";
        case error::stale_interaction_schema:
            return "stale_interaction_schema";
        case error::validation_failed:
            return "validation_failed";
        case error::command_busy:
            return "command_busy";
        case error::unknown_command:
            return "unknown_command";
        case error::invalid_lifecycle:
            return "invalid_lifecycle";
        case error::resync_required:
            return "resync_required";
        case error::resource_limit:
            return "resource_limit";
    }
    return "validation_failed";
}
auto negotiate( const negotiation_request &request ) -> std::expected<negotiated_contract, error>
{
    namespace ranges = std::ranges;
    if( ranges::find( request.supported_versions,
                      contract_version ) == request.supported_versions.end() ) {
        return std::unexpected( error::negotiation_failed );
    }
    if( !ranges::all_of( request.required_capabilities, supported ) ) {
        return std::unexpected( error::unsupported_capability );
    }
    auto result = negotiated_contract{};
    for( const auto capability : capabilities ) {
        if( ranges::find( request.required_capabilities,
                          capability ) != request.required_capabilities.end() ||
            ranges::find( request.optional_capabilities, capability ) != request.optional_capabilities.end() ) {
            result.capabilities.emplace_back( capability );
        }
    }
    return result;
}
auto new_session_epoch() -> std::expected<std::string, error>
{
    try {
        auto entropy = std::random_device{};
        auto result = std::string{"epoch:"};
        constexpr auto digits = "0123456789abcdef";
        for( [[maybe_unused]] const auto part : std::views::iota( 0, 4 ) ) {
            const auto value = entropy();
            for( const auto shift : std::views::iota( 0, 8 ) ) {
                result += digits[( value >> ( shift * 4 ) ) & 15U];
            }
        }
        return result;
    } catch( const std::exception & ) {
        return std::unexpected( error::resource_limit );
    }
}
auto capture_state( const capture_options &options ) -> std::expected<state_value, error>
{
    if( options.session_epoch.empty() || options.page.limit == 0 ||
        options.page.limit > maximum_rows || options.page.offset > maximum_safe_integer ) {
        return std::unexpected( error::resource_limit );
    }
    const auto native = game_client::current_input_id();
    auto result = state_value{};
    result.ready = options.ready;
    result.input_boundary_id = game_client::opaque_interaction_id( "boundary", {options.session_epoch, std::to_string( native )} );
    result.view = {.offset = options.page.offset, .limit = options.page.limit};
    for( const auto &entry : game_client::available_input_actions() ) {
        result.actions.push_back( {.id = entry.id, .name = entry.name} );
    }
    auto interaction = game_client::current_interaction( options.page );
    if( interaction.structured ) { result.interaction = std::move( interaction ); }
    if( const auto valid = validate_state( result ); !valid ) { return std::unexpected( valid.error() ); }
    if( serialize_state( result ).size() > maximum_inline_bytes ) { return std::unexpected( error::resource_limit ); }
    return result;
}
auto validate_state( const state_value &state ) -> std::expected<void, error>
{
    if( state.view.offset > maximum_safe_integer || state.view.limit == 0 ||
        state.view.limit > maximum_rows ) { return std::unexpected( error::resource_limit ); }
    constexpr auto phases = std::array{"starting", "menu", "loading", "waiting_for_input", "executing", "shutting_down"};
    if( std::ranges::find( phases, state.ready.phase ) == phases.end() ||
        !valid_id( state.input_boundary_id ) ||
    !std::ranges::all_of( state.actions, []( const auto & entry ) { return valid_id( entry.id ); } ) ) {
        return std::unexpected( error::validation_failed );
    }
    if( state.interaction ) {
        const auto &value = *state.interaction;
        if( value.choice_offset > maximum_safe_integer || value.choice_total > maximum_safe_integer ||
            value.choices.size() > state.view.limit ) { return std::unexpected( error::resource_limit ); }
        if( !value.structured || !valid_id( value.schema_id ) ||
            value.choice_offset != std::min( state.view.offset, value.choice_total ) ||
            value.choices.size() > value.choice_total - value.choice_offset ) {
            return std::unexpected( error::validation_failed );
        }
        for( const auto &choice : value.choices ) {
            if( !valid_id( choice.id ) || ( choice.pane_id && !valid_id( *choice.pane_id ) ) ||
                ( choice.area_id && !valid_id( *choice.area_id ) ) ) {
                return std::unexpected( error::validation_failed );
            }
            if( ( choice.available_count && *choice.available_count > maximum_safe_integer ) ||
                ( choice.selected_count && *choice.selected_count > maximum_safe_integer ) ||
                ( choice.minimum_count && *choice.minimum_count > maximum_safe_integer ) ) {
                return std::unexpected( error::resource_limit );
            }
        }
        if( !std::ranges::all_of( value.panes, []( const auto & pane ) { return valid_id( pane.id ); } ) ||
        ( value.field && ( !valid_id( value.field->id ) ||
                           ( value.field->type != "text" && value.field->type != "integer" ) ) ) ) {
            return std::unexpected( error::validation_failed );
        }
        if( value.target && ( value.target->coordinate_space != "bubble_ms" ||
        !std::ranges::all_of( value.target->candidates, []( const auto & entry ) { return valid_id( entry.id ); } ) ) ) {
            return std::unexpected( error::validation_failed );
        }
    }
    return {};
}
auto same_state( const state_value &left, const state_value &right ) -> bool
{
    return serialize_state( left ) == serialize_state( right );
}

auto same_boundary_state( const state_value &left, const state_value &right ) -> bool
{
    const auto metadata = []( auto state ) {
        state.view = {};
        if( state.interaction ) {
            state.interaction->choices.clear();
            state.interaction->choice_offset = 0;
        }
        return state;
    };
    return same_state( metadata( left ), metadata( right ) );
}

command_lifecycle::command_lifecycle( std::string session_epoch,
                                      const command_authority &authority ) :
    epoch_( std::move( session_epoch ) ), authority_( authority ) {}

auto command_lifecycle::submit( command_request request ) -> std::expected<command_result, error>
{
    if( result_ && !terminal( result_->stage ) ) { return std::unexpected( error::command_busy ); }
    if( const auto semantic = std::get_if<semantic_operation>( &request.operation );
        semantic && semantic->command.value.size() > maximum_inline_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    if( !validate_shape( request ) ) { return std::unexpected( error::validation_failed ); }
    if( next_command_ == std::numeric_limits<counter>::max() || !valid_id( epoch_ ) ) {
        return std::unexpected( error::resource_limit );
    }
    const auto next_command = next_command_ + 1;
    auto received = command_result{
        .session_epoch = epoch_,
        .command_id = game_client::opaque_interaction_id( "command", {epoch_, std::to_string( next_command )} ),
    };
    auto response = received;
    request_ = std::move( request );
    result_ = std::move( received );
    next_command_ = next_command;
    return response;
}
auto command_lifecycle::validate( const snapshot &current, const point screen_size )
-> std::expected<input_event, error>
{
    if( !result_ || result_->stage != command_stage::received ) {
        return std::unexpected( error::invalid_lifecycle );
    }
    const auto reject = [&]( const error reason ) -> std::expected<input_event, error> {
        result_->stage = command_stage::rejected;
        result_->failure = reason;
        request_.reset();
        return std::unexpected( reason );
    };
    const auto &request = *request_;
    if( request.session_epoch != epoch_ || current.session_epoch != epoch_ ) { return reject( error::stale_epoch ); }
    if( request.state_revision != current.state_revision ) { return reject( error::stale_revision ); }
    const auto live_boundary = game_client::opaque_interaction_id( "boundary", {epoch_, std::to_string( game_client::current_input_id() )} );
    if( request.input_boundary_id != current.state.input_boundary_id ||
        request.input_boundary_id != live_boundary ) {
        return reject( error::stale_boundary );
    }
    const auto permissions = authority_.current_permissions();
    auto resolved = std::expected<input_event, std::string> { input_event{} };
    if( const auto semantic = std::get_if<semantic_operation>( &request.operation ) ) {
        if( !permissions.accepts_interaction_commands || !current.state.interaction ) { return reject( error::not_ready ); }
        if( request.interaction_schema_id != current.state.interaction->schema_id ) {
            return reject( error::stale_interaction_schema );
        }
        auto command = semantic->command;
        command.input_id = game_client::current_input_id();
        if( semantic->target ) {
            if( semantic->target->frame_id != live_boundary ) { return reject( error::stale_boundary ); }
            command.position = semantic->target->position;
        }
        auto checked = game_client::resolve_checked_interaction( {
            .command = command,
            .expected_schema = request.interaction_schema_id,
.target_space = semantic->target ? std::optional<std::string_view>{"bubble_ms"} : std::nullopt,
        } );
        if( !checked ) {
            if( checked.error().kind == game_client::interaction_rejection::stale_boundary ) {
                return reject( error::stale_boundary );
            }
            return reject( checked.error().kind == game_client::interaction_rejection::stale_schema ?
                           error::stale_interaction_schema : error::validation_failed );
        }
        resolved->type = input_event_t::interaction;
        resolved->interaction = std::move( *checked );
    } else {
        if( !permissions.accepts_registered_actions ) { return reject( error::not_ready ); }
        auto native = game_client::input_command{};
        native.action = std::get<registered_action>( request.operation ).id;
        native.input_id = game_client::current_input_id();
        resolved = game_client::resolve_input_command( native, screen_size );
        if( !resolved ) { return reject( error::validation_failed ); }
    }
    result_->stage = command_stage::validated;
    result_->validation_succeeded = true;
    return std::move( *resolved );
}
auto command_lifecycle::execution_started() -> std::expected<void, error>
{
    if( !result_ || result_->stage != command_stage::validated ) { return std::unexpected( error::invalid_lifecycle ); }
    result_->stage = command_stage::executing;
    result_->execution_started = true;
    return {};
}
auto command_lifecycle::complete_at_boundary( const snapshot &current, const bool resync_required )
-> std::expected<void, error>
{
    if( !result_ || result_->stage != command_stage::executing || !request_ ||
        current.state.input_boundary_id == request_->input_boundary_id ) {
        return std::unexpected( error::invalid_lifecycle );
    }
    if( current.session_epoch != epoch_ ) { return std::unexpected( error::stale_epoch ); }
    if( current.state_revision <= request_->state_revision ) { return std::unexpected( error::stale_revision ); }
    result_->stage = command_stage::completed;
    result_->completed = completion{
        .state_revision = current.state_revision,
        .through_public_sequence = current.through_public_sequence,
        .resync_required = resync_required,
    };
    request_.reset();
    return {};
}
auto command_lifecycle::interrupt() -> std::expected<void, error>
{
    if( !result_ || terminal( result_->stage ) ) { return std::unexpected( error::invalid_lifecycle ); }
    result_->stage = command_stage::interrupted;
    request_.reset();
    return {};
}
auto command_lifecycle::result( const std::string &command_id ) const ->
std::expected<command_result, error>
{
    if( !result_ || result_->command_id != command_id ) { return std::unexpected( error::unknown_command ); }
    return *result_;
}
} // namespace engine_client

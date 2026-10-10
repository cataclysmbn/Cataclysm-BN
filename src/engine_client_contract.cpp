#include "engine_client_contract.h"
#include "client_interaction_validation.h"
#include "game.h"
#include "map/map.h"

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

auto terminal( const command_stage stage ) -> bool
{
    return stage == command_stage::rejected || stage == command_stage::completed ||
           stage == command_stage::interrupted;
}
auto valid_id( const std::string &value ) -> bool
{
    return !value.empty() && value.size() <= maximum_id_bytes;
}
auto live_boundary_id( const std::string &epoch ) -> std::string
{
    return game_client::opaque_interaction_id( "boundary", {epoch, std::to_string( game_client::current_input_id() )} );
}
auto validate_shape( const command_request &request ) -> bool
{
    if( !valid_id( request.epoch ) || !valid_id( request.expect.boundary_id ) ||
        ( request.expect.schema_id && !valid_id( *request.expect.schema_id ) ) ) { return false; }
    const auto semantic = std::get_if<semantic_operation>( &request.operation );
    if( semantic == nullptr ) { return valid_id( std::get<registered_action>( request.operation ).id ); }
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
                   semantic->target;
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
auto stage_name( const command_stage stage ) -> std::string
{
    switch( stage ) {
        case command_stage::received:
            return "received";
        case command_stage::validated:
            return "validated";
        case command_stage::executing:
            return "executing";
        case command_stage::rejected:
            return "rejected";
        case command_stage::completed:
            return "completed";
        case command_stage::interrupted:
            return "interrupted";
    }
    return "interrupted";
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

auto current_bubble_frame() -> bubble_frame
{
    const auto origin = bub_to_abs( tripoint_bub_ms::zero() );
    return { .dim = g->get_current_dimension_id().str(), .x = origin.x(), .y = origin.y() };
}

auto capture_boundary( const capture_options &options ) -> std::expected<boundary_state, error>
{
    if( options.epoch.empty() ) { return std::unexpected( error::resource_limit ); }
    auto result = boundary_state{};
    result.ready = options.ready;
    result.id = live_boundary_id( options.epoch );
    result.frame = current_bubble_frame();
    for( const auto &entry : game_client::available_input_actions() ) {
        result.actions.push_back( {.id = entry.id, .name = entry.name} );
    }
    // A value over the byte bound keeps fewer rows; choice_total still states the full count.
    for( auto limit = maximum_rows; limit > 0; limit /= 2 ) {
        auto interaction = game_client::current_interaction( {.offset = 0, .limit = limit} );
        if( !interaction.structured ) { result.interaction.reset(); break; }
        result.interaction = std::move( interaction );
        if( serialize_boundary( result ).size() <= maximum_inline_bytes ) { break; }
        if( limit == 1 ) { return std::unexpected( error::resource_limit ); }
    }
    if( const auto valid = validate_boundary( result ); !valid ) { return std::unexpected( valid.error() ); }
    return result;
}
auto validate_boundary( const boundary_state &state ) -> std::expected<void, error>
{
    constexpr auto phases = std::array{"starting", "menu", "loading", "waiting_for_input", "executing", "shutting_down"};
    if( std::ranges::find( phases, state.ready.phase ) == phases.end() || !valid_id( state.id ) ||
    !std::ranges::all_of( state.actions, []( const auto & entry ) { return valid_id( entry.id ); } ) ) {
        return std::unexpected( error::validation_failed );
    }
    if( !state.interaction ) { return {}; }
    const auto &value = *state.interaction;
    if( value.choice_total > maximum_safe_integer || value.choices.size() > maximum_rows ) {
        return std::unexpected( error::resource_limit );
    }
    if( !value.structured || !valid_id( value.schema_id ) || value.choice_offset != 0 ||
        value.choices.size() > value.choice_total ) { return std::unexpected( error::validation_failed ); }
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
                       ( value.field->type != "text" && value.field->type != "integer" ) ) ) ||
    ( value.target && !std::ranges::all_of( value.target->candidates, []( const auto & entry ) { return valid_id( entry.id ); } ) ) ) {
        return std::unexpected( error::validation_failed );
    }
    return {};
}
auto same_boundary( const boundary_state &left, const boundary_state &right ) -> bool
{
    return serialize_boundary( left ) == serialize_boundary( right );
}

auto read_choices( const choices_request &request, const std::string &epoch )
-> std::expected<choices_page, error>
{
    if( request.epoch != epoch ) { return std::unexpected( error::stale_epoch ); }
    if( request.boundary_id != live_boundary_id( epoch ) ) { return std::unexpected( error::stale_boundary ); }
    if( request.limit == 0 || request.limit > maximum_rows || request.offset > maximum_safe_integer ) {
        return std::unexpected( error::resource_limit );
    }
    auto interaction = game_client::current_interaction( {.offset = request.offset, .limit = request.limit} );
    if( !interaction.structured ) { return std::unexpected( error::not_ready ); }
    return choices_page{
        .boundary_id = request.boundary_id,
        .total = interaction.choice_total,
        .choices = std::move( interaction.choices ),
    };
}

command_lifecycle::command_lifecycle( std::string epoch, const command_authority &authority ) :
    epoch_( std::move( epoch ) ), authority_( authority ) {}

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
        .epoch = epoch_,
        .command_id = game_client::opaque_interaction_id( "command", {epoch_, std::to_string( next_command )} ),
    };
    auto response = received;
    request_ = std::move( request );
    result_ = std::move( received );
    next_command_ = next_command;
    return response;
}
auto command_lifecycle::validate( const clock_point &at, const boundary_state &current,
                                  const point screen_size ) -> std::expected<input_event, error>
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
    if( request.epoch != epoch_ || at.epoch != epoch_ ) { return reject( error::stale_epoch ); }
    if( request.expect.revision != at.revision ) { return reject( error::stale_revision ); }
    if( request.expect.boundary_id != current.id || current.id != live_boundary_id( epoch_ ) ) {
        return reject( error::stale_boundary );
    }
    const auto live_schema = current.interaction ? std::optional{current.interaction->schema_id} :
                             std::nullopt;
    if( request.expect.schema_id != live_schema ) { return reject( error::stale_interaction_schema ); }
    const auto permissions = authority_.current_permissions();
    auto resolved = std::expected<input_event, std::string> { input_event{} };
    if( const auto semantic = std::get_if<semantic_operation>( &request.operation ) ) {
        if( !permissions.accepts_interaction_commands || !current.interaction ) { return reject( error::not_ready ); }
        auto command = semantic->command;
        command.input_id = game_client::current_input_id();
        if( semantic->target ) {
            // Absolute target to the live bubble; the native bounds and range still decide.
            const auto frame = current_bubble_frame();
            if( semantic->target->dim != frame.dim ) { return reject( error::validation_failed ); }
            command.position = game_client::interaction_position{
                .x = semantic->target->x - frame.x,
                .y = semantic->target->y - frame.y,
                .z = semantic->target->z,
            };
        }
        auto checked = game_client::resolve_checked_interaction( {
            .command = command,
            .expected_schema = *request.expect.schema_id,
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
    return std::move( *resolved );
}
auto command_lifecycle::execution_started() -> std::expected<void, error>
{
    if( !result_ || result_->stage != command_stage::validated ) { return std::unexpected( error::invalid_lifecycle ); }
    result_->stage = command_stage::executing;
    return {};
}
auto command_lifecycle::complete_at_boundary( const clock_point &at, const boundary_state &current )
-> std::expected<void, error>
{
    if( !result_ || result_->stage != command_stage::executing || !request_ ||
        current.id == request_->expect.boundary_id ) {
        return std::unexpected( error::invalid_lifecycle );
    }
    if( at.epoch != epoch_ ) { return std::unexpected( error::stale_epoch ); }
    result_->stage = command_stage::completed;
    result_->at = at;
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

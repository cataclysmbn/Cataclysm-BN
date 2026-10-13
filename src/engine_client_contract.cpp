#include "engine_client_contract.h"
#include "engine_client_component.h"
#include "action.h"
#include "activity_actor_definitions.h"
#include "player_activity.h"
#include "pickup_token.h"
#include "avatar.h"
#include "client_input.h"
#include "client_memory.h"
#include "client_interaction_validation.h"
#include "cursesdef.h"
#include "cursesport.h"
#include "game.h"
#include "input.h"
#include "translations.h"
#include "map/map.h"

#include <algorithm>
#include <array>
#include <limits>
#include <random>
#include <ranges>
#include <string_view>
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

/// The schemas of the semantic inputs `item_operation_event` makes; no screen has them.
constexpr auto pickup_schema = std::string_view { "item_operation:pickup" };
constexpr auto drop_schema = std::string_view { "item_operation:drop" };

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
    const auto &expect = request.expect;
    // A screen expectation names no boundary, and an invoke names no schema.
    if( !valid_id( request.epoch ) ||
        ( expect.screen_id ? !valid_id( *expect.screen_id ) :
          !valid_id( expect.boundary_id ) || ( expect.schema_id && !valid_id( *expect.schema_id ) ) ) ) { return false; }
    if( const auto invoke = std::get_if<invoke_operation>( &request.operation ) ) {
        return valid_id( invoke->affordance_id );
    }
    const auto semantic = std::get_if<semantic_operation>( &request.operation );
    if( semantic == nullptr ) {
        if( const auto item = std::get_if<item_operation>( &request.operation ) ) {
            return valid_id( item->item_id ) && item->count > 0 && item->count <= maximum_safe_integer;
        }
        const auto action = std::get_if<registered_action>( &request.operation );
        return action == nullptr || valid_id( action->id );
    }
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
        // A client invokes an affordance with `invoke_operation`; the lifecycle makes the event.
        case game_client::interaction_operation::invoke:
            return false;
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
        case error::read_only:
            return "read_only";
        case error::invalid_arguments:
            return "invalid_arguments";
        case error::unknown_affordance:
            return "unknown_affordance";
        case error::stale_screen:
            return "stale_screen";
    }
    return "validation_failed";
}
auto error_text( const error value ) -> std::string
{
    switch( value ) {
        case error::negotiation_failed:
            return _( "The protocol version is not supported." );
        case error::not_ready:
            return _( "The game is not ready." );
        case error::stale_epoch:
            return _( "The world changed." );
        case error::stale_revision:
        case error::stale_boundary:
        case error::stale_interaction_schema:
            return _( "The screen changed, try again." );
        case error::validation_failed:
            return _( "Not possible here." );
        case error::command_busy:
            return _( "Another command is still running." );
        case error::unknown_command:
            return _( "Unknown command." );
        case error::invalid_lifecycle:
            return _( "The command cannot run now." );
        case error::resync_required:
            return _( "Resynchronizing with the game." );
        case error::resource_limit:
            return _( "Too much data was requested." );
        case error::read_only:
            return _( "This connection can only watch the game." );
        case error::invalid_arguments:
            return _( "That needs other details." );
        case error::unknown_affordance:
            return _( "That is not possible on this screen." );
        case error::stale_screen:
            return _( "That screen has closed." );
    }
    return _( "Not possible here." );
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
    if( game_client::active_input_context().category == "OVERMAP" ) { return { .omt = true }; }
    const auto origin = bub_to_abs( tripoint_bub_ms::zero() );
    return { .dim = g->get_current_dimension_id().str(), .x = origin.x(), .y = origin.y() };
}

auto to_bubble( const position &target, const bubble_frame &frame )
-> std::optional<game_client::interaction_position>
{
    constexpr auto limits = std::numeric_limits<int> {};
    const auto x = std::int64_t{target.x} - frame.x;
    const auto y = std::int64_t{target.y} - frame.y;
    if( target.dim != frame.dim || x < limits.min() || x > limits.max() || y < limits.min() ||
        y > limits.max() ) { return std::nullopt; }
    return game_client::interaction_position{ .x = static_cast<int>( x ), .y = static_cast<int>( y ), .z = target.z };
}

auto travel_click( const travel_operation &operation ) -> std::optional<game_client::input_command>
{
    const auto target = to_bubble( operation.target, current_bubble_frame() );
    if( !target ) { return std::nullopt; }
    const auto at = tripoint_bub_ms( target->x, target->y, target->z );
    const auto &you = get_avatar();
    const auto step = at - you.bub_pos();
    // The route planner takes no step into a closed door; native movement opens it.
    if( operation.button == "left" && step.z() == 0 &&
        square_dist( at.xy().raw(), you.bub_pos().xy().raw() ) == 1 &&
        get_map().can_open_door( &you, at, !get_map().is_outside( you.bub_pos() ) ) ) {
        return game_client::input_command{
            .action = action_ident( get_movement_action_from_delta( step, iso_rotate::yes ) ),
            .input_id = game_client::current_input_id() };
    }
    const auto cell = g->click_cell_of( at );
    if( !cell ) { return std::nullopt; }
    return game_client::input_command{ .mouse_position = *cell, .mouse_button = operation.button };
}

auto item_operation_input() -> game_client::input_command
{
    return { .key = "TIMEOUT", .input_id = game_client::current_input_id() };
}

auto item_operation_event( const item_operation &operation ) -> input_event
{
    auto result = input_event{};
    result.type = input_event_t::interaction;
    result.interaction = game_client::interaction_event{
        .operation = game_client::interaction_operation::set_count,
        .schema_id = std::string( operation.drop ? drop_schema : pickup_schema ),
        .target_id = operation.item_id,
        .count = operation.count,
    };
    return result;
}

auto item_operation_of( const input_event &event ) -> std::optional<item_operation>
{
    const auto &semantic = event.interaction;
    if( !semantic || semantic->operation != game_client::interaction_operation::set_count ||
        !semantic->count ||
        ( semantic->schema_id != drop_schema && semantic->schema_id != pickup_schema ) ) { return std::nullopt; }
    return item_operation{ .drop = semantic->schema_id == drop_schema, .item_id = semantic->target_id, .count = *semantic->count };
}

auto item_operation_stack( const item_operation &operation ) -> std::optional<world::item_stack>
{
    auto stacks = world::item_stacks( get_map(), get_avatar() );
    auto &rows = operation.drop ? stacks.inventory : stacks.ground;
    const auto row = std::ranges::find( rows, operation.item_id, []( const auto & stack ) { return stack.row.id; } );
    if( row == rows.end() || operation.count > row->row.count.value_or( 0 ) ) { return std::nullopt; }
    return *row;
}

auto run_item_operation( const item_operation &operation ) -> bool
{
    const auto stack = item_operation_stack( operation );
    if( !stack ) { return false; }
    auto &you = get_avatar();
    const auto count = static_cast<int>( operation.count );
    if( operation.drop ) {
        // As the native drop menu: the first item of the stack and how much of the stack goes, in charges
        // for a charges item.
        you.drop( { drop_location( *stack->items.front(), count ) }, you.bub_pos() );
        return true;
    }
    // As the native pickup menu: the stack's items in order until `count` is taken, a charges item in charges.
    auto items = std::vector<item *> {};
    auto quantities = std::vector<int> {};
    for( auto left = count; auto *thing : stack->items ) {
        if( left <= 0 ) { break; }
        const auto taken = std::min( left, thing->count() );
        items.push_back( thing );
        quantities.push_back( thing->count_by_charges() ? taken : 0 );
        left -= taken;
    }
    auto targets = pickup::optimize_pickup( items, quantities );
    you.assign_activity( std::make_unique<player_activity>( std::make_unique<pickup_activity_actor>
                         ( targets, you.bub_pos() ) ) );
    return true;
}

auto describe_tile( const describe_request &request, const std::string &epoch )
-> std::expected<tile_description, error>
{
    if( request.epoch != epoch ) { return std::unexpected( error::stale_epoch ); }
    if( request.boundary_id != live_boundary_id( epoch ) ) { return std::unexpected( error::stale_boundary ); }
    const auto target = to_bubble( request.target, current_bubble_frame() );
    const auto at = target ? tripoint_bub_ms( target->x, target->y, target->z ) : tripoint_bub_ms();
    if( !target || !g->click_cell_of( at ) ) { return std::unexpected( error::validation_failed ); }
    return tile_description{ .lines = tile_info_lines( *target ) };
}

auto tile_info_lines( const game_client::interaction_position &at ) -> std::vector<std::string>
{
    // The views print into a window; read the text back from a private one.
    constexpr auto width = 60;
    constexpr auto height = 24;
    const auto window = catacurses::newwin( height, width, point_zero );
    auto line = 1;
    g->pre_print_all_tile_info( tripoint_bub_ms( at.x, at.y, at.z ), window, line, height - 2,
                                get_map().get_visibility_variables_cache() );
    const auto *const printed = window.get<cata_cursesport::WINDOW>();
    auto result = std::vector<std::string> {};
    for( const auto &row : printed->line ) {
        auto text = std::string{};
        for( const auto &cell : row.chars ) { text += cell.ch; }
        const auto last = text.find_last_not_of( ' ' );
        if( last != std::string::npos ) { result.push_back( text.substr( 0, last + 1 ) ); }
    }
    return result;
}

/// What a view without a structured interaction shows: the text of the composed screen, so a client
/// can display it and drive it with the view's registered actions. The map itself is not a screen.
auto screen_interaction( const std::string &context ) ->
std::optional<game_client::interaction_snapshot>
{
    if( context.empty() || context == "DEFAULTMODE" ) { return std::nullopt; }
    auto lines = std::vector<std::string> {};
    auto text = game_client::memory::snapshot().text;
    for( auto start = std::size_t{ 0 }; start <= text.size(); ) {
        const auto end = std::min( text.find( '\n', start ), text.size() );
        auto line = text.substr( start, end - start );
        line.erase( line.find_last_not_of( ' ' ) + 1 );
        lines.push_back( std::move( line ) );
        start = end + 1;
    }
    while( !lines.empty() && lines.back().empty() ) { lines.pop_back(); }
    if( lines.empty() ) { return std::nullopt; }
    auto message = std::string{};
    for( const auto &line : lines ) { message += line + '\n'; }
    message.pop_back();
    return game_client::interaction_snapshot{
        .schema_id = game_client::opaque_interaction_id( "screen", { context } ),
        .context = context,
        .kind = game_client::interaction_kind::custom,
        .message = std::move( message ),
        .structured = true,
        .actions_only = true,
    };
}

auto capture_boundary( const capture_options &options ) -> std::expected<boundary_state, error>
{
    if( options.epoch.empty() ) { return std::unexpected( error::resource_limit ); }
    auto result = boundary_state{};
    result.ready = options.ready;
    result.id = live_boundary_id( options.epoch );
    result.epoch = options.epoch;
    result.frame = current_bubble_frame();
    for( const auto &entry : game_client::available_input_actions() ) {
        auto keys = entry.bindings
        | std::views::filter( []( const auto & event ) {
            return event.type == input_event_t::keyboard && event.sequence.size() == 1;
        } )
        | std::views::transform( []( const auto & event ) {
            return inp_mngr.get_keyname( event.sequence.front(), event.type, true );
        } )
        | std::ranges::to<std::vector>();
        result.actions.push_back( {.id = entry.id, .name = entry.name, .keys = std::move( keys )} );
    }
    const auto open_screens = [&]() { return std::make_shared<const std::vector<screen_view>>( interaction_stack( result ) ); };
    // A value over the byte bound keeps fewer rows; choice_total still states the full count.
    for( auto limit = maximum_rows; limit > 0; limit /= 2 ) {
        auto interaction = game_client::current_interaction( {.offset = 0, .limit = limit} );
        if( !interaction.structured ) {
            // A view with no structured interaction still shows its composed screen to the client.
            result.interaction = screen_interaction( std::string(
                                     game_client::active_input_context().category ) );
            result.stack = open_screens();
            break;
        }
        result.interaction = std::move( interaction );
        // The value that goes out is the larger one: the interaction members and the stack.
        result.stack = open_screens();
        if( serialize_interaction_boundary( result ).size() <= maximum_inline_bytes ) { break; }
        if( limit == 1 ) { return std::unexpected( error::resource_limit ); }
    }
    if( const auto valid = validate_boundary( result ); !valid ) { return std::unexpected( valid.error() ); }
    return result;
}
auto live_boundary_of( const std::string &epoch ) -> std::string { return live_boundary_id( epoch ); }
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
        if( choice.editor && ( !valid_id( choice.editor->type ) ||
        !std::ranges::all_of( choice.editor->values, []( const auto & entry ) { return valid_id( entry.id ); } ) ) ) {
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
                       ( value.field->type != "text" && value.field->type != "integer" &&
                         value.field->type != "key" ) ) ) ||
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
    auto page = choices_page{
        .boundary_id = request.boundary_id,
        .total = interaction.choice_total,
        .choices = std::move( interaction.choices ),
    };
    // A page that cannot be sent is a recoverable error: the client asks for fewer rows.
    if( serialize_choices( page ).size() > maximum_inline_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    return page;
}

namespace
{
/// Every member of the shape is supplied and no other; named rows are rows of their component.
auto matches_shape( const screen_view &view, const arg_shape &shape,
                    const invoke_args &args ) -> bool
{
    const auto present = []( const auto & wanted, const auto & given ) { return wanted.has_value() == given.has_value(); };
    if( !present( shape.row, args.row ) || !present( shape.rows, args.rows ) ||
        !present( shape.count, args.count ) ||
        !present( shape.text, args.text ) || !present( shape.pos, args.pos ) ||
        !present( shape.pane, args.pane ) ) {
        return false;
    }
    const auto component_in = [&]( const std::string & component_id ) -> const component * { // *NOPAD*
        return view.root ? find_component( *view.root, component_id ) : nullptr;
    };
    // The avatar's rows and a paged list's unread rows are checked natively.
    const auto holds = [&]( const std::string & component_id, const std::string & row ) {
        if( component_id == ground_rows || component_id == inventory_rows ) { return true; }
        const auto *const found = component_in( component_id );
        if( found == nullptr ) { return false; }
        const auto ids = row_ids( *found );
        return !ids || std::ranges::contains( *ids, row );
    };
    if( shape.row && ( !valid_id( *args.row ) || !holds( shape.row->component_id, *args.row ) ) ) { return false; }
    if( shape.rows && ( args.rows->empty() ||
    !std::ranges::all_of( *args.rows, [&]( const auto & row ) {
    return valid_id( row.id ) && ( shape.rows->counts || !row.count ) &&
               holds( shape.rows->component_id, row.id );
    } ) ) ) { return false; }
    if( shape.count && ( ( shape.count->min && *args.count < *shape.count->min ) ||
                         ( shape.count->max && *args.count > *shape.count->max ) ) ) { return false; }
    if( shape.text && args.text->size() > maximum_inline_bytes ) { return false; }
    if( shape.pane ) {
        const auto *const parent = component_in( shape.pane->component_id );
        if( parent == nullptr || !find_component( *parent, *args.pane ) ) { return false; }
    }
    return true;
}
auto semantic_of( const game_client::interaction_operation kind ) -> semantic_operation
{
    return { .command = { .operation = kind } };
}

struct resolve_options {
    const boundary_state &state;
    const screen_view &top;
    const affordance &found;
    const invoke_args &args;
};
/// The native operation the invoke of an offered affordance stands for.
/// The id the native command names for `row` of `component_id`: a row the screen lists by another identity
/// answers with its native id, and a row off the page is looked up in the whole native list.
auto native_row( const screen_view &top, const std::string &component_id,
                 const std::string &row_id ) -> std::string
{
    const auto *const found = top.root ? find_component( *top.root, component_id ) : nullptr;
    const auto *const body = found ? std::get_if<list_body>( &found->body ) : nullptr;
    if( body == nullptr ) { return row_id; }
    const auto entry = std::ranges::find( body->rows, row_id, &row::id );
    if( entry == body->rows.end() ) { return native_row_id( row_id ).value_or( row_id ); }
    return entry->native_id.empty() ? row_id : entry->native_id;
}
auto resolve_affordance( const resolve_options &opts ) -> std::expected<command_operation, error>
{
    const auto &state = opts.state;
    const auto &found = opts.found;
    const auto &args = opts.args;
    if( !matches_shape( opts.top, found.args, args ) ) { return std::unexpected( error::invalid_arguments ); }
    // Readiness gates as in the 0.1 lifecycle; `enabled` is the screen's own availability.
    const auto registered = found.runs == invoke_kind::action || found.runs == invoke_kind::travel ||
                            found.runs == invoke_kind::context || found.runs == invoke_kind::pickup ||
                            found.runs == invoke_kind::drop;
    if( !( registered ? state.ready.accepts_registered_actions :
           state.ready.accepts_interaction_commands ) ) {
        return std::unexpected( error::not_ready );
    }
    if( !found.enabled ) { return std::unexpected( error::validation_failed ); }
    using game_client::interaction_operation;
    switch( found.runs ) {
        case invoke_kind::choose: {
            auto result = semantic_of( interaction_operation::choose );
            result.command.target_id = args.row ? native_row( opts.top, found.args.row->component_id,
                                       *args.row ) : found.target;
            return result;
        }
        case invoke_kind::fill:
        case invoke_kind::submit: {
            auto result = semantic_of( interaction_operation::fill );
            const auto &field = found.args.text->field_id;
            result.command.target_id = field.empty() ? native_row( opts.top, found.args.row->component_id,
                                       *args.row ) : field;
            result.command.value = *args.text;
            result.command.submit = found.runs == invoke_kind::submit;
            return result;
        }
        case invoke_kind::set_count: {
            auto result = semantic_of( interaction_operation::set_count );
            result.command.target_id = native_row( opts.top, found.args.row->component_id, *args.row );
            result.command.count = static_cast<std::uint64_t>( *args.count );
            return result;
        }
        case invoke_kind::cancel:
            return semantic_of( interaction_operation::cancel );
        case invoke_kind::action:
            return registered_action{ .id = found.target };
        case invoke_kind::travel:
        case invoke_kind::context:
            return travel_operation{ .target = *args.pos, .button = found.runs == invoke_kind::travel ? "left" : "right" };
        case invoke_kind::pickup:
        case invoke_kind::drop:
            return item_operation{ .drop = found.runs == invoke_kind::drop, .item_id = *args.row,
                                   .count = static_cast<std::uint64_t>( *args.count ) };
        case invoke_kind::screen:
            return invoke_operation{ .affordance_id = found.id, .args = args };
    }
    return std::unexpected( error::unknown_affordance );
}

/// The invoke's arguments as the screen's loop reads them, with the position in the bubble; nullopt
/// when the position is in another dimension or outside the native range.
auto arguments_of( const invoke_args &args, const bubble_frame &frame )
-> std::optional<game_client::interaction_arguments>
{
    auto result = game_client::interaction_arguments{ .row = args.row, .count = args.count, .text = args.text, .pane = args.pane };
    if( args.rows ) {
        result.rows = *args.rows | std::views::transform( []( const invoke_row & row ) {
            return game_client::interaction_row_count{ .id = row.id, .count = row.count };
        } ) | std::ranges::to<std::vector>();
    }
    if( args.pos ) {
        result.position = to_bubble( *args.pos, frame );
        if( !result.position ) { return std::nullopt; }
    }
    return result;
}
} // namespace

auto scope_name( const expect_scope scope ) -> std::string
{
    return scope == expect_scope::screen ? "screen" : "boundary";
}

auto resolve_invoke( const boundary_state &state, const invoke_operation &invoke )
-> std::expected<command_operation, error>
{
    const auto top = top_screen( state );
    const auto found = std::ranges::find( top.affordances, invoke.affordance_id, &affordance::id );
    if( found == top.affordances.end() ) {
        // Before the game offers anything, as the 0.1 lifecycle answers.
        const auto offers = state.ready.accepts_interaction_commands ||
                            state.ready.accepts_registered_actions;
        return std::unexpected( offers ? error::unknown_affordance : error::not_ready );
    }
    return resolve_affordance( { .state = state, .top = top, .found = *found, .args = invoke.args } );
}

command_lifecycle::command_lifecycle( std::string epoch, const command_authority &authority ) :
    epoch_( std::move( epoch ) ), authority_( authority ) {}

auto command_lifecycle::receive( const command_request &request ) ->
std::expected<command_result, error>
{
    if( const auto semantic = std::get_if<semantic_operation>( &request.operation );
        semantic && semantic->command.value.size() > maximum_inline_bytes ) {
        return std::unexpected( error::resource_limit );
    }
    if( !validate_shape( request ) ) { return std::unexpected( error::validation_failed ); }
    if( next_command_ == std::numeric_limits<counter>::max() || !valid_id( epoch_ ) ) {
        return std::unexpected( error::resource_limit );
    }
    ++next_command_;
    return command_result{
        .epoch = epoch_,
        .command_id = game_client::opaque_interaction_id( "command", {epoch_, std::to_string( next_command_ )} ),
    };
}
auto command_lifecycle::busy() const -> bool { return result_ && !terminal( result_->stage ); }
auto command_lifecycle::submit( command_request request ) -> std::expected<command_result, error>
{
    if( busy() ) { return std::unexpected( error::command_busy ); }
    auto received = receive( request );
    if( !received ) { return received; }
    request_ = std::move( request );
    result_ = *received;
    return received;
}
auto command_lifecycle::enqueue( command_request request ) -> std::expected<command_result, error>
{
    if( !busy() ) { return std::unexpected( error::invalid_lifecycle ); }
    // Only a screen expectation can outlive the boundary it was made at.
    if( !std::holds_alternative<invoke_operation>( request.operation ) || !request.expect.screen_id ||
        queue_.size() >= maximum_queued_commands ) { return std::unexpected( error::command_busy ); }
    auto received = receive( request );
    if( !received ) { return received; }
    queue_.push_back( { .result = *received, .request = std::move( request ) } );
    return received;
}
auto command_lifecycle::promote() -> std::optional<promoted_command>
{
    if( busy() || queue_.empty() ) { return std::nullopt; }
    auto next = std::move( queue_.front() );
    queue_.pop_front();
    request_ = next.request;
    result_ = next.result;
    return next;
}
auto command_lifecycle::drain( const error reason ) -> std::vector<command_result>
{
    auto ended = std::vector<command_result> {};
    for( auto &entry : std::exchange( queue_, {} ) ) {
        entry.result.stage = command_stage::interrupted;
        entry.result.failure = reason;
        ended.push_back( std::move( entry.result ) );
    }
    return ended;
}
auto command_lifecycle::validate( const clock_point &at, const boundary_state &current,
                                  const point screen_size ) -> std::expected<input_event, error>
{
    if( !result_ || result_->stage != command_stage::received ) {
        return std::unexpected( error::invalid_lifecycle );
    }
    const auto reject = [&]( const error reason, std::string message = {} ) ->
    std::expected<input_event, error> {
        result_->stage = command_stage::rejected;
        result_->failure = reason;
        result_->message = std::move( message );
        request_.reset();
        return std::unexpected( reason );
    };
    const auto &request = *request_;
    if( request.epoch != epoch_ || at.epoch != epoch_ ) { return reject( error::stale_epoch ); }
    const auto &expect = request.expect;
    // The native operation to run: the request's own, or the one an invoke resolves to.
    auto operation = request.operation;
    auto top = std::optional<screen_view> {};
    if( const auto invoke = std::get_if<invoke_operation>( &request.operation ) ) {
        top = top_screen( current );
        const auto found = std::ranges::find( top->affordances, invoke->affordance_id, &affordance::id );
        // A screen expectation holds while its screen is on top, however the boundary has moved since.
        if( expect.screen_id ) {
            if( *expect.screen_id != top->screen_id ) { return reject( error::stale_screen ); }
        } else {
            if( expect.revision != at.revision ) { return reject( error::stale_revision ); }
            if( expect.boundary_id != current.id || current.id != live_boundary_id( epoch_ ) ) {
                return reject( error::stale_boundary );
            }
        }
        if( found == top->affordances.end() ) {
            // Before the game offers anything, as the 0.1 lifecycle answers.
            return reject( current.ready.accepts_interaction_commands ||
                           current.ready.accepts_registered_actions ?
                           error::unknown_affordance : error::not_ready );
        }
        if( ( scope_of( *found ) == expect_scope::screen ) != expect.screen_id.has_value() ) {
            return reject( error::invalid_arguments );
        }
        auto native = resolve_affordance( { .state = current, .top = *top, .found = *found, .args = invoke->args } );
        if( !native ) { return reject( native.error() ); }
        operation = std::move( *native );
    } else {
        if( expect.screen_id ) { return reject( error::invalid_arguments ); }
        if( expect.revision != at.revision ) { return reject( error::stale_revision ); }
        if( expect.boundary_id != current.id || current.id != live_boundary_id( epoch_ ) ) {
            return reject( error::stale_boundary );
        }
        const auto live_schema = current.interaction ? std::optional{current.interaction->schema_id} :
                                 std::nullopt;
        if( expect.schema_id != live_schema ) { return reject( error::stale_interaction_schema ); }
    }
    const auto permissions = authority_.current_permissions();
    auto resolved = std::expected<input_event, std::string> { input_event{} };
    if( const auto semantic = std::get_if<semantic_operation>( &operation ) ) {
        if( !permissions.accepts_interaction_commands || !current.interaction ) { return reject( error::not_ready ); }
        auto command = semantic->command;
        command.input_id = game_client::current_input_id();
        if( semantic->target ) {
            // Absolute target to the live bubble; the native bounds and range still decide.
            command.position = to_bubble( *semantic->target, current_bubble_frame() );
            if( !command.position ) { return reject( error::validation_failed ); }
        }
        auto checked = game_client::resolve_checked_interaction( {
            .command = command,
            // An invoke was matched to its screen already and carries no schema.
.expected_schema = expect.schema_id ? std::optional<std::string_view>{*expect.schema_id} : std::nullopt,
.target_space = semantic->target ? std::optional<std::string_view>{current_bubble_frame().omt ? "omt" : "bubble_ms"} : std::nullopt,
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
    } else if( const auto travel = std::get_if<travel_operation>( &operation ) ) {
        if( !permissions.accepts_registered_actions ||
            game_client::active_input_context().category != "DEFAULTMODE" ) { return reject( error::not_ready ); }
        const auto click = travel_click( *travel );
        if( !click ) { return reject( error::validation_failed, _( "Travel rejected: that square is outside the map window the engine can resolve." ) ); }
        resolved = game_client::resolve_input_command( *click, screen_size );
        if( !resolved ) { return reject( error::validation_failed ); }
    } else if( const auto item = std::get_if<item_operation>( &operation ) ) {
        if( !permissions.accepts_registered_actions ||
            game_client::active_input_context().category != "DEFAULTMODE" ||
            get_avatar().is_dead_state() ) { return reject( error::not_ready ); }
        if( !item_operation_stack( *item ) ) {
            return reject( error::validation_failed,
                           item->drop ? _( "You do not have that many of that item to drop." ) :
                           _( "There are not that many of that item here to pick up." ) );
        }
        resolved = item_operation_event( *item );
    } else if( const auto screen = std::get_if<invoke_operation>( &operation ) ) {
        // An affordance the screen runs itself reaches its loop as a recorded input.
        if( !permissions.accepts_interaction_commands ) { return reject( error::not_ready ); }
        auto arguments = arguments_of( screen->args, current_bubble_frame() );
        if( !arguments ) { return reject( error::validation_failed ); }
        resolved->type = input_event_t::interaction;
        // The invoke names no schema: the screen's context stands in, so a recording says where it was made.
        resolved->interaction = game_client::interaction_event{
            .operation = game_client::interaction_operation::invoke,
            .schema_id = top->context.empty() ? top->role : top->context,
            .target_id = screen->affordance_id,
            .arguments = std::move( *arguments ),
        };
    } else {
        if( !permissions.accepts_registered_actions ) { return reject( error::not_ready ); }
        auto native = game_client::input_command{};
        native.action = std::get<registered_action>( operation ).id;
        native.input_id = game_client::current_input_id();
        resolved = game_client::resolve_input_command( native, screen_size );
        if( !resolved ) { return reject( error::validation_failed ); }
    }
    result_->stage = command_stage::validated;
    boundary_ = current.id;
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
        current.id == boundary_ ) {
        return std::unexpected( error::invalid_lifecycle );
    }
    if( at.epoch != epoch_ ) { return std::unexpected( error::stale_epoch ); }
    result_->stage = command_stage::completed;
    result_->at = at;
    request_.reset();
    return {};
}
auto command_lifecycle::interrupt( const error reason ) -> std::expected<void, error>
{
    if( !result_ || terminal( result_->stage ) ) { return std::unexpected( error::invalid_lifecycle ); }
    result_->stage = command_stage::interrupted;
    result_->failure = reason;
    request_.reset();
    return {};
}
auto command_lifecycle::result( const std::string &command_id ) const ->
std::expected<command_result, error>
{
    if( result_ && result_->command_id == command_id ) { return *result_; }
    // A waiting command has only its receipt.
    const auto waiting = std::ranges::find( queue_, command_id, []( const auto & entry ) { return entry.result.command_id; } );
    if( waiting == queue_.end() ) { return std::unexpected( error::unknown_command ); }
    return waiting->result;
}
} // namespace engine_client

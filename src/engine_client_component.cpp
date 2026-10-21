#include "engine_client_component.h"

#include "action.h"
#include "client_input.h"
#include "client_shortcut.h"
#include "editor/editor_access.h"
#include "game.h"
#include "input.h"
#include "translations.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <ranges>
#include <set>
#include <type_traits>
#include <utility>

namespace engine_client
{
namespace
{
using game_client::interaction_choice;
using game_client::interaction_snapshot;

struct live_screen {
    std::uint64_t serial = 0;
    const input_context *context = nullptr;
    std::string kind = {};
    std::string role = {};
    view_provider view = {};
};
/// Outermost first, as screens nest.
auto live_screens() -> std::vector<live_screen> & // *NOPAD*
{
    static auto screens = std::vector<live_screen> {};
    return screens;
}
auto mint_serial() -> std::uint64_t
{
    static auto last = std::uint64_t{0};
    return ++last;
}

/// Generated affordance ids hold a `:`, which no registered action id does.
auto choose_id( const std::string &component_id ) -> std::string { return "bn:choose:" + component_id; }

/// A list's `select` affordance only moves the cursor; the affordance that runs the row has this prefix
/// before the select's id, which `invoked` takes off again.
constexpr auto activate_prefix = std::string_view { "bn:activate:" };

/// The native name of the first of the actions `ids` that has one: the context's own, else the input
/// manager's default. Empty when native names none: never a word the adapter made up.
auto native_label( const boundary_state &state, const std::vector<std::string> &ids ) -> std::string
{
    const auto *const context = game_client::active_input_context().context;
    for( const auto &id : ids ) {
        const auto found = std::ranges::find( state.actions, id, &action::id );
        if( found != state.actions.end() ) { return found->name; }
        // get_action_name answers the id itself when native has no name for it.
        if( context != nullptr && context->get_action_name( id ) != id ) { return context->get_action_name( id ); }
    }
    return {};
}

/// What the adapter builds besides the tree: the affordances it places, in order.
struct build {
    const interaction_snapshot &native;
    const boundary_state &state;
    std::vector<affordance> affordances = {};
    auto add( affordance value ) -> std::string {
        affordances.push_back( std::move( value ) );
        return affordances.back().id;
    }
};

auto row_of( const interaction_choice &choice ) -> row
{
    return {
        .id = choice.identity.empty() ? choice.id : choice.identity,
.native_id = choice.identity.empty() ? std::string{} :
        choice.id, .label = choice.label, .description = choice.description, .denial = choice.denial,
        .hotkey = choice.hotkey, .enabled = choice.enabled, .selectable = choice.selectable,
        .selected = choice.selected, .columns = choice.columns, .favorite = choice.favorite,
        .color = choice.color, .key_color = choice.key_color, .markup = choice.markup,
        .parent_id = choice.parent_id, .nest_color = choice.nest_color, .mark = choice.mark,
        .selected_count = choice.selected_count, .minimum_count = choice.minimum_count,
        .max = choice.available_count, .toggled = choice.toggled,
    };
}

/// The registered action that opens a list's filter prompt.
constexpr auto filter_action = "FILTER";

auto text_part( std::string id, std::string markup ) -> component
{
    return { .id = std::move( id ), .body = text_body{ .markup = std::move( markup ) } };
}

auto make_list( build &b ) -> std::optional<component>
{
    const auto &native = b.native;
    // A filter that leaves no row still shows its list, which is where it is changed.
    if( native.choice_total == 0 && !native.filterable ) { return std::nullopt; }
    const auto id = std::string{ "list:choices" };
    auto result = list( { .id = id, .rows = native.choices | std::views::transform( row_of ) | std::ranges::to<std::vector>(),
                          .total = native.choice_total, .offset = native.choice_offset, .filterable = native.filterable } );
    if( native.filterable &&
        std::ranges::find( b.state.actions, filter_action, &action::id ) != b.state.actions.end() ) {
        result.affordances.push_back( filter_action );
    }
    if( std::get<list_body>( result.body ).selection != "none" ) {
        auto &body = std::get<list_body>( result.body );
        body.activate = b.add( { .id = choose_id( id ), .label = native_label( b.state, { "CONFIRM" } ),
                                 .args = { .row = row_shape{ .component_id = id } }, .runs = invoke_kind::choose } );
        result.affordances.push_back( body.activate );
    }
    return result;
}

auto make_editors( build &b ) -> std::optional<component>
{
    auto body = form_body{};
    for( const auto &choice : b.native.choices | std::views::filter( []( const auto & choice ) { return choice.editor.has_value(); } ) ) {
        const auto &editor = *choice.editor;
        body.fields.push_back( { .id = choice.id, .label = choice.label, .description = choice.description,
                                 .value = editor.value, .type = editor.type, .max_length = editor.max_length,
                                 .options = editor.values | std::views::transform( []( const auto & value )
        {
            return form_option{ .id = value.id, .label = value.label };
        } ) | std::ranges::to<std::vector>(),
        .min = editor.minimum, .max = editor.maximum } );
    }
    if( body.fields.empty() ) { return std::nullopt; }
    const auto id = std::string{ "form:editors" };
    body.submit = b.add( { .id = "bn:submit:" + id, .label = native_label( b.state, { "CONFIRM" } ),
                           .args = { .row = row_shape{ .component_id = id }, .text = text_shape{} }, .runs = invoke_kind::submit } );
    return component{ .id = id, .affordances = { body.submit }, .body = std::move( body ) };
}

auto make_field( build &b ) -> std::optional<component>
{
    if( !b.native.field ) { return std::nullopt; }
    const auto &field = *b.native.field;
    const auto id = std::string{ "form:field" };
    // The screen's own word for typing is the field's label.
    const auto fill = b.add( { .id = "bn:fill:" + id, .label = field.label,
                               .args = { .text = text_shape{ .field_id = field.id } }, .runs = invoke_kind::fill } );
    const auto submit = b.add( { .id = "bn:submit:" + id, .label = native_label( b.state, { "TEXT.CONFIRM", "CONFIRM" } ),
                                 .role = "confirm", .args = { .text = text_shape{ .field_id = field.id } },
                                 .runs = invoke_kind::submit } );
    return component{ .id = id, .affordances = { fill, submit }, .body = form_body{
            .fields = { {
                    .id = field.id, .label = field.label, .description = field.description, .value = field.value,
                    .type = field.type, .max_length = field.max_length, .printable = field.printable
                }
            },
            .submit = submit } };
}

auto native_affordances( build &b ) -> void
{
    const auto &native = b.native;
    if( native.allow_cancel ) {
        // The cancel is the native QUIT: it takes that action's keys and stands in for it.
        const auto quit = std::ranges::find_if( b.state.actions, []( const action & entry ) {
            return entry.id == "QUIT" || entry.id == "TEXT.QUIT";
        } );
        const auto found = quit != b.state.actions.end();
        b.add( { .id = "bn:cancel", .label = native_label( b.state, { "QUIT", "TEXT.QUIT" } ),
                 .keys = found ? quit->keys : std::vector<std::string> {}, .role = "cancel",
                 .runs = invoke_kind::cancel, .covers = found ? quit->id : std::string{} } );
    }
    if( !native.any_key_choice_id.empty() ) {
        // The screen's own word: the row any key acknowledges.
        const auto acknowledged = std::ranges::find( native.choices, native.any_key_choice_id,
                                  &interaction_choice::id );
        b.add( { .id = "bn:any_key", .label = acknowledged != native.choices.end() ? acknowledged->label : native_label( b.state, { "ANY_INPUT" } ),
                 .role = "any_key", .runs = invoke_kind::choose, .target = native.any_key_choice_id } );
    }
}

/// The role a registered action plays on every screen that has it: CONFIRM accepts, QUIT leaves, any key
/// acknowledges where a screen names that key, the cursor, paging, tab and scroll actions and the mouse's
/// moves and clicks only navigate, and the keybindings menu is the help. On the map the cursor keys are steps,
/// which navigate nothing.
auto registered_role( const std::string &id, const bool map ) -> std::string
{
    constexpr auto navigation = std::array{
        "UP", "DOWN", "LEFT", "RIGHT", "LEFTUP", "LEFTDOWN", "RIGHTUP", "RIGHTDOWN", "PAGE_UP", "PAGE_DOWN",
        "HOME", "END", "NEXT_TAB", "PREV_TAB", "NEXT_CATEGORY_TAB", "PREV_CATEGORY_TAB", "SCROLL_UP",
        "SCROLL_DOWN", "MOUSE_MOVE", "SELECT", "TEXT.LEFT", "TEXT.RIGHT", "TEXT.UP", "TEXT.DOWN", "TEXT.HOME", "TEXT.END",
        "TEXT.PAGE_UP", "TEXT.PAGE_DOWN", "LEFT_WIDE", "RIGHT_WIDE", "UP_WIDE", "DOWN_WIDE"
    };
    return id == "CONFIRM" ? "confirm" : id == "QUIT" ? "cancel" : id == "ANY_INPUT" ? "any_key" :
           id == "HELP_KEYBINDINGS" ? "help" : !map &&
           std::ranges::contains( navigation, id ) ? "navigate" : std::string{};
}

/// Whether the action is a button: only one native names, since the name of an unnamed action is its id.
auto offered( const action &entry ) -> bool { return entry.name != entry.id; }

/// Whether the registered action is one of the eight steps of the map.
auto is_step( const std::string &id ) -> bool
{
    constexpr auto steps = std::array{ ACTION_MOVE_FORTH, ACTION_MOVE_FORTH_RIGHT, ACTION_MOVE_RIGHT,
                                       ACTION_MOVE_BACK_RIGHT, ACTION_MOVE_BACK, ACTION_MOVE_BACK_LEFT, ACTION_MOVE_LEFT,
                                       ACTION_MOVE_FORTH_LEFT };
    return std::ranges::any_of( steps, [&]( const action_id step ) { return action_ident( step ) == id; } );
}

/// The context's actions as affordances; on the map its steps may be held.
auto registered_affordances( const boundary_state &state,
                             const bool map ) -> std::vector<affordance>
{
    return state.actions | std::views::filter( offered ) | std::views::transform( [&](
    const auto & entry ) {
        return affordance{ .id = entry.id, .label = entry.name, .keys = entry.keys, .role = registered_role( entry.id, map ), .target = entry.id,
                           .holdable = map && is_step( entry.id ) };
    } ) | std::ranges::to<std::vector>();
}

/// Whether the generic path describes the screen: a list of rows, a field and editors, with nothing a
/// screen of its own describes (panes, areas, a target, a drawn window).
auto generic( const interaction_snapshot &native ) -> bool
{
    using game_client::interaction_kind;
    return native.structured && !native.actions_only &&
           ( native.kind == interaction_kind::choices || native.kind == interaction_kind::field ) &&
           native.panes.empty() && native.areas.empty() && !native.target &&
    std::ranges::none_of( native.choices, []( const auto & choice ) {
        return choice.pane_id || choice.area_id;
    } );
}

/// The generic screen: message, field, list and editors, in reading order.
auto generic_view( const boundary_state &state ) -> screen_view
{
    const auto &native = *state.interaction;
    auto b = build{ .native = native, .state = state };
    auto parts = std::vector<component> {};
    const auto add = [&]( std::optional<component> part ) {
        if( part ) { parts.push_back( std::move( *part ) ); }
    };
    if( !native.message.empty() ) {
        parts.push_back( { .id = "text:message", .body = text_body{ .markup = native.message, .tone = native.message_tone } } );
    }
    add( make_field( b ) );
    // The row any key acknowledges is the `any_key` affordance, not a row to pick.
    const auto only_acknowledgement = !native.any_key_choice_id.empty() &&
    std::ranges::all_of( native.choices, [&]( const auto & choice ) { return choice.id == native.any_key_choice_id; } );
    if( !only_acknowledgement ) { add( make_list( b ) ); }
    add( make_editors( b ) );
    native_affordances( b );
    const auto focused = std::ranges::find( native.choices, native.focus_choice_id,
                                            &interaction_choice::id );
    const auto focus_row = focused == native.choices.end() ? native.focus_choice_id : row_of(
                               *focused ).id;
    auto result = screen_view{ .title = native.title, .affordances = std::move( b.affordances ),
                               .focus_row = focus_row, .refresh_ms = native.refresh_ms };
    result.root = parts.empty() ? text_part( "text:message",
                  "" ) : parts.size() == 1 ? std::move( parts.front() ) :
                  split( { .id = "split:root", .children = std::move( parts ) } );
    // Only a component the tree holds; an off-page row's list may not be there.
    const auto focus = !native.focus_choice_id.empty() ? "list:choices" : native.field ? "form:field" :
                       "";
    if( find_component( *result.root, focus ) != nullptr ) { result.focus_component = focus; }
    return result;
}

/// The map: the streamed world, the map's clicks and item rows, and its actions.
auto map_view( const boundary_state &state ) -> screen_view
{
    auto result = screen_view{
        .role = "map",
        .root = component{
            .id = "map:world", .affordances = { "bn:travel", "bn:context" },
            .body = map_body{
                .item_lists = {
                    { .id = ground_rows, .label = _( "Ground" ), .verb = _( "Pick up" ) },
                    { .id = inventory_rows, .label = _( "Inventory" ), .verb = _( "Drop" ) },
                } },
        },
        .affordances = {
            {
                .id = "bn:travel", .label = native_label( state, { "SELECT" } ), .args = { .pos = pos_shape{} },
                .runs = invoke_kind::travel
            },
            {
                .id = "bn:context", .label = native_label( state, { "SEC_SELECT" } ), .args = { .pos = pos_shape{} },
                .runs = invoke_kind::context
            },
            {
                .id = "bn:pickup", .label = native_label( state, { "pickup" } ),
                .args = { .row = row_shape{ .component_id = ground_rows }, .count = count_shape{ .min = 1 } },
                .runs = invoke_kind::pickup
            },
            {
                .id = "bn:drop", .label = native_label( state, { "drop" } ),
                .args = { .row = row_shape{ .component_id = inventory_rows }, .count = count_shape{ .min = 1 } },
                .runs = invoke_kind::drop
            },
        },
    };
    if( auto denial = editor::editor_denial(); !denial.empty() ) {
        result.denials.push_back( { .id = "editor", .reason = std::move( denial ) } );
    }
    return result;
}

/// The shortcut bar: every data-defined button whose action the map's input context names. The label and keys
/// are the map context's own, so a covered map still shows them; the button runs only while the map is on top.
auto shortcuts_of( const boundary_state &state, const bool top ) -> std::vector<shortcut>
{
    const auto context = get_default_mode_input_context();
    const auto registered = context.get_registered_actions_copy();
    const auto runnable = [&]( const std::string & id ) {
        return top && state.ready.accepts_registered_actions &&
        std::ranges::any_of( state.actions, [&]( const action & entry ) { return entry.id == id && offered( entry ); } );
    };
    return client_shortcuts::all()
    | std::views::filter( [&]( const client_shortcut & entry ) { return std::ranges::contains( registered, entry.action ); } )
    | std::views::transform( [&]( const client_shortcut & entry ) {
        return shortcut{ .affordance_id = entry.action, .label = context.get_action_name( entry.action ),
                         .keys = key_names( inp_mngr.get_input_for_action( entry.action, "DEFAULTMODE" ) ),
                         .icon = entry.icon, .glyph = entry.glyph, .enabled = runnable( entry.action ) };
    } )
    | std::views::filter( [&]( const shortcut & entry ) { return entry.label != entry.affordance_id; } )
    | std::ranges::to<std::vector>();
}

/// One entry of the stack before its content is read.
struct slot {
    std::string id = {};
    std::string kind = {};
    std::string role = {};
    std::string context = {};
    const view_provider *view = nullptr;
    bool map = false;
};

auto slots_of( const boundary_state &state ) -> std::vector<slot>
{
    auto result = std::vector<slot> {};
    if( state.ready.game_ready ) { result.push_back( { .id = "screen:map:" + state.epoch, .kind = "map", .role = "map", .map = true } ); }
    for( const auto &screen : live_screens() ) {
        result.push_back( { .id = "screen:" + std::to_string( screen.serial ), .kind = screen.kind, .role = screen.role,
                            .context = screen.context->category_name(), .view = &screen.view } );
    }
    // A view that reads input without a scope lives for one read: its id is new on every boundary, so a
    // key meant for the last one never lands on the next.
    const auto active = game_client::active_input_context();
    const auto scoped = active.context != nullptr &&
    std::ranges::any_of( live_screens(), [&]( const auto & screen ) { return screen.context == active.context; } );
    const auto beside_map = !active.category.empty() && active.category != "DEFAULTMODE";
    if( !scoped && ( state.interaction || result.empty() || beside_map ) ) {
        result.push_back( { .id = "screen:read:" + state.id, .role = state.ready.game_ready ? "dialog" : "screen",
                            .context = state.interaction ? state.interaction->context : std::string{} } );
    }
    return result;
}

auto clear_affordances( component &value ) -> void;
auto give_reason( bool enabled, std::string &denial ) -> void;
auto give_reasons( component &value ) -> void;

/// A list that activates through a `select` affordance gets an affordance of its own that runs the row,
/// so the select only moves the cursor. `label` is the native name of CONFIRM.
auto separate_activation( component &value, std::vector<affordance> &affordances,
                          const std::string &label ) -> void
{
    if( auto *const body = std::get_if<list_body>( &value.body ); body != nullptr &&
        !body->activate.empty() ) {
        const auto selecting = std::ranges::find_if( affordances, [&]( const affordance & entry ) {
            return entry.id == body->activate && entry.role == "select";
        } );
        if( selecting != affordances.end() ) {
            auto activating = *selecting;
            activating.id = std::string{ activate_prefix } + activating.id;
            activating.role.clear();
            activating.covers = "CONFIRM";
            activating.label = label.empty() ? activating.label : label;
            value.affordances.push_back( std::exchange( body->activate, activating.id ) );
            if( !std::ranges::contains( affordances, activating.id, &affordance::id ) ) { affordances.push_back( std::move( activating ) ); }
        }
    }
    for( auto &child : value.children ) { separate_activation( child, affordances, label ); }
}

/// The content of an entry. Only the top screen reads the 0.1 interaction, for it belongs to the view
/// that is reading input now, and only the top has affordances.
auto screen_of( const boundary_state &state, const slot &entry, const bool top ) -> screen_view
{
    auto result = screen_view{};
    auto described = true;
    const auto provided = entry.view != nullptr && *entry.view;
    if( entry.map ) {
        result = map_view( state );
    } else if( provided ) {
        result = ( *entry.view )( state );
    } else if( top && state.interaction && generic( *state.interaction ) ) {
        result = generic_view( state );
    } else {
        described = false;
    }
    result.screen_id = entry.id;
    result.kind = entry.kind;
    result.role = entry.role;
    result.context = entry.context;
    if( entry.map ) { result.shortcuts = shortcuts_of( state, top ); }
    if( !top ) {
        result.affordances.clear();
        if( result.root ) { clear_affordances( *result.root ); }
    } else if( described ) {
        // Enter on a row is the native CONFIRM; a screen that registers none has no Enter to offer.
        if( provided && result.root && std::ranges::contains( state.actions, "CONFIRM", &action::id ) ) {
            separate_activation( *result.root, result.affordances, native_label( state, { "CONFIRM" } ) );
        }
        // The registered actions of the context follow the screen's own affordances.
        auto registered = registered_affordances( state, entry.map );
        // An affordance that performs a registered action answers to its keys, unless it runs a row, which
        // the client runs from the row itself.
        for( auto &own : result.affordances ) {
            const auto covered = std::ranges::find( registered, own.covers, &affordance::id );
            if( own.keys.empty() && !own.args.row && !own.covers.empty() && covered != registered.end() ) { own.keys = covered->keys; }
        }
        // A registered action that an affordance of the screen performs is not offered a second time, unless it has a
        // denial. An affordance that runs a row performs it only while its list has a row to run.
        const auto performs = [&]( const affordance & own ) {
            if( !own.args.row || !result.root ) { return true; }
            const auto *const list = find_component( *result.root, own.args.row->component_id );
            const auto ids = list != nullptr ? row_ids( *list ) : std::nullopt;
            return list != nullptr && ( !ids || !ids->empty() );
        };
        std::erase_if( registered, [&]( const affordance & offered_action ) {
            return std::ranges::any_of( result.affordances, [&]( const affordance & own ) {
                return own.covers == offered_action.id && performs( own );
            } )&& !std::ranges::contains( result.denials, offered_action.id, &denial::id );
        } );
        for( const auto &[id, reason] : result.denials ) {
            const auto found = std::ranges::find( registered, id, &affordance::id );
            if( found != registered.end() ) { found->enabled = false; found->denial = reason; }
        }
        // A role has one holder: the screen's own affordance keeps it over the registered action of that role.
        for( auto &offered_action : registered ) {
            if( std::ranges::contains( result.affordances, offered_action.role, &affordance::role ) ) { offered_action.role.clear(); }
        }
        std::ranges::move( registered, std::back_inserter( result.affordances ) );
    }
    for( auto &offered : result.affordances ) { give_reason( offered.enabled, offered.denial ); }
    if( result.root ) { give_reasons( *result.root ); }
    return result;
}

/// A covered screen offers nothing, so its components name no affordance either.
auto clear_affordances( component &value ) -> void
{
    value.affordances.clear();
    if( auto *const body = std::get_if<list_body>( &value.body ) ) { body->activate.clear(); }
    if( auto *const body = std::get_if<tabs_body>( &value.body ) ) { body->select.clear(); }
    if( auto *const body = std::get_if<form_body>( &value.body ) ) { body->submit.clear(); }
    for( auto &child : value.children ) { clear_affordances( child ); }
}

/// Says why the player cannot pick something that is off, where the screen gave no reason of its own.
auto give_reason( const bool enabled, std::string &denial ) -> void
{
    if( !enabled && denial.empty() ) { denial = _( "Not available now." ); }
}

/// Every row, tab and cell under `value` that is off and could be picked says why; a row that cannot be
/// picked, such as a heading, needs no reason.
auto give_reasons( component &value ) -> void
{
    if( auto *const body = std::get_if<list_body>( &value.body ) ) {
        for( auto &entry : body->rows | std::views::filter( &row::selectable ) ) { give_reason( entry.enabled, entry.denial ); }
    }
    if( auto *const body = std::get_if<tabs_body>( &value.body ) ) {
        for( auto &tab : body->tabs ) { give_reason( tab.enabled, tab.denial ); }
    }
    if( auto *const body = std::get_if<grid_body>( &value.body ) ) {
        for( auto &cell : body->cells ) { give_reason( cell.enabled, cell.denial ); }
    }
    for( auto &child : value.children ) { give_reasons( child ); }
}

auto collect_rows( const component &value, std::vector<std::string> &into ) -> bool
{
    const auto own = std::visit( [&]<typename Body>( const Body & body ) {
        if constexpr( std::is_same_v<Body, list_body> ) {
            if( body.offset != 0 || body.total != body.rows.size() ) { return false; }
            // A heading, and a row the screen does not let be picked, is no row an affordance names.
            std::ranges::transform( body.rows | std::views::filter( []( const row & entry ) { return !entry.header && entry.selectable; } ),
            std::back_inserter( into ), &row::id );
        } else if constexpr( std::is_same_v<Body, tabs_body> ) {
            std::ranges::transform( body.tabs | std::views::filter( &tab_entry::enabled ),
                                    std::back_inserter( into ), &tab_entry::id );
        } else if constexpr( std::is_same_v<Body, grid_body> ) {
            std::ranges::transform( body.cells | std::views::filter( &grid_cell::enabled ),
                                    std::back_inserter( into ), &grid_cell::id );
        } else if constexpr( std::is_same_v<Body, form_body> ) {
            std::ranges::transform( body.fields, std::back_inserter( into ), &form_field::id );
        } else if constexpr( std::is_same_v<Body, map_body> ) {
            if( body.target ) {
                std::ranges::transform( body.target->candidates, std::back_inserter( into ),
                                        &game_client::interaction_target_candidate::id );
            }
        }
        return true;
    }, value.body );
    // A pane or a split holds the rows of the lists inside it; the page of a tab strip is not its rows.
    const auto holds_parts = std::holds_alternative<split_body>( value.body ) ||
                             std::holds_alternative<panel_body>( value.body );
    return own && ( !holds_parts ||
    std::ranges::all_of( value.children, [&]( const component & child ) { return collect_rows( child, into ); } ) );
}
} // namespace

auto scope_of( const affordance &value ) -> expect_scope
{
    const auto &args = value.args;
    return args.row || args.rows || args.pos ||
           args.text ? expect_scope::boundary : expect_scope::screen;
}

auto component_kind( const component &value ) -> std::string
{
    constexpr auto names = std::array{ "list", "tabs", "split", "panel", "form", "text", "grid", "map", "canvas" };
    static_assert( names.size() == std::variant_size_v<component_body> );
    return names[value.body.index()];
}

auto find_component( const component &root,
                     const std::string_view id ) -> const component * // *NOPAD*
{
    if( root.id == id ) { return &root; }
    for( const auto &child : root.children ) {
        if( const auto *const found = find_component( child, id ) ) { return found; }
    }
    return nullptr;
}

auto row_ids( const component &value ) -> std::optional<std::vector<std::string>>
{
    auto result = std::vector<std::string> {};
    if( !collect_rows( value, result ) ) { return std::nullopt; }
    return result;
}

auto list( list_options options ) -> component
{
    auto body = list_body{ .rows = std::move( options.rows ), .offset = options.offset, .activate = options.activate,
                           .filterable = options.filterable };
    body.total = std::max( options.total.value_or( 0 ), options.offset + body.rows.size() );
    auto seen = std::set<std::string> {};
    for( const auto &entry : body.rows ) {
        for( const auto &column : entry.columns ) {
            if( seen.insert( column.key ).second ) { body.columns.push_back( { .key = column.key, .label = column.label } ); }
        }
    }
    if( options.selection.empty() ) {
        // A list with no row on this page is still picked from, by rows read later.
        const auto selectable = body.rows.empty() || std::ranges::any_of( body.rows, &row::selectable );
        const auto counted = std::ranges::any_of( body.rows, []( const row & entry ) {
            return entry.mark || entry.selected_count || entry.max;
        } );
        body.selection = !selectable ? "none" : counted ? "multi" : "single";
    } else {
        body.selection = std::move( options.selection );
    }
    auto result = component{ .id = std::move( options.id ) };
    if( !body.activate.empty() ) { result.affordances.push_back( body.activate ); }
    result.body = std::move( body );
    return result;
}

auto panel( panel_options options ) -> component
{
    auto result = component{ .id = std::move( options.id ), .body = panel_body{
            .title = std::move( options.title ), .subtitle = std::move( options.subtitle ),
            .stats = std::move( options.stats ), .filter = std::move( options.filter ) } };
    if( options.child ) { result.children.push_back( std::move( *options.child ) ); }
    return result;
}

auto split( split_options options ) -> component
{
    return { .id = std::move( options.id ), .body = split_body{ .active = std::move( options.active ),
                                                                .relation = std::move( options.relation ) },
             .children = std::move( options.children ) };
}

auto tabs( tabs_options options ) -> component
{
    auto result = component{ .id = std::move( options.id ), .body = tabs_body{ .tabs = std::move( options.tabs ), .select = options.select },
                             .children = children_of( std::move( options.child ) ) };
    if( !options.select.empty() ) { result.affordances.push_back( options.select ); }
    return result;
}

auto invoke_affordance( invoke_options options ) -> affordance
{
    return {
        .id = std::move( options.id ), .label = std::move( options.label ), .keys = std::move( options.keys ),
        .enabled = options.enabled, .denial = std::move( options.denial ), .role = std::move( options.role ),
        .args = std::move( options.args ), .runs = invoke_kind::screen, .covers = std::move( options.covers ),
    };
}

auto invoked( const input_event &event ) -> std::optional<invoked_affordance>
{
    const auto &semantic = event.interaction;
    if( !semantic || semantic->operation != game_client::interaction_operation::invoke ||
        !semantic->arguments ) { return std::nullopt; }
    const auto activates = semantic->target_id.starts_with( activate_prefix );
    return invoked_affordance{ .affordance_id = semantic->target_id.substr( activates ? activate_prefix.size() : 0 ),
                               .activates = activates, .args = *semantic->arguments };
}

screen_scope::screen_scope( const input_context &context, std::string kind,
                            screen_options options ) :
    serial_( mint_serial() )
{
    live_screens().push_back( { .serial = serial_, .context = &context, .kind = std::move( kind ), .role = std::move( options.role ),
                                .view = std::move( options.view ) } );
}

screen_scope::~screen_scope()
{
    auto &screens = live_screens();
    const auto found = std::ranges::find( screens, serial_, &live_screen::serial );
    if( found != screens.end() ) { screens.erase( found ); }
}

auto interaction_stack( const boundary_state &state ) -> std::vector<screen_view>
{
    const auto slots = slots_of( state );
    auto result = std::vector<screen_view> {};
    result.reserve( slots.size() );
    for( const auto &[index, entry] : std::views::enumerate( slots ) ) {
        result.push_back( screen_of( state, entry,
                                     static_cast<std::size_t>( index ) + 1 == slots.size() ) );
    }
    return result;
}

auto top_screen( const boundary_state &state ) -> screen_view
{
    return screen_of( state, slots_of( state ).back(), true );
}

auto native_row_id( const std::string &row_id ) -> std::optional<std::string>
{
    for( auto offset = std::size_t{ 0 };; offset += maximum_rows ) {
        const auto native = game_client::current_interaction( { .offset = offset, .limit = maximum_rows } );
        if( !native.structured || native.choices.empty() ) { return std::nullopt; }
        for( const auto &choice : native.choices ) {
            const auto found = row_of( choice );
            if( found.id == row_id ) {
                return found.native_id.empty() ? std::nullopt : std::optional{ found.native_id };
            }
        }
    }
}

auto read_rows( const rows_request &request,
                const boundary_state &state ) -> std::expected<rows_page, error>
{
    if( request.epoch != state.epoch ) { return std::unexpected( error::stale_epoch ); }
    if( request.boundary_id != state.id ) { return std::unexpected( error::stale_boundary ); }
    if( request.limit == 0 || request.limit > maximum_rows ) { return std::unexpected( error::resource_limit ); }
    const auto top = top_screen( state );
    const auto *const found = top.root ? find_component( *top.root, request.component_id ) : nullptr;
    const auto *const held = found ? std::get_if<list_body>( &found->body ) : nullptr;
    if( held == nullptr ) { return std::unexpected( error::validation_failed ); }
    auto result = rows_page{ .boundary_id = state.id, .component_id = request.component_id, .total = held->total };
    if( !row_ids( *found ) && found->id == "list:choices" ) {
        // The generic list holds one page of a longer native list: the rest is read from the screen.
        auto native = game_client::current_interaction( { .offset = request.offset, .limit = request.limit } );
        if( !native.structured ) { return std::unexpected( error::not_ready ); }
        result.total = native.choice_total;
        result.rows = native.choices | std::views::transform( row_of ) | std::ranges::to<std::vector>();
        return result;
    }
    // Rows the list holds start at its own offset.
    const auto size = held->rows.size();
    const auto first = std::min( request.offset > held->offset ? request.offset - held->offset : 0,
                                 size );
    const auto last = std::max( first, std::min( request.offset + request.limit > held->offset ?
                                request.offset + request.limit - held->offset : 0, size ) );
    result.rows.assign( held->rows.begin() + first, held->rows.begin() + last );
    return result;
}

} // namespace engine_client

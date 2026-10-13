#include "engine_client_component.h"

#include "client_input.h"
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
using game_client::interaction_pane;
using game_client::interaction_snapshot;

struct published_view {
    const input_context *context = nullptr;
    view_provider provider = {};
};
/// Innermost last, as screens nest.
auto published_views() -> std::vector<published_view> & // *NOPAD*
{
    static auto views = std::vector<published_view> {};
    return views;
}

/// Generated affordance ids hold a `:`, which no registered action id does.
auto choose_id( const std::string &component_id ) -> std::string { return "bn:choose:" + component_id; }
auto set_count_id( const std::string &component_id ) -> std::string { return "bn:set_count:" + component_id; }

auto tab_row( const interaction_choice &choice ) -> bool { return choice.id.starts_with( "tab:" ); }
auto subtab_row( const interaction_choice &choice ) -> bool { return choice.id.starts_with( "subtab:" ); }

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

/// Children moved in; a braced list would copy whole subtrees.
template<typename... Parts>
auto children_of( Parts &&... parts ) -> std::vector<component>
{
    auto result = std::vector<component> {};
    result.reserve( sizeof...( parts ) );
    ( result.push_back( std::forward<Parts>( parts ) ), ... );
    return result;
}
auto lines_of( const std::vector<std::string> &lines ) -> std::string
{
    return lines | std::views::join_with( '\n' ) | std::ranges::to<std::string>();
}

auto text_part( std::string id, std::string markup ) -> component
{
    return { .id = std::move( id ), .body = text_body{ .markup = std::move( markup ) } };
}

struct list_options {
    std::string id;
    std::vector<interaction_choice> rows;
    /// Rows of the full list, and how many of them come before `rows`.
    std::size_t total = 0;
    std::size_t offset = 0;
};
auto make_list( build &b, const list_options &opts ) -> component
{
    const auto &id = opts.id;
    const auto &rows = opts.rows;
    auto body = list_body{ .total = opts.total, .offset = opts.offset };
    auto seen = std::set<std::string> {};
    for( const auto &choice : rows ) {
        body.rows.push_back( { .choice = choice } );
        for( const auto &column : choice.columns ) {
            if( seen.insert( column.key ).second ) { body.columns.push_back( { .key = column.key, .label = column.label } ); }
        }
    }
    // A list with no row on this page is still picked from, by rows read later.
    const auto selectable = rows.empty() ||
                            std::ranges::any_of( rows, &interaction_choice::selectable );
    const auto counted = std::ranges::any_of( rows, []( const auto & choice ) {
        return choice.mark || choice.selected_count || choice.available_count;
    } );
    body.selection = !selectable ? "none" : !counted ? "single" : b.native.allow_set_count ? "count" :
                     "multi";
    auto result = component{ .id = id };
    if( selectable ) {
        body.activate = b.add( { .id = choose_id( id ), .label = _( "Choose" ), .args = { .row = row_shape{ .component_id = id } },
                                 .runs = invoke_kind::choose } );
        result.affordances.push_back( body.activate );
    }
    if( b.native.allow_set_count && selectable ) {
        result.affordances.push_back( b.add( { .id = set_count_id( id ), .label = _( "Set count" ),
                                               .args = { .row = row_shape{ .component_id = id }, .count = count_shape{ .min = 0 } },
                                               .runs = invoke_kind::set_count } ) );
    }
    result.body = std::move( body );
    return result;
}

struct tabs_options {
    std::string id;
    std::vector<interaction_choice> rows;
    component child;
};
auto make_tabs( build &b, tabs_options opts ) -> component
{
    const auto &id = opts.id;
    auto body = tabs_body{};
    for( const auto &choice : opts.rows ) {
        body.tabs.push_back( { .id = choice.id, .label = choice.label, .selected = choice.selected, .enabled = choice.enabled } );
    }
    body.select = b.add( { .id = choose_id( id ), .label = _( "Switch tab" ), .args = { .row = row_shape{ .component_id = id } },
                           .runs = invoke_kind::choose } );
    return { .id = id, .affordances = { body.select }, .body = std::move( body ), .children = children_of( std::move( opts.child ) ) };
}

auto totals_stats( const game_client::interaction_totals &totals ) -> std::vector<panel_stat>
{
    return {
        {
            .key = "weight", .label = _( "Weight" ), .value = totals.weight, .max = totals.weight_max,
            .unit = totals.weight_unit, .tone = totals.over_weight ? "bad" : ""
        },
        {
            .key = "volume", .label = _( "Volume" ), .value = totals.volume, .max = totals.volume_max,
            .unit = totals.volume_unit, .tone = totals.over_volume ? "bad" : ""
        },
    };
}

auto pane_has_facts( const interaction_pane &pane ) -> bool
{
    return !pane.area_label.empty() || !pane.area_description.empty() || !pane.filter.empty() ||
           pane.totals ||
           !pane.sort.empty();
}

auto make_panel( const interaction_pane &pane, std::optional<component> list ) -> component
{
    auto body = panel_body{ .title = pane.label, .subtitle = pane.area_label, .filter = pane.filter };
    if( pane.totals ) { body.stats = totals_stats( *pane.totals ); }
    if( !pane.sort.empty() ) { body.stats.push_back( { .key = "sort", .label = _( "Sort" ), .value = pane.sort } ); }
    auto result = component{ .id = "panel:" + pane.id, .body = std::move( body ) };
    auto description = pane.area_description.empty() ? std::nullopt :
                       std::optional{ text_part( "text:" + pane.id, pane.area_description ) };
    if( description && list ) {
        result.children.push_back( { .id = "split:" + pane.id, .body = split_body{ .relation = "detail" },
                                     .children = children_of( std::move( *description ), std::move( *list ) ) } );
    } else if( description || list ) {
        result.children.push_back( std::move( description ? *description : *list ) );
    }
    return result;
}

/// The rows grouped by pane into lists and panels, under the tab rows: the selected tab's content.
auto make_body( build &b ) -> std::optional<component>
{
    const auto &native = b.native;
    auto keys = std::vector<std::string> {};
    auto groups = std::vector<std::vector<interaction_choice>> {};
    const auto group_of = [&]( const std::string & key ) -> std::vector<interaction_choice> & {
        const auto found = std::ranges::find( keys, key );
        if( found != keys.end() ) { return groups[static_cast<std::size_t>( found - keys.begin() )]; }
        keys.push_back( key );
        return groups.emplace_back();
    };
    for( const auto &pane : native.panes ) { group_of( pane.id ); }
    for( const auto &choice : native.choices | std::views::filter( []( const auto & choice ) {
    return !tab_row( choice ) && !subtab_row( choice );
    } ) ) {
        group_of( choice.pane_id.value_or( "" ) ).push_back( choice );
    }
    // A pane's list counts its rows in the full list, not only those on this page. Tab rows share the
    // pane of their category, and come first, so only those on the page are taken off.
    const auto list_of = [&]( const std::string & key, const std::vector<interaction_choice> &rows ) {
        const auto group = native.groups.find( key );
        const auto tabs_here = static_cast<std::size_t>( std::ranges::count_if( native.choices, [&](
        const auto & choice ) {
            return ( tab_row( choice ) || subtab_row( choice ) ) && choice.pane_id.value_or( "" ) == key;
        } ) );
        const auto total = group == native.groups.end() ? rows.size() : group->second.total - std::min(
                               tabs_here, group->second.total );
        const auto offset = group == native.groups.end() || rows.empty() ? std::size_t{0} :
                            group->second.offset;
        return total == 0 ? std::nullopt : std::optional{ make_list( b, {
                .id = key.empty() ? "list" : "list:" + key, .rows = rows, .total = std::max( total, offset + rows.size() ),
                .offset = offset
            } ) };
    };
    auto parts = std::vector<component> {};
    auto active = std::string{};
    for( const auto &[key, rows] : std::views::zip( keys, groups ) ) {
        const auto pane = std::ranges::find( native.panes, key, &interaction_pane::id );
        auto list = list_of( key, rows );
        if( pane == native.panes.end() ) {
            if( list ) { parts.push_back( std::move( *list ) ); }
            continue;
        }
        // A tab page that is not shown has neither rows nor facts; its tab row stands for it.
        if( !list && !pane_has_facts( *pane ) ) { continue; }
        parts.push_back( make_panel( *pane, std::move( list ) ) );
        if( native.focus_pane_id == pane->id ) { active = parts.back().id; }
    }
    auto body = parts.empty() ? std::nullopt : parts.size() == 1 ? std::optional{ std::move( parts.front() ) } :
                std::optional{ component{ .id = "body", .body = split_body{ .active = active }, .children = std::move( parts ) } };
    const auto rows_where = [&]( const auto & predicate ) {
        return native.choices | std::views::filter( predicate ) | std::ranges::to<std::vector>();
    };
    for( const auto &[id, is_tab] : { std::pair{ "subtabs", &subtab_row }, std::pair{ "tabs", &tab_row } } ) {
        auto rows = rows_where( is_tab );
        if( rows.empty() ) { continue; }
        auto child = body ? std::move( *body ) : text_part( "empty", "" );
        body = make_tabs( b, { .id = id, .rows = std::move( rows ), .child = std::move( child ) } );
    }
    return body;
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
    const auto id = std::string{ "editors" };
    body.submit = b.add( { .id = "bn:submit:" + id, .label = _( "Set" ),
                           .args = { .row = row_shape{ .component_id = id }, .text = text_shape{} }, .runs = invoke_kind::submit } );
    return component{ .id = id, .affordances = { body.submit }, .body = std::move( body ) };
}

auto make_field( build &b ) -> std::optional<component>
{
    if( !b.native.field ) { return std::nullopt; }
    const auto &field = *b.native.field;
    const auto id = std::string{ "field" };
    const auto fill = b.add( { .id = "bn:fill:" + id, .label = _( "Type" ),
                               .args = { .text = text_shape{ .field_id = field.id } }, .runs = invoke_kind::fill } );
    const auto submit = b.add( { .id = "bn:submit:" + id, .label = _( "Confirm" ), .role = "confirm",
                                 .args = { .text = text_shape{ .field_id = field.id } }, .runs = invoke_kind::submit } );
    return component{ .id = id, .affordances = { fill, submit }, .body = form_body{
            .fields = { {
                    .id = field.id, .label = field.label, .description = field.description, .value = field.value,
                    .type = field.type, .max_length = field.max_length, .printable = field.printable
                }
            },
            .submit = submit } };
}

auto make_map( build &b ) -> std::optional<component>
{
    const auto &native = b.native;
    if( !native.target && !native.overmap ) { return std::nullopt; }
    const auto id = std::string{ "map" };
    auto result = component{ .id = id, .body = map_body{ .mode = native.overmap || b.state.frame.omt ? "overmap" : "world",
                                                         .target = native.target, .overmap = native.overmap } };
    if( native.target ) {
        result.affordances.push_back( b.add( { .id = "bn:set_target", .label = _( "Move the cursor" ),
                                               .args = { .pos = pos_shape{ .unit = b.state.frame.omt ? "overmap" : "square" } },
                                               .runs = invoke_kind::set_target } ) );
        if( !native.target->candidates.empty() ) {
            result.affordances.push_back( b.add( { .id = "bn:target_candidate", .label = _( "Target" ),
                                                   .args = { .row = row_shape{ .component_id = id } }, .runs = invoke_kind::target_candidate } ) );
        }
    }
    return result;
}

auto make_areas( build &b ) -> std::optional<component>
{
    const auto &native = b.native;
    if( native.areas.empty() ) { return std::nullopt; }
    const auto active = std::ranges::find( native.panes, native.focus_pane_id.value_or( "" ),
                                           &interaction_pane::id );
    const auto blocked = [&]( const std::string & action ) {
        return active != native.panes.end() && std::ranges::contains( active->blocked_areas, action );
    };
    // A list, not a grid: the picker's character-cell layout is presentation (lane F3 drops it).
    // TODO(F3): row ids become the stable area ids, and the pane's area is marked by id.
    const auto id = std::string{ "areas" };
    auto body = list_body{ .total = native.areas.size(), .selection = "single" };
    for( const auto &area : native.areas ) {
        const auto open = area.available && !blocked( area.action_id );
        body.rows.push_back( { .choice = { .id = area.action_id, .label = area.label, .enabled = open } } );
    }
    body.activate = b.add( { .id = choose_id( id ), .label = _( "Choose" ), .args = { .row = row_shape{ .component_id = id } },
                             .runs = invoke_kind::row_action } );
    return component{ .id = id, .affordances = { body.activate }, .body = std::move( body ) };
}

auto make_vehicle( build &b ) -> std::optional<component>
{
    if( !b.native.vehicle ) { return std::nullopt; }
    const auto &veh = *b.native.vehicle;
    // The cursor square is on the grid even when no part is on it.
    auto xs = veh.cells | std::views::transform( &game_client::interaction_vehicle_cell::dx ) |
              std::ranges::to<std::vector>();
    auto ys = veh.cells | std::views::transform( &game_client::interaction_vehicle_cell::dy ) |
              std::ranges::to<std::vector>();
    xs.push_back( 0 );
    ys.push_back( 0 );
    const auto [min_x, max_x] = std::ranges::minmax( xs );
    const auto [min_y, max_y] = std::ranges::minmax( ys );
    auto grid = grid_body{ .cols = max_x - min_x + 1, .rows = max_y - min_y + 1, .cursor = grid_point{ .x = -min_x, .y = -min_y } };
    for( const auto &cell : veh.cells ) {
        const auto cursor = cell.dx == 0 && cell.dy == 0;
        grid.cells.push_back( { .at = { .x = cell.dx - min_x, .y = cell.dy - min_y }, .glyph = cell.glyph, .color = cell.color,
                                .enabled = !( cursor && veh.blocked ), .selected = cursor } );
    }
    auto parts = list_body{ .columns = { { .key = "kind", .label = _( "Kind" ) }, { .key = "health", .label = _( "Health" ) } },
        .total = veh.parts.size() };
    // The native list marks structure with [ ] and armor with ( ). A key, not a word to show.
    const auto kind = []( const game_client::interaction_vehicle_part & part ) -> std::string {
        return part.open == "[" ? "structure" : part.open == "(" ? "armor" : "other";
    };
    for( const auto &[index, part] : std::views::enumerate( veh.parts ) ) {
        parts.rows.push_back( { .choice = { .id = "vehicle-part:" + std::to_string( index ), .label = part.name,
                                            .enabled = !part.broken, .selectable = false,
        .columns = {
            { .key = "kind", .label = _( "Kind" ), .value = kind( part ) },
            { .key = "health", .label = _( "Health" ), .value = std::to_string( part.health ) }
        }
                                          } } );
    }
    auto children = children_of(
                        component{ .id = "vehicle:grid", .body = std::move( grid ) },
                        component{ .id = "vehicle:parts", .body = std::move( parts ) },
                        text_part( "vehicle:stats", lines_of( veh.stats ) ) );
    if( veh.detail ) {
        const auto &detail = *veh.detail;
        auto info = panel_body{ .title = detail.name };
        if( !detail.time.empty() ) { info.stats.push_back( { .key = "time", .label = detail.time_heading, .value = detail.time } ); }
        for( const auto &[index, stat] : std::views::enumerate( detail.stats ) ) {
            info.stats.push_back( { .key = "stat:" + std::to_string( index ), .label = stat.label, .value = stat.value } );
        }
        auto needs = list_body{};
        for( const auto &group : detail.groups ) {
            needs.rows.push_back( { .choice = { .id = "group:" + group.kind, .label = group.heading, .selectable = false }, .header = true } );
            const auto before = needs.rows.size();
            for( const auto &[index, need] : std::views::enumerate( detail.requirements ) ) {
                if( need.kind != group.kind ) { continue; }
                needs.rows.push_back( { .choice = { .id = "need:" + std::to_string( index ), .label = need.text,
                                                    .enabled = need.met, .selectable = false
                                                  } } );
            }
            if( needs.rows.size() == before && group.empty_text.empty() ) { needs.rows.pop_back(); }
            else if( needs.rows.size() == before ) {
                needs.rows.push_back( { .choice = { .id = "need:none:" + group.kind, .label = group.empty_text, .selectable = false } } );
            }
        }
        needs.total = needs.rows.size();
        children.push_back( { .id = "vehicle:detail", .body = std::move( info ), .children = children_of( component{
                .id = "vehicle:detail:body", .body = split_body{ .relation = "detail" }, .children = children_of(
                    text_part( "vehicle:description", detail.description ),
                component{ .id = "vehicle:requirements", .body = std::move( needs ) } ) } ) } );
    }
    auto panel = panel_body{ .title = veh.name, .subtitle = veh.verb, .filter = veh.tabs ? veh.tabs->filter : "" };
    if( !veh.owner.empty() ) { panel.stats.push_back( { .key = "owner", .label = _( "Owner" ), .value = veh.owner } ); }
    return component{ .id = "vehicle", .body = std::move( panel ), .children = children_of( component{
            .id = "vehicle:body", .body = split_body{}, .children = std::move( children ) } ) };
}

auto native_affordances( build &b ) -> void
{
    const auto &native = b.native;
    if( native.allow_cancel ) {
        b.add( { .id = "bn:cancel", .label = _( "Cancel" ), .role = "cancel", .runs = invoke_kind::cancel } );
    }
    if( !native.any_key_choice_id.empty() ) {
        b.add( { .id = "bn:any_key", .label = _( "Continue" ), .role = "any_key", .runs = invoke_kind::choose,
                 .target = native.any_key_choice_id } );
    }
}

auto registered_affordances( const boundary_state &state ) -> std::vector<affordance>
{
    return state.actions | std::views::transform( [&]( const auto & entry ) {
        return affordance{ .id = entry.id, .label = entry.name, .keys = entry.keys, .target = entry.id };
    } ) | std::ranges::to<std::vector>();
}

/// The map boundary: the streamed world, the map's clicks and item rows, and its actions.
auto map_view( const boundary_state &state ) -> interaction_view
{
    auto result = interaction_view{
        .root = { .id = "map", .body = map_body{} },
        .affordances = {
            { .id = "bn:travel", .label = _( "Travel" ), .args = { .pos = pos_shape{} }, .runs = invoke_kind::travel },
            { .id = "bn:context", .label = _( "Examine" ), .args = { .pos = pos_shape{} }, .runs = invoke_kind::context },
            {
                .id = "bn:pickup", .label = _( "Pick up" ),
                .args = { .row = row_shape{ .component_id = ground_rows }, .count = count_shape{ .min = 1 } }, .runs = invoke_kind::pickup
            },
            {
                .id = "bn:drop", .label = _( "Drop" ),
                .args = { .row = row_shape{ .component_id = inventory_rows }, .count = count_shape{ .min = 1 } }, .runs = invoke_kind::drop
            },
        },
    };
    result.root.affordances = { "bn:travel", "bn:context" };
    std::ranges::move( registered_affordances( state ), std::back_inserter( result.affordances ) );
    return result;
}

auto focus_component( const interaction_snapshot &native ) -> std::string
{
    const auto focus = std::ranges::find( native.choices, native.focus_choice_id,
                                          &interaction_choice::id );
    if( native.focus_choice_id.empty() ) { return {}; }
    if( focus != native.choices.end() && tab_row( *focus ) ) { return "tabs"; }
    if( focus != native.choices.end() && subtab_row( *focus ) ) { return "subtabs"; }
    const auto pane = focus != native.choices.end() ? focus->pane_id : native.focus_pane_id;
    return pane && !pane->empty() ? "list:" + *pane : "list";
}

auto collect_rows( const component &value, std::vector<std::string> &into ) -> bool
{
    return std::visit( [&]<typename Body>( const Body & body ) {
        if constexpr( std::is_same_v<Body, list_body> ) {
            if( body.offset != 0 || body.total != body.rows.size() ) { return false; }
            std::ranges::transform( body.rows, std::back_inserter( into ), []( const auto & row ) { return row.choice.id; } );
        } else if constexpr( std::is_same_v<Body, tabs_body> ) {
            std::ranges::transform( body.tabs, std::back_inserter( into ), &tab_entry::id );
        } else if constexpr( std::is_same_v<Body, grid_body> ) {
            std::ranges::transform( body.cells, std::back_inserter( into ), &grid_cell::id );
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
}
} // namespace

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

view_scope::view_scope( const input_context &context, view_provider provider ) : context( &context )
{
    published_views().push_back( { .context = &context, .provider = std::move( provider ) } );
}

view_scope::~view_scope()
{
    auto &views = published_views();
    const auto found = std::ranges::find_last( views, context, &published_view::context );
    if( !found.empty() ) { views.erase( found.begin() ); }
}

auto interaction_components( const boundary_state &state ) -> std::optional<interaction_view>
{
    if( const auto *const active = game_client::active_input_context().context ) {
        const auto published = std::ranges::find_last( published_views(), active,
                               &published_view::context );
        if( !published.empty() ) {
            auto result = published.front().provider( state );
            std::ranges::move( registered_affordances( state ), std::back_inserter( result.affordances ) );
            return result;
        }
    }
    if( !state.interaction ) {
        if( state.ready.game_ready ) { return map_view( state ); }
        if( state.actions.empty() ) { return std::nullopt; }
        return interaction_view{ .root = text_part( "message", "" ), .affordances = registered_affordances( state ) };
    }
    const auto &native = *state.interaction;
    auto b = build{ .native = native, .state = state };
    auto parts = std::vector<component> {};
    if( !native.banner.empty() ) {
        parts.push_back( { .id = "banner", .body = text_body{ .markup = lines_of( native.banner ), .role = "banner" } } );
    }
    if( !native.message.empty() ) {
        // The computer terminal files its answers under its own pane.
        const auto terminal = std::ranges::any_of( native.choices, []( const auto & choice ) { return choice.pane_id == "terminal"; } );
        parts.push_back( { .id = "message", .body = text_body{ .markup = native.message, .tone = native.message_tone,
                                                               .role = terminal ? "terminal_output" : "" } } );
    }
    const auto add = [&]( std::optional<component> part ) {
        if( part ) { parts.push_back( std::move( *part ) ); }
    };
    // In reading order; each part places its affordances as it is built.
    add( make_areas( b ) );
    add( make_map( b ) );
    add( make_vehicle( b ) );
    add( make_field( b ) );
    add( make_body( b ) );
    add( make_editors( b ) );
    if( native.canvas ) { parts.push_back( { .id = "canvas", .body = *native.canvas } ); }
    native_affordances( b );
    auto result = interaction_view{ .schema_id = native.schema_id, .context = native.context, .title = native.title,
                                    .affordances = std::move( b.affordances ), .focus_row = native.focus_choice_id,
                                    .refresh_ms = native.refresh_ms };
    result.root = parts.empty() ? text_part( "message",
                  "" ) : parts.size() == 1 ? std::move( parts.front() ) :
                  component{ .id = "root", .body = split_body{}, .children = std::move( parts ) };
    // Only a component the tree holds; an off-page row's list may not be there.
    if( const auto focus = focus_component( native );
        find_component( result.root, focus ) != nullptr ) {
        result.focus_component = focus;
    }
    std::ranges::move( registered_affordances( state ), std::back_inserter( result.affordances ) );
    return result;
}

} // namespace engine_client

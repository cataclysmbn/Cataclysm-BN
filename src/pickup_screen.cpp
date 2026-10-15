#include "pickup_screen.h"

#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "color.h"
#include "drop_token.h"
#include "engine_client_world.h"
#include "game.h"
#include "input.h"
#include "item.h"
#include "iteminfo_request.h"
#include "item_serial.h"
#include "options.h"
#include "output.h"
#include "player.h"
#include "string_formatter.h"
#include "translations.h"
#include "units_utility.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <ranges>
#include <unordered_map>

namespace
{
namespace ec = engine_client;

/// The row text with its color tags: the stack size in front, the charges colored, money as cards and cash.
auto name_markup( const std::list<item_stack::iterator> &stack,
                  const pickup_count &selection ) -> std::string
{
    const auto &candidate = **stack.front();
    auto name = std::string{};
    if( candidate.is_money() ) {
        const auto sum_charges = []( const auto & iters ) {
            return std::ranges::fold_left( iters, 0u, []( const auto sum, const auto iter ) { return sum + ( *iter )->charges; } );
        };
        const auto charges_total = sum_charges( stack );
        const auto partial = selection.pick && selection.count;
        name = partial ? candidate.display_money( *selection.count, charges_total,
                sum_charges( stack | std::views::take( *selection.count ) ) ) :
               candidate.display_money( stack.size(), charges_total );
    } else {
        name = candidate.display_name( stack.size() );
    }
    return stack.size() > 1 ? string_format( "%d %s", stack.size(), name ) : name;
}

/// The row as the native list prints it: the ITEM_SYMBOLS glyph, and a red `!` before an item that is not the avatar's.
auto row_markup( const std::list<item_stack::iterator> &stack,
                 const pickup_count &selection ) -> std::string
{
    const auto &candidate = **stack.front();
    auto name = name_markup( stack, selection );
    if( get_option<bool>( "ITEM_SYMBOLS" ) ) {
        name = string_format( "%s %s", candidate.symbol(), name );
    }
    return candidate.is_owned_by( g->u, true ) ? name : string_format( "<color_light_red>!</color> %s",
            name );
}

/// The key the native list prints before the row at `index`: one of `chars`, else a backtick and two of them.
auto pickup_hotkey( const std::string &chars, const std::size_t index ) -> std::string
{
    if( index < chars.size() ) { return std::string( 1, chars[index] ); }
    const auto pair = index - chars.size();
    return pair < chars.size() * chars.size() ? std::string{ '`', chars[pair / chars.size()], chars[pair % chars.size()] } :
           std::string{};
}

/// The color of the slash the native list draws before a stack nested in another; empty for a plain stack.
auto nest_color( const std::vector<pickup_count> &getitem,
                 const pickup_count &selection ) -> std::string
{
    if( !selection.parent ) { return {}; }
    const auto &parent = getitem[*selection.parent];
    return parent.pick ? ( parent.all_children_picked ? "c_light_blue" : "c_yellow" ) : "c_dark_gray";
}

auto source_name( const pickup::source_kind source ) -> std::string
{
    switch( source ) {
        case pickup::source_kind::ground:
            return _( "ground" );
        case pickup::source_kind::vehicle_cargo:
            return _( "vehicle cargo" );
        case pickup::source_kind::nearby:
            return _( "ground or vehicle cargo nearby" );
    }
    return {};
}

auto weight_text( const units::mass value ) -> std::string
{
    return string_format( "%.1f", round_up( convert_weight( value ), 1 ) );
}

/// The weight and volume the native header prints: what the avatar would carry against what it can.
auto carry_stats( const pickup::screen_options &opts ) -> std::vector<ec::panel_stat>
{
    const auto weight_capacity = g->u.weight_capacity();
    const auto volume_capacity = g->u.volume_capacity();
    return {
        {
            .key = "weight", .label = _( "Weight" ), .value = weight_text( opts.weight_predict ),
            .max = weight_text( weight_capacity ), .unit = weight_units(),
            .tone = opts.weight_predict > weight_capacity ? "bad" : ""
        },
        {
            .key = "volume", .label = _( "Volume" ), .value = format_volume( opts.volume_predict ),
            .max = format_volume( volume_capacity ), .unit = volume_units_abbr(),
            .tone = opts.volume_predict > volume_capacity ? "bad" : ""
        },
    };
}

} // namespace

namespace pickup
{

auto choice_ids( const screen_options &opts ) -> std::vector<std::string>
{
    return opts.matches | std::views::transform( [&]( const auto stack_index ) {
        const auto members = opts.stacked_here[static_cast<std::size_t>( stack_index )]
                             | std::views::transform( []( const auto iter ) -> const item * { return *iter; } )
                             | std::ranges::to<std::vector>();
        return game_client::stack_row_id( members );
    } ) | std::ranges::to<std::vector>();
}

auto screen_of( const screen_options &opts ) -> ec::screen_view
{
    const auto ids = choice_ids( opts );
    auto rows = std::vector<ec::row> {};
    rows.reserve( opts.matches.size() );
    for( const auto visible_index : std::views::iota( std::size_t{ 0 }, opts.matches.size() ) ) {
        const auto stack_index = static_cast<std::size_t>( opts.matches[visible_index] );
        const auto &stack = opts.stacked_here[stack_index];
        const auto &candidate = **stack.front();
        const auto &selection = opts.getitem[stack_index];
        const auto available = candidate.count_by_charges() ? candidate.charges :
                               static_cast<int>( stack.size() );
        const auto selected_count = selection.pick ? selection.count.value_or( available ) : 0;
        const auto parent_visible = selection.parent ? std::ranges::find( opts.matches,
                                    static_cast<int>( *selection.parent ) ) : opts.matches.end();
        // The native window describes the item under the cursor only.
        const auto under_cursor = static_cast<int>( visible_index ) == opts.selected;
        rows.push_back( {
            .id = ids[visible_index],
            .label = remove_color_tags( name_markup( stack, selection ) ),
            .description = under_cursor ? remove_color_tags( candidate.info_string( { .mode = iteminfo_mode::observation } ) ) : std::string{},
            .hotkey = pickup_hotkey( opts.hotkeys, visible_index ),
            .selected = selection.pick,
            .color = string_from_color( candidate.color_in_inventory() ),
            .markup = row_markup( stack, selection ),
            .parent_id = parent_visible != opts.matches.end() ? std::optional( ids[std::distance( opts.matches.begin(), parent_visible )] ) : std::nullopt,
            .nest_color = nest_color( opts.getitem, selection ),
.mark = selection.pick ? game_client::interaction_mark{ .glyph = selection.count ? "#" : "+", .color = "c_light_blue" } :
            game_client::interaction_mark{ .glyph = "-" },
            .selected_count = static_cast<std::uint64_t>( selected_count ),
            .minimum_count = 0,
            .max = static_cast<std::uint64_t>( available ),
            .look = ec::world::item_appearance( candidate ),
        } );
    }
    auto items = ec::list( { .id = items_id, .rows = std::move( rows ), .selection = "single", .activate = toggle_id } );
    items.affordances.emplace_back( count_id );
    const auto cursor = opts.selected >= 0 &&
                        opts.selected < static_cast<int>( ids.size() ) ? ids[opts.selected] : std::string{};
    return {
        .title = _( "Pick up" ),
        .root = ec::panel( {
            .id = "pickup", .subtitle = source_name( opts.source ),
            .stats = carry_stats( opts ), .filter = opts.filter, .child = std::move( items )
        } ),
        .affordances = {
            // A hotkey or a click marks a stack or unmarks it, as the letter before the native row does.
            ec::invoke_affordance( { .id = toggle_id, .label = _( "Mark" ), .args = { .row = ec::row_shape{ .component_id = items_id } } } ),
            // Zero unmarks the stack; a number marks that many of it, and the whole stack when it reaches it.
            ec::invoke_affordance( { .id = count_id, .label = _( "Count" ), .args = { .row = ec::row_shape{ .component_id = items_id }, .count = ec::count_shape{ .min = 0 } } } ),
        },
        .focus_component = items_id,
        .focus_row = cursor,
        .dock = get_option<std::string>( "PICKUP_POSITION" ),
    };
}

} // namespace pickup

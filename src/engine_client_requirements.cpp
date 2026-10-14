#include "engine_client_requirements.h"

#include "character.h"
#include "color.h"
#include "filter_utils.h"
#include "inventory.h"
#include "item.h"
#include "requirements.h"
#include "skill.h"
#include "string_utils.h"
#include "translations.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace engine_client
{
namespace
{

auto needed( const quality_requirement &comp, const int /*batch*/ ) -> int { return comp.count; }
auto needed( const tool_comp &comp, const int batch ) -> int
{
    return comp.by_charges() ? comp.count * batch : std::abs( comp.count );
}
auto needed( const item_comp &comp, const int batch ) -> int { return std::abs( comp.count ) * batch; }

/// How many the crafter has, when native counts them; qualities and tools that are not charged are had or not.
auto held( const quality_requirement &/*comp*/,
           const requirement_view_options &/*options*/ ) -> std::optional<int> { return std::nullopt; }
auto held( const tool_comp &comp, const requirement_view_options &options ) -> std::optional<int>
{
    return comp.by_charges() ? std::optional{ options.available.charges_of( comp.type, INT_MAX, options.filter ) } :
           std::nullopt;
}
auto held( const item_comp &comp, const requirement_view_options &options ) -> std::optional<int>
{
    return item::count_by_charges( comp.type ) ?
           options.available.charges_of( comp.type, INT_MAX, options.filter ) :
           options.available.amount_of( comp.type, false, INT_MAX, options.filter );
}

template<typename T>
auto alternative_row( const T &comp, const std::string &group,
                      const requirement_view_options &options ) -> row
{
    const auto have = held( comp, options );
    auto columns = std::vector<game_client::interaction_column> {
        { .key = "required", .label = _( "Required" ), .value = std::to_string( needed( comp, options.batch ) ), .color = {} },
    };
    if( have ) {
        columns.push_back( { .key = "available", .label = _( "Available" ), .value = std::to_string( *have ), .color = {} } );
    }
    return { .id = group + ":" + comp.type.str(), .label = comp.to_string( options.batch, have.value_or( 0 ) ),
             .enabled = comp.has( options.available, options.filter, options.batch ), .selectable = false,
             .columns = std::move( columns ) };
}

/// The rows of every group of `groups`: a group of one alternative is its row, a group of several is a
/// parent row, enabled when one of them is, over one row for each.
template<typename T>
auto group_rows( const std::string &kind, const std::vector<std::vector<T>> &groups,
                 const requirement_view_options &options ) -> std::vector<row>
{
    auto rows = std::vector<row> {};
    for( const auto &[index, alternatives] : std::views::enumerate( groups ) ) {
        const auto group = kind + ":" + std::to_string( index );
        auto members = std::vector<row> {};
        std::ranges::for_each( alternatives, [&]( const T & comp ) {
            auto member = alternative_row( comp, group, options );
            if( std::ranges::none_of( members, [&]( const row & seen ) { return seen.id == member.id; } ) ) {
                members.push_back( std::move( member ) );
            }
        } );
        if( members.size() > 1 ) {
            const auto labels = members | std::views::transform( &row::label ) | std::ranges::to<std::vector>();
            rows.push_back( { .id = group, .label = join( labels, _( " OR " ) ),
                              .enabled = std::ranges::any_of( members, &row::enabled ), .selectable = false } );
            std::ranges::for_each( members, [&]( row & member ) { member.parent_id = group; } );
        }
        std::ranges::move( members, std::back_inserter( rows ) );
    }
    return rows;
}

} // namespace

auto caption( std::string native ) -> std::string
{
    auto result = trim( std::move( native ) );
    if( result.ends_with( ':' ) ) { result.pop_back(); }
    return result;
}

auto skill_requirements( const skill_view_options &options ) -> std::optional<component>
{
    if( options.skills.empty() ) { return std::nullopt; }
    auto rows = options.skills | std::views::transform( [&]( const auto & need ) {
        const auto level = options.crafter.get_skill_level( need.first );
        const auto standing = level < need.second ? c_red : level < need.second * 1.25 ? c_light_blue :
                              c_green;
        return row{ .id = need.first.str(), .label = need.first.obj().name(), .enabled = level >= need.second,
                    .selectable = false,
        .columns = {
            { .key = "required", .label = _( "Required" ), .value = std::to_string( need.second ), .color = {} },
            // The native construction list colors a level short of 125% of the need as marginal.
            { .key = "level", .label = _( "Level" ), .value = std::to_string( level ), .color = string_from_color( standing ) },
        } };
    } ) | std::ranges::to<std::vector>();
    auto result = list( { .id = options.id, .rows = std::move( rows ), .selection = "none" } );
    result.label = options.label;
    return result;
}

auto speed_stats( const std::vector<speed_modifier> &modifiers,
                  const bool verbose ) -> std::vector<panel_stat>
{
    const auto percent_of = []( const speed_modifier & modifier ) { return static_cast<int>( modifier.multiplier * 100 ); };
    const auto stat_of = [&]( const speed_modifier & modifier ) {
        const auto percent = percent_of( modifier );
        return panel_stat{ .key = "speed_" + modifier.key, .label = modifier.label, .value = std::to_string( percent ),
                           .unit = "%", .tone = percent > 100 ? "good" : percent < 100 ? "bad" : "" };
    };
    if( modifiers.empty() ) { return {}; }
    auto stats = modifiers | std::views::drop( 1 ) | std::views::filter( [&]( const auto & modifier ) {
        return verbose || percent_of( modifier ) != 100;
    } ) | std::views::transform( stat_of ) | std::ranges::to<std::vector>();
    if( verbose || percent_of( modifiers.front() ) != 100 || !stats.empty() ) {
        stats.insert( stats.begin(), stat_of( modifiers.front() ) );
    }
    return stats;
}

auto tool_requirements( const requirement_view_options &options ) -> component
{
    auto tools = options;
    tools.filter = return_true<item>;
    auto rows = group_rows( "quality", options.requirements.get_qualities(), tools );
    std::ranges::move( group_rows( "tool", options.requirements.get_tools(), tools ),
                       std::back_inserter( rows ) );
    auto result = list( { .id = options.prefix + "tools", .rows = std::move( rows ), .selection = "none" } );
    result.label = caption( _( "Tools required:" ) );
    return result;
}

auto component_requirements( const requirement_view_options &options ) -> component
{
    auto components = options;
    if( !components.filter ) { components.filter = return_true<item>; }
    auto result = list( { .id = options.prefix + "components",
                          .rows = group_rows( "component", options.requirements.get_components(), components ),
                          .selection = "none" } );
    result.label = caption( _( "Components required:" ) );
    return result;
}

} // namespace engine_client

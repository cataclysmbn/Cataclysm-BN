#include "monster_food_lure.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <ranges>
#include <string>
#include <unordered_set>

#include "avatar.h"
#include "character.h"
#include "game.h"
#include "line.h"
#include "item.h"
#include "itype.h"
#include "map/map.h"
#include "messages.h"
#include "monster.h"
#include "mtype.h"
#include "translations.h"

namespace food_lure
{

auto is_domestic( const mtype &type ) -> bool
{
    return !type.petfood.food.empty();
}

auto matches_pet_food( const mtype &type, const item &food ) -> bool
{
    namespace ranges = std::ranges;
    if( !food.is_comestible() ) {
        return false;
    }
    const auto &item_tags = food.get_comestible()->petfood;
    const auto has_tag = [&]( const auto & tag ) { return item_tags.contains( tag ); };
    return ranges::any_of( type.petfood.food, has_tag );
}

auto matches_diet( const mtype &type, const item &food ) -> bool
{
    return !type.diet.empty() && food.is_comestible() && food.made_of_any( type.diet );
}

auto accepts_tamer( const mtype &type, const Character &tamer ) -> bool
{
    namespace ranges = std::ranges;
    return ranges::all_of( type.petfood.tamer_traits,
                           std::bind_front( &Character::has_one_of_traits, &tamer ) );
}

auto seeks_food( const monster &mon ) -> bool
{
    return !mon.is_hallucination() && ( is_domestic( *mon.type ) || !mon.type->diet.empty() );
}

namespace
{

const auto effect_mon_feeding = efftype_id( "mon_feeding" );
const auto effect_well_fed = efftype_id( "well_fed" );

/// Z-levels on which a monster looks for food.
struct level_span {
    int lowest;
    int highest;
};

/// A flier in the sky above ground also looks down through open air, so it can land on food.
/// Underground levels cannot be seen from the sky, so the search stops at ground level.
auto lure_levels( const monster &mon ) -> level_span
{
    const auto z = mon.bub_pos().z();
    if( !mon.flies() || z <= 0 ) {
        return { .lowest = z, .highest = z };
    }
    return { .lowest = std::max( 0, z - lure_radius ), .highest = z };
}

auto in_lure_reach( const monster &mon, const tripoint_bub_ms &p ) -> bool
{
    const auto levels = lure_levels( mon );
    const auto pos = mon.bub_pos();
    return p.z() >= levels.lowest && p.z() <= levels.highest &&
           rl_dist( pos.xy(), p.xy() ) <= lure_radius;
}

const auto flag_CONTAINER = std::string( "CONTAINER" );

/// Adds the comestibles on `p` to `tiles`, once per tile. Tiles without items are not hashed.
/// Food that no monster can stand on, or that lies in a sealed container, is left out.
auto append_tile( map &here, const tripoint_bub_ms &p,
                  std::unordered_set<tripoint_bub_ms> &visited, lure_snapshot &tiles ) -> void
{
    if( !here.has_items( p ) || !visited.insert( p ).second || here.impassable( p ) ) {
        return;
    }
    const auto enclosed = here.has_flag_ter_or_furn( flag_CONTAINER, p );
    if( enclosed && here.has_flag_ter_or_furn( TFLAG_SEALED, p ) ) {
        return;
    }
    auto foods = std::vector<const item *> {};
    for( const item *it : here.i_at( p ) ) {
        if( it->is_comestible() ) {
            foods.push_back( it );
        }
    }
    if( !foods.empty() ) {
        tiles.push_back( {
            .pos = p,
            .feeder = here.has_flag_furn( "PET_FEEDER", p ),
            .enclosed = enclosed,
            .foods = std::move( foods ),
        } );
    }
}

auto append_tiles_near( map &here, const monster &mon,
                        std::unordered_set<tripoint_bub_ms> &visited, lure_snapshot &tiles ) -> void
{
    const auto levels = lure_levels( mon );
    const auto pos = mon.bub_pos();
    // Pets only eat from feeders, so tiles without furniture hold nothing for them.
    const auto feeders_only = mon.friendly != 0;
    for( auto z = levels.highest; z >= levels.lowest; --z ) {
        const auto center = tripoint_bub_ms( pos.xy(), z );
        for( const auto &p : here.points_in_radius( center, lure_radius ) ) {
            if( feeders_only && !here.has_furn( p ) ) {
                continue;
            }
            append_tile( here, p, visited, tiles );
        }
    }
}

/// Whether a search can give the monster a food goal at all.
auto is_hungry_seeker( const monster &mon ) -> bool
{
    return seeks_food( mon ) && !mon.has_effect( effect_well_fed );
}

/// Tiles for one monster: its surroundings on a search turn, otherwise only the meal underfoot
/// and the goal it already heads for.
auto append_tiles_for( map &here, const monster &mon, const tripoint_bub_ms &goal,
                       std::unordered_set<tripoint_bub_ms> &visited, lure_snapshot &tiles ) -> void
{
    if( is_search_turn( mon ) ) {
        append_tiles_near( here, mon, visited, tiles );
    } else {
        append_tile( here, mon.bub_pos(), visited, tiles );
        append_tile( here, goal, visited, tiles );
    }
}

auto is_held_pet_food( const mtype &type, const item &held ) -> bool
{
    return matches_pet_food( type, held ) ||
           ( !held.is_container_empty() && matches_pet_food( type, held.get_contained() ) );
}

auto find_wanted_food_at( const monster &mon, const tripoint_bub_ms &pos ) -> item *
{
    auto &here = get_map();
    if( !here.has_items( pos ) ) {
        return nullptr;
    }
    const auto feeder = here.has_flag_furn( "PET_FEEDER", pos );
    for( item *it : here.i_at( pos ) ) {
        if( it->is_comestible() && wants_food( mon, *it, feeder ) ) {
            return it;
        }
    }
    return nullptr;
}

/// Consumes one unit of exactly the chosen item, not of another stack of the same type.
auto eat_one( const tripoint_bub_ms &pos, item &food ) -> void
{
    if( food.count_by_charges() && food.charges > 1 ) {
        food.mod_charges( -1 );
    } else {
        get_map().i_rem( pos, &food );
    }
}

} // namespace

auto collect_lure_tiles( map &here, std::span<monster *const> critters ) -> lure_snapshot
{
    auto visited = std::unordered_set<tripoint_bub_ms> {};
    auto tiles = lure_snapshot{};
    for( auto *critter : critters ) {
        if( critter == nullptr || critter->is_dead() || !is_hungry_seeker( *critter ) ) {
            continue;
        }
        append_tiles_for( here, *critter, critter->move_target(), visited, tiles );
    }
    return tiles;
}

auto collect_lure_tiles_near( map &here, const monster &mon,
                              const tripoint_bub_ms &goal ) -> lure_snapshot
{
    auto visited = std::unordered_set<tripoint_bub_ms> {};
    auto tiles = lure_snapshot{};
    if( is_hungry_seeker( mon ) ) {
        append_tiles_for( here, mon, goal, visited, tiles );
    }
    return tiles;
}

auto is_search_turn( const monster &mon ) -> bool
{
    // The position spreads the monsters over the period, so they do not all search on one turn.
    // It is the same on every run of a save, unlike the address of the monster.
    const auto p = mon.abs_pos();
    auto hash = static_cast<std::uint32_t>( p.x() ) * 73856093u ^
                static_cast<std::uint32_t>( p.y() ) * 19349663u ^
                static_cast<std::uint32_t>( p.z() ) * 83492791u;
    hash ^= hash >> 16;
    hash *= 0x45d9f3bu;
    hash ^= hash >> 16;
    const auto offset = static_cast<int>( hash % search_period );
    return ( to_turn<int>( calendar::turn ) + offset ) % search_period == 0;
}

auto wants_food( const monster &mon, const item &food, const bool on_feeder ) -> bool
{
    if( mon.friendly != 0 ) {
        return on_feeder && matches_pet_food( *mon.type, food );
    }
    return matches_pet_food( *mon.type, food ) || matches_diet( *mon.type, food );
}

auto is_lured_by_held_food( const monster &mon, const Character &who ) -> bool
{
    namespace ranges = std::ranges;
    if( !is_domestic( *mon.type ) || mon.is_hallucination() ||
        rl_dist( mon.bub_pos(), who.bub_pos() ) > lure_radius ) {
        return false;
    }
    // Most of the time nothing is held, so the cheapest test goes first.
    const auto is_lure = [&]( const item * held ) { return is_held_pet_food( *mon.type, *held ); };
    return ranges::any_of( who.wielded_items(), is_lure ) && accepts_tamer( *mon.type, who ) &&
           mon.attitude( &who ) != MATT_ATTACK && mon.sees( who );
}

auto pick_lure_goal( const lure_goal_query &query ) -> std::optional<tripoint_bub_ms>
{
    using namespace std::views;
    namespace ranges = std::ranges;
    const auto &mon = query.mon;
    const auto pos = mon.bub_pos();

    auto food_tiles = std::vector<tripoint_bub_ms> {};
    if( !mon.has_effect( effect_well_fed ) ) {
        const auto in_reach = [&]( const lure_tile & t ) {
            // Food inside an open container shows only from next to it, as it does for the player.
            return ( query.full_search || t.pos == pos || t.pos == query.current_goal ) &&
                   ( !t.enclosed || rl_dist( pos, t.pos ) <= 1 ) && in_lure_reach( mon, t.pos );
        };
        const auto has_wanted = [&]( const lure_tile & t ) {
            const auto wanted = [&]( const item * f ) { return wants_food( mon, *f, t.feeder ); };
            return ranges::any_of( t.foods, wanted );
        };
        food_tiles = query.tiles
                     | filter( in_reach )
                     | filter( has_wanted )
                     | transform( &lure_tile::pos )
                     | ranges::to<std::vector>();
        ranges::sort( food_tiles, {}, [&]( const auto & p ) { return rl_dist( pos, p ); } );
    }

    if( !food_tiles.empty() && food_tiles.front() == pos ) {
        return pos;
    }
    if( query.lured_by_player ) {
        return rl_dist( pos, query.player_pos ) <= 1 ? pos : query.player_pos;
    }
    if( food_tiles.empty() ) {
        return std::nullopt;
    }
    if( !query.full_search ) {
        // Only the remembered goal is left, which the monster saw on its search turn.
        return food_tiles.front();
    }
    // Food behind walls must not cost a line of sight check per tile.
    const auto nearest = food_tiles | take( max_sight_checks );
    const auto seen = [&]( const tripoint_bub_ms & p ) { return mon.sees( p ); };
    const auto visible = ranges::find_if( nearest, seen );
    if( visible == nearest.end() ) {
        return std::nullopt;
    }
    return *visible;
}

auto holds_position( const monster &mon ) -> bool
{
    return rl_dist( mon.bub_pos(), g->u.bub_pos() ) <= 1 && is_lured_by_held_food( mon, g->u );
}

auto is_feeding( const monster &mon ) -> bool
{
    return mon.has_effect( effect_mon_feeding );
}

auto process_feeding( monster &mon ) -> bool
{
    if( !seeks_food( mon ) ) {
        return false;
    }
    const auto feeding = mon.has_effect( effect_mon_feeding );
    if( !feeding && ( mon.has_effect( effect_well_fed ) || !mon.is_wandering() ) ) {
        return false;
    }
    auto *food = find_wanted_food_at( mon, mon.bub_pos() );
    if( feeding ) {
        // Fleeing, chasing or being led away leaves the meal unfinished.
        if( food == nullptr || !mon.is_wandering() ) {
            mon.remove_effect( effect_mon_feeding );
            return false;
        }
        const auto started = mon.get_effect( effect_mon_feeding ).get_start_time();
        if( calendar::turn - started < feeding_time ) {
            return true;
        }
        if( g->u.sees( mon ) ) {
            add_msg( _( "The %1$s finishes eating the %2$s." ), mon.name(), food->tname() );
        }
        eat_one( mon.bub_pos(), *food );
        mon.remove_effect( effect_mon_feeding );
        mon.add_effect( effect_well_fed, well_fed_duration );
        return true;
    }
    if( food == nullptr ) {
        return false;
    }
    if( g->u.sees( mon ) ) {
        add_msg( _( "The %1$s starts eating the %2$s." ), mon.name(), food->tname() );
    }
    // Margin so that the effect cannot run out before the meal completes.
    mon.add_effect( effect_mon_feeding, feeding_time + 1_minutes );
    return true;
}

} // namespace food_lure

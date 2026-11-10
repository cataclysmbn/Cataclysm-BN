#include "monster_food_lure.h"

#include <algorithm>
#include <functional>
#include <ranges>
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

auto append_tiles_near( map &here, const tripoint_bub_ms &center,
                        std::unordered_set<tripoint_bub_ms> &visited, lure_snapshot &tiles ) -> void
{
    for( const auto &p : here.points_in_radius( center, lure_radius ) ) {
        if( !visited.insert( p ).second || !here.has_items( p ) ) {
            continue;
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
                .foods = std::move( foods ),
            } );
        }
    }
}

auto is_held_pet_food( const mtype &type, const item &held ) -> bool
{
    return matches_pet_food( type, held ) ||
           ( !held.is_container_empty() && matches_pet_food( type, held.get_contained() ) );
}

auto find_wanted_food_here( const monster &mon ) -> item *
{
    auto &here = get_map();
    const auto pos = mon.bub_pos();
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

auto eat_one( const tripoint_bub_ms &pos, const item &food ) -> void
{
    auto &here = get_map();
    auto consumed = 1;
    const auto id = food.typeId();
    if( food.count_by_charges() ) {
        here.use_charges( pos, 0, id, consumed );
    } else {
        here.use_amount( pos, 0, id, consumed );
    }
}

} // namespace

auto collect_lure_tiles( map &here, std::span<monster *const> critters ) -> lure_snapshot
{
    auto visited = std::unordered_set<tripoint_bub_ms> {};
    auto tiles = lure_snapshot{};
    for( const auto *critter : critters ) {
        if( critter != nullptr && !critter->is_dead() && seeks_food( *critter ) ) {
            append_tiles_near( here, critter->bub_pos(), visited, tiles );
        }
    }
    return tiles;
}

auto collect_lure_tiles_near( map &here, const monster &mon ) -> lure_snapshot
{
    auto visited = std::unordered_set<tripoint_bub_ms> {};
    auto tiles = lure_snapshot{};
    if( seeks_food( mon ) ) {
        append_tiles_near( here, mon.bub_pos(), visited, tiles );
    }
    return tiles;
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
        rl_dist( mon.bub_pos(), who.bub_pos() ) > lure_radius ||
        !accepts_tamer( *mon.type, who ) ) {
        return false;
    }
    const auto is_lure = [&]( const item * held ) { return is_held_pet_food( *mon.type, *held ); };
    return ranges::any_of( who.wielded_items(), is_lure ) &&
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
            return t.pos.z() == pos.z() && rl_dist( pos, t.pos ) <= lure_radius;
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
    const auto seen = [&]( const tripoint_bub_ms & p ) { return mon.sees( p ); };
    const auto visible = ranges::find_if( food_tiles, seen );
    if( visible == food_tiles.end() ) {
        return std::nullopt;
    }
    return *visible;
}

auto holds_position( const monster &mon ) -> bool
{
    return rl_dist( mon.bub_pos(), g->u.bub_pos() ) <= 1 && is_lured_by_held_food( mon, g->u );
}

auto process_feeding( monster &mon ) -> bool
{
    if( !seeks_food( mon ) ) {
        return false;
    }
    const auto *food = find_wanted_food_here( mon );
    const auto seen = g->u.sees( mon );
    if( mon.has_effect( effect_mon_feeding ) ) {
        // Fleeing, chasing or being led away leaves the meal unfinished.
        if( food == nullptr || !mon.is_wandering() ) {
            mon.remove_effect( effect_mon_feeding );
            return false;
        }
        const auto started = mon.get_effect( effect_mon_feeding ).get_start_time();
        if( calendar::turn - started < feeding_time ) {
            return true;
        }
        if( seen ) {
            add_msg( _( "The %1$s finishes eating the %2$s." ), mon.name(), food->tname() );
        }
        eat_one( mon.bub_pos(), *food );
        mon.remove_effect( effect_mon_feeding );
        mon.add_effect( effect_well_fed, well_fed_duration );
        return true;
    }
    if( food == nullptr || mon.has_effect( effect_well_fed ) || !mon.is_wandering() ) {
        return false;
    }
    if( seen ) {
        add_msg( _( "The %1$s starts eating the %2$s." ), mon.name(), food->tname() );
    }
    // Margin so that the effect cannot run out before the meal completes.
    mon.add_effect( effect_mon_feeding, feeding_time + 1_minutes );
    return true;
}

} // namespace food_lure

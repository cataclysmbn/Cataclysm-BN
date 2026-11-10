#include "monster_food_lure.h"

#include <algorithm>
#include <functional>
#include <unordered_set>

#include "character.h"
#include "item.h"
#include "itype.h"
#include "map/map.h"
#include "monster.h"
#include "mtype.h"

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

auto collect_lure_tiles( map &here, std::span<monster *const> critters ) -> lure_snapshot
{
    auto visited = std::unordered_set<tripoint_bub_ms> {};
    auto tiles = lure_snapshot{};
    for( const auto *critter : critters ) {
        if( critter == nullptr || critter->is_dead() || !seeks_food( *critter ) ) {
            continue;
        }
        for( const auto &p : here.points_in_radius( critter->bub_pos(), lure_radius ) ) {
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
    return tiles;
}

} // namespace food_lure

#include "monster_food_lure.h"

#include <algorithm>
#include <functional>

#include "character.h"
#include "item.h"
#include "itype.h"
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

} // namespace food_lure

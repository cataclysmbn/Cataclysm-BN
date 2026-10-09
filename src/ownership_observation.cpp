#include "ownership_observation.h"

#include "character.h"
#include "faction.h"
#include "game.h"
#include "item.h"

auto item_available_for_crafting_observation( const item &value, const Character &actor ) -> bool
{
    if( value.owner.is_null() ) {
        return true;
    }
    const auto owner = g->faction_manager_ptr->get_for_display( value.owner );
    if( !owner ) {
        // Native ownership validation would remove this missing owner and allow the component.
        return true;
    }
    const auto actor_id = actor.get_faction_id_for_display();
    // Compare the stored owner ID, as native is_owned_by does, not the metadata's runtime ID.
    return ( actor_id && *actor_id == value.owner ) || owner->likes_u < -10;
}

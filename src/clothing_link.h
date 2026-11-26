#pragma once
#ifndef CATA_SRC_CLOTHING_LINK_H
#define CATA_SRC_CLOTHING_LINK_H

#include "character.h"
#include "item.h"
#include "units_volume.h"

/// Reserve storage for inventory items linked to this particular clothing item.
inline auto can_link_to_clothing( const Character &owner, const item &candidate,
                                  const item &clothing ) -> bool
{
    auto remaining = clothing.get_storage();
    if( &candidate == &clothing || remaining <= 0_ml ) {
        return false;
    }
    const auto clothing_id = clothing.get_var( "DROP_WITH_CLOTHING_ID", 0 );
    if( clothing_id > 0 ) {
        for( const auto *stack : owner.inv_const_slice() ) {
            for( const auto *linked : *stack ) {
                if( linked != &candidate &&
                    linked->get_var( "DROP_WITH_CLOTHING_TARGET", 0 ) == clothing_id ) {
                    remaining -= linked->volume();
                }
            }
        }
    }
    return candidate.volume() <= remaining;
}

#endif // CATA_SRC_CLOTHING_LINK_H

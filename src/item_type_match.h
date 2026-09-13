#pragma once

#include "item.h"
#include "itype.h"
#include "iuse_actor.h"

#include <ranges>

/// True when @p base has a transform use that produces @p on_id (flashlight → flashlight_on).
inline auto itype_transforms_into( const itype &base, const itype_id &on_id ) -> bool
{
    namespace ranges = std::ranges;
    return ranges::any_of( base.use_methods, [&]( const auto &entry ) {
        const auto *actor = dynamic_cast<const iuse_transform *>( entry.second.get_actor_ptr() );
        return actor != nullptr && actor->target == on_id;
    } );
}

/// True when @p it is @p id, or is the transform-"on" form of @p id (revert_to plus matching transform).
inline auto item_matches_itype( const item &it, const itype_id &id ) -> bool
{
    if( it.typeId() == id ) {
        return true;
    }
    if( !id.is_valid() || !it.is_tool() || !it.type->tool ) {
        return false;
    }
    const auto &revert = it.type->tool->revert_to;
    if( !revert || *revert != id ) {
        return false;
    }
    return itype_transforms_into( *id, it.typeId() );
}

/// Turn a matched "on" tool into the requested type before it is consumed as a component.
inline auto normalize_consumed_item( item &it, const itype_id &wanted ) -> void
{
    if( it.typeId() == wanted ) {
        return;
    }
    it.convert( wanted );
    it.deactivate();
}

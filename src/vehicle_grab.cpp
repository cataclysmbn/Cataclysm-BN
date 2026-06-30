#include "vehicle_grab.h"

#include "mapbuffer.h"
#include "mapdata.h"

namespace
{

auto vehicle_grab_target_at_exact( mapbuffer &here,
                                   const tripoint_abs_ms &pos ) -> std::optional<vehicle_grab_target>
{
    if( const auto vp = here.veh_at( pos ) ) {
        return vehicle_grab_target{ .pos = vp->abs_pos(), .vp = *vp };
    }

    return std::nullopt;
}

} // namespace

auto vehicle_grab_target_at( mapbuffer &here,
                             const tripoint_abs_ms &pos ) -> std::optional<vehicle_grab_target>
{
    if( const auto target = vehicle_grab_target_at_exact( here, pos ) ) {
        return target;
    }
    auto tile = abs_tile_handle::fetch( here, pos );
    if( !tile ) {
        return std::nullopt;
    }

    const auto above = pos + tripoint_above;
    const auto below = pos + tripoint_below;

    if( tile->has_flag( TFLAG_RAMP_UP ) ) {
        if( const auto target = vehicle_grab_target_at_exact( here, above ) ) {
            return target;
        }
    }

    if( tile->has_flag( TFLAG_RAMP_DOWN ) ) {
        if( const auto target = vehicle_grab_target_at_exact( here, below ) ) {
            return target;
        }
    }

    if( above.z() <= OVERMAP_HEIGHT && here.has_flag( TFLAG_RAMP_DOWN, above ) ) {
        if( const auto target = vehicle_grab_target_at_exact( here, above ) ) {
            return target;
        }
    }

    if( below.z() >= -OVERMAP_DEPTH && here.has_flag( TFLAG_RAMP_UP, below ) ) {
        if( const auto target = vehicle_grab_target_at_exact( here, below ) ) {
            return target;
        }
    }

    return std::nullopt;
}

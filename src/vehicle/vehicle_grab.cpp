#include "vehicle_grab.h"

#include "map.h"
#include "mapdata.h"

namespace
{

auto vehicle_grab_target_at_exact( const map &here,
                                   const tripoint_bub_ms &pos ) -> std::optional<vehicle_grab_target>
{
    if( const auto vp = here.veh_at( pos ) ) {
        return vehicle_grab_target{ .pos = vp->pos(), .vp = *vp };
    }

    return std::nullopt;
}

} // namespace

auto vehicle_grab_target_at( const map &here,
                             const tripoint_bub_ms &pos ) -> std::optional<vehicle_grab_target>
{
    if( const auto target = vehicle_grab_target_at_exact( here, pos ) ) {
        return target;
    }

    const auto above = pos + tripoint_above;
    const auto below = pos + tripoint_below;

    if( here.has_flag( TFLAG_RAMP_UP, pos ) ) {
        if( const auto target = vehicle_grab_target_at_exact( here, above ) ) {
            return target;
        }
    }

    if( here.has_flag( TFLAG_RAMP_DOWN, pos ) ) {
        if( const auto target = vehicle_grab_target_at_exact( here, below ) ) {
            return target;
        }
    }

    if( here.inbounds_z( above.z() ) && here.has_flag( TFLAG_RAMP_DOWN, above ) ) {
        if( const auto target = vehicle_grab_target_at_exact( here, above ) ) {
            return target;
        }
    }

    if( here.inbounds_z( below.z() ) && here.has_flag( TFLAG_RAMP_UP, below ) ) {
        if( const auto target = vehicle_grab_target_at_exact( here, below ) ) {
            return target;
        }
    }

    return std::nullopt;
}

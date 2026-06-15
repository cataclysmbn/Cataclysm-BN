#pragma once

#include <vector>

#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "vpart_range.h"

namespace vehicle_lighting
{

inline auto active_light_parts( vehicle &veh ) -> std::vector<vpart_reference>
{
    auto lights = std::vector<vpart_reference> {};
    for( const auto &part : veh.get_all_parts() ) {
        const auto &vehicle_part = part.part();
        if( vehicle_part.enabled && vehicle_part.is_available() && vehicle_part.is_light() ) {
            lights.push_back( part );
        }
    }
    return lights;
}

inline auto is_directional( const vpart_info &info ) -> bool
{
    return info.has_flag( VPFLAG_CONE_LIGHT ) || info.has_flag( VPFLAG_WIDE_CONE_LIGHT ) ||
           info.has_flag( VPFLAG_HALF_CIRCLE_LIGHT );
}

inline auto arc_width( const vpart_info &info ) -> units::angle
{
    if( info.has_flag( VPFLAG_CONE_LIGHT ) ) {
        return 45_degrees;
    }
    if( info.has_flag( VPFLAG_WIDE_CONE_LIGHT ) ) {
        return 90_degrees;
    }
    if( info.has_flag( VPFLAG_HALF_CIRCLE_LIGHT ) ) {
        return 180_degrees;
    }
    return 360_degrees;
}

inline auto is_external( const vpart_reference &part ) -> bool
{
    const auto &info = part.info();
    return info.location == "on_roof" || is_directional( info ) || info.rotating_light.has_value();
}

inline auto circle_light_is_active( const vpart_info &info, const bool odd_turn ) -> bool
{
    if( !info.has_flag( VPFLAG_CIRCLE_LIGHT ) ) {
        return true;
    }
    return ( odd_turn && info.has_flag( VPFLAG_ODDTURN ) ) ||
           ( !odd_turn && info.has_flag( VPFLAG_EVENTURN ) ) ||
           ( !( info.has_flag( VPFLAG_EVENTURN ) || info.has_flag( VPFLAG_ODDTURN ) ) );
}

} // namespace vehicle_lighting

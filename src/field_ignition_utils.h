#pragma once

#include <algorithm>

#include "calendar.h"
#include "field_type.h"

/// Some liquid puddles need to spawn a self-sustaining secondary field rather
/// than a one-turn spark, otherwise connected spills do not reliably propagate
/// the effect onward.
inline auto fuel_field_fire_intensity( const int fuel_intensity ) -> int
{
    if( fuel_intensity <= 0 ) {
        return 1;
    }
    return std::min( fd_fire.obj().get_max_intensity(), std::max( 2, fuel_intensity ) );
}

inline auto fuel_field_fire_age( const int fuel_intensity ) -> time_duration
{
    if( fuel_intensity <= 0 ) {
        return 10_minutes;
    }
    return -10_minutes * fuel_intensity;
}

inline auto conductive_field_electricity_intensity( const int conductive_field_intensity ) -> int
{
    if( conductive_field_intensity <= 0 ) {
        return 1;
    }
    return std::min( fd_electricity.obj().get_max_intensity(),
                     std::max( 2, conductive_field_intensity ) );
}

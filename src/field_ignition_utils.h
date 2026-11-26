#pragma once

#include <algorithm>
#include <vector>

#include "calendar.h"
#include "map/field_type.h"
#include "map/field.h"

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

struct flammable_field_data {
    std::vector<field_type_id> types;
    int intensity = 0;
};

/// Snapshot combustible fields before callers remove them through their cache-aware API.
inline auto flammable_fields( const field &fields ) -> flammable_field_data
{
    auto result = flammable_field_data{};
    for( const auto &[type, entry] : fields ) {
        if( type->flammable && entry.get_field_intensity() > 0 ) {
            result.types.push_back( type );
            result.intensity = std::max( result.intensity, entry.get_field_intensity() );
        }
    }
    return result;
}

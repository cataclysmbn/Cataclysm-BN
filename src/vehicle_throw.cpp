#include "vehicle_throw.h"

namespace vehicle_throw
{

auto strength_requirement( const units::mass vehicle_mass ) -> int
{
    return std::max<int>( 1, units::to_kilogram( vehicle_mass ) / kilograms_per_strength );
}

auto strength_requirement( const vehicle &veh ) -> int
{
    return strength_requirement( veh.total_mass() );
}

auto throw_range( const int throw_strength, const int strength_requirement ) -> int
{
    if( throw_strength < strength_requirement ) {
        return 0;
    }

    return std::max( base_throw_range,
                     ( throw_strength - strength_requirement ) / range_strength_step +
                     base_throw_range );
}

} // namespace vehicle_throw

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

auto strength_penalty( const creature_size thrower_size ) -> int
{
    return std::max( 0, static_cast<int>( creature_size::large ) -
                     static_cast<int>( thrower_size ) ) * smaller_than_large_strength_penalty;
}

auto effective_throw_strength( const creature_size thrower_size, const int throw_strength ) -> int
{
    return std::max( 0, throw_strength - strength_penalty( thrower_size ) );
}

auto throw_range( const int throw_strength, const int strength_requirement ) -> int
{
    if( throw_strength < strength_requirement ) {
        return 0;
    }

    return std::clamp(
               ( throw_strength - strength_requirement ) / range_strength_step + base_throw_range,
               base_throw_range, max_throw_range );
}

auto shove_velocity( const int throw_strength, const int strength_requirement ) -> int
{
    return std::clamp( base_shove_velocity +
                       shove_velocity_per_excess_strength * std::max( 0, throw_strength - strength_requirement ),
                       base_shove_velocity, max_shove_velocity );
}

} // namespace vehicle_throw

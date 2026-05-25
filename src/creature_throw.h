#pragma once

#include <algorithm>
#include <cmath>

#include "enums.h"

namespace creature_throw
{

constexpr auto min_stamina_cost = 100;
constexpr auto max_stamina_cost = 800;
constexpr auto equal_size_throw_min_str = 12;

inline auto can_throw_grabbed_creature_size( const creature_size thrower_size,
        const int thrower_str, const creature_size target_size ) -> bool
{
    const auto thrower_size_value = static_cast<int>( thrower_size );
    const auto target_size_value = static_cast<int>( target_size );

    if( target_size_value < thrower_size_value ) {
        return true;
    }

    return target_size_value == thrower_size_value && thrower_str >= equal_size_throw_min_str;
}

inline auto grabbed_stamina_cost( const float throwforce ) -> int
{
    return std::clamp( static_cast<int>( std::lround( throwforce * 4.0f ) ),
                       min_stamina_cost, max_stamina_cost );
}

inline auto grabbed_throw_velocity( const int distance ) -> float
{
    return std::max( 1, distance ) * 10.0f;
}

} // namespace creature_throw

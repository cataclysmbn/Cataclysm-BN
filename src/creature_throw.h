#pragma once

#include <algorithm>
#include <cmath>

#include "enums.h"

namespace creature_throw
{

constexpr auto min_stamina_cost = 100;
constexpr auto max_stamina_cost = 800;
constexpr auto equal_size_throw_min_str = 12;
constexpr auto obstacle_bash_reference_weight_grams = 60000;
constexpr auto min_obstacle_bash_weight_percent = 10;
constexpr auto max_obstacle_bash_weight_percent = 200;

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

inline auto flung_creature_bash_damage( const creature_size size, const int weight_in_grams,
                                        const float velocity ) -> int
{
    const auto weight_percent = std::clamp(
                                    weight_in_grams * 100 / obstacle_bash_reference_weight_grams,
                                    min_obstacle_bash_weight_percent, max_obstacle_bash_weight_percent );
    const auto large_size_bonus = std::max( 0, static_cast<int>( size ) -
                                            static_cast<int>( creature_size::medium ) ) * 4;
    return std::max( 0, static_cast<int>( velocity ) * weight_percent / 100 + large_size_bonus );
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

#pragma once

#include <algorithm>
#include <cmath>

namespace creature_throw
{

constexpr auto min_stamina_cost = 100;
constexpr auto max_stamina_cost = 800;

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

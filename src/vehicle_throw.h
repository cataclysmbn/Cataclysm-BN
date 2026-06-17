#pragma once

#include <algorithm>

#include "units.h"
#include "vehicle.h"

namespace vehicle_throw
{

constexpr int kilograms_per_strength = 100;
constexpr int smaller_than_large_strength_penalty = 5;
constexpr int base_throw_range = 1;
constexpr int max_throw_range = 30;
constexpr int range_strength_step = 2;
constexpr int base_shove_velocity = 1000;
constexpr int shove_velocity_per_excess_strength = 125;
constexpr int max_shove_velocity = 2000;

auto strength_requirement( const units::mass vehicle_mass ) -> int;
auto strength_requirement( const vehicle &veh ) -> int;
auto strength_penalty( creature_size thrower_size ) -> int;
auto effective_throw_strength( creature_size thrower_size, int throw_strength ) -> int;
auto throw_range( int throw_strength, int strength_requirement ) -> int;
auto shove_velocity( int throw_strength, int strength_requirement ) -> int;

} // namespace vehicle_throw

#pragma once

#include <algorithm>

#include "units.h"
#include "vehicle.h"

namespace vehicle_throw
{

constexpr int kilograms_per_strength = 100;
constexpr int base_throw_range = 1;
constexpr int range_strength_step = 2;

auto strength_requirement( const units::mass vehicle_mass ) -> int;
auto strength_requirement( const vehicle &veh ) -> int;
auto throw_range( int throw_strength, int strength_requirement ) -> int;

} // namespace vehicle_throw

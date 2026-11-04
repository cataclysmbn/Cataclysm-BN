#pragma once

#include "engine_client_state.h"

class vehicle;

namespace engine_client::world
{

/// What part `part` of `veh` itself looks like facing `degrees` (0 east, clockwise), with its own
/// colours, though another part of its mount shows over it on the map.
auto vehicle_part_own_appearance( const vehicle &veh, int part, int degrees ) -> look;

} // namespace engine_client::world

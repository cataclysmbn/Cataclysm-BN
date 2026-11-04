#pragma once

#include "engine_client_state.h"

class vpart_info;

namespace engine_client::world
{

/// What a vehicle part of type `info` shows, whole and facing north, as the map publishes it.
auto vehicle_part_type_appearance( const vpart_info &info ) -> look;

} // namespace engine_client::world

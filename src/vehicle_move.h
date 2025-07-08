#pragma once

#include "units_angle.h"
#include "point.h"

class map;
class vehicle;

namespace vehicle_movement
{

struct rail_processing_result {
    bool do_turn = false;
    bool do_shift = false;
    units::angle turn_dir;
    tripoint shift_amount;
};

/**
 * Decides how the vehicle should move in order to follow rails, or get on rails.
 */
rail_processing_result process_movement_on_rails( const map &m, const vehicle &veh );

/**
 * Returns whether the vehicle is currently on rails.
 */
bool is_on_rails( const map &m, const vehicle &veh );

} // namespace vehicle_movement



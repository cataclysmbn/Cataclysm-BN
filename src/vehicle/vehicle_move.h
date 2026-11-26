#pragma once

#include "coordinates.h"
#include "units_angle.h"

class map;
class vehicle;

namespace vehicle_movement {

struct rail_processing_result {
    bool do_turn = false;
    bool do_shift = false;
    units::angle turn_dir;
    tripoint_rel_ms shift_amount;
};

/**
 * Decides how the vehicle should move in order to follow rails, or get on rails.
 */
auto process_movement_on_rails(const map& m, const vehicle& veh) -> rail_processing_result;

/**
 * Returns whether the vehicle is currently on rails.
 */
auto is_on_rails(const map& m, const vehicle& veh) -> bool;

} // namespace vehicle_movement

#pragma once

#include "calendar.h"
#include "units_temperature.h"

namespace rot {

/// Real time for @p remaining_rot to accumulate at @p temp; INDEFINITELY_LONG_DURATION if it never
/// does.
auto time_to_rot(time_duration remaining_rot, units::temperature temp) -> time_duration;

} // namespace rot

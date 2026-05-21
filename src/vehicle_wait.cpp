#include "vehicle_wait.h"

#include "vehicle.h"

namespace vehicle_wait
{

auto is_wait_blocked_by_movement( const vehicle &veh ) -> bool
{
    return ( veh.velocity != 0 || veh.cruise_velocity != 0 ) && !veh.is_flying_in_air();
}

auto should_offer_flying_wait_durations( const vehicle &veh ) -> bool
{
    return veh.is_flying_in_air();
}

} // namespace vehicle_wait

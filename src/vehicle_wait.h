#pragma once

class vehicle;

namespace vehicle_wait
{

auto is_wait_blocked_by_movement( const vehicle &veh ) -> bool;
auto should_offer_flying_wait_durations( const vehicle &veh ) -> bool;

} // namespace vehicle_wait

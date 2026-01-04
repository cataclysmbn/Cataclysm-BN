#pragma once

class vehicle;
struct vehicle_part;

namespace vehicle_funcs
{

///Attempt to automatically reload a turret from vehicle cargo
///@param veh the vehicle containing the turret
///@param pt the turret part to reload
///@return true if reload was successful
auto try_autoload_turret( vehicle &veh, vehicle_part &pt ) -> bool;

///Process all autoloaders on the vehicle, reloading turrets at regular intervals
///@param veh the vehicle to process
void process_autoloaders( vehicle &veh );

} // namespace vehicle_funcs

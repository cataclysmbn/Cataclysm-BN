#pragma once

class vehicle;

namespace vehicle_control_helpers
{

inline auto get_vertical_controlled_vehicle( vehicle *player_vehicle,
        bool local_vehicle_in_control,
        vehicle *remote_vehicle ) -> vehicle * // *NOPAD*
{
    if( remote_vehicle != nullptr ) {
        return remote_vehicle;
    }

    if( local_vehicle_in_control ) {
        return player_vehicle;
    }

    return nullptr;
}

} // namespace vehicle_control_helpers

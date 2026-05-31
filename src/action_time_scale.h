#pragma once

#include "options.h"

namespace action_time_scale
{

inline constexpr auto percent_denominator = 100;
inline constexpr auto factor_denominator = percent_denominator * percent_denominator;

inline auto scaled_action_factor( const char *option_id ) -> int
{
    return get_option<int>( "TIME_ACTION_SCALE" ) * get_option<int>( option_id );
}

inline auto player_action_factor() -> int
{
    return scaled_action_factor( "PLAYER_ACTION_SCALE" );
}

inline auto npc_action_factor() -> int
{
    return scaled_action_factor( "NPC_ACTION_SCALE" );
}

inline auto monster_action_factor() -> int
{
    return scaled_action_factor( "MONSTER_SPEED" );
}

} // namespace action_time_scale

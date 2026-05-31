#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

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

inline auto activity_progress_factor() -> int
{
    return scaled_action_factor( "ACTIVITY_PROGRESS_SCALE" );
}

inline auto scaled_moves( const int base_moves, const int action_factor ) -> int
{
    const auto scaled = static_cast<int64_t>( base_moves ) * action_factor;
    return std::max( 1, static_cast<int>( ( scaled + factor_denominator / 2 ) /
                                          factor_denominator ) );
}

inline auto activity_progress_per_turn() -> int
{
    return scaled_moves( 100, activity_progress_factor() );
}

inline auto activity_progress_for_turns( const int turns ) -> int
{
    const auto progress = static_cast<int64_t>( std::max( 0, turns ) ) *
                          activity_progress_per_turn();
    return static_cast<int>( std::min<int64_t>( progress, std::numeric_limits<int>::max() ) );
}

} // namespace action_time_scale

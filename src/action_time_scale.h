#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "calendar.h"
#include "options.h"

namespace action_time_scale
{

inline constexpr auto percent_denominator = 100;
inline constexpr auto factor_denominator = percent_denominator * percent_denominator;
inline constexpr auto triple_factor_denominator = factor_denominator * percent_denominator;
inline constexpr auto base_moves_per_turn = 100;

inline auto calendar_turns_this_tick() -> int;

inline auto scaled_action_factor( const char *option_id ) -> int
{
    return get_option<int>( "TIME_ACTION_SCALE" ) * get_option<int>( option_id );
}

inline auto scaled_tick_action_factor( const char *option_id ) -> int
{
    return std::max( percent_denominator, get_option<int>( "TIME_ACTION_SCALE" ) ) *
           get_option<int>( option_id );
}

inline auto player_action_factor() -> int
{
    return scaled_action_factor( "PLAYER_ACTION_SCALE" );
}

inline auto player_tick_action_factor() -> int
{
    return scaled_tick_action_factor( "PLAYER_ACTION_SCALE" );
}

inline auto npc_action_factor() -> int
{
    return scaled_action_factor( "NPC_ACTION_SCALE" );
}

inline auto npc_tick_action_factor() -> int
{
    return scaled_tick_action_factor( "NPC_ACTION_SCALE" );
}

template<typename CharacterLike>
inline auto character_action_factor( const CharacterLike &who ) -> int
{
    return who.is_npc() ? npc_action_factor() : player_action_factor();
}

template<typename CharacterLike>
inline auto character_tick_action_factor( const CharacterLike &who ) -> int
{
    return who.is_npc() ? npc_tick_action_factor() : player_tick_action_factor();
}

inline auto monster_action_factor() -> int
{
    return scaled_action_factor( "MONSTER_SPEED" );
}

inline auto monster_tick_action_factor() -> int
{
    return scaled_tick_action_factor( "MONSTER_SPEED" );
}

inline auto vehicle_control_factor() -> int
{
    return scaled_action_factor( "VEHICLE_CONTROL_SCALE" );
}

inline auto vehicle_control_tick_factor() -> int
{
    return scaled_tick_action_factor( "VEHICLE_CONTROL_SCALE" );
}

inline auto overmap_horde_factor() -> int64_t
{
    return static_cast<int64_t>( monster_action_factor() ) *
           get_option<int>( "OVERMAP_HORDE_SCALE" );
}

inline auto scaled_overmap_horde_speed( const double base_speed ) -> double
{
    return base_speed * static_cast<double>( overmap_horde_factor() ) /
           triple_factor_denominator;
}

inline auto activity_progress_factor() -> int
{
    return scaled_action_factor( "ACTIVITY_PROGRESS_SCALE" );
}

inline auto activity_tick_progress_factor() -> int
{
    return scaled_tick_action_factor( "ACTIVITY_PROGRESS_SCALE" );
}

inline auto scaled_moves( const int base_moves, const int action_factor ) -> int
{
    const auto scaled = static_cast<int64_t>( base_moves ) * action_factor;
    return std::max( 1, static_cast<int>( ( scaled + factor_denominator / 2 ) /
                                          factor_denominator ) );
}

inline auto scaled_relative_cost( const int base_cost, const int actor_factor,
                                  const int system_factor ) -> int
{
    if( base_cost <= 0 ) {
        return 0;
    }
    const auto scaled = static_cast<int64_t>( base_cost ) * std::max( 1, actor_factor );
    const auto rate = static_cast<int64_t>( std::max( 1, system_factor ) );
    const auto cost = ( scaled + rate / 2 ) / rate;
    return static_cast<int>( std::clamp<int64_t>( cost, 1, std::numeric_limits<int>::max() ) );
}

inline auto activity_progress_per_tick() -> int
{
    return scaled_moves( base_moves_per_turn, activity_tick_progress_factor() );
}

inline auto activity_progress_per_calendar_turn() -> int
{
    return scaled_moves( base_moves_per_turn, activity_progress_factor() );
}

inline auto calendar_progress_per_tick() -> int
{
    const auto progress = static_cast<int64_t>( base_moves_per_turn ) *
                          calendar_turns_this_tick();
    return static_cast<int>( std::min<int64_t>( progress, std::numeric_limits<int>::max() ) );
}

inline auto calendar_progress_for_turns( const int turns ) -> int
{
    const auto progress = static_cast<int64_t>( std::max( 0, turns ) ) *
                          base_moves_per_turn;
    return static_cast<int>( std::min<int64_t>( progress, std::numeric_limits<int>::max() ) );
}

inline auto activity_progress_for_turns( const int turns ) -> int
{
    const auto progress = static_cast<int64_t>( std::max( 0, turns ) ) *
                          activity_progress_per_calendar_turn();
    return static_cast<int>( std::min<int64_t>( progress, std::numeric_limits<int>::max() ) );
}

inline auto activity_progress_from_actor_moves( const int actor_moves,
        const int actor_factor ) -> double
{
    if( actor_moves <= 0 ) {
        return 0.0;
    }

    const auto actor_moves_per_turn = scaled_moves( base_moves_per_turn, actor_factor );
    return actor_moves * static_cast<double>( activity_progress_per_tick() ) / actor_moves_per_turn;
}

inline auto actor_moves_for_activity_progress( const double progress_moves,
        const int actor_factor ) -> int
{
    if( progress_moves <= 0.0 ) {
        return 0;
    }

    const auto actor_moves_per_turn = scaled_moves( base_moves_per_turn, actor_factor );
    const auto cost = std::ceil( progress_moves * actor_moves_per_turn /
                                 activity_progress_per_tick() );
    return static_cast<int>( std::clamp( cost, 1.0,
                                         static_cast<double>( std::numeric_limits<int>::max() ) ) );
}

template<typename CharacterLike>
inline auto character_moves_per_tick( const CharacterLike &who ) -> int
{
    return scaled_moves( who.get_speed(), character_tick_action_factor( who ) );
}

template<typename CharacterLike>
inline auto activity_progress_from_actor_moves( const CharacterLike &who ) -> double
{
    return activity_progress_from_actor_moves( who.get_moves(), character_tick_action_factor( who ) );
}

template<typename CharacterLike>
inline auto actor_moves_for_activity_progress( const CharacterLike &who,
        const double progress_moves ) -> int
{
    return actor_moves_for_activity_progress( progress_moves, character_tick_action_factor( who ) );
}

template<typename CharacterLike>
inline auto vehicle_control_cost( const CharacterLike &who, const int base_cost ) -> int
{
    return scaled_relative_cost( base_cost, character_tick_action_factor( who ),
                                 vehicle_control_tick_factor() );
}

inline auto turns_for_progress( const int progress_moves, const int progress_per_turn ) -> int
{
    const auto moves = static_cast<int64_t>( std::max( 0, progress_moves ) );
    const auto rate = static_cast<int64_t>( std::max( 1, progress_per_turn ) );
    const auto turns = ( moves + rate - 1 ) / rate;
    return static_cast<int>( std::min<int64_t>( turns, std::numeric_limits<int>::max() ) );
}

inline auto activity_turns_for_progress( const int progress_moves ) -> int
{
    return turns_for_progress( progress_moves, activity_progress_per_calendar_turn() );
}

inline auto calendar_turns_this_tick_ref() -> int &
{
    static auto turns = 1;
    return turns;
}

inline auto set_calendar_turns_this_tick( const int turns ) -> void
{
    calendar_turns_this_tick_ref() = std::max( 1, turns );
}

inline auto calendar_turns_this_tick() -> int
{
    return calendar_turns_this_tick_ref();
}

class scoped_calendar_turns_this_tick
{
    private:
        int old_turns = calendar_turns_this_tick();

    public:
        explicit scoped_calendar_turns_this_tick( const int turns ) {
            set_calendar_turns_this_tick( turns );
        }

        ~scoped_calendar_turns_this_tick() {
            set_calendar_turns_this_tick( old_turns );
        }

        scoped_calendar_turns_this_tick( const scoped_calendar_turns_this_tick & ) = delete;
        auto operator=( const scoped_calendar_turns_this_tick & ) -> scoped_calendar_turns_this_tick & =
            delete;
};

inline auto calendar_duration_this_tick() -> time_duration
{
    return time_duration::from_turns( calendar_turns_this_tick() );
}

inline auto calendar_ticks_crossed_this_tick( const time_point &to,
        const time_duration &event_frequency ) -> int
{
    if( event_frequency <= 0_turns ) {
        return 0;
    }
    return calendar::ticks_between( to - calendar_duration_this_tick(), to, event_frequency );
}

inline auto calendar_ticks_crossed_this_tick( const time_duration &event_frequency ) -> int
{
    return calendar_ticks_crossed_this_tick( calendar::turn, event_frequency );
}

inline auto once_every_this_tick( const time_duration &event_frequency ) -> bool
{
    return calendar_ticks_crossed_this_tick( event_frequency ) > 0;
}

inline auto calendar_turns_for_remainder( const int turn_remainder ) -> int
{
    const auto global_scale = get_option<int>( "TIME_ACTION_SCALE" );
    if( global_scale >= percent_denominator ) {
        return 1;
    }

    const auto turns = ( turn_remainder + percent_denominator ) / global_scale;
    return std::max( 1, turns );
}

inline auto set_calendar_turns_this_tick_to_next_tick( const int turn_remainder ) -> void
{
    set_calendar_turns_this_tick( calendar_turns_for_remainder( turn_remainder ) );
}

inline auto calendar_turns_for_next_tick( int &turn_remainder ) -> int
{
    const auto global_scale = get_option<int>( "TIME_ACTION_SCALE" );
    if( global_scale >= percent_denominator ) {
        turn_remainder = 0;
        return 1;
    }

    turn_remainder += percent_denominator;
    const auto turns = turn_remainder / global_scale;
    turn_remainder %= global_scale;
    return std::max( 1, turns );
}

} // namespace action_time_scale

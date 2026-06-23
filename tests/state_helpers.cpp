#include "state_helpers.h"

#include "calendar.h"
#include "cata_arena.h"
#include "map.h"
#include "map_helpers.h"
#include "name.h"
#include "player_helpers.h"
#include "weather.h"

namespace
{

auto full_test_state() -> enum_bitset<test_state>
{
    return state::avatar | state::creature | state::npc | state::vehicle |
           state::map | state::time | state::name | state::arena | state::weather;
}

} // namespace

auto clear_states( const enum_bitset<test_state> &states ) -> void
{
    auto normalized_states = states;

    disable_mapgen = true;

    if( normalized_states.test( state::npc ) ) {
        normalized_states.set( state::avatar );
    }

    if( normalized_states.test( state::weather ) ) {
        auto &weather = get_weather();
        weather.weather_id = weather_type_id( "clear" );
        weather.weather_override = weather_type_id::NULL_ID();
        weather.nextweather = calendar::before_time_starts;
        weather.temperature = 0_c;
        weather.water_temperature = 0_c;
        weather.lightning_active = false;
        weather.winddirection = 0;
        weather.windspeed = 0;
        weather.wind_direction_override = std::nullopt;
        weather.windspeed_override = std::nullopt;
        weather.clear_temp_cache();
    }

    if( normalized_states.test( state::avatar ) ) {
        clear_avatar();
    }

    if( normalized_states.test( state::map ) ) {
        clear_map();
    } else {
        if( normalized_states.test( state::npc ) ) {
            clear_npcs();
        }
        if( normalized_states.test( state::creature ) ) {
            clear_creatures();
        }
        if( normalized_states.test( state::vehicle ) ) {
            clear_vehicles();
        }
    }

    if( normalized_states.test( state::time ) ) {
        set_time( calendar::turn_zero );
    }
    if( normalized_states.test( state::name ) ) {
        Name::clear();
    }
    if( normalized_states.test( state::arena ) ) {
        cleanup_arenas();
    }
}

auto clear_all_state() -> void
{
    clear_states( full_test_state() );
}

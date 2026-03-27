#include "state_helpers.h"

#include "calendar.h"
#include "cata_arena.h"
#include "map.h"
#include "map_helpers.h"
#include "name.h"
#include "player_helpers.h"
#include "weather.h"

void clear_all_state( )
{
    disable_mapgen = true;
    get_weather().weather_id = weather_type_id( "clear" );
    // clear_avatar() must come before clear_map() so the player is at the map
    // center (60,60) when clear_npcs() calls reload_npcs().  If a previous test
    // moved the player far from the default spawn area, reload_npcs() would miss
    // NPCs placed near (50–60, 60) and leave stale entries in the submaps.
    clear_avatar();
    clear_map();
    set_time( calendar::turn_zero );
    Name::clear();


    cleanup_arenas();
}

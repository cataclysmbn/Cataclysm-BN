#include "catch/catch.hpp"

#include <memory>

#include "calendar.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "mapdata.h"
#include "monster.h"
#include "options_helpers.h"
#include "state_helpers.h"

struct tripoint;

static monster &spawn_and_clear( const tripoint &pos, bool set_floor )
{
    if( set_floor ) {
        get_map().set( pos, t_floor, f_null );
    }
    return spawn_test_monster( "mon_zombie", pos );
}

static const time_point midday = calendar::turn_zero + 12_hours;

TEST_CASE( "monsters shouldn't see through floors", "[vision]" )
{
    clear_all_state();
    override_option opt( "ZLEVELS", "true" );
    override_option opt2( "FOV_3D", "true" );
    bool old_fov_3d = fov_3d;
    fov_3d = true;
    calendar::turn = midday;
    monster &upper = spawn_and_clear( { 5, 5, 0 }, true );
    monster &adjacent = spawn_and_clear( { 5, 6, 0 }, true );
    monster &distant = spawn_and_clear( { 5, 3, 0 }, true );
    monster &lower = spawn_and_clear( { 5, 5, -1 }, true );
    monster &deep = spawn_and_clear( { 5, 5, -2 }, true );
    monster &sky = spawn_and_clear( { 5, 5, 1 }, false );

    // First check monsters whose vision should be blocked by floors.
    // One intervening floor between monsters.
    CHECK( !upper.sees( lower ) );
    CHECK( !lower.sees( upper ) );
    // Two intervening floors between monsters.
    CHECK( !upper.sees( deep ) );
    CHECK( !deep.sees( upper ) );
    // One intervening floor and an open space between monsters.
    CHECK( !sky.sees( lower ) );
    CHECK( !lower.sees( sky ) );
    // One intervening floor between monsters, and offset one tile horizontally.
    CHECK( !adjacent.sees( lower ) );
    CHECK( !lower.sees( adjacent ) );
    // One intervening floor between monsters, and offset two tiles horizontally.
    CHECK( !distant.sees( lower ) );
    CHECK( !lower.sees( distant ) );
    // Two intervening floors between monsters, and offset one tile horizontally.
    CHECK( !adjacent.sees( deep ) );
    CHECK( !deep.sees( adjacent ) );
    // Two intervening floor between monsters, and offset two tiles horizontally.
    CHECK( !distant.sees( deep ) );
    CHECK( !deep.sees( distant ) );

    // Then cases where they should be able to see each other.
    // No floor between monsters
    CHECK( upper.sees( sky ) );
    CHECK( sky.sees( upper ) );
    // Adjacent monsters.
    CHECK( upper.sees( adjacent ) );
    CHECK( adjacent.sees( upper ) );
    // distant monsters.
    CHECK( upper.sees( distant ) );
    CHECK( distant.sees( upper ) );
    // One intervening vertical tile and one intervening horizontal tile.
    CHECK( sky.sees( adjacent ) );
    CHECK( adjacent.sees( sky ) );
    // One intervening vertical tile and two intervening horizontal tiles.
    CHECK( sky.sees( distant ) );
    CHECK( distant.sees( sky ) );
    fov_3d = old_fov_3d;
}

TEST_CASE( "monsters_dont_see_through_vehicle_holes", "[vision]" )
{
    clear_all_state();
    calendar::turn = midday;
    put_player_underground();
    tripoint origin( 60, 60, 0 );

    get_map().add_vehicle( vproto_id( "apc" ), origin, -45_degrees, 0, 0 );
    get_map().build_map_cache( 0 );

    tripoint mon_origin = origin + tripoint( -2, 1, 0 );

    monster &inside = spawn_test_monster( "mon_zombie", mon_origin );

    tripoint second_origin = mon_origin + tripoint_north_west;

    monster &outside = spawn_test_monster( "mon_zombie", second_origin );

    CHECK( !inside.sees( outside ) );
    CHECK( !outside.sees( inside ) );

}

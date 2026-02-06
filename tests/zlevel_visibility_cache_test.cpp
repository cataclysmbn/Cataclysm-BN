#include "catch/catch.hpp"
#include "map_helpers.h"

#include "avatar.h"
#include "calendar.h"
#include "game.h"
#include "map.h"
#include "options_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE( "opening_floor_invalidates_below_seen_cache", "[vision][zlevel]" )
{
    clear_all_state();

    map &here = get_map();

    const ter_id t_floor( "t_floor" );
    const ter_id t_open_air( "t_open_air" );

    // Place the player on z=1 so we have a meaningful "below".
    g->place_player( tripoint( 60, 60, 1 ) );

    const tripoint hole_pos = g->u.pos() + point_east;

    // Ensure deterministic starting terrain.
    here.ter_set( hole_pos, t_floor );
    here.ter_set( hole_pos + tripoint_below, t_floor );

    // Simulate the pre-breach state where the below-z tile wasn't previously seen.
    level_cache &below_cache = here.access_cache( hole_pos.z - 1 );
    below_cache.seen_cache_dirty = false;
    below_cache.seen_cache[hole_pos.x][hole_pos.y] = 0.0f;
    below_cache.camera_cache[hole_pos.x][hole_pos.y] = 0.0f;

    REQUIRE_FALSE( here.access_cache( hole_pos.z - 1 ).seen_cache_dirty );

    // Breach the floor (what explosions often do). This must invalidate the below-z seen cache.
    here.ter_set( hole_pos, t_open_air );

    CHECK( here.access_cache( hole_pos.z - 1 ).seen_cache_dirty );
}

TEST_CASE( "opening_floor_rebuilds_below_light", "[vision][zlevel]" )
{
    clear_all_state();

    override_option fov3d( "FOV_3D", "false" );

    map &here = get_map();

    const ter_id t_floor( "t_floor" );
    const ter_id t_open_air( "t_open_air" );

    g->place_player( tripoint( 60, 60, 1 ) );

    calendar::turn = calendar::turn_zero + 12_hours;

    const tripoint hole_pos = g->u.pos() + point_east;

    here.ter_set( hole_pos, t_open_air );
    here.ter_set( hole_pos + tripoint_below, t_floor );

    here.build_map_cache( g->u.posz() );
    here.update_visibility_cache( g->u.posz() );

    const level_cache &below_cache = here.access_cache( hole_pos.z - 1 );

    CHECK( below_cache.seen_cache[hole_pos.x][hole_pos.y] > 0.0f );
    CHECK( below_cache.visibility_cache[hole_pos.x][hole_pos.y] != lit_level::BLANK );
}

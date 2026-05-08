#include "map_helpers.h"

#include "catch/catch.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "distribution_grid.h"
#include "field.h"
#include "game.h"
#include "game_constants.h"
#include "map.h"
#include "mapbuffer.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "npc.h"
#include "overmapbuffer.h"
#include "point.h"
#include "type_id.h"

// Remove all vehicles from the map
void clear_vehicles()
{
    std::vector<vehicle *> vehicles;
    vehicles.reserve( g->m.get_vehicles().size() );

    for( wrapped_vehicle &veh : g->m.get_vehicles() ) {
        vehicles.push_back( veh.v );
    }

    for( vehicle *veh : vehicles ) {
        g->m.destroy_vehicle( veh );
    }
}

void wipe_map_terrain()
{
    map &here = get_map();
    const int mapsize = here.getmapsize() * SEEX;
    for( int z = -1; z <= OVERMAP_HEIGHT; ++z ) {
        const ter_id terrain = z == 0 ? t_grass : z < 0 ? t_rock : t_open_air;
        for( int x = 0; x < mapsize; ++x ) {
            for( int y = 0; y < mapsize; ++y ) {
                g->m.set( { x, y, z}, terrain, f_null );
            }
        }
    }
    clear_vehicles();
    g->m.invalidate_map_cache( 0 );
    g->m.build_map_cache( 0, true );
}

void clear_creatures()
{
    // Remove any interfering monsters.
    g->clear_zombies();
}

void clear_npcs()
{
    // Reload to ensure that all active NPCs are in the overmapbuffer.
    g->reload_npcs();
    for( npc &n : g->all_npcs() ) {
        n.die( nullptr );
    }
    g->cleanup_dead();
}

void clear_fields( const int zlevel )
{
    const int mapsize = g->m.getmapsize() * SEEX;
    for( int x = 0; x < mapsize; ++x ) {
        for( int y = 0; y < mapsize; ++y ) {
            const tripoint p( x, y, zlevel );
            std::vector<field_type_id> fields;
            for( auto &pr : g->m.field_at( p ) ) {
                fields.push_back( pr.second.get_field_type() );
            }
            for( field_type_id f : fields ) {
                g->m.remove_field( p, f );
            }
        }
    }
}

void clear_items( const int zlevel )
{
    const int mapsize = g->m.getmapsize() * SEEX;
    for( int x = 0; x < mapsize; ++x ) {
        for( int y = 0; y < mapsize; ++y ) {
            g->m.i_clear( { x, y, zlevel } );
        }
    }
}

void clear_overmap()
{
    MAPBUFFER.clear();
    ACTIVE_OVERMAP_BUFFER.clear();
}

void clear_map()
{
    // Clearing all z-levels is rather slow, so just clear the ones I know the
    // tests use for now.
    for( int z = -2; z <= 0; ++z ) {
        clear_fields( z );
    }
    wipe_map_terrain();
    clear_npcs();
    clear_creatures();
    g->m.clear_traps();
    for( int z = -2; z <= 0; ++z ) {
        clear_items( z );
    }
    // Reset the distribution grid tracker so that stale grids from a previous
    // test's Catch2 WHEN section do not bleed into the next run.  The tracker
    // is a global singleton; grid_at() rebuilds on demand, so clearing here is safe.
    get_distribution_grid_tracker().clear();
}

void put_player_underground()
{
    // Make sure the player doesn't block the path of the monster being tested.
    g->u.setpos( { 0, 0, -2 } );
}

monster &spawn_test_monster( const std::string &monster_type, const tripoint &start )
{
    monster *const added = g->place_critter_at( mtype_id( monster_type ), start );
    REQUIRE( added );
    return *added;
}

// Build a map of size MAPSIZE_X x MAPSIZE_Y around tripoint_zero with a given
// terrain, and no furniture, traps, or items.
void build_test_map( const ter_id &terrain )
{
    for( const tripoint &p : g->m.points_in_rectangle( tripoint_zero,
            tripoint( MAPSIZE * SEEX, MAPSIZE * SEEY, 0 ) ) ) {
        g->m.furn_set( p, furn_id( "f_null" ) );
        g->m.ter_set( p, terrain );
        g->m.trap_set( p, trap_id( "tr_null" ) );
        g->m.i_clear( p );
    }

    g->m.invalidate_map_cache( 0 );
    g->m.build_map_cache( 0, true );
}

void build_water_test_map( const ter_id &surface, const ter_id &mid, const ter_id &bottom )
{
    constexpr int z_surface = 0;
    constexpr int z_bottom = -2;

    map &here = get_map();
    for( const tripoint &p : here.points_in_rectangle( tripoint_zero,
            tripoint( MAPSIZE * SEEX, MAPSIZE * SEEY, z_bottom ) ) ) {

        if( p.z == z_surface ) {
            here.ter_set( p, surface );
        } else if( p.z < z_surface && p.z > z_bottom ) {
            here.ter_set( p, mid );
        } else if( p.z == z_bottom ) {
            here.ter_set( p, bottom );
        }
    }

    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0, true );
}

void set_time( const time_point &time )
{
    calendar::turn = time;
    g->reset_light_level();
    const auto z = g->u.posz();
    g->m.update_visibility_cache( z );
    g->m.invalidate_map_cache( z );
    g->m.build_map_cache( z );
}

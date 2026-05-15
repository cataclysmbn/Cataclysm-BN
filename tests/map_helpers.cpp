#include "map_helpers.h"

#include "catch/catch.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "coordinates.h"
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
#include "submap.h"
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
                g->m.set( tripoint_bub_ms{ x, y, z}, terrain, f_null );
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
    map &here = get_map();
    const int mapsize = here.getmapsize();
    for( int x = 0; x < mapsize; ++x ) {
        for( int y = 0; y < mapsize; ++y ) {
            const tripoint_bub_sm grid_pos( x, y, zlevel );
            submap *const sm = here.get_submap_at_grid( grid_pos );
            if( sm == nullptr || sm->field_count == 0 ) {
                continue;
            }

            const auto clear_field_at = [&]( const point_sm_ms & local ) {
                const tripoint_bub_ms p = project_combine( grid_pos, local );
                field &field_at_pos = sm->get_field( local );
                if( field_at_pos.field_count() == 0 ) {
                    return;
                }

                std::vector<field_type_id> fields;
                std::ranges::transform( field_at_pos, std::back_inserter( fields ),
                []( const std::pair<const field_type_id, field_entry> &pr ) {
                    return pr.second.get_field_type();
                } );

                std::ranges::for_each( fields, [&]( const field_type_id & f ) {
                    here.remove_field( p, f );
                } );
            };

            const auto field_positions = sm->field_cache;
            std::ranges::for_each( field_positions, clear_field_at );
            if( sm->field_count != 0 ) {
                std::ranges::for_each( submap_tiles(), clear_field_at );
                sm->field_count = 0;
            }
            sm->field_cache.clear();
        }
    }
}

void clear_items( const int zlevel )
{
    const int mapsize = g->m.getmapsize() * SEEX;
    for( int x = 0; x < mapsize; ++x ) {
        for( int y = 0; y < mapsize; ++y ) {
            g->m.i_clear( tripoint_bub_ms{ x, y, zlevel } );
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
    g->m.set_abs_sub( tripoint_abs_sm( g->m.get_abs_sub().xy(), 0 ) );

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
    g->u.setpos( tripoint_bub_ms{ 0, 0, -2 } );
}

monster &spawn_test_monster( const std::string &monster_type, const tripoint_bub_ms &start )
{
    monster *const added = g->place_critter_at( mtype_id( monster_type ), start );
    REQUIRE( added );
    return *added;
}

// Build a map of size MAPSIZE_X x MAPSIZE_Y around tripoint_bub_ms::zero() with a given
// terrain, and no furniture, traps, or items.
void build_test_map( const ter_id &terrain )
{
    for( const tripoint_bub_ms &p : g->m.points_in_rectangle( tripoint_bub_ms::zero(),
            tripoint_bub_ms( MAPSIZE * SEEX, MAPSIZE * SEEY, 0 ) ) ) {
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
    for( const tripoint_bub_ms &p : here.points_in_rectangle( tripoint_bub_ms::zero(),
            tripoint_bub_ms( MAPSIZE * SEEX, MAPSIZE * SEEY, z_bottom ) ) ) {

        if( p.z() == z_surface ) {
            here.ter_set( p, surface );
        } else if( p.z() < z_surface && p.z() > z_bottom ) {
            here.ter_set( p, mid );
        } else if( p.z() == z_bottom ) {
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
    const auto z = g->u.bub_pos().z();
    g->m.update_visibility_cache( z );
    g->m.invalidate_map_cache( z );
    g->m.build_map_cache( z );
}

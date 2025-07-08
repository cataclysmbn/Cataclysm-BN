#include "mapgen_functions.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <initializer_list>
#include <iterator>
#include <map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "calendar.h"
#include "character_id.h"
#include "debug.h"
#include "enums.h"
#include "field_type.h"
#include "flood_fill.h"
#include "game.h"
#include "game_constants.h"
#include "int_id.h"
#include "line.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "mapgen.h"
#include "mapgendata.h"
#include "mapgenformat.h"
#include "omdata.h"
#include "overmap.h"
#include "point.h"
#include "regional_settings.h"
#include "rng.h"
#include "string_id.h"
#include "trap.h"
#include "vehicle_group.h"
#include "weighted_list.h"

static const itype_id itype_hat_hard( "hat_hard" );
static const itype_id itype_jackhammer( "jackhammer" );
static const itype_id itype_mask_dust( "mask_dust" );

static const mtype_id mon_bee( "mon_bee" );
static const mtype_id mon_beekeeper( "mon_beekeeper" );
static const mtype_id mon_zombie_jackson( "mon_zombie_jackson" );

static const mongroup_id GROUP_ZOMBIE( "GROUP_ZOMBIE" );

class npc_template;

tripoint rotate_point( const tripoint &p, int rotations )
{
    if( p.x < 0 || p.x >= SEEX * 2 ||
        p.y < 0 || p.y >= SEEY * 2 ) {
        debugmsg( "Point out of range: %d,%d,%d", p.x, p.y, p.z );
        // Mapgen is vulnerable, don't supply invalid points, debugmsg is enough
        return tripoint( 0, 0, p.z );
    }

    rotations = rotations % 4;

    tripoint ret = p;
    switch( rotations ) {
        case 0:
            break;
        case 1:
            ret.x = p.y;
            ret.y = SEEX * 2 - 1 - p.x;
            break;
        case 2:
            ret.x = SEEX * 2 - 1 - p.x;
            ret.y = SEEY * 2 - 1 - p.y;
            break;
        case 3:
            ret.x = SEEY * 2 - 1 - p.y;
            ret.y = p.x;
            break;
    }

    return ret;
}

building_gen_pointer get_mapgen_cfunction( const std::string &ident )
{
    static const std::map<std::string, building_gen_pointer> pointers = { {
            { "null",             &mapgen_null },
            { "test",             &mapgen_test },
            { "crater",           &mapgen_crater },
            { "field",            &mapgen_field },
            { "forest",           &mapgen_forest },
            { "forest_trail_straight",    &mapgen_forest_trail_straight },
            { "forest_trail_curved",      &mapgen_forest_trail_curved },
            // TODO: Add a dedicated dead-end function. For now it copies the straight section above.
            { "forest_trail_end",         &mapgen_forest_trail_straight },
            { "forest_trail_tee",         &mapgen_forest_trail_tee },
            { "forest_trail_four_way",    &mapgen_forest_trail_four_way },
            { "hive",             &mapgen_hive },
            { "road_straight",    &mapgen_road },
            { "road_curved",      &mapgen_road },
            { "road_end",         &mapgen_road },
            { "road_tee",         &mapgen_road },
            { "road_four_way",    &mapgen_road },
            { "field",            &mapgen_field },
            { "highway",          &mapgen_highway },
            { "railroad_straight", &mapgen_railroad },
            { "railroad_curved",   &mapgen_railroad },
            { "railroad_end",      &mapgen_railroad },
            { "railroad_tee",      &mapgen_railroad },
            { "railroad_four_way", &mapgen_railroad },
            { "railroad_bridge",   &mapgen_railroad_bridge },
            { "river_center", &mapgen_river_center },
            { "river_curved_not", &mapgen_river_curved_not },
            { "river_straight",   &mapgen_river_straight },
            { "river_curved",     &mapgen_river_curved },
            { "river_shore",      &mapgen_river_shore },
            { "parking_lot",      &mapgen_parking_lot },
            { "cavern", &mapgen_cavern },
            { "open_air", &mapgen_open_air },
            { "rift", &mapgen_rift },
            { "hellmouth", &mapgen_hellmouth },
            // New rock function - should be default, but isn't yet for compatibility reasons (old overmaps)
            { "empty_rock", &mapgen_rock },
            // Old rock behavior, for compatibility and near caverns and slime pits
            { "rock", &mapgen_rock_partial },

            { "subway_straight",    &mapgen_subway },
            { "subway_curved",      &mapgen_subway },
            { "subway_end",         &mapgen_subway },
            { "subway_tee",         &mapgen_subway },
            { "subway_four_way",    &mapgen_subway },

            { "sewer_straight",    &mapgen_sewer },
            { "sewer_curved",      &mapgen_sewer },
            { "sewer_end",         &mapgen_sewer },
            { "sewer_tee",         &mapgen_sewer },
            { "sewer_four_way",    &mapgen_sewer },

            { "tutorial", &mapgen_tutorial },
            { "lake_shore", &mapgen_lake_shore },
        }
    };
    const auto iter = pointers.find( ident );
    return iter == pointers.end() ? nullptr : iter->second;
}

ter_id grass_or_dirt()
{
    if( one_in( 4 ) ) {
        return t_grass;
    }
    return t_dirt;
}

ter_id clay_or_sand()
{
    if( one_in( 16 ) ) {
        return t_alluvial_deposit;
    }
    if( one_in( 16 ) ) {
        return t_sand;
    }
    return t_clay;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
///// builtin terrain-specific mapgen functions. big multi-overmap-tile terrains are located in
///// mapgen_functions_big.cpp

void mapgen_null( mapgendata &dat )
{
    debugmsg( "Generating null terrain, please report this as a bug" );
    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            dat.m.ter_set( point( i, j ), t_null );
            dat.m.set_radiation( point( i, j ), 0 );
        }
    }
}

void mapgen_test( mapgendata &dat )
{
    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            dat.m.ter_set( point( i, j ), t_grass );
            dat.m.set_radiation( point( i, j ), 0 );
        }
    }
}

void mapgen_crater( mapgendata &dat )
{
    map *const m = &dat.m;
    for( int i = 0; i < 4; i++ ) {
        if( dat.t_nesw[i] != "crater" ) {
            dat.set_dir( i, 6 );
        }
    }

    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( rng( 0, dat.w_fac ) <= i && rng( 0, dat.e_fac ) <= SEEX * 2 - 1 - i &&
                rng( 0, dat.n_fac ) <= j && rng( 0, dat.s_fac ) <= SEEX * 2 - 1 - j ) {
                m->ter_set( point( i, j ), t_pit_shallow );
                m->make_rubble( tripoint( i,  j, m->get_abs_sub().z ), f_rubble_rock );
                m->set_radiation( point( i, j ), rng( 0, 4 ) * rng( 0, 2 ) );
            } else {
                m->ter_set( point( i, j ), dat.groundcover() );
                m->set_radiation( point( i, j ), rng( 0, 2 ) * rng( 0, 2 ) * rng( 0, 2 ) );
            }
        }
    }
    m->place_items( item_group_id( "wreckage" ), 83, point_zero, point( SEEX * 2 - 1, SEEY * 2 - 1 ),
                    true, dat.when() );
}

// TODO: make void map::ter_or_furn_set(const int x, const int y, const ter_furn_id & tfid);
static void ter_or_furn_set( map *m, point p, const ter_furn_id &tfid )
{
    if( tfid.ter != t_null ) {
        m->ter_set( p, tfid.ter );
    } else if( tfid.furn != f_null ) {
        m->furn_set( p, tfid.furn );
    }
}

/*
 * Default above ground non forested 'blank' area; typically a grassy field with a scattering of shrubs,
 *  but changes according to dat->region
 */
void mapgen_field( mapgendata &dat )
{
    map *const m = &dat.m;
    // random area of increased vegetation. Or lava / toxic sludge / etc
    const bool boosted_vegetation = ( dat.region.field_coverage.boost_chance > rng( 0, 1000000 ) );
    const int &mpercent_bush = ( boosted_vegetation ?
                                 dat.region.field_coverage.boosted_mpercent_coverage :
                                 dat.region.field_coverage.mpercent_coverage
                               );

    // one dominant plant type ( for boosted_vegetation == true )
    ter_furn_id altbush = dat.region.field_coverage.pick( true );

    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            point p( i, j );
            // default is
            m->ter_set( p, dat.groundcover() );
            // yay, a shrub ( or tombstone )
            if( mpercent_bush > rng( 0, 1000000 ) ) {
                if( boosted_vegetation && dat.region.field_coverage.boosted_other_mpercent > rng( 0, 1000000 ) ) {
                    // already chose the lucky terrain/furniture/plant/rock/etc
                    ter_or_furn_set( m, p, altbush );
                } else {
                    // pick from weighted list
                    ter_or_furn_set( m, p, dat.region.field_coverage.pick( false ) );
                }
            }
        }
    }

    // FIXME: take 'rock' out and add as regional biome setting
    m->place_items( item_group_id( "field" ), 60, point_zero, point( SEEX * 2 - 1, SEEY * 2 - 1 ), true,
                    dat.when() );
}

void mapgen_hive( mapgendata &dat )
{
    map *const m = &dat.m;
    // Start with a basic forest pattern
    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            int rn = rng( 0, 14 );
            if( rn > 13 ) {
                m->ter_set( point( i, j ), t_tree );
            } else if( rn > 11 ) {
                m->ter_set( point( i, j ), t_tree_young );
            } else if( rn > 10 ) {
                m->ter_set( point( i, j ), t_underbrush );
            } else {
                m->ter_set( point( i, j ), dat.groundcover() );
            }
        }
    }

    // j and i loop through appropriate hive-cell center squares
    const bool is_center = dat.t_nesw[0] == "hive" && dat.t_nesw[1] == "hive" &&
                           dat.t_nesw[2] == "hive" && dat.t_nesw[3] == "hive";
    for( int j = 5; j < SEEY * 2 - 5; j += 6 ) {
        for( int i = ( j == 5 || j == 17 ? 3 : 6 ); i < SEEX * 2 - 5; i += 6 ) {
            if( !one_in( 8 ) ) {
                // Caps are always there
                m->ter_set( point( i, j - 5 ), t_wax );
                m->ter_set( point( i, j + 5 ), t_wax );
                for( int k = -2; k <= 2; k++ ) {
                    for( int l = -1; l <= 1; l++ ) {
                        m->ter_set( point( i + k, j + l ), t_floor_wax );
                    }
                }
                m->add_spawn( mon_bee, 2, { i, j, m->get_abs_sub().z } );
                m->add_spawn( mon_beekeeper, 1, { i, j, m->get_abs_sub().z } );
                m->ter_set( point( i, j - 3 ), t_floor_wax );
                m->ter_set( point( i, j + 3 ), t_floor_wax );
                m->ter_set( point( i - 1, j - 2 ), t_floor_wax );
                m->ter_set( point( i, j - 2 ), t_floor_wax );
                m->ter_set( point( i + 1, j - 2 ), t_floor_wax );
                m->ter_set( point( i - 1, j + 2 ), t_floor_wax );
                m->ter_set( point( i, j + 2 ), t_floor_wax );
                m->ter_set( point( i + 1, j + 2 ), t_floor_wax );

                // Up to two of these get skipped; an entrance to the cell
                int skip1 = rng( 0, SEEX * 2 - 1 );
                int skip2 = rng( 0, SEEY * 2 - 1 );

                m->ter_set( point( i - 1, j - 4 ), t_wax );
                m->ter_set( point( i, j - 4 ), t_wax );
                m->ter_set( point( i + 1, j - 4 ), t_wax );
                m->ter_set( point( i - 2, j - 3 ), t_wax );
                m->ter_set( point( i - 1, j - 3 ), t_wax );
                m->ter_set( point( i + 1, j - 3 ), t_wax );
                m->ter_set( point( i + 2, j - 3 ), t_wax );
                m->ter_set( point( i - 3, j - 2 ), t_wax );
                m->ter_set( point( i - 2, j - 2 ), t_wax );
                m->ter_set( point( i + 2, j - 2 ), t_wax );
                m->ter_set( point( i + 3, j - 2 ), t_wax );
                m->ter_set( point( i - 3, j - 1 ), t_wax );
                m->ter_set( point( i - 3, j ), t_wax );
                m->ter_set( point( i - 3, j - 1 ), t_wax );
                m->ter_set( point( i - 3, j + 1 ), t_wax );
                m->ter_set( point( i - 3, j ), t_wax );
                m->ter_set( point( i - 3, j + 1 ), t_wax );
                m->ter_set( point( i - 2, j + 3 ), t_wax );
                m->ter_set( point( i - 1, j + 3 ), t_wax );
                m->ter_set( point( i + 1, j + 3 ), t_wax );
                m->ter_set( point( i + 2, j + 3 ), t_wax );
                m->ter_set( point( i - 1, j + 4 ), t_wax );
                m->ter_set( point( i, j + 4 ), t_wax );
                m->ter_set( point( i + 1, j + 4 ), t_wax );

                if( skip1 == 0 || skip2 == 0 ) {
                    m->ter_set( point( i - 1, j - 4 ), t_floor_wax );
                }
                if( skip1 == 1 || skip2 == 1 ) {
                    m->ter_set( point( i, j - 4 ), t_floor_wax );
                }
                if( skip1 == 2 || skip2 == 2 ) {
                    m->ter_set( point( i + 1, j - 4 ), t_floor_wax );
                }
                if( skip1 == 3 || skip2 == 3 ) {
                    m->ter_set( point( i - 2, j - 3 ), t_floor_wax );
                }
                if( skip1 == 4 || skip2 == 4 ) {
                    m->ter_set( point( i - 1, j - 3 ), t_floor_wax );
                }
                if( skip1 == 5 || skip2 == 5 ) {
                    m->ter_set( point( i + 1, j - 3 ), t_floor_wax );
                }
                if( skip1 == 6 || skip2 == 6 ) {
                    m->ter_set( point( i + 2, j - 3 ), t_floor_wax );
                }
                if( skip1 == 7 || skip2 == 7 ) {
                    m->ter_set( point( i - 3, j - 2 ), t_floor_wax );
                }
                if( skip1 == 8 || skip2 == 8 ) {
                    m->ter_set( point( i - 2, j - 2 ), t_floor_wax );
                }
                if( skip1 == 9 || skip2 == 9 ) {
                    m->ter_set( point( i + 2, j - 2 ), t_floor_wax );
                }
                if( skip1 == 10 || skip2 == 10 ) {
                    m->ter_set( point( i + 3, j - 2 ), t_floor_wax );
                }
                if( skip1 == 11 || skip2 == 11 ) {
                    m->ter_set( point( i - 3, j - 1 ), t_floor_wax );
                }
                if( skip1 == 12 || skip2 == 12 ) {
                    m->ter_set( point( i - 3, j ), t_floor_wax );
                }
                if( skip1 == 13 || skip2 == 13 ) {
                    m->ter_set( point( i - 3, j - 1 ), t_floor_wax );
                }
                if( skip1 == 14 || skip2 == 14 ) {
                    m->ter_set( point( i - 3, j + 1 ), t_floor_wax );
                }
                if( skip1 == 15 || skip2 == 15 ) {
                    m->ter_set( point( i - 3, j ), t_floor_wax );
                }
                if( skip1 == 16 || skip2 == 16 ) {
                    m->ter_set( point( i - 3, j + 1 ), t_floor_wax );
                }
                if( skip1 == 17 || skip2 == 17 ) {
                    m->ter_set( point( i - 2, j + 3 ), t_floor_wax );
                }
                if( skip1 == 18 || skip2 == 18 ) {
                    m->ter_set( point( i - 1, j + 3 ), t_floor_wax );
                }
                if( skip1 == 19 || skip2 == 19 ) {
                    m->ter_set( point( i + 1, j + 3 ), t_floor_wax );
                }
                if( skip1 == 20 || skip2 == 20 ) {
                    m->ter_set( point( i + 2, j + 3 ), t_floor_wax );
                }
                if( skip1 == 21 || skip2 == 21 ) {
                    m->ter_set( point( i - 1, j + 4 ), t_floor_wax );
                }
                if( skip1 == 22 || skip2 == 22 ) {
                    m->ter_set( point( i, j + 4 ), t_floor_wax );
                }
                if( skip1 == 23 || skip2 == 23 ) {
                    m->ter_set( point( i + 1, j + 4 ), t_floor_wax );
                }

                if( is_center ) {
                    m->place_items( item_group_id( "hive_center" ), 90, point( i - 2, j - 2 ), point( i + 2, j + 2 ),
                                    false,
                                    dat.when() );
                } else {
                    m->place_items( item_group_id( "hive" ), 80, point( i - 2, j - 2 ), point( i + 2, j + 2 ), false,
                                    dat.when() );
                }
            }
        }
    }

    if( is_center ) {
        m->place_npc( point( SEEX, SEEY ), string_id<npc_template>( "apis" ) );
    }
}

int terrain_type_to_nesw_array( oter_id terrain_type, bool array[4] )
{
    // count and mark which directions the road goes
    const auto &oter( *terrain_type );
    int num_dirs = 0;
    for( const auto dir : om_direction::all ) {
        num_dirs += ( array[static_cast<int>( dir )] = oter.has_connection( dir ) );
    }
    return num_dirs;
}

// perform dist counterclockwise rotations on a nesw or neswx array
template<typename T>
void nesw_array_rotate( T *array, size_t len, size_t dist )
{
    if( len == 4 ) {
        while( dist-- ) {
            T temp = array[0];
            array[0] = array[1];
            array[1] = array[2];
            array[2] = array[3];
            array[3] = temp;
        }
    } else {
        while( dist-- ) {
            // N E S W NE SE SW NW
            T temp = array[0];
            array[0] = array[4];
            array[4] = array[1];
            array[1] = array[5];
            array[5] = array[2];
            array[2] = array[6];
            array[6] = array[3];
            array[3] = array[7];
            array[7] = temp;
        }
    }
}

// take x/y coordinates in a map and rotate them counterclockwise around the center
static void coord_rotate_cw( int &x, int &y, int rot )
{
    for( ; rot--; ) {
        int temp = y;
        y = x;
        x = ( SEEY * 2 - 1 ) - temp;
    }
}

static bool compare_neswx( bool *a1, std::initializer_list<int> a2 )
{
    return std::equal( std::begin( a2 ), std::end( a2 ), a1,
    []( int a, bool b ) {
        return static_cast<bool>( a ) == b;
    } );
}

// mapgen_road replaces previous mapgen_road_straight _end _curved _tee _four_way
void mapgen_road( mapgendata &dat )
{
    map *const m = &dat.m;
    // start by filling the whole map with grass/dirt/etc
    dat.fill_groundcover();

    // which and how many neighbors have sidewalks?
    bool sidewalks_neswx[8] = {};
    int neighbor_sidewalks = 0;
    // N E S W NE SE SW NW
    for( int dir = 0; dir < 8; dir++ ) {
        sidewalks_neswx[dir] = dat.t_nesw[dir]->has_flag( oter_flags::has_sidewalk );
        neighbor_sidewalks += sidewalks_neswx[dir];
    }

    // which of the cardinal directions get roads?
    bool roads_nesw[4] = {};
    int num_dirs = terrain_type_to_nesw_array( dat.terrain_type(), roads_nesw );
    // if this is a dead end, extend past the middle of the tile
    int dead_end_extension = ( num_dirs == 1 ? 8 : 0 );

    // which way should our roads curve, based on neighbor roads?
    int curvedir_nesw[4] = {};
    // N E S W
    for( int dir = 0; dir < 4; dir++ ) {
        if( !roads_nesw[dir] || dat.t_nesw[dir]->get_type_id().str() != "road" ) {
            continue;
        }

        // n_* contain details about the neighbor being considered
        bool n_roads_nesw[4] = {};
        // TODO: figure out how to call this function without creating a new oter_id object
        int n_num_dirs = terrain_type_to_nesw_array( dat.t_nesw[dir], n_roads_nesw );
        // if 2-way neighbor has a road facing us
        if( n_num_dirs == 2 && n_roads_nesw[( dir + 2 ) % 4] ) {
            // curve towards the direction the neighbor turns
            // our road curves counterclockwise
            if( n_roads_nesw[( dir - 1 + 4 ) % 4] ) {
                curvedir_nesw[dir]--;
            }
            // our road curves clockwise
            if( n_roads_nesw[( dir + 1 ) % 4] ) {
                curvedir_nesw[dir]++;
            }
        }
    }

    // calculate how far to rotate the map so we can work with just one orientation
    // also keep track of diagonal roads and plazas
    int rot = 0;
    bool diag = false;
    int plaza_dir = -1;
    bool fourways_neswx[8] = {};
    // TODO: reduce amount of logical/conditional constructs here
    // TODO: make plazas include adjacent tees
    switch( num_dirs ) {
        case 4:
            // 4-way intersection
            for( int dir = 0; dir < 8; dir++ ) {
                fourways_neswx[dir] = ( dat.t_nesw[dir].id() == "road_nesw" ||
                                        dat.t_nesw[dir].id() == "road_nesw_manhole" );
            }
            // is this the middle, or which side or corner, of a plaza?
            plaza_dir = compare_neswx( fourways_neswx, {1, 1, 1, 1, 1, 1, 1, 1} ) ? 8 :
                        compare_neswx( fourways_neswx, {0, 1, 1, 0, 0, 1, 0, 0} ) ? 7 :
                        compare_neswx( fourways_neswx, {1, 1, 0, 0, 1, 0, 0, 0} ) ? 6 :
                        compare_neswx( fourways_neswx, {1, 0, 0, 1, 0, 0, 0, 1} ) ? 5 :
                        compare_neswx( fourways_neswx, {0, 0, 1, 1, 0, 0, 1, 0} ) ? 4 :
                        compare_neswx( fourways_neswx, {1, 1, 1, 0, 1, 1, 0, 0} ) ? 3 :
                        compare_neswx( fourways_neswx, {1, 1, 0, 1, 1, 0, 0, 1} ) ? 2 :
                        compare_neswx( fourways_neswx, {1, 0, 1, 1, 0, 0, 1, 1} ) ? 1 :
                        compare_neswx( fourways_neswx, {0, 1, 1, 1, 0, 1, 1, 0} ) ? 0 :
                        -1;
            if( plaza_dir > -1 ) {
                rot = plaza_dir % 4;
            }
            break;
        case 3:
            // tee
            // E/S/W, rotate 180 degrees
            if( !roads_nesw[0] ) {
                rot = 2;
                break;
            }
            // N/S/W, rotate 270 degrees
            if( !roads_nesw[1] ) {
                rot = 3;
                break;
            }
            // N/E/S, rotate  90 degrees
            if( !roads_nesw[3] ) {
                rot = 1;
                break;
            }
            // N/E/W, don't rotate
            break;
        case 2:
            // straight or diagonal
            // E/W, rotate  90 degrees
            if( roads_nesw[1] && roads_nesw[3] ) {
                rot = 1;
                break;
            }
            // E/S, rotate  90 degrees
            if( roads_nesw[1] && roads_nesw[2] ) {
                rot = 1;
                diag = true;
                break;
            }
            // S/W, rotate 180 degrees
            if( roads_nesw[2] && roads_nesw[3] ) {
                rot = 2;
                diag = true;
                break;
            }
            // W/N, rotate 270 degrees
            if( roads_nesw[3] && roads_nesw[0] ) {
                rot = 3;
                diag = true;
                break;
            }
            // N/E, don't rotate
            if( roads_nesw[0] && roads_nesw[1] ) {
                diag = true;
                break;
            }
            // N/S, don't rotate
            break;
        case 1:
            // dead end
            // E, rotate  90 degrees
            if( roads_nesw[1] ) {
                rot = 1;
                break;
            }
            // S, rotate 180 degrees
            if( roads_nesw[2] ) {
                rot = 2;
                break;
            }
            // W, rotate 270 degrees
            if( roads_nesw[3] ) {
                rot = 3;
                break;
            }
            // N, don't rotate
            break;
    }

    // rotate the arrays left by rot steps
    nesw_array_rotate<bool>( sidewalks_neswx, 8, rot * 2 );
    nesw_array_rotate<bool>( roads_nesw,      4, rot );
    nesw_array_rotate<int> ( curvedir_nesw,   4, rot );

    // now we have only these shapes: '   |   '-   -'-   -|-

    if( diag ) {
        // diagonal roads get drawn differently from all other types
        // draw sidewalks if a S/SW/W neighbor has_sidewalk
        if( sidewalks_neswx[4] || sidewalks_neswx[5] || sidewalks_neswx[6] ) {
            for( int y = 0; y < SEEY * 2; y++ ) {
                for( int x = 0; x < SEEX * 2; x++ ) {
                    if( x > y - 4 && ( x < 4 || y > SEEY * 2 - 5 || y >= x ) ) {
                        m->ter_set( point( x, y ), t_sidewalk );
                    }
                }
            }
        }
        // draw diagonal road
        for( int y = 0; y < SEEY * 2; y++ ) {
            for( int x = 0; x < SEEX * 2; x++ ) {
                if( x > y && // definitely only draw in the upper right half of the map
                    ( ( x > 3 && y < ( SEEY * 2 - 4 ) ) || // middle, for both corners and diagonals
                      ( x < 4 && curvedir_nesw[0] < 0 ) || // diagonal heading northwest
                      ( y > ( SEEY * 2 - 5 ) && curvedir_nesw[1] > 0 ) ) ) { // diagonal heading southeast
                    if( ( x + rot / 2 ) % 4 && ( x - y == SEEX - 1 + ( 1 - ( rot / 2 ) ) ||
                                                 x - y == SEEX + ( 1 - ( rot / 2 ) ) ) ) {
                        m->ter_set( point( x, y ), t_pavement_y );
                    } else {
                        m->ter_set( point( x, y ), t_pavement );
                    }
                }
            }
        }
    } else { // normal road drawing
        bool cul_de_sac = false;
        // dead ends become cul de sacs, 1/3 of the time, if a neighbor has_sidewalk
        if( num_dirs == 1 && one_in( 3 ) && neighbor_sidewalks ) {
            cul_de_sac = true;
            fill_background( m, t_sidewalk );
        }

        // draw normal sidewalks
        for( int dir = 0; dir < 4; dir++ ) {
            if( roads_nesw[dir] ) {
                // sidewalk west of north road, etc
                if( sidewalks_neswx[( dir + 3 ) % 4     ] ||   // has_sidewalk west?
                    sidewalks_neswx[( dir + 3 ) % 4 + 4 ] ||   // has_sidewalk northwest?
                    sidewalks_neswx[   dir               ] ) { // has_sidewalk north?
                    int x1 = 0;
                    int y1 = 0;
                    int x2 = 3;
                    int y2 = SEEY - 1 + dead_end_extension;
                    coord_rotate_cw( x1, y1, dir );
                    coord_rotate_cw( x2, y2, dir );
                    square( m, t_sidewalk, point( x1, y1 ), point( x2, y2 ) );
                }
                // sidewalk east of north road, etc
                if( sidewalks_neswx[( dir + 1 ) % 4 ] ||   // has_sidewalk east?
                    sidewalks_neswx[   dir + 4       ] ||  // has_sidewalk northeast?
                    sidewalks_neswx[   dir           ] ) { // has_sidewalk north?
                    int x1 = SEEX * 2 - 5;
                    int y1 = 0;
                    int x2 = SEEX * 2 - 1;
                    int y2 = SEEY - 1 + dead_end_extension;
                    coord_rotate_cw( x1, y1, dir );
                    coord_rotate_cw( x2, y2, dir );
                    square( m, t_sidewalk, point( x1, y1 ), point( x2, y2 ) );
                }
            }
        }

        //draw dead end sidewalk
        if( dead_end_extension > 0 && sidewalks_neswx[ 2 ] ) {
            square( m, t_sidewalk, point( 0, SEEY + dead_end_extension ), point( SEEX * 2 - 1,
                    SEEY + dead_end_extension + 4 ) );
        }

        // draw 16-wide pavement from the middle to the edge in each road direction
        // also corner pieces to curve towards diagonal neighbors
        for( int dir = 0; dir < 4; dir++ ) {
            if( roads_nesw[dir] ) {
                int x1 = 4;
                int y1 = 0;
                int x2 = SEEX * 2 - 1 - 4;
                int y2 = SEEY - 1 + dead_end_extension;
                coord_rotate_cw( x1, y1, dir );
                coord_rotate_cw( x2, y2, dir );
                square( m, t_pavement, point( x1, y1 ), point( x2, y2 ) );
                if( curvedir_nesw[dir] != 0 ) {
                    for( int x = 1; x < 4; x++ ) {
                        for( int y = 0; y < x; y++ ) {
                            int ty = y, tx = ( curvedir_nesw[dir] == -1 ? x : SEEX * 2 - 1 - x );
                            coord_rotate_cw( tx, ty, dir );
                            m->ter_set( point( tx, ty ), t_pavement );
                        }
                    }
                }
            }
        }

        // draw yellow dots on the pavement
        for( int dir = 0; dir < 4; dir++ ) {
            if( roads_nesw[dir] ) {
                int max_y = SEEY;
                if( num_dirs == 4 || ( num_dirs == 3 && dir == 0 ) ) {
                    // dots don't extend into some intersections
                    max_y = 4;
                }
                for( int x = SEEX - 1; x <= SEEX; x++ ) {
                    for( int y = 0; y < max_y; y++ ) {
                        if( ( y + ( ( dir + rot ) / 2 % 2 ) ) % 4 ) {
                            int xn = x;
                            int yn = y;
                            coord_rotate_cw( xn, yn, dir );
                            m->ter_set( point( xn, yn ), t_pavement_y );
                        }
                    }
                }
            }
        }

        // draw round pavement for cul de sac late, to overdraw the yellow dots
        if( cul_de_sac ) {
            circle( m, t_pavement, double( SEEX ) - 0.5, double( SEEY ) - 0.5, 11.0 );
        }

        // overwrite part of intersection with rotary/plaza
        if( plaza_dir > -1 ) {
            if( plaza_dir == 8 ) {
                // plaza center
                fill_background( m, t_sidewalk );
                // TODO: something interesting here
            } else if( plaza_dir < 4 ) {
                // plaza side
                square( m, t_pavement, point( 0, SEEY - 10 ), point( SEEX * 2 - 1, SEEY - 1 ) );
                square( m, t_sidewalk, point( 0, SEEY - 2 ), point( SEEX * 2 - 1, SEEY * 2 - 1 ) );
                if( one_in( 3 ) ) {
                    line( m, t_tree_young, point( 1, SEEY ), point( SEEX * 2 - 2, SEEY ) );
                }
                if( one_in( 3 ) ) {
                    line_furn( m, f_bench, point( 2, SEEY + 2 ), point( 5, SEEY + 2 ) );
                    line_furn( m, f_bench, point( 10, SEEY + 2 ), point( 13, SEEY + 2 ) );
                    line_furn( m, f_bench, point( 18, SEEY + 2 ), point( 21, SEEY + 2 ) );
                }
            } else { // plaza corner
                circle( m, t_pavement, point( 0, SEEY * 2 - 1 ), 21 );
                circle( m, t_sidewalk, point( 0, SEEY * 2 - 1 ), 13 );
                if( one_in( 3 ) ) {
                    circle( m, t_tree_young, point( 0, SEEY * 2 - 1 ), 11 );
                    circle( m, t_sidewalk,   point( 0, SEEY * 2 - 1 ), 10 );
                }
                if( one_in( 3 ) ) {
                    circle( m, t_water_sh, point( 4, SEEY * 2 - 5 ), 3 );
                }
            }
        }
    }

    // spawn some vehicles
    if( plaza_dir != 8 ) {
        vspawn_id( neighbor_sidewalks ? "default_city" : "default_country" ).obj().apply(
            *m,
            num_dirs == 4 ? "road_four_way" :
            num_dirs == 3 ? "road_tee"      :
            num_dirs == 1 ? "road_end"      :
            diag          ? "road_curved"   :
            "road_straight"
        );
    }

    // spawn some monsters
    if( neighbor_sidewalks ) {
        m->place_spawns( GROUP_ZOMBIE, 2, point_zero, point( SEEX * 2 - 1, SEEX * 2 - 1 ),
                         dat.monster_density() );
        // 1 per 10 overmaps
        if( one_in( 10000 ) ) {
            m->add_spawn( mon_zombie_jackson, 1, { SEEX, SEEY, m->get_abs_sub().z } );
        }
    }

    // add some items
    bool plaza = ( plaza_dir > -1 );
    m->place_items( item_group_id( plaza ? "trash" : "road" ), 5, point_zero, point( SEEX * 2 - 1,
                    SEEX * 2 - 1 ), plaza,
                    dat.when() );

    // add a manhole if appropriate
    if( dat.terrain_type() == "road_nesw_manhole" ) {
        m->ter_set( point( rng( 6, SEEX * 2 - 6 ), rng( 6, SEEX * 2 - 6 ) ), t_manhole_cover );
    }

    // finally, unrotate the map
    m->rotate( rot );

}
///////////////////

void mapgen_subway( mapgendata &dat )
{
    map *const m = &dat.m;
    // start by filling the whole map with grass/dirt/etc
    dat.fill_groundcover();

    // which of the cardinal directions get subway?
    bool subway_nesw[4] = {};
    int num_dirs = terrain_type_to_nesw_array( dat.terrain_type(), subway_nesw );

    // N E S W
    for( int dir = 0; dir < 4; dir++ ) {
        if( dat.t_nesw[dir]->has_flag( oter_flags::subway_connection ) && !subway_nesw[dir] ) {
            num_dirs++;
            subway_nesw[dir] = true;
        }
    }

    // which way should our subway curve, based on neighbor subway?
    int curvedir_nesw[4] = {};
    // N E S W
    for( int dir = 0; dir < 4; dir++ ) {
        if( !subway_nesw[dir] ) {
            continue;
        }

        if( dat.t_nesw[dir]->get_type_id().str() != "subway" &&
            !dat.t_nesw[dir]->has_flag( oter_flags::subway_connection ) ) {
            continue;
        }
        // n_* contain details about the neighbor being considered
        bool n_subway_nesw[4] = {};
        // TODO: figure out how to call this function without creating a new oter_id object
        int n_num_dirs = terrain_type_to_nesw_array( dat.t_nesw[dir], n_subway_nesw );
        for( int dir = 0; dir < 4; dir++ ) {
            if( dat.t_nesw[dir]->has_flag( oter_flags::subway_connection ) && !n_subway_nesw[dir] ) {
                n_num_dirs++;
                n_subway_nesw[dir] = true;
            }
        }
        // if 2-way neighbor has a subway facing us
        if( n_num_dirs == 2 && n_subway_nesw[( dir + 2 ) % 4] ) {
            // curve towards the direction the neighbor turns
            // our subway curves counterclockwise
            if( n_subway_nesw[( dir - 1 + 4 ) % 4] ) {
                curvedir_nesw[dir]--;
            }
            // our subway curves clockwise
            if( n_subway_nesw[( dir + 1 ) % 4] ) {
                curvedir_nesw[dir]++;
            }
        }
    }

    // calculate how far to rotate the map so we can work with just one orientation
    // also keep track of diagonal subway
    int rot = 0;
    bool diag = false;
    // TODO: reduce amount of logical/conditional constructs here
    switch( num_dirs ) {
        case 4:
            // 4-way intersection
            break;
        case 3:
            // tee
            // E/S/W, rotate 180 degrees
            if( !subway_nesw[0] ) {
                rot = 2;
                break;
            }
            // N/S/W, rotate 270 degrees
            if( !subway_nesw[1] ) {
                rot = 3;
                break;
            }
            // N/E/S, rotate  90 degrees
            if( !subway_nesw[3] ) {
                rot = 1;
                break;
            }
            // N/E/W, don't rotate
            break;
        case 2:
            // straight or diagonal
            // E/W, rotate  90 degrees
            if( subway_nesw[1] && subway_nesw[3] ) {
                rot = 1;
                break;
            }
            // E/S, rotate  90 degrees
            if( subway_nesw[1] && subway_nesw[2] ) {
                rot = 1;
                diag = true;
                break;
            }
            // S/W, rotate 180 degrees
            if( subway_nesw[2] && subway_nesw[3] ) {
                rot = 2;
                diag = true;
                break;
            }
            // W/N, rotate 270 degrees
            if( subway_nesw[3] && subway_nesw[0] ) {
                rot = 3;
                diag = true;
                break;
            }
            // N/E, don't rotate
            if( subway_nesw[0] && subway_nesw[1] ) {
                diag = true;
                break;
            }
            break;                                                                  // N/S, don't rotate
        case 1:
            // dead end
            // E, rotate  90 degrees
            if( subway_nesw[1] ) {
                rot = 1;
                break;
            }
            // S, rotate 180 degrees
            if( subway_nesw[2] ) {
                rot = 2;
                break;
            }
            // W, rotate 270 degrees
            if( subway_nesw[3] ) {
                rot = 3;
                break;
            }
            // N, don't rotate
            break;
    }

    // rotate the arrays left by rot steps
    nesw_array_rotate<bool>( subway_nesw, 4, rot );
    nesw_array_rotate<int> ( curvedir_nesw,  4, rot );

    // now we have only these shapes: '   |   '-   -'-   -|-

    switch( num_dirs ) {
        case 4:
            // 4-way intersection
            mapf::formatted_set_simple( m, point_zero,
                                        "...^X^^^X^....^X^^^X^...\n"
                                        "..^DX^^DX^....^XD^^XD^..\n"
                                        ".^D^X^D^X^....^X^D^X^D^.\n"
                                        "^D^^XD^^X^^^^^^X^^DX^^D^\n"
                                        "XXXXDXXXXXXXXXXXXXXDXXXX\n"
                                        "^^^DX^^^X^^^^^^X^^^XD^^^\n"
                                        "^^D^X^^^X^^^^^^X^^^X^D^^\n"
                                        "^D^^X^^^X^^^^^^X^^^X^^D^\n"
                                        "XXXXXXXXXXXXXXXXXXXXXXXX\n"
                                        "^^^^X^^^X^^^^^^X^^^X^^^^\n"
                                        "...^X^^^X^....^X^^^X^...\n"
                                        "...^X^^^X^....^X^^^X^...\n"
                                        "...^X^^^X^....^X^^^X^...\n"
                                        "...^X^^^X^....^X^^^X^...\n"
                                        "^^^^X^^^X^^^^^^X^^^X^^^^\n"
                                        "XXXXXXXXXXXXXXXXXXXXXXXX\n"
                                        "^D^^X^^^X^^^^^^X^^^X^^D^\n"
                                        "^^D^X^^^X^^^^^^X^^^X^D^^\n"
                                        "^^^DX^^^X^^^^^^X^^^XD^^^\n"
                                        "XXXXDXXXXXXXXXXXXXXDXXXX\n"
                                        "^D^^XD^^X^^^^^^X^^DX^^D^\n"
                                        ".^D^X^D^X^....^X^D^X^D^.\n"
                                        "..^DX^^DX^....^XD^^XD^..\n"
                                        "...^X^^^X^....^X^^^X^...",
                                        mapf::ter_bind( ". # ^ D X",
                                                t_rock_floor,
                                                t_rock,
                                                t_railroad_rubble,
                                                t_railroad_track_d,
                                                t_railroad_track ),
                                        mapf::furn_bind( ". # ^ D X",
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null ) );
            break;
        case 3:
            // tee
            mapf::formatted_set_simple( m, point_zero,
                                        "..^D^^^D^....^D^^^D^....\n"
                                        ".^D^^^D^....^D^^^D^.....\n"
                                        "^D^^^D^....^D^^^D^......\n"
                                        "D^^^D^^^^^^D^^^D^^^^^^^^\n"
                                        "XXXDXXXXXXDXXXDXXXXXXXXX\n"
                                        "^^D^^^^^^D^^^D^^^^^^^^^^\n"
                                        "^D^^^^^^D^^^D^^^^^^^^^^^\n"
                                        "D^^^^^^D^^^D^^^^^^^^^^^^\n"
                                        "XXXXXXDXXXDXXXXXXXXXXXXX\n"
                                        "^^^^^D^^^D^^^^^^^^^^^^^^\n"
                                        "...^D^^^D^..............\n"
                                        "..^D^^^D^...............\n"
                                        ".^D^^^D^................\n"
                                        "^D^^^D^.................\n"
                                        "D^^^D^^^^^|^^|^^|^^|^^|^\n"
                                        "XXXDXXXXXXxXXxXXxXXxXXxX\n"
                                        "^^D^^^^^^^|^^|^^|^^|^^|^\n"
                                        "^D^^^^^^^^|^^|^^|^^|^^|^\n"
                                        "D^^^^^^^^^|^^|^^|^^|^^|^\n"
                                        "XXXXXXXXXXxXXxXXxXXxXXxX\n"
                                        "^^^^^^^^^^|^^|^^|^^|^^|^\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................",
                                        mapf::ter_bind( ". # ^ | X x D",
                                                t_rock_floor,
                                                t_rock,
                                                t_railroad_rubble,
                                                t_railroad_tie,
                                                t_railroad_track,
                                                t_railroad_track_on_tie,
                                                t_railroad_track_d ),
                                        mapf::furn_bind( ". # ^ | X x D",
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null ) );
            break;
        case 2:
            // straight or diagonal
            if( diag ) { // diagonal subway get drawn differently from all other types
                mapf::formatted_set_simple( m, point_zero,
                                            "...^D^^^D^....^D^^^D^...\n"
                                            "....^D^^^D^....^D^^^D^..\n"
                                            ".....^D^^^D^....^D^^^D^.\n"
                                            "......^D^^^D^....^D^^^D^\n"
                                            ".......^D^^^D^....^D^^^D\n"
                                            "#.......^D^^^D^....^D^^^\n"
                                            "##.......^D^^^D^....^D^^\n"
                                            "###.......^D^^^D^....^D^\n"
                                            "####.......^D^^^D^....^D\n"
                                            "#####.......^D^^^D^....^\n"
                                            "######.......^D^^^D^....\n"
                                            "#######.......^D^^^D^...\n"
                                            "########.......^D^^^D^..\n"
                                            "#########.......^D^^^D^.\n"
                                            "##########.......^D^^^D^\n"
                                            "###########.......^D^^^D\n"
                                            "############.......^D^^^\n"
                                            "#############.......^D^^\n"
                                            "##############.......^D^\n"
                                            "###############.......^D\n"
                                            "################.......^\n"
                                            "#################.......\n"
                                            "##################......\n"
                                            "###################.....",
                                            mapf::ter_bind( ". # ^ D",
                                                    t_rock_floor,
                                                    t_rock,
                                                    t_railroad_rubble,
                                                    t_railroad_track_d ),
                                            mapf::furn_bind( ". # ^ D",
                                                    f_null,
                                                    f_null,
                                                    f_null,
                                                    f_null ) );
            } else { // normal subway drawing
                mapf::formatted_set_simple( m, point_zero,
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...-x---x-....-x---x-...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...-x---x-....-x---x-...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...-x---x-....-x---x-...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...-x---x-....-x---x-...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...-x---x-....-x---x-...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...-x---x-....-x---x-...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...-x---x-....-x---x-...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...^X^^^X^....^X^^^X^...\n"
                                            "...-x---x-....-x---x-...\n"
                                            "...^X^^^X^....^X^^^X^...",
                                            mapf::ter_bind( ". # ^ - X x",
                                                    t_rock_floor,
                                                    t_rock,
                                                    t_railroad_rubble,
                                                    t_railroad_tie,
                                                    t_railroad_track,
                                                    t_railroad_track_on_tie ),
                                            mapf::furn_bind( ". # ^ - X x",
                                                    f_null,
                                                    f_null,
                                                    f_null,
                                                    f_null,
                                                    f_null,
                                                    f_null ) );
            }
            break;
        case 1:
            // dead end
            mapf::formatted_set_simple( m, point_zero,
                                        "...^X^^^X^....^D^^^D^...\n"
                                        "...-x---x-...^DX^^DX^...\n"
                                        "...^X^^^X^..^D^X^D^X^...\n"
                                        "...^X^^^X^.^D^^XD^^X^...\n"
                                        "...^X^^^X^^D^^^D^^^X^...\n"
                                        "...^X^^^X^D^^^DX^^^X^...\n"
                                        "...^X^^^XD^^^D^X^^^X^...\n"
                                        "...^X^^^D^^^D^-x---x-...\n"
                                        "...^X^^DX^^D^.^X^^^X^...\n"
                                        "...^X^D^X^D^..^X^^^X^...\n"
                                        "...^XD^^XD^...-x---x-...\n"
                                        "...^D^^^D^....^X^^^X^...\n"
                                        "...^X^^^X^....^X^^^X^...\n"
                                        "...-x---x-....-x---x-...\n"
                                        "...^X^^^X^....^X^^^X^...\n"
                                        "...^X^^^X^....^X^^^X^...\n"
                                        "...-x---x-....-x---x-...\n"
                                        "...^X^^^X^....^X^^^X^...\n"
                                        "...^X^^^X^....^X^^^X^...\n"
                                        "...^S^^^S^....^S^^^S^...\n"
                                        "...^^^^^^^....^^^^^^^...\n"
                                        "#......................#\n"
                                        "##....................##\n"
                                        "########################",
                                        mapf::ter_bind( ". # S ^ - D X x",
                                                t_rock_floor,
                                                t_rock,
                                                t_buffer_stop,
                                                t_railroad_rubble,
                                                t_railroad_tie,
                                                t_railroad_track_d,
                                                t_railroad_track,
                                                t_railroad_track_on_tie ),
                                        mapf::furn_bind( ". # S ^ - D X x",
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null ) );
            VehicleSpawn::apply( vspawn_id( "default_subway_deadend" ), *m, "subway" );
            break;
    }

    // finally, unrotate the map
    m->rotate( rot );
}

void mapgen_sewer( mapgendata &dat )
{
    map *const m = &dat.m;

    bool sewer_nesw[4] = {};
    int num_dirs = terrain_type_to_nesw_array( dat.terrain_type(), sewer_nesw );

    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            bool fill = true;
            if( j >= SEEY - 2 && j <= SEEY + 1 ) {
                if( i <= SEEX - 2 ) {
                    if( sewer_nesw[3] ) {
                        fill = false;
                    }
                } else if( i >= SEEX + 1 ) {
                    if( sewer_nesw[1] ) {
                        fill = false;
                    }
                } else {
                    // Central area, always empty
                    fill = false;
                }
            } else if( i >= SEEX - 2 && i <= SEEX + 1 ) {
                if( j <= SEEY - 2 ) {
                    if( sewer_nesw[0] ) {
                        fill = false;
                    }
                } else {
                    if( sewer_nesw[2] ) {
                        fill = false;
                    }
                }
            }

            m->ter_set( point( i, j ), fill ? t_rock : t_sewage );
        }
    }
    m->place_items( item_group_id( "sewer" ), 4 + ( num_dirs * 6 ), point_zero,
                    point( SEEX * 2 - 1, SEEY * 2 - 1 ), true, dat.when() );
}

void mapgen_highway( mapgendata &dat )
{
    map *const m = &dat.m;
    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( i < 3 || i >= SEEX * 2 - 3 ) {
                m->ter_set( point( i, j ), dat.groundcover() );
            } else if( i == 3 || i == SEEX * 2 - 4 ) {
                m->ter_set( point( i, j ), t_railing );
            } else {
                if( ( i == SEEX - 1 || i == SEEX ) && j % 4 != 0 ) {
                    m->ter_set( point( i, j ), t_pavement_y );
                } else {
                    m->ter_set( point( i, j ), t_pavement );
                }
            }
        }
    }

    // spawn regular road out of fuel vehicles
    VehicleSpawn::apply( vspawn_id( "default_highway" ), *m, "highway" );

    if( dat.terrain_type() == "hiway_ew" ) {
        m->rotate( 1 );
    }
    m->place_items( item_group_id( "road" ), 8, point_zero, point( SEEX * 2 - 1, SEEX * 2 - 1 ), false,
                    dat.when() );
}

// mapgen_railroad
// TODO: Refactor and combine with other similiar functions (e.g. road).
void mapgen_railroad( mapgendata &dat )
{
    map *const m = &dat.m;
    // start by filling the whole map with grass/dirt/etc
    dat.fill_groundcover();
    // which of the cardinal directions get railroads?
    bool railroads_nesw[4] = {};
    int num_dirs = terrain_type_to_nesw_array( dat.terrain_type(), railroads_nesw );
    // which way should our railroads curve, based on neighbor railroads?
    int curvedir_nesw[4] = {};
    for( int dir = 0; dir < 4; dir++ ) { // N E S W
        if( !railroads_nesw[dir] || dat.t_nesw[dir]->get_type_id().str() != "railroad" ) {
            continue;
        }
        // n_* contain details about the neighbor being considered
        bool n_railroads_nesw[4] = {};
        // TODO: figure out how to call this function without creating a new oter_id object
        int n_num_dirs = terrain_type_to_nesw_array( dat.t_nesw[dir], n_railroads_nesw );
        // if 2-way neighbor has a railroad facing us
        if( n_num_dirs == 2 && n_railroads_nesw[( dir + 2 ) % 4] ) {
            // curve towards the direction the neighbor turns
            if( n_railroads_nesw[( dir - 1 + 4 ) % 4] ) {
                curvedir_nesw[dir]--;    // our railroad curves counterclockwise
            }
            if( n_railroads_nesw[( dir + 1 ) % 4] ) {
                curvedir_nesw[dir]++;    // our railroad curves clockwise
            }
        }
    }
    // calculate how far to rotate the map so we can work with just one orientation
    // also keep track of diagonal railroads
    int rot = 0;
    bool diag = false;
    // TODO: reduce amount of logical/conditional constructs here
    switch( num_dirs ) {
        case 4:
            // 4-way intersection
            break;
        case 3:
            // tee
            if( !railroads_nesw[0] ) {
                rot = 2;    // E/S/W, rotate 180 degrees
                break;
            }
            if( !railroads_nesw[1] ) {
                rot = 3;    // N/S/W, rotate 270 degrees
                break;
            }
            if( !railroads_nesw[3] ) {
                rot = 1;    // N/E/S, rotate  90 degrees
                break;
            }
            break;                                       // N/E/W, don't rotate
        case 2:
            // straight or diagonal
            if( railroads_nesw[1] && railroads_nesw[3] ) {
                rot = 1;    // E/W, rotate  90 degrees
                break;
            }
            if( railroads_nesw[1] && railroads_nesw[2] ) {
                rot = 1;    // E/S, rotate  90 degrees
                diag = true;
                break;
            }
            if( railroads_nesw[2] && railroads_nesw[3] ) {
                rot = 2;    // S/W, rotate 180 degrees
                diag = true;
                break;
            }
            if( railroads_nesw[3] && railroads_nesw[0] ) {
                rot = 3;    // W/N, rotate 270 degrees
                diag = true;
                break;
            }
            // N/E, don't rotate
            if( railroads_nesw[0] && railroads_nesw[1] ) {
                diag = true;
                break;
            }
            // N/S, don't rotate
            break;
        case 1:
            // dead end
            // E, rotate  90 degrees
            if( railroads_nesw[1] ) {
                rot = 1;
                break;
            }
            // S, rotate 180 degrees
            if( railroads_nesw[2] ) {
                rot = 2;
                break;
            }
            // W, rotate 270 degrees
            if( railroads_nesw[3] ) {

                rot = 3;
                break;
            }
            // N, don't rotate
            break;
    }
    // rotate the arrays left by rot steps
    nesw_array_rotate<bool>( railroads_nesw, 4, rot );
    nesw_array_rotate<int> ( curvedir_nesw,  4, rot );
    // now we have only these shapes: '   |   '-   -'-   -|-
    switch( num_dirs ) {
        case 4:
            // 4-way intersection
            mapf::formatted_set_simple( m, point_zero,
                                        ".DD^^DD^........^DD^^DD.\n"
                                        "DD^^DD^..........^DD^^DD\n"
                                        "D^^DD^............^DD^^D\n"
                                        "^^DD^..............^DD^^\n"
                                        "^DD^................^DD^\n"
                                        "DD^..................^DD\n"
                                        "D^....................^D\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "D^....................^D\n"
                                        "DD^..................^DD\n"
                                        "^DD^................^DD^\n"
                                        "^^DD^..............^DD^^\n"
                                        "D^^DD^............^DD^^D\n"
                                        "DD^^DD^..........^DD^^DD\n"
                                        ".DD^^DD^........^DD^^DD.",
                                        mapf::ter_bind( ". ^ D",
                                                t_dirt,
                                                t_railroad_rubble,
                                                t_railroad_track_d ),
                                        mapf::furn_bind( ". ^ D",
                                                f_null,
                                                f_null,
                                                f_null ) );
            break;
        case 3:
            // tee
            mapf::formatted_set_simple( m, point_zero,
                                        ".DD^^DD^........^DD^^DD.\n"
                                        "DD^^DD^..........^DD^^DD\n"
                                        "D^^DD^............^DD^^D\n"
                                        "^^DD^..............^DD^^\n"
                                        "^DD^................^DD^\n"
                                        "DD^..................^DD\n"
                                        "D^....................^D\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "^|^^|^^|^^|^^|^^|^^|^^|^\n"
                                        "XxXXxXXxXXxXXxXXxXXxXXxX\n"
                                        "^|^^|^^|^^|^^|^^|^^|^^|^\n"
                                        "^|^^|^^|^^|^^|^^|^^|^^|^\n"
                                        "^|^^|^^|^^|^^|^^|^^|^^|^\n"
                                        "XxXXxXXxXXxXXxXXxXXxXXxX\n"
                                        "^|^^|^^|^^|^^|^^|^^|^^|^\n"
                                        "........................",
                                        mapf::ter_bind( ". ^ | X x / D",
                                                t_dirt,
                                                t_railroad_rubble,
                                                t_railroad_tie,
                                                t_railroad_track,
                                                t_railroad_track_on_tie,
                                                t_railroad_tie_d,
                                                t_railroad_track_d ),
                                        mapf::furn_bind( ". ^ | X x / D",
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null ) );
            break;
        case 2:
            // straight or diagonal
            if( diag ) {
                // diagonal railroads get drawn differently from all other types
                mapf::formatted_set_simple( m, point_zero,
                                            ".^DD^^DD^.......^DD^^DD^\n"
                                            "..^DD^^DD^.......^DD^^DD\n"
                                            "...^DD^^DD^.......^DD^^D\n"
                                            "....^DD^^DD^.......^DD^^\n"
                                            ".....^DD^^DD^.......^DD^\n"
                                            "......^DD^^DD^.......^DD\n"
                                            ".......^DD^^DD^.......^D\n"
                                            "........^DD^^DD^.......^\n"
                                            ".........^DD^^DD^.......\n"
                                            "..........^DD^^DD^......\n"
                                            "...........^DD^^DD^.....\n"
                                            "............^DD^^DD^....\n"
                                            ".............^DD^^DD^...\n"
                                            "..............^DD^^DD^..\n"
                                            "...............^DD^^DD^.\n"
                                            "................^DD^^DD^\n"
                                            ".................^DD^^DD\n"
                                            "..................^DD^^D\n"
                                            "...................^DD^^\n"
                                            "....................^DD^\n"
                                            ".....................^DD\n"
                                            "......................^D\n"
                                            ".......................^\n"
                                            "........................",
                                            mapf::ter_bind( ". ^ D",
                                                    t_dirt,
                                                    t_railroad_rubble,
                                                    t_railroad_track_d ),
                                            mapf::furn_bind( ". ^ D",
                                                    f_null,
                                                    f_null,
                                                    f_null ) );
            } else { // normal railroads drawing
                mapf::formatted_set_simple( m, point_zero,
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".-x---x-........-x---x-.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".-x---x-........-x---x-.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".-x---x-........-x---x-.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".-x---x-........-x---x-.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".-x---x-........-x---x-.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".-x---x-........-x---x-.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".-x---x-........-x---x-.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".^X^^^X^........^X^^^X^.\n"
                                            ".-x---x-........-x---x-.\n"
                                            ".^X^^^X^........^X^^^X^.",
                                            mapf::ter_bind( ". ^ - X x",
                                                    t_dirt,
                                                    t_railroad_rubble,
                                                    t_railroad_tie,
                                                    t_railroad_track,
                                                    t_railroad_track_on_tie ),
                                            mapf::furn_bind( ". ^ - X x",
                                                    f_null,
                                                    f_null,
                                                    f_null,
                                                    f_null,
                                                    f_null ) );
            }
            break;
        case 1:
            // dead end
            mapf::formatted_set_simple( m, point_zero,
                                        ".^X^^^X^........^X^^^X^.\n"
                                        ".-x---x-........-x---x-.\n"
                                        ".^X^^^X^........^X^^^X^.\n"
                                        ".^X^^^X^........^X^^^X^.\n"
                                        ".-x---x-........-x---x-.\n"
                                        ".^X^^^X^........^X^^^X^.\n"
                                        ".^X^^^X^........^X^^^X^.\n"
                                        ".-x---x-........-x---x-.\n"
                                        ".^X^^^X^........^X^^^X^.\n"
                                        ".^X^^^X^........^X^^^X^.\n"
                                        ".-x---x-........-x---x-.\n"
                                        ".^X^^^X^........^X^^^X^.\n"
                                        ".^S^^^S^........^S^^^S^.\n"
                                        ".^^^^^^^........^^^^^^^.\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................\n"
                                        "........................",
                                        mapf::ter_bind( ". ^ S - X x",
                                                t_dirt,
                                                t_railroad_rubble,
                                                t_buffer_stop,
                                                t_railroad_tie,
                                                t_railroad_track,
                                                t_railroad_track_on_tie ),
                                        mapf::furn_bind( ". ^ S - X x",
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null,
                                                f_null ) );
            break;
    }
    // finally, unrotate the map
    m->rotate( rot );
}
///////////////////
void mapgen_railroad_bridge( mapgendata &dat )
{
    map *const m = &dat.m;
    mapf::formatted_set_simple( m, point_zero,
                                "r^X^^^X^________^X^^^X^r\n"
                                "r-x---x-________-x---x-r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r-x---x-________-x---x-r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r-x---x-________-x---x-r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r-x---x-________-x---x-r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r-x---x-________-x---x-r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r-x---x-________-x---x-r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r-x---x-________-x---x-r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r^X^^^X^________^X^^^X^r\n"
                                "r-x---x-________-x---x-r\n"
                                "r^X^^^X^________^X^^^X^r",
                                mapf::ter_bind( ". _ r ^ - X x", t_dirt, t_concrete, t_railing, t_railroad_rubble, t_railroad_tie,
                                        t_railroad_track, t_railroad_track_on_tie ),
                                mapf::furn_bind( ". _ r ^ - X x", f_null, f_null, f_null, f_null, f_null, f_null, f_null )
                              );
    m->rotate( static_cast<int>( dat.terrain_type()->get_dir() ) );
}

void mapgen_river_center( mapgendata &dat )
{
    fill_background( &dat.m, t_water_moving_dp );
}

void mapgen_river_curved_not( mapgendata &dat )
{
    map *const m = &dat.m;
    fill_background( m, t_water_moving_dp );
    // this is not_ne, so deep on all sides except ne corner, which is shallow
    // shallow is 20,0, 23,4
    int north_edge = rng( 16, 18 );
    int east_edge = rng( 4, 8 );

    for( int x = north_edge; x < SEEX * 2; x++ ) {
        for( int y = 0; y < east_edge; y++ ) {
            int circle_edge = ( ( SEEX * 2 - x ) * ( SEEX * 2 - x ) ) + ( y * y );
            if( circle_edge <= 8 ) {
                m->ter_set( point( x, y ), grass_or_dirt() );
            } else if( circle_edge == 9 && one_in( 20 ) ) {
                m->ter_set( point( x, y ), clay_or_sand() );
            } else if( circle_edge <= 36 ) {
                m->ter_set( point( x, y ), t_water_moving_sh );
            }
        }
    }

    if( dat.terrain_type() == "river_c_not_se" ) {
        m->rotate( 1 );
    }
    if( dat.terrain_type() == "river_c_not_sw" ) {
        m->rotate( 2 );
    }
    if( dat.terrain_type() == "river_c_not_nw" ) {
        m->rotate( 3 );
    }
}

void mapgen_river_straight( mapgendata &dat )
{
    map *const m = &dat.m;
    fill_background( m, t_water_moving_dp );

    for( int x = 0; x < SEEX * 2; x++ ) {
        int ground_edge = rng( 1, 3 );
        int shallow_edge = rng( 4, 6 );
        line( m, grass_or_dirt(), point( x, 0 ), point( x, ground_edge ) );
        if( one_in( 20 ) ) {
            m->ter_set( point( x, ++ground_edge ), clay_or_sand() );
        }
        line( m, t_water_moving_sh, point( x, ++ground_edge ), point( x, shallow_edge ) );
    }

    if( dat.terrain_type() == "river_east" ) {
        m->rotate( 1 );
    }
    if( dat.terrain_type() == "river_south" ) {
        m->rotate( 2 );
    }
    if( dat.terrain_type() == "river_west" ) {
        m->rotate( 3 );
    }
}

void mapgen_river_curved( mapgendata &dat )
{
    map *const m = &dat.m;
    fill_background( m, t_water_moving_dp );
    // NE corner deep, other corners are shallow.  do 2 passes: one x, one y
    for( int x = 0; x < SEEX * 2; x++ ) {
        int ground_edge = rng( 1, 3 );
        int shallow_edge = rng( 4, 6 );
        line( m, grass_or_dirt(), point( x, 0 ), point( x, ground_edge ) );
        if( one_in( 20 ) ) {
            m->ter_set( point( x, ++ground_edge ), clay_or_sand() );
        }
        line( m, t_water_moving_sh, point( x, ++ground_edge ), point( x, shallow_edge ) );
    }
    for( int y = 0; y < SEEY * 2; y++ ) {
        int ground_edge = rng( 19, 21 );
        int shallow_edge = rng( 16, 18 );
        line( m, grass_or_dirt(), point( ground_edge, y ), point( SEEX * 2 - 1, y ) );
        if( one_in( 20 ) ) {
            m->ter_set( point( --ground_edge, y ), clay_or_sand() );
        }
        line( m, t_water_moving_sh, point( shallow_edge, y ), point( --ground_edge, y ) );
    }

    if( dat.terrain_type() == "river_se" ) {
        m->rotate( 1 );
    }
    if( dat.terrain_type() == "river_sw" ) {
        m->rotate( 2 );
    }
    if( dat.terrain_type() == "river_nw" ) {
        m->rotate( 3 );
    }
}

void mapgen_river_shore( mapgendata &dat )
{
    map *const m = &dat.m;
    fill_background( m, t_water_moving_dp );

    bool ground_neswx[8] = {};
    // N E S W NE SE SW NW
    for( int dir = 0; dir < 8; dir++ ) {
        ground_neswx[dir] = !( dat.t_nesw[dir]->is_river() ||
                               dat.t_nesw[dir]->is_lake() ||
                               dat.t_nesw[dir]->is_lake_shore() );
    }

    // Draw shores on sides
    for( int dir = 0; dir < 4; dir++ ) {
        if( ground_neswx[dir] ) {
            m->rotate( 4 - dir );
            for( int x = 0; x < SEEX * 2; x++ ) {
                int ground_edge = rng( 1, 3 );
                int shallow_edge = rng( 4, 6 );
                line( m, grass_or_dirt(), point( x, 0 ), point( x, ground_edge ) );
                if( one_in( 20 ) ) {
                    m->ter_set( point( x, ++ground_edge ), clay_or_sand() );
                }
                line( m, t_water_moving_sh, point( x, ++ground_edge ), point( x, shallow_edge ) );
            }
            m->rotate( dir );
        }
    }

    // Bite corner, unless there's a shore already
    for( int dir = 0; dir < 4; dir++ ) {
        if( ground_neswx[dir + 4] && !ground_neswx[dir] && !ground_neswx[( dir + 1 ) % 4] ) {
            m->rotate( 4 - dir );
            int north_edge = rng( 16, 18 );
            int east_edge = rng( 4, 8 );
            for( int x = north_edge; x < SEEX * 2; x++ ) {
                for( int y = 0; y < east_edge; y++ ) {
                    int circle_edge = ( ( SEEX * 2 - x ) * ( SEEX * 2 - x ) ) + ( y * y );
                    if( circle_edge <= 8 ) {
                        m->ter_set( point( x, y ), grass_or_dirt() );
                    } else if( circle_edge == 9 && one_in( 20 ) ) {
                        m->ter_set( point( x, y ), clay_or_sand() );
                    } else if( circle_edge <= 36 ) {
                        m->ter_set( point( x, y ), t_water_moving_sh );
                    }
                }
            }
            m->rotate( dir );
        }
    }
}

void mapgen_parking_lot( mapgendata &dat )
{
    map *const m = &dat.m;
    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( ( j == 5 || j == 9 || j == 13 || j == 17 || j == 21 ) &&
                ( ( i > 1 && i < 8 ) || ( i > 14 && i < SEEX * 2 - 2 ) ) ) {
                m->ter_set( point( i, j ), t_pavement_y );
            } else if( ( j < 2 && i > 7 && i < 17 ) || ( j >= 2 && j < SEEY * 2 - 2 && i > 1 &&
                       i < SEEX * 2 - 2 ) ) {
                m->ter_set( point( i, j ), t_pavement );
            } else {
                m->ter_set( point( i, j ), dat.groundcover() );
            }
        }
    }

    VehicleSpawn::apply( vspawn_id( "default_parkinglot" ), *m, "parkinglot" );

    m->place_items( item_group_id( "road" ), 8, point_zero, point( SEEX * 2 - 1, SEEY * 2 - 1 ), false,
                    dat.when() );
    for( int i = 1; i < 4; i++ ) {
        const std::string &id = dat.t_nesw[i].id().str();
        if( id.size() > 5 && id.starts_with( "road_" ) ) {
            m->rotate( i );
        }
    }
}

void mapgen_cavern( mapgendata &dat )
{
    map *const m = &dat.m;

    // FIXME: don't look at me like that, this was messed up before I touched it :P - AD
    for( int i = 0; i < 4; i++ ) {
        dat.set_dir( i,
                     ( dat.t_nesw[i] == "cavern" || dat.t_nesw[i] == "subway_ns" ||
                       dat.t_nesw[i] == "subway_ew" ? 0 : 3 )
                   );
    }
    dat.e_fac = SEEX * 2 - 1 - dat.e_fac;
    dat.s_fac = SEEY * 2 - 1 - dat.s_fac;

    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( ( j < dat.n_fac || j > dat.s_fac || i < dat.w_fac || i > dat.e_fac ) &&
                ( !one_in( 3 ) || j == 0 || j == SEEY * 2 - 1 || i == 0 || i == SEEX * 2 - 1 ) ) {
                m->ter_set( point( i, j ), t_rock );
            } else {
                m->ter_set( point( i, j ), t_rock_floor );
            }
        }
    }

    // Number of pillars
    int rn = rng( 0, 2 ) * rng( 0, 3 ) + rng( 0, 1 );
    for( int n = 0; n < rn; n++ ) {
        point p{ rng( 5, SEEX * 2 - 6 ), rng( 5, SEEY * 2 - 6 ) };
        for( int i = p.x - 1; i <= p.x + 1; i++ ) {
            for( int j = p.y - 1; j <= p.y + 1; j++ ) {
                m->ter_set( point( i, j ), t_rock );
            }
        }
    }

    if( connects_to( dat.north(), 2 ) ) {
        for( int i = SEEX - 2; i <= SEEX + 3; i++ ) {
            for( int j = 0; j <= SEEY; j++ ) {
                m->ter_set( point( i, j ), t_rock_floor );
            }
        }
    }
    if( connects_to( dat.east(), 3 ) ) {
        for( int i = SEEX; i <= SEEX * 2 - 1; i++ ) {
            for( int j = SEEY - 2; j <= SEEY + 3; j++ ) {
                m->ter_set( point( i, j ), t_rock_floor );
            }
        }
    }
    if( connects_to( dat.south(), 0 ) ) {
        for( int i = SEEX - 2; i <= SEEX + 3; i++ ) {
            for( int j = SEEY; j <= SEEY * 2 - 1; j++ ) {
                m->ter_set( point( i, j ), t_rock_floor );
            }
        }
    }
    if( connects_to( dat.west(), 1 ) ) {
        for( int i = 0; i <= SEEX; i++ ) {
            for( int j = SEEY - 2; j <= SEEY + 3; j++ ) {
                m->ter_set( point( i, j ), t_rock_floor );
            }
        }
    }
    m->place_items( item_group_id( "cavern" ), 60, point_zero, point( SEEX * 2 - 1, SEEY * 2 - 1 ),
                    false, dat.when() );
    if( one_in( 6 ) ) { // Miner remains
        point p2;
        do {
            p2.x = rng( 0, SEEX * 2 - 1 );
            p2.y = rng( 0, SEEY * 2 - 1 );
        } while( m->impassable( p2 ) );
        if( !one_in( 3 ) ) {
            m->spawn_item( p2, itype_jackhammer );
        }
        if( one_in( 3 ) ) {
            m->spawn_item( p2, itype_mask_dust );
        }
        if( one_in( 2 ) ) {
            m->spawn_item( p2, itype_hat_hard );
        }
        while( !one_in( 3 ) ) {
            for( int i = 0; i < 3; ++i ) {
                m->put_items_from_loc( item_group_id( "cannedfood" ), tripoint( p2, m->get_abs_sub().z ),
                                       dat.when() );
            }
        }
    }

}

void mapgen_rock_partial( mapgendata &dat )
{
    map *const m = &dat.m;
    fill_background( m, t_rock );
    for( int i = 0; i < 4; i++ ) {
        if( dat.t_nesw[i] == "cavern" || dat.t_nesw[i] == "slimepit" ||
            dat.t_nesw[i] == "slimepit_down" ) {
            dat.dir( i ) = 6;
        } else {
            dat.dir( i ) = 0;
        }
    }

    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( rng( 0, dat.n_fac ) > j || rng( 0, dat.s_fac ) > SEEY * 2 - 1 - j ||
                rng( 0, dat.w_fac ) > i || rng( 0, dat.e_fac ) > SEEX * 2 - 1 - i ) {
                m->ter_set( point( i, j ), t_rock_floor );
            }
        }
    }
}

void mapgen_rock( mapgendata &dat )
{
    fill_background( &dat.m, t_rock );
}

void mapgen_open_air( mapgendata &dat )
{
    fill_background( &dat.m, t_open_air );
}

void mapgen_rift( mapgendata &dat )
{
    map *const m = &dat.m;

    if( dat.north() != "rift" && dat.north() != "hellmouth" ) {
        if( connects_to( dat.north(), 2 ) ) {
            dat.n_fac = rng( -6, -2 );
        } else {
            dat.n_fac = rng( 2, 6 );
        }
    }
    if( dat.east() != "rift" && dat.east() != "hellmouth" ) {
        if( connects_to( dat.east(), 3 ) ) {
            dat.e_fac = rng( -6, -2 );
        } else {
            dat.e_fac = rng( 2, 6 );
        }
    }
    if( dat.south() != "rift" && dat.south() != "hellmouth" ) {
        if( connects_to( dat.south(), 0 ) ) {
            dat.s_fac = rng( -6, -2 );
        } else {
            dat.s_fac = rng( 2, 6 );
        }
    }
    if( dat.west() != "rift" && dat.west() != "hellmouth" ) {
        if( connects_to( dat.west(), 1 ) ) {
            dat.w_fac = rng( -6, -2 );
        } else {
            dat.w_fac = rng( 2, 6 );
        }
    }
    // Negative *_fac values indicate rock floor connection, otherwise solid rock
    // Of course, if we connect to a rift, *_fac = 0, and thus lava extends all the
    //  way.
    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( ( dat.n_fac < 0 && j < dat.n_fac * -1 ) || ( dat.s_fac < 0 && j >= SEEY * 2 - dat.s_fac ) ||
                ( dat.w_fac < 0 && i < dat.w_fac * -1 ) || ( dat.e_fac < 0 && i >= SEEX * 2 - dat.e_fac ) ) {
                m->ter_set( point( i, j ), t_rock_floor );
            } else if( j < dat.n_fac || j >= SEEY * 2 - dat.s_fac ||
                       i < dat.w_fac || i >= SEEX * 2 - dat.e_fac ) {
                m->ter_set( point( i, j ), t_rock );
            } else {
                m->ter_set( point( i, j ), t_lava );
            }
        }
    }

}

void mapgen_hellmouth( mapgendata &dat )
{
    map *const m = &dat.m;
    // what is this, doom?
    // .. seriously, though...
    for( int i = 0; i < 4; i++ ) {
        if( dat.t_nesw[i] != "rift" && dat.t_nesw[i] != "hellmouth" ) {
            dat.dir( i ) = 6;
        }
    }

    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( j < dat.n_fac || j >= SEEY * 2 - dat.s_fac || i < dat.w_fac || i >= SEEX * 2 - dat.e_fac ||
                ( i >= 6 && i < SEEX * 2 - 6 && j >= 6 && j < SEEY * 2 - 6 ) ) {
                m->ter_set( point( i, j ), t_rock_floor );
            } else {
                m->ter_set( point( i, j ), t_lava );
            }
            if( i >= SEEX - 1 && i <= SEEX && j >= SEEY - 1 && j <= SEEY ) {
                m->ter_set( point( i, j ), t_slope_down );
            }
        }
    }
    switch( rng( 0, 4 ) ) { // Randomly chosen "altar" design
        case 0:
            for( int i = 7; i <= 16; i += 3 ) {
                m->ter_set( point( i, 6 ), t_rock );
                m->ter_set( point( i, 17 ), t_rock );
                m->ter_set( point( 6, i ), t_rock );
                m->ter_set( point( 17, i ), t_rock );
                if( i > 7 && i < 16 ) {
                    m->ter_set( point( i, 10 ), t_rock );
                    m->ter_set( point( i, 13 ), t_rock );
                } else {
                    m->ter_set( point( i - 1, 6 ), t_rock );
                    m->ter_set( point( i - 1, 10 ), t_rock );
                    m->ter_set( point( i - 1, 13 ), t_rock );
                    m->ter_set( point( i - 1, 17 ), t_rock );
                }
            }
            break;
        case 1:
            for( int i = 6; i < 11; i++ ) {
                m->ter_set( point( i, i ), t_lava );
                m->ter_set( point( SEEX * 2 - 1 - i, i ), t_lava );
                m->ter_set( point( i, SEEY * 2 - 1 - i ), t_lava );
                m->ter_set( point( SEEX * 2 - 1 - i, SEEY * 2 - 1 - i ), t_lava );
                if( i < 10 ) {
                    m->ter_set( point( i + 1, i ), t_lava );
                    m->ter_set( point( SEEX * 2 - i, i ), t_lava );
                    m->ter_set( point( i + 1, SEEY * 2 - 1 - i ), t_lava );
                    m->ter_set( point( SEEX * 2 - i, SEEY * 2 - 1 - i ), t_lava );

                    m->ter_set( point( i, i + 1 ), t_lava );
                    m->ter_set( point( SEEX * 2 - 1 - i, i + 1 ), t_lava );
                    m->ter_set( point( i, SEEY * 2 - i ), t_lava );
                    m->ter_set( point( SEEX * 2 - 1 - i, SEEY * 2 - i ), t_lava );
                }
                if( i < 9 ) {
                    m->ter_set( point( i + 2, i ), t_rock );
                    m->ter_set( point( SEEX * 2 - i + 1, i ), t_rock );
                    m->ter_set( point( i + 2, SEEY * 2 - 1 - i ), t_rock );
                    m->ter_set( point( SEEX * 2 - i + 1, SEEY * 2 - 1 - i ), t_rock );

                    m->ter_set( point( i, i + 2 ), t_rock );
                    m->ter_set( point( SEEX * 2 - 1 - i, i + 2 ), t_rock );
                    m->ter_set( point( i, SEEY * 2 - i + 1 ), t_rock );
                    m->ter_set( point( SEEX * 2 - 1 - i, SEEY * 2 - i + 1 ), t_rock );
                }
            }
            break;
        case 2:
            for( int i = 7; i < 17; i++ ) {
                m->ter_set( point( i, 6 ), t_rock );
                m->ter_set( point( 6, i ), t_rock );
                m->ter_set( point( i, 17 ), t_rock );
                m->ter_set( point( 17, i ), t_rock );
                if( i != 7 && i != 16 && i != 11 && i != 12 ) {
                    m->ter_set( point( i, 8 ), t_rock );
                    m->ter_set( point( 8, i ), t_rock );
                    m->ter_set( point( i, 15 ), t_rock );
                    m->ter_set( point( 15, i ), t_rock );
                }
                if( i == 11 || i == 12 ) {
                    m->ter_set( point( i, 10 ), t_rock );
                    m->ter_set( point( 10, i ), t_rock );
                    m->ter_set( point( i, 13 ), t_rock );
                    m->ter_set( point( 13, i ), t_rock );
                }
            }
            break;
        case 3:
            for( int i = 6; i < 11; i++ ) {
                for( int j = 6; j < 11; j++ ) {
                    m->ter_set( point( i, j ), t_lava );
                    m->ter_set( point( SEEX * 2 - 1 - i, j ), t_lava );
                    m->ter_set( point( i, SEEY * 2 - 1 - j ), t_lava );
                    m->ter_set( point( SEEX * 2 - 1 - i, SEEY * 2 - 1 - j ), t_lava );
                }
            }
            break;
    }

}

void mapgen_tutorial( mapgendata &dat )
{
    map *const m = &dat.m;
    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( j == 0 || j == SEEY * 2 - 1 ) {
                m->ter_set( point( i, j ), t_wall );
            } else if( i == 0 || i == SEEX * 2 - 1 ) {
                m->ter_set( point( i, j ), t_wall );
            } else if( j == SEEY ) {
                if( i % 4 == 2 ) {
                    m->ter_set( point( i, j ), t_door_c );
                } else if( i % 5 == 3 ) {
                    m->ter_set( point( i, j ), t_window_domestic );
                } else {
                    m->ter_set( point( i, j ), t_wall );
                }
            } else {
                m->ter_set( point( i, j ), t_floor );
            }
        }
    }
    m->furn_set( point( 7, SEEY * 2 - 4 ), f_rack );
    m->place_gas_pump( point( SEEX * 2 - 2, SEEY * 2 - 4 ), rng( 500, 1000 ) );
    if( dat.zlevel() < 0 ) {
        m->ter_set( point( SEEX - 2, SEEY + 2 ), t_stairs_up );
        m->ter_set( point( 2, 2 ), t_water_sh );
        m->ter_set( point( 2, 3 ), t_water_sh );
        m->ter_set( point( 3, 2 ), t_water_sh );
        m->ter_set( point( 3, 3 ), t_water_sh );
    } else {
        m->spawn_item( point( 5, SEEY + 1 ), "helmet_bike" );
        m->spawn_item( point( 4, SEEY + 1 ), "backpack" );
        m->spawn_item( point( 3, SEEY + 1 ), "pants_cargo" );
        m->spawn_item( point( 7, SEEY * 2 - 4 ), "machete" );
        m->spawn_item( point( 7, SEEY * 2 - 4 ), "9mm" );
        m->spawn_item( point( 7, SEEY * 2 - 4 ), "9mmP" );
        m->spawn_item( point( 7, SEEY * 2 - 4 ), "uzi" );
        m->spawn_item( point( 7, SEEY * 2 - 4 ), "uzimag" );
        m->spawn_item( point( SEEX * 2 - 2, SEEY + 5 ), "bubblewrap" );
        m->spawn_item( point( SEEX * 2 - 2, SEEY + 6 ), "grenade" );
        m->spawn_item( point( SEEX * 2 - 3, SEEY + 6 ), "flashlight" );
        m->spawn_item( point( SEEX * 2 - 3, SEEY + 6 ), "light_disposable_cell" );
        m->spawn_item( point( SEEX * 2 - 2, SEEY + 7 ), "cig" );
        m->spawn_item( point( SEEX * 2 - 2, SEEY + 7 ), "codeine" );
        m->spawn_item( point( SEEX * 2 - 3, SEEY + 7 ), "water" );
        m->ter_set( point( SEEX - 2, SEEY + 2 ), t_stairs_down );
    }
}

void mapgen_forest( mapgendata &dat )
{
    map *const m = &dat.m;
    // Adjacency factor is basically used to weight the frequency of a feature
    // being placed by the relative sparseness of the current terrain to its
    // neighbors. For example, a forest_thick surrounded by forest_thick on
    // all sides can be much more dense than a forest_water surrounded by
    // fields on all sides. It's a little magic-number-y but somewhat replicates
    // the behavior of the previous forest mapgen when fading forest terrains
    // into each other and non-forest terrains.

    const auto get_sparseness_adjacency_factor = [&dat]( const oter_id & ot ) {
        const auto biome = dat.region.forest_composition.biomes.find( ot );
        if( biome == dat.region.forest_composition.biomes.end() ) {
            // If there is no defined biome for this oter, use 0. It's possible
            // to specify biomes in the forest regional settings that are not
            // rendered by this forest map gen method, in order to control
            // how terrains are blended together (e.g. specify roads with an equal
            // sparsness adjacency factor to forests so that forests don't fade out
            // as they transition to roads.
            return 0;
        }
        return biome->second.sparseness_adjacency_factor;
    };

    const auto fill_adjacency_factor = [&dat, &get_sparseness_adjacency_factor]( int self_factor ) {
        dat.fill( self_factor );
        for( int i = 0; i < 4; i++ ) {
            dat.dir( i ) += get_sparseness_adjacency_factor( dat.t_nesw[i] );
        }
    };

    const ter_furn_id no_ter_furn = ter_furn_id();

    const auto get_feature_for_neighbor = [&dat,
                                           &no_ter_furn]( const std::map<oter_id, ter_furn_id> &biome_features,
    const om_direction::type dir ) {
        const oter_id dir_ot = dat.neighbor_at( dir );
        const auto feature = biome_features.find( dir_ot );
        if( feature == biome_features.end() ) {
            // If we have no biome for this neighbor, then we just return any empty feature.
            // As with the sparseness adjacency factor, it's possible to define non-forest
            // biomes in the regional settings so that they provide neighbor features
            // here for blending purposes (e.g. define dirt terrain for roads so that the
            // ground fades from forest ground cover to dirt as blends with roads.
            return no_ter_furn;
        }
        return feature->second;
    };

    // The max sparseness is calculated across all the possible biomes, not just the adjacent ones.
    const auto get_max_sparseness_adjacency_factor = [&dat]() {
        if( dat.region.forest_composition.biomes.empty() ) {
            return 0;
        }
        std::vector<int> factors;
        factors.reserve( dat.region.forest_composition.biomes.size() );
        for( auto &b : dat.region.forest_composition.biomes ) {
            factors.push_back( b.second.sparseness_adjacency_factor );
        }
        return *std::ranges::max_element( factors );
    };

    // Get the sparesness factor for this terrain, and fill it.
    const int factor = get_sparseness_adjacency_factor( dat.terrain_type() );
    fill_adjacency_factor( factor );

    const int max_factor = get_max_sparseness_adjacency_factor();

    // Our margins for blending divide the overmap terrain into nine sections.
    static constexpr int margin_x = SEEX * 2 / 3;
    static constexpr int margin_y = SEEY * 2 / 3;

    const auto get_blended_feature = [&no_ter_furn, &max_factor, &factor,
                  &get_feature_for_neighbor, &dat]( point  p ) {
        // Pick one random feature from each biome according to the biome defs and save it into a lookup.
        // We'll blend these features together below based on the current and adjacent terrains.
        std::map<oter_id, ter_furn_id> biome_features;
        for( auto &b : dat.region.forest_composition.biomes ) {
            biome_features[b.first] = b.second.pick();
        }

        // Get a feature for ourself and each of the adjacent overmap terrains.
        const ter_furn_id east_feature = get_feature_for_neighbor( biome_features,
                                         om_direction::type::east );
        const ter_furn_id west_feature = get_feature_for_neighbor( biome_features,
                                         om_direction::type::west );
        const ter_furn_id north_feature = get_feature_for_neighbor( biome_features,
                                          om_direction::type::north );
        const ter_furn_id south_feature = get_feature_for_neighbor( biome_features,
                                          om_direction::type::south );
        const ter_furn_id self_feature = biome_features[dat.terrain_type()];

        // We'll use our margins and the four adjacent overmap terrains to pick a blended
        // feature based on the features we picked above and a linear weight as we
        // transition through the margins.
        //
        // (0,0)     NORTH
        //      ---------------
        //      | NW | W | NE |
        //      |----|---|----|
        // WEST | W  |   | E  |  EAST
        //      |----|---|----|
        //      | SW | S | SE |
        //      ---------------
        //           SOUTH      (SEEX * 2, SEEY * 2)

        const int west_weight = std::max( margin_x - p.x, 0 );
        const int east_weight = std::max( p.x - ( SEEX * 2 - margin_x ) + 1, 0 );
        const int north_weight = std::max( margin_y - p.y, 0 );
        const int south_weight = std::max( p.y - ( SEEY * 2 - margin_y ) + 1, 0 );

        // We'll build a weighted list of features to pull from at the end.
        weighted_int_list<const ter_furn_id> feature_pool;

        // W sections
        if( p.x < margin_x ) {
            // NW corner - blend N, W, and self
            if( p.y < margin_y ) {
                feature_pool.add( no_ter_furn, 3 * max_factor - ( dat.n_fac + dat.w_fac + factor * 2 ) );
                feature_pool.add( self_feature, 1 );
                feature_pool.add( west_feature, west_weight );
                feature_pool.add( north_feature, north_weight );
            }
            // SW corner - blend S, W, and self
            else if( p.y > SEEY * 2 - margin_y ) {
                feature_pool.add( no_ter_furn, 3 * max_factor - ( dat.s_fac + dat.w_fac + factor * 2 ) );
                feature_pool.add( self_feature, factor );
                feature_pool.add( west_feature, west_weight );
                feature_pool.add( south_feature, south_weight );
            }
            // W edge - blend W and self
            else {
                feature_pool.add( no_ter_furn, 2 * max_factor - ( dat.w_fac + factor * 2 ) );
                feature_pool.add( self_feature, factor );
                feature_pool.add( west_feature, west_weight );
            }
        }
        // E sections
        else if( p.x > SEEX * 2 - margin_x ) {
            // NE corner - blend N, E, and self
            if( p.y < margin_y ) {
                feature_pool.add( no_ter_furn, 3 * max_factor - ( dat.n_fac + dat.e_fac + factor * 2 ) );
                feature_pool.add( self_feature, factor );
                feature_pool.add( east_feature, east_weight );
                feature_pool.add( north_feature, north_weight );
            }
            // SE corner - blend S, E, and self
            else if( p.y > SEEY * 2 - margin_y ) {
                feature_pool.add( no_ter_furn, 3 * max_factor - ( dat.s_fac + dat.e_fac + factor * 2 ) );
                feature_pool.add( self_feature, factor );
                feature_pool.add( east_feature, east_weight );
                feature_pool.add( south_feature, south_weight );
            }
            // E edge - blend E and self
            else {
                feature_pool.add( no_ter_furn, 2 * max_factor - ( dat.e_fac + factor * 2 ) );
                feature_pool.add( self_feature, factor );
                feature_pool.add( east_feature, east_weight );
            }
        }
        // Central sections
        else {
            // N edge - blend N and self
            if( p.y < margin_y ) {
                feature_pool.add( no_ter_furn, 2 * max_factor - ( dat.n_fac + factor * 2 ) );
                feature_pool.add( self_feature, factor );
                feature_pool.add( north_feature, north_weight );
            }
            // S edge - blend S, and self
            else if( p.y > SEEY * 2 - margin_y ) {
                feature_pool.add( no_ter_furn, 2 * max_factor - ( dat.s_fac + factor * 2 ) );
                feature_pool.add( self_feature, factor );
                feature_pool.add( south_feature, south_weight );
            }
            // center - no blending
            else {
                feature_pool.add( no_ter_furn, max_factor - factor * 2 );
                feature_pool.add( self_feature, factor );
            }
        }

        // Pick a single feature from the pool we built above and return it.
        const ter_furn_id *feature = feature_pool.pick();
        if( feature == nullptr ) {
            return no_ter_furn;
        } else {
            return *feature;
        }
    };

    // Get the current biome def for this terrain.
    const auto current_biome_def_it = dat.region.forest_composition.biomes.find( dat.terrain_type() );

    // If there is no biome def for this terrain, fill in with the region's default ground cover
    // and bail--nothing more to be done.
    if( current_biome_def_it == dat.region.forest_composition.biomes.end() ) {
        dat.fill_groundcover();
        return;
    }

    const forest_biome current_biome_def = current_biome_def_it->second;

    // If this biome does not define its own groundcover, then fill with the region's ground
    // cover. Otherwise, fill with the biome defs groundcover.
    if( current_biome_def.groundcover.empty() ) {
        dat.fill_groundcover();
    } else {
        m->draw_fill_background( current_biome_def.groundcover );
    }

    // There is a chance of placing terrain dependent furniture, e.g. f_cattails on t_water_sh.
    const auto set_terrain_dependent_furniture = [&current_biome_def, &m]( const ter_id & tid,
    point  p ) {
        const auto terrain_dependent_furniture_it = current_biome_def.terrain_dependent_furniture.find(
                    tid );
        if( terrain_dependent_furniture_it == current_biome_def.terrain_dependent_furniture.end() ) {
            // No terrain dependent furnitures for this terrain, so bail.
            return;
        }

        const forest_biome_terrain_dependent_furniture tdf = terrain_dependent_furniture_it->second;
        if( tdf.furniture.get_weight() <= 0 ) {
            // We've got furnitures, but their weight is 0 or less, so bail.
            return;
        }

        if( one_in( tdf.chance ) ) {
            // Pick a furniture and set it on the map right now.
            const auto fid = tdf.furniture.pick();
            m->furn_set( p, *fid );
        }
    };

    // Loop through each location in this overmap terrain and attempt to place a feature and
    // terrain dependent furniture.
    for( int x = 0; x < SEEX * 2; x++ ) {
        for( int y = 0; y < SEEY * 2; y++ ) {
            point p( x, y );
            const ter_furn_id feature = get_blended_feature( p );
            ter_or_furn_set( m, p, feature );
            set_terrain_dependent_furniture( feature.ter, p );
        }
    }

    // Place items on this terrain as defined in the biome.
    for( int i = 0; i < current_biome_def.item_spawn_iterations; i++ ) {
        m->place_items( current_biome_def.item_group, current_biome_def.item_group_chance, point_zero,
                        point( SEEX * 2 - 1, SEEY * 2 - 1 ), true, dat.when() );
    }
}

void mapgen_forest_trail_straight( mapgendata &dat )
{
    map *const m = &dat.m;
    mapgendata forest_mapgen_dat( dat, oter_str_id( "forest_thick" ).id() );
    mapgen_forest( forest_mapgen_dat );

    const auto center_offset = [&dat]() {
        return rng( -dat.region.forest_trail.trail_center_variance,
                    dat.region.forest_trail.trail_center_variance );
    };

    const auto width_offset = [&dat]() {
        return rng( dat.region.forest_trail.trail_width_offset_min,
                    dat.region.forest_trail.trail_width_offset_max );
    };

    point center( SEEX + center_offset(), SEEY + center_offset() );

    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( i > center.x - width_offset() && i < center.x + width_offset() ) {
                m->furn_set( point( i, j ), f_null );
                m->ter_set( point( i, j ), *dat.region.forest_trail.trail_terrain.pick() );
            }
        }
    }

    if( dat.terrain_type() == "forest_trail_ew" || dat.terrain_type() == "forest_trail_end_east" ||
        dat.terrain_type() == "forest_trail_end_west" ) {
        m->rotate( 1 );
    }

    m->place_items( item_group_id( "forest_trail" ), 75, center + point( -2, -2 ),
                    center + point( 2, 2 ), true, dat.when() );
}

void mapgen_forest_trail_curved( mapgendata &dat )
{
    map *const m = &dat.m;
    mapgendata forest_mapgen_dat( dat, oter_str_id( "forest_thick" ).id() );
    mapgen_forest( forest_mapgen_dat );

    const auto center_offset = [&dat]() {
        return rng( -dat.region.forest_trail.trail_center_variance,
                    dat.region.forest_trail.trail_center_variance );
    };

    const auto width_offset = [&dat]() {
        return rng( dat.region.forest_trail.trail_width_offset_min,
                    dat.region.forest_trail.trail_width_offset_max );
    };

    point center( SEEX + center_offset(), SEEY + center_offset() );

    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( ( i > center.x - width_offset() && i < center.x + width_offset() &&
                  j < center.y + width_offset() ) ||
                ( j > center.y - width_offset() && j < center.y + width_offset() &&
                  i > center.x - width_offset() ) ) {
                m->furn_set( point( i, j ), f_null );
                m->ter_set( point( i, j ), *dat.region.forest_trail.trail_terrain.pick() );
            }
        }
    }

    if( dat.terrain_type() == "forest_trail_es" ) {
        m->rotate( 1 );
    }
    if( dat.terrain_type() == "forest_trail_sw" ) {
        m->rotate( 2 );
    }
    if( dat.terrain_type() == "forest_trail_wn" ) {
        m->rotate( 3 );
    }

    m->place_items( item_group_id( "forest_trail" ), 75, center + point( -2, -2 ),
                    center + point( 2, 2 ), true, dat.when() );
}

void mapgen_forest_trail_tee( mapgendata &dat )
{
    map *const m = &dat.m;
    mapgendata forest_mapgen_dat( dat, oter_str_id( "forest_thick" ).id() );
    mapgen_forest( forest_mapgen_dat );

    const auto center_offset = [&dat]() {
        return rng( -dat.region.forest_trail.trail_center_variance,
                    dat.region.forest_trail.trail_center_variance );
    };

    const auto width_offset = [&dat]() {
        return rng( dat.region.forest_trail.trail_width_offset_min,
                    dat.region.forest_trail.trail_width_offset_max );
    };

    point center( SEEX + center_offset(), SEEY + center_offset() );

    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( ( i > center.x - width_offset() && i < center.x + width_offset() ) ||
                ( j > center.y - width_offset() &&
                  j < center.y + width_offset() && i > center.x - width_offset() ) ) {
                m->furn_set( point( i, j ), f_null );
                m->ter_set( point( i, j ), *dat.region.forest_trail.trail_terrain.pick() );
            }
        }
    }

    if( dat.terrain_type() == "forest_trail_esw" ) {
        m->rotate( 1 );
    }
    if( dat.terrain_type() == "forest_trail_nsw" ) {
        m->rotate( 2 );
    }
    if( dat.terrain_type() == "forest_trail_new" ) {
        m->rotate( 3 );
    }

    m->place_items( item_group_id( "forest_trail" ), 75, center + point( -2, -2 ),
                    center + point( 2, 2 ), true, dat.when() );
}

void mapgen_forest_trail_four_way( mapgendata &dat )
{
    map *const m = &dat.m;
    mapgendata forest_mapgen_dat( dat, oter_str_id( "forest_thick" ).id() );
    mapgen_forest( forest_mapgen_dat );

    const auto center_offset = [&dat]() {
        return rng( -dat.region.forest_trail.trail_center_variance,
                    dat.region.forest_trail.trail_center_variance );
    };

    const auto width_offset = [&dat]() {
        return rng( dat.region.forest_trail.trail_width_offset_min,
                    dat.region.forest_trail.trail_width_offset_max );
    };

    point center( SEEX + center_offset(), SEEY + center_offset() );

    for( int i = 0; i < SEEX * 2; i++ ) {
        for( int j = 0; j < SEEY * 2; j++ ) {
            if( ( i > center.x - width_offset() && i < center.x + width_offset() ) ||
                ( j > center.y - width_offset() &&
                  j < center.y + width_offset() ) ) {
                m->furn_set( point( i, j ), f_null );
                m->ter_set( point( i, j ), *dat.region.forest_trail.trail_terrain.pick() );
            }
        }
    }

    m->place_items( item_group_id( "forest_trail" ), 75, center + point( -2, -2 ),
                    center + point( 2, 2 ), true, dat.when() );
}

void mapgen_lake_shore( mapgendata &dat )
{
    map *const m = &dat.m;
    // Our lake shores may "extend" adjacent terrain, if the adjacent types are defined as being
    // extendable in our regional settings. What this effectively means is that if the lake shore is
    // adjacent to one of these, e.g. a forest, then rather than the lake shore simply having the
    // region's default groundcover for the land parts of the terrain, instead we run the mapgen
    // for this location as if it were the adjacent terrain, and then carve our water out of it as
    // per usual. I think it looks a lot nicer, e.g. in the case of a forest, to have the trees and
    // ground clutter of the forest abutting the water rather than simply some empty ground.

    // To accomplish this extension, we simply count up the adjacent terrains that are in the
    // defined extendable terrain setting, choose the most common one, and then run its mapgen.
    bool did_extend_adjacent_terrain = false;
    if( !dat.region.overmap_lake.shore_extendable_overmap_terrain.empty() ) {
        std::map<oter_id, int> adjacent_type_count;
        for( oter_id &adjacent : dat.t_nesw ) {
            // Define the terrain we'll look for a match on.
            oter_id match = adjacent;

            // Check if this terrain has an alias to something we actually will extend, and if so, use it.
            for( auto &alias : dat.region.overmap_lake.shore_extendable_overmap_terrain_aliases ) {
                if( is_ot_match( alias.overmap_terrain, adjacent, alias.match_type ) ) {
                    match = alias.alias;
                    break;
                }
            }

            if( std::ranges::find( dat.region.overmap_lake.shore_extendable_overmap_terrain,

                                   match ) != dat.region.overmap_lake.shore_extendable_overmap_terrain.end() ) {
                adjacent_type_count[match] += 1;
            }
        }

        if( !adjacent_type_count.empty() ) {
            const auto most_common_adjacent = std::ranges::max_element( adjacent_type_count,
                                              []( const std::pair<oter_id, int> &p1,
            const std::pair<oter_id, int> &p2 ) {
                return p1.second < p2.second;
            } );

            mapgendata forest_mapgen_dat( dat, most_common_adjacent->first );
            mapgen_forest( forest_mapgen_dat );
            did_extend_adjacent_terrain = run_mapgen_func( most_common_adjacent->first.id().str(),
                                          forest_mapgen_dat );

            // One fun side effect of running another mapgen here is that it may have placed items in locations
            // that we're later going to turn into water. Let's just remove all items.
            if( did_extend_adjacent_terrain ) {
                for( int x = 0; x < SEEX * 2; x++ ) {
                    for( int y = 0; y < SEEY * 2; y++ ) {
                        m->i_clear( point( x, y ) );
                    }
                }
            }
        }
    }

    // If we didn't extend an adjacent terrain, then just fill this entire location with the default
    // groundcover for the region.
    if( !did_extend_adjacent_terrain ) {
        if( dat.zlevel() >= 0 ) {
            dat.fill_groundcover();
        } else {
            fill_background( m, t_rock );
        }
    }

    const oter_id river_center( "river_center" );

    enum class n_type {
        lake,
        shore,
        river,
        solid
    };
    n_type neighbours[8];

    int lakes = 0;
    for( int i = 0; i < 8; i++ ) {
        const oter_id n = dat.t_nesw[i];

        if( n == river_center || n.obj().is_lake() ) {
            neighbours[i] = n_type::lake;
            lakes++;
        } else if( n.obj().is_lake_shore() ) {
            neighbours[i] = n_type::shore;
        } else if( n.obj().is_river() ) {
            neighbours[i] = n_type::river;
        } else {
            neighbours[i] = n_type::solid;
        }
    }

    // If we don't have any adjacent lakes, then we don't need to worry about a shoreline,
    // and are done at this point.
    if( lakes == 0 ) {
        return;
    }

    // Each shore have up to 8 connection points where coast line can be attached. Let's say
    // we have river_north at the west, it will be attached to slot 2.  Another shore at the
    // north, with one mutual lake at NE, it will be attached to slot 1. One more shore at east
    // with two mutual lakes at SE and NE, that'll make it attach to both 6 and 7 slots. That's
    // what we need to get in the end:

    //  -1----0-
    // |....    |      Overmap:
    // 2........7        FSL
    // | .......|        R@S
    // 3  ......6        LLL
    // |        |
    //  -4----5-

    // So far we have four points(1, 2, 6 and 7) which need to be connected to matching pairs.
    // To do so first we need to know where the water is relating to our points.  If there's two
    // points on one side(1+2, 3+4, 5+6, 7+8) it means that's a shore with a ground is between
    // of those points. If we have only one point on side it means that water is directed toward
    // middle of side. So, to find a pair for a point we're determining direction, and then
    // checking other points either clockwise, or counter clockwise. At the end we'll have two
    // lines 1->7 and 2->6 which will for our shore lines after some jittering.


    const bool n_lake  = neighbours[0] == n_type::lake;
    const bool e_lake  = neighbours[1] == n_type::lake;
    const bool s_lake  = neighbours[2] == n_type::lake;
    const bool w_lake  = neighbours[3] == n_type::lake;
    const bool ne_lake = neighbours[4] == n_type::lake;
    const bool se_lake = neighbours[5] == n_type::lake;
    const bool sw_lake = neighbours[6] == n_type::lake;
    const bool nw_lake = neighbours[7] == n_type::lake;

    const bool n_shore = neighbours[0] == n_type::shore;
    const bool e_shore = neighbours[1] == n_type::shore;
    const bool s_shore = neighbours[2] == n_type::shore;
    const bool w_shore = neighbours[3] == n_type::shore;

    const bool n_river_bank = neighbours[0] == n_type::river;
    const bool e_river_bank = neighbours[1] == n_type::river;
    const bool s_river_bank = neighbours[2] == n_type::river;
    const bool w_river_bank = neighbours[3] == n_type::river;

    const bool n_water  = neighbours[0] != n_type::solid;
    const bool e_water  = neighbours[1] != n_type::solid;
    const bool s_water  = neighbours[2] != n_type::solid;
    const bool w_water  = neighbours[3] != n_type::solid;
    const bool ne_water = neighbours[4] != n_type::solid;
    const bool se_water = neighbours[5] != n_type::solid;
    const bool sw_water = neighbours[6] != n_type::solid;
    const bool nw_water = neighbours[7] != n_type::solid;

    // This is length we end up pushing things about by as a baseline.
    const int mx = SEEX * 2 - 1;
    const int my = SEEY * 2 - 1;
    const int sector_length = SEEX * 2 / 3;

    // Define the corners of the map. These won't change.
    // NOLINTNEXTLINE(cata-point-initialization, cata-use-named-point-constants)
    static constexpr point nw_corner( 0, 0 );
    static constexpr point ne_corner( mx, 0 );
    static constexpr point se_corner( mx, my );
    static constexpr point sw_corner( 0, my );

    static constexpr point river_beach_x( sector_length / 2, 0 );
    static constexpr point river_beach_y( 0, sector_length / 2 );
    static constexpr point lake_beach_x( sector_length * 2, 0 );
    static constexpr point lake_beach_y( 0, sector_length * 2 );

    std::vector<point> shore_points;
    std::map<int, point> slots;

    // We need to detect whether river bank is touching this map with its side, or corner, and where.
    // Checking our surrounding can give just enough information for that
    if( n_river_bank ) {
        if( nw_water && ( !ne_water || !e_water ) ) {
            slots[0] = ne_corner - river_beach_x;
        }
        if( ne_water && ( !nw_water || !w_water ) ) {
            slots[1] = nw_corner + river_beach_x;
        }
    }

    if( w_river_bank ) {
        if( sw_water && ( !nw_water || !n_water ) ) {
            slots[2] = nw_corner + river_beach_y;
        }
        if( nw_water && ( !sw_water || !s_water ) ) {
            slots[3] = sw_corner - river_beach_y;
        }
    }

    if( s_river_bank ) {
        if( se_water && ( !sw_water || !w_water ) ) {
            slots[4] = sw_corner + river_beach_x;
        }
        if( sw_water && ( !se_water || !e_water ) ) {
            slots[5] = se_corner - river_beach_x;
        }
    }

    if( e_river_bank ) {
        if( ne_water && ( !se_water || !s_water ) ) {
            slots[6] = se_corner - river_beach_y;
        }
        if( se_water && ( !ne_water || !n_water ) ) {
            slots[7] = ne_corner + river_beach_y;
        }
    }

    // Shores are weird beings, they can be found not only on coast, but also in open ground,
    // and in middle of lake. We need to connect to those ones which neighbouring same lake surface
    if( n_shore ) {
        if( nw_lake || w_lake ) {
            slots[0] = ne_corner - lake_beach_x;
        }
        if( ne_lake || e_lake ) {
            slots[1] = nw_corner + lake_beach_x;
        }
    }

    if( w_shore ) {
        if( sw_lake || s_lake ) {
            slots[2] = nw_corner + lake_beach_y;
        }
        if( nw_lake || n_lake ) {
            slots[3] = sw_corner - lake_beach_y;
        }
    }

    if( s_shore ) {
        if( se_lake || e_lake ) {
            slots[4] = sw_corner + lake_beach_x;
        }
        if( sw_lake || w_lake ) {
            slots[5] = se_corner - lake_beach_x;
        }
    }

    if( e_shore ) {
        if( ne_lake || n_lake ) {
            slots[6] = se_corner - lake_beach_y;
        }
        if( se_lake || s_lake ) {
            slots[7] = ne_corner + lake_beach_y;
        }
    }

    std::vector<std::vector<point>> line_segments;
    // Ok, now we know everything we need about adjacent terrain, time to link the points

    bool open[8] = { false };
    for( int i = 0; i < 8; i++ ) {
        open[i] = slots.contains( i );
        // Shores with two connections per side have overlapping offsets, we need to swap them
        if( i % 2 && open[i] && open[i - 1] ) {
            point p = slots[i];
            slots[i] = slots[i - 1];
            slots[i - 1] = p;
        }
    }

    for( int i = 0; i < 8; i++ ) {
        if( !open[i] ) {
            continue;
        }

        // Check the next slot, and inverse direction if any
        int next = i % 2 == 0 ? 1 : -1;
        int dir = slots.contains( i + next ) ? -next : next;

        // Now make a full round loop from our current point
        int pair = i;
        for( int j = modulo( i + dir, 8 ); j != i; j = modulo( j + dir, 8 ) ) {
            if( open[j] ) {
                pair = j;
                break;
            }
        }

        if( pair == i ) {
            // No pair. Let's attach coast line to the middle of closest non-water tile
            bool water[8] = { n_water, n_water, w_water, w_water, s_water, s_water, e_water, e_water };
            int fallback = i;
            for( int j = modulo( i + dir, 8 ); j != i; j = modulo( j + dir, 8 ) ) {
                if( !water[j] ) {
                    fallback = j;
                    break;
                }
            }
            if( fallback < 2 ) {
                line_segments.push_back( { slots[i], { mx / 2, 0 } } );
            } else if( fallback < 4 ) {
                line_segments.push_back( { slots[i], { 0, my / 2 } } );
            } else if( fallback < 6 ) {
                line_segments.push_back( { slots[i], { mx / 2, my } } );
            } else if( fallback < 8 ) {
                line_segments.push_back( { slots[i], { mx, my / 2 } } );
            }
        } else if( pair == i + next ) {
            // We returned to our neighbour on same side, let's connect them in the middle
            point mid = { mx / 2, my / 2 };
            line_segments.push_back( { mid, slots[i] } );
            line_segments.push_back( { mid, slots[pair] } );
        } else {
            // Make a shore line with given two points
            line_segments.push_back( { slots[i], slots[pair] } );
        }

        // Close both points, so we won't link anything to them again
        open[i] = false;
        open[pair] = false;
    }

    // We have no shores at all, make a small reef surrounded by water
    if( line_segments.empty() ) {
        point nw_inner = nw_corner + point( sector_length, sector_length );
        point ne_inner = ne_corner + point( -sector_length, sector_length );
        point se_inner = se_corner + point( -sector_length, -sector_length );
        point sw_inner = sw_corner + point( sector_length, -sector_length );
        line_segments.insert( line_segments.end(), {
            {ne_inner, nw_inner}, {nw_inner, sw_inner},
            {sw_inner, se_inner}, {se_inner, ne_inner}
        } );
    }

    static constexpr inclusive_rectangle<point> map_boundaries( nw_corner, se_corner );

    // This will draw our shallow water coastline from the "from" point to the "to" point.
    // It buffers the points a bit for a thicker line. It also clears any furniture that might
    // be in the location as a result of our extending adjacent mapgen.
    const auto draw_shallow_water = [&]( point  from, point  to ) {
        std::vector<point> points = line_to( from, to );
        for( auto &p : points ) {
            for( point bp : closest_points_first( p, 1 ) ) {
                if( !map_boundaries.contains( bp ) ) {
                    continue;
                }
                // Use t_null for now instead of t_water_sh, because sometimes our extended terrain
                // has put down a t_water_sh, and we need to be able to flood-fill over that.
                m->ter_set( bp, t_null );
                m->furn_set( bp, f_null );
            }
        }
    };

    // We need to have same shoreline on different z levels, to match surface shore
    // with submerged shore, to do so we'll jitter shore lines using deterministic
    // random seeded with x\y coordinates
    // NOLINTNEXTLINE(cata-determinism)
    std::mt19937 prng( std::hash<point_abs_omt>()( dat.pos.xy() ) ^ g->get_seed() );

    // Given two points, return a point that is midway between the two points and then
    // jittered by a random amount in proportion to the length of the line segment.
    const auto jittered_midpoint = [&]( point  from, point  to ) {
        const int jitter = rl_dist( from, to ) / 4;
        std::uniform_int_distribution<int> roll( -jitter, jitter );
        const point midpoint( ( from.x + to.x ) / 2 + roll( prng ),
                              ( from.y + to.y ) / 2 + roll( prng ) );
        return midpoint;
    };

    ter_id edge_tile = dat.zlevel() >= 0 ? t_water_sh : t_rock;
    ter_id water_tile = dat.zlevel() >= 0 ? t_water_dp :
                        dat.zlevel() == dat.region.overmap_lake.lake_depth ? t_lake_bed :
                        t_water_cube;

    // For each of our valid shoreline line segments, generate a slightly more interesting
    // set of line segments by splitting the line into four segments with jittered
    // midpoints, and then draw shallow water for four each of those.
    for( auto &ls : line_segments ) {
        const point mp1 = jittered_midpoint( ls[0], ls[1] );
        const point mp2 = jittered_midpoint( ls[0], mp1 );
        const point mp3 = jittered_midpoint( mp1, ls[1] );

        draw_shallow_water( ls[0], mp2 );
        draw_shallow_water( mp2, mp1 );
        draw_shallow_water( mp1, mp3 );
        draw_shallow_water( mp3, ls[1] );
    }

    // Now that we've done our ground mapgen and laid down a contiguous shoreline of shallow water,
    // we'll floodfill the sections adjacent to the lake with deep water. As before, we also clear
    // out any furniture that we placed by the extended mapgen.
    std::unordered_set<point> visited;

    const auto should_fill = [&]( point  p ) {
        if( !map_boundaries.contains( p ) ) {
            return false;
        }
        return m->ter( p ) != t_null;
    };

    const auto fill_deep_water = [&]( point  starting_point ) {
        std::vector<point> water_points = ff::point_flood_fill_4_connected( starting_point, visited,
                                          should_fill );
        for( auto &wp : water_points ) {
            m->ter_set( wp, water_tile );
            m->furn_set( wp, f_null );
        }
    };

    // We'll flood fill from the four corners, using the corner if any of the locations
    // adjacent to it were a lake.
    if( n_lake || nw_lake || w_lake ) {
        fill_deep_water( nw_corner );
    }

    if( s_lake || sw_lake || w_lake ) {
        fill_deep_water( sw_corner );
    }

    if( n_lake || ne_lake || e_lake ) {
        fill_deep_water( ne_corner );
    }

    if( s_lake || se_lake || e_lake ) {
        fill_deep_water( se_corner );
    }

    // We previously placed our shallow water but actually did a t_null instead to make sure that we didn't
    // pick up shallow water from our extended terrain. Now turn those nulls into t_water_sh.
    m->translate( t_null, edge_tile );
}

void mremove_trap( map *m, point p )
{
    tripoint actual_location( p, m->get_abs_sub().z );
    m->remove_trap( actual_location );
}

void mtrap_set( map *m, point p, trap_id type )
{
    tripoint actual_location( p, m->get_abs_sub().z );
    m->trap_set( actual_location, type );
}

void madd_field( map *m, point p, field_type_id type, int intensity )
{
    tripoint actual_location( p, m->get_abs_sub().z );
    m->add_field( actual_location, type, intensity, 0_turns );
}

void resolve_regional_terrain_and_furniture( const mapgendata &dat )
{
    for( const tripoint &p : dat.m.points_on_zlevel() ) {
        const ter_id tid_before = dat.m.ter( p );
        const ter_id tid_after = dat.region.region_terrain_and_furniture.resolve( tid_before );
        if( tid_after != tid_before ) {
            dat.m.ter_set( p, tid_after );
        }
        const furn_id fid_before = dat.m.furn( p );
        const furn_id fid_after = dat.region.region_terrain_and_furniture.resolve( fid_before );
        if( fid_after != fid_before ) {
            dat.m.furn_set( p, fid_after );
        }
    }
}

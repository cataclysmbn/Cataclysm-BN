#include "submap.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <span>
#include <utility>

#include "debug.h"
#include "int_id.h"
#include "lightmap.h"
#include "map.h"
#include "mapdata.h"
#include "tileray.h"
#include "trap.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "weather.h"


template<int sx, int sy>
void maptile_soa<sx, sy>::swap_soa_tile( point p1, point p2 )
{

    std::swap( ter[p1.x][p1.y], ter[p2.x][p2.y] );
    std::swap( frn[p1.x][p1.y], frn[p2.x][p2.y] );
    std::swap( lum[p1.x][p1.y], lum[p2.x][p2.y] );
    std::swap( itm[p1.x][p1.y], itm[p2.x][p2.y] );
    std::swap( fld[p1.x][p1.y], fld[p2.x][p2.y] );
    std::swap( trp[p1.x][p1.y], trp[p2.x][p2.y] );
    std::swap( rad[p1.x][p1.y], rad[p2.x][p2.y] );
}

void submap::swap( submap &first, submap &second )
{
    std::swap( first.ter, second.ter );
    std::swap( first.frn, second.frn );
    std::swap( first.lum, second.lum );
    std::swap( first.fld, second.fld );
    std::swap( first.trp, second.trp );
    std::swap( first.rad, second.rad );
    std::swap( first.is_uniform, second.is_uniform );
    std::swap( first.active_items, second.active_items );
    std::swap( first.field_count, second.field_count );
    std::swap( first.last_touched, second.last_touched );
    std::swap( first.spawns, second.spawns );
    std::swap( first.vehicles, second.vehicles );
    std::swap( first.partial_constructions, second.partial_constructions );
    std::swap( first.active_furniture, second.active_furniture );
    std::swap( first.transformer_last_run, second.transformer_last_run );
    std::swap( first.is_uniform, second.is_uniform );
    std::swap( first.computers, second.computers );
    std::swap( first.legacy_computer, second.legacy_computer );
    std::swap( first.temperature, second.temperature );
    std::swap( first.cosmetics, second.cosmetics );

    for( int x = 0; x < SEEX; x++ ) {
        for( int y = 0; y < SEEY; y++ ) {
            std::swap( first.itm[x][y], second.itm[x][y] );
        }
    }
}

//There's not a briefer way to write this I don't think
template<int sx, int sy>
maptile_soa<sx, sy>::maptile_soa( tripoint offset ) : itm{{
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        location_vector{ new tile_item_location( offset + point( 0, 0 ) )},
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        location_vector{ new tile_item_location( offset + point( 0, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 0, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 0, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 0, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 0, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 0, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 0, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 0, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 0, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 0, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 0, 11 ) )},
    },
    {
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        location_vector{ new tile_item_location( offset + point( 1, 0 ) )},
        // NOLINTNEXTLINE(cata-use-named-point-constants)
        location_vector{ new tile_item_location( offset + point( 1, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 1, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 1, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 1, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 1, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 1, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 1, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 1, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 1, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 1, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 1, 11 ) )},
    }, {
        location_vector{ new tile_item_location( offset + point( 2, 0 ) )},
        location_vector{ new tile_item_location( offset + point( 2, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 2, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 2, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 2, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 2, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 2, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 2, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 2, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 2, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 2, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 2, 11 ) )},
    }, {
        location_vector{ new tile_item_location( offset + point( 3, 0 ) )},
        location_vector{ new tile_item_location( offset + point( 3, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 3, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 3, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 3, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 3, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 3, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 3, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 3, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 3, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 3, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 3, 11 ) )},
    }, {
        location_vector{ new tile_item_location( offset + point( 4, 0 ) )},
        location_vector{ new tile_item_location( offset + point( 4, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 4, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 4, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 4, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 4, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 4, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 4, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 4, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 4, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 4, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 4, 11 ) )},
    }, {
        location_vector{ new tile_item_location( offset + point( 5, 0 ) )},
        location_vector{ new tile_item_location( offset + point( 5, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 5, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 5, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 5, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 5, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 5, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 5, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 5, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 5, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 5, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 5, 11 ) )},
    }, {
        location_vector{ new tile_item_location( offset + point( 6, 0 ) )},
        location_vector{ new tile_item_location( offset + point( 6, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 6, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 6, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 6, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 6, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 6, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 6, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 6, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 6, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 6, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 6, 11 ) )},
    }, {
        location_vector{ new tile_item_location( offset + point( 7, 0 ) )},
        location_vector{ new tile_item_location( offset + point( 7, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 7, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 7, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 7, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 7, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 7, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 7, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 7, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 7, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 7, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 7, 11 ) )},
    }, {
        location_vector{ new tile_item_location( offset + point( 8, 0 ) )},
        location_vector{ new tile_item_location( offset + point( 8, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 8, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 8, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 8, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 8, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 8, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 8, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 8, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 8, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 8, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 8, 11 ) )},
    }, {
        location_vector{ new tile_item_location( offset + point( 9, 0 ) )},
        location_vector{ new tile_item_location( offset + point( 9, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 9, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 9, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 9, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 9, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 9, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 9, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 9, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 9, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 9, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 9, 11 ) )},
    }, {
        location_vector{ new tile_item_location( offset + point( 10, 0 ) )},
        location_vector{ new tile_item_location( offset + point( 10, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 10, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 10, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 10, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 10, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 10, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 10, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 10, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 10, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 10, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 10, 11 ) )},
    }, {
        location_vector{ new tile_item_location( offset + point( 11, 0 ) )},
        location_vector{ new tile_item_location( offset + point( 11, 1 ) )},
        location_vector{ new tile_item_location( offset + point( 11, 2 ) )},
        location_vector{ new tile_item_location( offset + point( 11, 3 ) )},
        location_vector{ new tile_item_location( offset + point( 11, 4 ) )},
        location_vector{ new tile_item_location( offset + point( 11, 5 ) )},
        location_vector{ new tile_item_location( offset + point( 11, 6 ) )},
        location_vector{ new tile_item_location( offset + point( 11, 7 ) )},
        location_vector{ new tile_item_location( offset + point( 11, 8 ) )},
        location_vector{ new tile_item_location( offset + point( 11, 9 ) )},
        location_vector{ new tile_item_location( offset + point( 11, 10 ) )},
        location_vector{ new tile_item_location( offset + point( 11, 11 ) )},
    }}
{
}

submap::submap( tripoint offset ) : maptile_soa<SEEX, SEEY>( offset )
{
    std::uninitialized_fill_n( &ter[0][0], elements, t_null );
    std::uninitialized_fill_n( &frn[0][0], elements, f_null );
    std::uninitialized_fill_n( &lum[0][0], elements, 0 );
    std::uninitialized_fill_n( &trp[0][0], elements, tr_null );
    std::uninitialized_fill_n( &rad[0][0], elements, 0 );

    is_uniform = false;
}

submap::~submap() = default;

void submap::update_lum_rem( point p, const item &i )
{
    is_uniform = false;
    if( !i.is_emissive() ) {
        return;
    } else if( lum[p.x][p.y] && lum[p.x][p.y] < 255 ) {
        lum[p.x][p.y]--;
        return;
    }

    // Have to scan through all items to be sure removing i will actually lower
    // the count below 255.
    int count = 0;
    for( const auto &it : itm[p.x][p.y] ) {
        if( it->is_emissive() ) {
            count++;
        }
    }

    if( count <= 256 ) {
        lum[p.x][p.y] = static_cast<uint8_t>( count - 1 );
    }
}

void submap::insert_cosmetic( point p, const std::string &type, const std::string &str )
{
    cosmetic_t ins;

    ins.pos = p;
    ins.type = type;
    ins.str = str;

    cosmetics.push_back( ins );
}

static const std::string COSMETICS_GRAFFITI( "GRAFFITI" );
static const std::string COSMETICS_SIGNAGE( "SIGNAGE" );
// Handle GCC warning: 'warning: returning reference to temporary'
static const std::string STRING_EMPTY;

struct cosmetic_find_result {
    bool result;
    int ndx;
};
static cosmetic_find_result make_result( bool b, int ndx )
{
    cosmetic_find_result result;
    result.result = b;
    result.ndx = ndx;
    return result;
}
static cosmetic_find_result find_cosmetic(
    const std::vector<submap::cosmetic_t> &cosmetics, point p, const std::string &type )
{
    for( size_t i = 0; i < cosmetics.size(); ++i ) {
        if( cosmetics[i].pos == p && cosmetics[i].type == type ) {
            return make_result( true, i );
        }
    }
    return make_result( false, -1 );
}

bool submap::has_graffiti( point p ) const
{
    return find_cosmetic( cosmetics, p, COSMETICS_GRAFFITI ).result;
}

const std::string &submap::get_graffiti( point p ) const
{
    const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_GRAFFITI );
    if( fresult.result ) {
        return cosmetics[ fresult.ndx ].str;
    }
    return STRING_EMPTY;
}

void submap::set_graffiti( point p, const std::string &new_graffiti )
{
    is_uniform = false;
    // Find signage at p if available
    const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_GRAFFITI );
    if( fresult.result ) {
        cosmetics[ fresult.ndx ].str = new_graffiti;
    } else {
        insert_cosmetic( p, COSMETICS_GRAFFITI, new_graffiti );
    }
}

void submap::delete_graffiti( point p )
{
    is_uniform = false;
    const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_GRAFFITI );
    if( fresult.result ) {
        cosmetics[ fresult.ndx ] = cosmetics.back();
        cosmetics.pop_back();
    }
}
bool submap::has_signage( point p ) const
{
    if( frn[p.x][p.y].obj().has_flag( "SIGN" ) ) {
        return find_cosmetic( cosmetics, p, COSMETICS_SIGNAGE ).result;
    }

    return false;
}
std::string submap::get_signage( point p ) const
{
    if( frn[p.x][p.y].obj().has_flag( "SIGN" ) ) {
        const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_SIGNAGE );
        if( fresult.result ) {
            return cosmetics[ fresult.ndx ].str;
        }
    }

    return STRING_EMPTY;
}
void submap::set_signage( point p, const std::string &s )
{
    is_uniform = false;
    // Find signage at p if available
    const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_SIGNAGE );
    if( fresult.result ) {
        cosmetics[ fresult.ndx ].str = s;
    } else {
        insert_cosmetic( p, COSMETICS_SIGNAGE, s );
    }
}
void submap::delete_signage( point p )
{
    is_uniform = false;
    const auto fresult = find_cosmetic( cosmetics, p, COSMETICS_SIGNAGE );
    if( fresult.result ) {
        cosmetics[ fresult.ndx ] = cosmetics.back();
        cosmetics.pop_back();
    }
}

void submap::update_legacy_computer()
{
    if( legacy_computer ) {
        for( int x = 0; x < SEEX; ++x ) {
            for( int y = 0; y < SEEY; ++y ) {
                if( ter[x][y] == t_console ) {
                    computers.emplace( point( x, y ), *legacy_computer );
                }
            }
        }
        legacy_computer.reset();
    }
}

bool submap::has_computer( point p ) const
{
    return computers.contains( p ) || ( legacy_computer && ter[p.x][p.y] == t_console );
}

const computer *submap::get_computer( point p ) const
{
    // the returned object will not get modified (should not, at least), so we
    // don't yet need to update to std::map
    const auto it = computers.find( p );
    if( it != computers.end() ) {
        return &it->second;
    }
    if( legacy_computer && ter[p.x][p.y] == t_console ) {
        return legacy_computer.get();
    }
    return nullptr;
}

computer *submap::get_computer( point p )
{
    // need to update to std::map first so modifications to the returned object
    // only affects the exact point p
    update_legacy_computer();
    const auto it = computers.find( p );
    if( it != computers.end() ) {
        return &it->second;
    }
    return nullptr;
}

void submap::set_computer( point p, const computer &c )
{
    update_legacy_computer();
    const auto it = computers.find( p );
    if( it != computers.end() ) {
        it->second = c;
    } else {
        computers.emplace( p, c );
    }
}

void submap::delete_computer( point p )
{
    update_legacy_computer();
    computers.erase( p );
}

bool submap::contains_vehicle( vehicle *veh )
{
    const auto match = std::ranges::find_if(
                           vehicles,
    [veh]( const std::unique_ptr<vehicle> &v ) {
        return v.get() == veh;
    } );
    return match != vehicles.end();
}

void submap::rotate( int turns )
{
    turns = turns % 4;

    if( turns == 0 ) {
        return;
    }

    const auto rotate_point = [turns]( point  p ) {
        return p.rotate( turns, { SEEX, SEEY } );
    };

    if( turns == 2 ) {
        // Swap horizontal stripes.
        for( int j = 0, je = SEEY / 2; j < je; ++j ) {
            for( int i = j, ie = SEEX - j; i < ie; ++i ) {
                swap_soa_tile( { i, j }, rotate_point( { i, j } ) );
            }
        }
        // Swap vertical stripes so that they don't overlap with
        // the already swapped horizontals.
        for( int i = 0, ie = SEEX / 2; i < ie; ++i ) {
            for( int j = i + 1, je = SEEY - i - 1; j < je; ++j ) {
                swap_soa_tile( { i, j }, rotate_point( { i, j } ) );
            }
        }
    } else {
        for( int i = 0; i < SEEX / 2; i++ ) {
            for( int j = 0; j < SEEY / 2; j++ ) {

                /* We first number each of the four points as so:
                 * Clockwise            Anti-clockwise
                 *   12                     14
                 *   43                     23
                 * Then do a series of swaps:
                 *            Start
                 *   AB                     AB
                 *   CD                     CD
                 *           Swap 1 <-> 2
                 *   BA                     CB
                 *   CD                     AD
                 *           Swap 1 <-> 3
                 *   DA                     DB
                 *   CB                     AC
                 *           Swap 1 <-> 4
                 *   CA                     BD
                 *   DB                     AC
                 *   As you can see, this causes the desired rotation.
                 */

                point p1 = point( i, j );
                point p2 = rotate_point( p1 );
                point p3 = rotate_point( p2 );
                point p4 = rotate_point( p3 );

                swap_soa_tile( p1, p2 );
                swap_soa_tile( p1, p3 );
                swap_soa_tile( p1, p4 );
            }
        }
    }

    for( auto &elem : cosmetics ) {
        elem.pos = rotate_point( elem.pos );
    }

    for( auto &elem : spawns ) {
        elem.pos = rotate_point( elem.pos );
    }

    for( auto &elem : vehicles ) {
        const auto new_pos = rotate_point( elem->pos );

        elem->pos = new_pos;
        elem->set_facing( elem->turn_dir + turns * 90_degrees );
    }

    std::map<point, computer> rot_comp;
    for( auto &elem : computers ) {
        rot_comp.emplace( rotate_point( elem.first ), elem.second );
    }
    computers = rot_comp;

    std::map<point_sm_ms, cata::poly_serialized<active_tile_data>> rot_active_furn;
    for( auto &elem : active_furniture ) {
        rot_active_furn.emplace( point_sm_ms( rotate_point( elem.first.raw() ) ), elem.second );
    }
    active_furniture = rot_active_furn;

    std::map<point_sm_ms, time_point> rot_transformer_last_run;
    for( auto &elem : transformer_last_run ) {
        rot_transformer_last_run.emplace( point_sm_ms( rotate_point( elem.first.raw() ) ), elem.second );
    }
    transformer_last_run = rot_transformer_last_run;
}


auto submap::rebuild_outside_cache( const map &m, tripoint grid_pos ) -> void
{
    if( !outside_dirty ) {
        return;
    }
    // For each tile, mark inside (outside=false) if any tile in the 3×3
    // neighbourhood (including itself) has TFLAG_INDOORS on terrain or furniture.
    for( int sx = 0; sx < SEEX; ++sx ) {
        const int x = grid_pos.x * SEEX + sx;
        for( int sy = 0; sy < SEEY; ++sy ) {
            const int y = grid_pos.y * SEEY + sy;
            auto any_indoors = [&]() -> bool {
                for( int dx = -1; dx <= 1; ++dx )
                {
                    for( int dy = -1; dy <= 1; ++dy ) {
                        if( m.has_flag( TFLAG_INDOORS, tripoint( x + dx, y + dy, grid_pos.z ) ) ) {
                            return true;
                        }
                    }
                }
                return false;
            };
            outside_cache[sx][sy] = !any_indoors();
        }
    }
    outside_dirty = false;
}

auto submap::rebuild_floor_cache( const map &m, tripoint grid_pos ) -> void
{
    if( !floor_dirty ) {
        return;
    }
    // Default: has floor (non-zero).
    std::ranges::fill( std::span( &floor_cache[0][0], SEEX * SEEY ), '\x01' );

    const bool lowest_z = grid_pos.z <= -OVERMAP_DEPTH;
    const submap *below = lowest_z ? nullptr
                          : m.get_submap_at_grid( { grid_pos.x, grid_pos.y, grid_pos.z - 1 } );

    for( int sx = 0; sx < SEEX; ++sx ) {
        for( int sy = 0; sy < SEEY; ++sy ) {
            const point sp( sx, sy );
            const auto &ter_obj = get_ter( sp ).obj();
            if( ter_obj.has_flag( TFLAG_NO_FLOOR ) || ter_obj.has_flag( TFLAG_Z_TRANSPARENT ) ) {
                if( below && below->get_furn( sp ).obj().has_flag( TFLAG_SUN_ROOF_ABOVE ) ) {
                    continue;
                }
                floor_cache[sx][sy] = '\0';
            }
        }
    }
    floor_dirty = false;
}

auto submap::rebuild_pf_cache( const map &m, tripoint grid_pos ) -> void
{
    if( !pf_dirty ) {
        return;
    }
    for( int sx = 0; sx < SEEX; ++sx ) {
        for( int sy = 0; sy < SEEY; ++sy ) {
            const point sp( sx, sy );
            const tripoint p( grid_pos.x * SEEX + sx, grid_pos.y * SEEY + sy, grid_pos.z );
            auto cur_value = PF_NORMAL;

            const auto &terrain   = get_ter( sp ).obj();
            const auto &furniture = get_furn( sp ).obj();
            int vpart = -1;
            const vehicle *veh = m.veh_at_internal( p, vpart );
            const int cost = m.move_cost_internal( furniture, terrain, veh, vpart );

            if( cost > 2 ) {
                cur_value |= PF_SLOW;
            } else if( cost <= 0 ) {
                cur_value |= PF_WALL;
                if( terrain.has_flag( TFLAG_CLIMBABLE ) ) {
                    cur_value |= PF_CLIMBABLE;
                }
            }

            if( veh != nullptr ) {
                cur_value |= PF_VEHICLE;
            }

            for( const auto &fld : get_field( sp ) ) {
                const auto &cur_fld = fld.second;
                if( cur_fld.get_field_type().obj().get_dangerous(
                        cur_fld.get_field_intensity() - 1 ) ) {
                    cur_value |= PF_FIELD;
                }
            }

            if( !get_trap( sp ).obj().is_benign() || !terrain.trap.obj().is_benign() ) {
                cur_value |= PF_TRAP;
            }

            if( terrain.has_flag( TFLAG_GOES_DOWN ) || terrain.has_flag( TFLAG_GOES_UP ) ||
                terrain.has_flag( TFLAG_RAMP )      || terrain.has_flag( TFLAG_RAMP_UP ) ||
                terrain.has_flag( TFLAG_RAMP_DOWN ) ) {
                cur_value |= PF_UPDOWN;
            }

            if( terrain.has_flag( TFLAG_SHARP ) ) {
                cur_value |= PF_SHARP;
            }

            pf_special_cache[sx][sy] = cur_value;
        }
    }
    pf_dirty = false;
}

auto submap::rebuild_transparency_cache( const map &m, tripoint grid_pos ) -> void
{
    if( !transparency_dirty ) {
        return;
    }
    // outside_cache must be current before applying the weather sight penalty.
    if( outside_dirty ) {
        rebuild_outside_cache( m, grid_pos );
    }

    const float sight_penalty = get_weather().weather_id->sight_penalty;

    for( int sx = 0; sx < SEEX; ++sx ) {
        for( int sy = 0; sy < SEEY; ++sy ) {
            const point sp( sx, sy );

            if( !( get_ter( sp ).obj().transparent && get_furn( sp ).obj().transparent ) ) {
                transparency_cache[sx][sy] = LIGHT_TRANSPARENCY_SOLID;
                continue;
            }

            auto value = LIGHT_TRANSPARENCY_OPEN_AIR;
            if( outside_cache[sx][sy] ) {
                value *= sight_penalty;
            }

            for( const auto &fld : get_field( sp ) ) {
                if( !fld.first.is_valid() ) {
                    debugmsg( "rebuild_transparency_cache: invalid field type id %d at "
                              "grid(%d,%d,%d) tile(%d,%d) field_count=%d is_uniform=%d",
                              fld.first.to_i(), grid_pos.x, grid_pos.y, grid_pos.z,
                              sx, sy, field_count, static_cast<int>( is_uniform ) );
                    break;
                }
                const auto &cur = fld.second;
                if( !cur.is_transparent() ) {
                    value *= cur.translucency();
                }
            }

            transparency_cache[sx][sy] = value;
        }
    }
    transparency_dirty = false;
}

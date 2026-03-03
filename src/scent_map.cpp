#include "scent_map.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <span>

#include "assign.h"
#include "cached_options.h"
#include "calendar.h"
#include "color.h"
#include "cuboid_rectangle.h"
#include "cursesdef.h"
#include "debug.h"
#include "game.h"
#include "generic_factory.h"
#include "map.h"
#include "options.h"
#include "output.h"
#include "profile.h"
#include "string_id.h"
#include "submap.h"
#include "thread_pool.h"

static constexpr int SCENT_RADIUS = 40;

static nc_color sev( const size_t level )
{
    static const std::array<nc_color, 22> colors = { {
            c_cyan,
            c_light_cyan,
            c_light_blue,
            c_blue,
            c_light_green,
            c_green,
            c_yellow,
            c_pink,
            c_light_red,
            c_red,
            c_magenta,
            c_brown,
            c_cyan_red,
            c_light_cyan_red,
            c_light_blue_red,
            c_blue_red,
            c_light_green_red,
            c_green_red,
            c_yellow_red,
            c_pink_red,
            c_magenta_red,
            c_brown_red,
        }
    };
    return level < colors.size() ? colors[level] : c_dark_gray;
}

// ---------------------------------------------------------------------------
// Private helpers — direct per-submap access, no bounds check overhead.
// ---------------------------------------------------------------------------

auto scent_map::raw_scent_at( int x, int y, int z ) const -> int
{
    const auto *sm = m_.get_submap_at_grid( tripoint( x / SEEX, y / SEEY, z ) );
    return sm ? sm->scent_values[x % SEEX][y % SEEY] : 0;
}

auto scent_map::raw_scent_set( int x, int y, int z, int value ) -> void
{
    auto *sm = m_.get_submap_at_grid( tripoint( x / SEEX, y / SEEY, z ) );
    if( sm ) {
        sm->scent_values[x % SEEX][y % SEEY] = value;
    }
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void scent_map::reset()
{
    const int levz    = gm.get_levz();
    const int mapsize = m_.get_cache_ref( levz ).cache_x / SEEX;
    const int zmin    = levz - SCENT_MAP_Z_REACH;
    const int zmax    = levz + SCENT_MAP_Z_REACH;
    for( int z = zmin; z <= zmax; ++z ) {
        for( int smx = 0; smx < mapsize; ++smx ) {
            for( int smy = 0; smy < mapsize; ++smy ) {
                auto *sm = m_.get_submap_at_grid( { smx, smy, z } );
                if( !sm ) {
                    continue;
                }
                std::ranges::fill( std::span( &sm->scent_values[0][0], SEEX * SEEY ), 0 );
            }
        }
    }
    typescent = scenttype_id();
}

void scent_map::decay()
{
    ZoneScopedN( "scent_map::decay" );
    const int levz    = gm.get_levz();
    const int mapsize = m_.get_cache_ref( levz ).cache_x / SEEX;
    const int zmin    = levz - SCENT_MAP_Z_REACH;
    const int zmax    = levz + SCENT_MAP_Z_REACH;
    for( int z = zmin; z <= zmax; ++z ) {
        for( int smx = 0; smx < mapsize; ++smx ) {
            for( int smy = 0; smy < mapsize; ++smy ) {
                auto *sm = m_.get_submap_at_grid( { smx, smy, z } );
                if( !sm ) {
                    continue;
                }
                std::ranges::for_each( std::span( &sm->scent_values[0][0], SEEX * SEEY ),
                []( auto & v ) { v = std::max( 0, v - 1 ); } );
            }
        }
    }
}

void scent_map::draw( const catacurses::window &win, const int div, const tripoint &center ) const
{
    assert( div != 0 );
    const point max( getmaxx( win ), getmaxy( win ) );
    for( int x = 0; x < max.x; ++x ) {
        for( int y = 0; y < max.y; ++y ) {
            const int sn = get( center + point( -max.x / 2 + x, -max.y / 2 + y ) ) / div;
            mvwprintz( win, point( x, y ), sev( sn / 10 ), "%d", sn % 10 );
        }
    }
}

int scent_map::get( const tripoint &p ) const
{
    if( inbounds( p ) && raw_scent_at( p.x, p.y, p.z ) > 0 ) {
        return get_unsafe( p );
    }
    return 0;
}

void scent_map::set( const tripoint &p, int value, const scenttype_id &type )
{
    if( inbounds( p ) ) {
        set_unsafe( p, value, type );
    }
}

void scent_map::set_unsafe( const tripoint &p, int value, const scenttype_id &type )
{
    raw_scent_set( p.x, p.y, p.z, value );
    if( !type.is_empty() ) {
        typescent = type;
    }
}

int scent_map::get_unsafe( const tripoint &p ) const
{
    return raw_scent_at( p.x, p.y, p.z ) - std::abs( gm.get_levz() - p.z );
}

scenttype_id scent_map::get_type( const tripoint &p ) const
{
    scenttype_id id;
    if( inbounds( p ) && raw_scent_at( p.x, p.y, p.z ) > 0 ) {
        id = typescent;
    }
    return id;
}

bool scent_map::inbounds( const tripoint &p ) const
{
    // HACK: This weird long check here is a hack around the fact that scentmap is 2D
    // A z-level can access scentmap if it is within SCENT_MAP_Z_REACH flying z-level move from player's z-level
    // That is, if a flying critter could move directly up or down (or stand still) and be on same z-level as player
    const int levz = gm.get_levz();
    const bool scent_map_z_level_inbounds = ( p.z == levz ) ||
                                            ( std::abs( p.z - levz ) == SCENT_MAP_Z_REACH &&
                                                    get_map().valid_move( p, tripoint( p.xy(), levz ), false, true ) );
    if( !scent_map_z_level_inbounds ) {
        return false;
    }
    return m_.inbounds( p );
}

void scent_map::update( const tripoint &center, map &m )
{
    ZoneScoped;
    // Stop updating scent after X turns of the player not moving.
    // Once wind is added, need to reset this on wind shifts as well.
    if( !player_last_position || center != *player_last_position ) {
        player_last_position.emplace( center );
        player_last_moved = calendar::turn;
    } else if( player_last_moved + 1000_turns < calendar::turn ) {
        return;
    }

    //the block and reduce scent properties are folded into a single scent_transfer value here
    //block=0 reduce=1 normal=5
    auto &_scent_lc = m.access_cache( center.z );
    const int st_sy = _scent_lc.cache_y;
    auto scent_transfer = std::vector<char>( static_cast<size_t>( _scent_lc.cache_x ) * st_sy, 0 );
    const auto *blocked_data = _scent_lc.vehicle_obstructed_cache.data();

    std::array < std::array < int, 3 + SCENT_RADIUS * 2 >, 1 + SCENT_RADIUS * 2 > new_scent;
    std::array < std::array < int, 3 + SCENT_RADIUS * 2 >, 1 + SCENT_RADIUS * 2 > sum_3_scent_y;
    std::array < std::array < char, 3 + SCENT_RADIUS * 2 >, 1 + SCENT_RADIUS * 2 > squares_used_y;

    // for loop constants
    const int scentmap_minx = center.x - SCENT_RADIUS;
    const int scentmap_maxx = center.x + SCENT_RADIUS;
    const int scentmap_miny = center.y - SCENT_RADIUS;
    const int scentmap_maxy = center.y + SCENT_RADIUS;

    // The new scent flag searching function. Should be wayyy faster than the old one.
    m.scent_blockers( scent_transfer, st_sy, point( scentmap_minx - 1, scentmap_miny - 1 ),
                      point( scentmap_maxx + 1, scentmap_maxy + 1 ) );

    for( int x = 0; x < SCENT_RADIUS * 2 + 3; ++x ) {
        sum_3_scent_y[0][x] = 0;
        squares_used_y[0][x] = 0;
        sum_3_scent_y[SCENT_RADIUS * 2][x] = 0;
        squares_used_y[SCENT_RADIUS * 2][x] = 0;
    }

    const bool parallel_scent = parallel_enabled && parallel_scent_update;
    const int cz = center.z;

    // Y-pass: each x column is independent — no shared writes.
    if( parallel_scent ) {
        parallel_for( 0, SCENT_RADIUS * 2 + 3, [&]( int x ) {
            for( int y = 0; y < SCENT_RADIUS * 2 + 1; ++y ) {

                point abs( x + scentmap_minx - 1, y + scentmap_miny );

                // remember the sum of the scent val for the 3 neighboring squares that can defuse into
                sum_3_scent_y[y][x] = 0;
                squares_used_y[y][x] = 0;
                for( int i = abs.y - 1; i <= abs.y + 1; ++i ) {
                    sum_3_scent_y[y][x] += scent_transfer[abs.x * st_sy + i] * raw_scent_at( abs.x, i, cz );
                    squares_used_y[y][x] += scent_transfer[abs.x * st_sy + i];
                }
            }
        } );
    } else {
        for( int x = 0; x < SCENT_RADIUS * 2 + 3; ++x ) {
            for( int y = 0; y < SCENT_RADIUS * 2 + 1; ++y ) {

                point abs( x + scentmap_minx - 1, y + scentmap_miny );

                // remember the sum of the scent val for the 3 neighboring squares that can defuse into
                sum_3_scent_y[y][x] = 0;
                squares_used_y[y][x] = 0;
                for( int i = abs.y - 1; i <= abs.y + 1; ++i ) {
                    sum_3_scent_y[y][x] += scent_transfer[abs.x * st_sy + i] * raw_scent_at( abs.x, i, cz );
                    squares_used_y[y][x] += scent_transfer[abs.x * st_sy + i];
                }
            }
        }
    }
    // implicit barrier at end of parallel_for; sum_3_scent_y is fully populated

    // X-pass: reads sum_3_scent_y (now complete and read-only), writes new_scent[y][x].
    // Each output column x is independent.
    if( parallel_scent ) {
        parallel_for( 1, SCENT_RADIUS * 2 + 2, [&]( int x ) {
            for( int y = 0; y < SCENT_RADIUS * 2 + 1; ++y ) {
                const point abs( x + scentmap_minx - 1, y + scentmap_miny );

                int squares_used = squares_used_y[y][x - 1] + squares_used_y[y][x] + squares_used_y[y][x + 1];
                int total = sum_3_scent_y[y][x - 1] + sum_3_scent_y[y][x] + sum_3_scent_y[y][x + 1];

                //handle vehicle holes
                if( blocked_data[abs.x * st_sy + abs.y].nw &&
                    scent_transfer[( abs.x + 1 ) * st_sy + abs.y + 1] == 5 ) {
                    squares_used -= 4;
                    total -= 4 * raw_scent_at( abs.x + 1, abs.y + 1, cz );
                }
                if( blocked_data[abs.x * st_sy + abs.y].ne &&
                    scent_transfer[( abs.x - 1 ) * st_sy + abs.y + 1] == 5 ) {
                    squares_used -= 4;
                    total -= 4 * raw_scent_at( abs.x - 1, abs.y + 1, cz );
                }
                if( blocked_data[( abs.x - 1 ) * st_sy + abs.y - 1].nw &&
                    scent_transfer[( abs.x - 1 ) * st_sy + abs.y - 1] == 5 ) {
                    squares_used -= 4;
                    total -= 4 * raw_scent_at( abs.x - 1, abs.y - 1, cz );
                }
                if( blocked_data[( abs.x + 1 ) * st_sy + abs.y - 1].ne &&
                    scent_transfer[( abs.x + 1 ) * st_sy + abs.y - 1] == 5 ) {
                    squares_used -= 4;
                    total -= 4 * raw_scent_at( abs.x + 1, abs.y - 1, cz );
                }

                //Lingering scent
                const int cur = raw_scent_at( abs.x, abs.y, cz );
                int temp_scent = cur * ( 250 - squares_used * scent_transfer[abs.x * st_sy + abs.y] );
                temp_scent -= cur * scent_transfer[abs.x * st_sy + abs.y] * ( 45 - squares_used ) / 5;

                new_scent[y][x] = ( temp_scent + total * scent_transfer[abs.x * st_sy + abs.y] ) / 250;
            }
        } );
    } else {
        for( int x = 1; x < SCENT_RADIUS * 2 + 2; ++x ) {
            for( int y = 0; y < SCENT_RADIUS * 2 + 1; ++y ) {
                const point abs( x + scentmap_minx - 1, y + scentmap_miny );

                int squares_used = squares_used_y[y][x - 1] + squares_used_y[y][x] + squares_used_y[y][x + 1];
                int total = sum_3_scent_y[y][x - 1] + sum_3_scent_y[y][x] + sum_3_scent_y[y][x + 1];

                //handle vehicle holes
                if( blocked_data[abs.x * st_sy + abs.y].nw &&
                    scent_transfer[( abs.x + 1 ) * st_sy + abs.y + 1] == 5 ) {
                    squares_used -= 4;
                    total -= 4 * raw_scent_at( abs.x + 1, abs.y + 1, cz );
                }
                if( blocked_data[abs.x * st_sy + abs.y].ne &&
                    scent_transfer[( abs.x - 1 ) * st_sy + abs.y + 1] == 5 ) {
                    squares_used -= 4;
                    total -= 4 * raw_scent_at( abs.x - 1, abs.y + 1, cz );
                }
                if( blocked_data[( abs.x - 1 ) * st_sy + abs.y - 1].nw &&
                    scent_transfer[( abs.x - 1 ) * st_sy + abs.y - 1] == 5 ) {
                    squares_used -= 4;
                    total -= 4 * raw_scent_at( abs.x - 1, abs.y - 1, cz );
                }
                if( blocked_data[( abs.x + 1 ) * st_sy + abs.y - 1].ne &&
                    scent_transfer[( abs.x + 1 ) * st_sy + abs.y - 1] == 5 ) {
                    squares_used -= 4;
                    total -= 4 * raw_scent_at( abs.x + 1, abs.y - 1, cz );
                }

                //Lingering scent
                const int cur = raw_scent_at( abs.x, abs.y, cz );
                int temp_scent = cur * ( 250 - squares_used * scent_transfer[abs.x * st_sy + abs.y] );
                temp_scent -= cur * scent_transfer[abs.x * st_sy + abs.y] * ( 45 - squares_used ) / 5;

                new_scent[y][x] = ( temp_scent + total * scent_transfer[abs.x * st_sy + abs.y] ) / 250;
            }
        }
    }
    // implicit barrier; new_scent is fully populated
    // Write-back: new_scent is read-only here; has_flag is a read-only map query;
    // each x column writes to a distinct scent column — safe to parallelize.
    if( parallel_scent ) {
        parallel_for( 1, SCENT_RADIUS * 2 + 2, [&]( int x ) {
            for( int y = 0; y < SCENT_RADIUS * 2 + 1; ++y ) {
                // Don't spread scent into water unless the source is in water.
                // Keep scent trails in the water when we exit until rain disturbs them.
                if( ( get_map().has_flag( TFLAG_LIQUID, center ) &&
                      rl_dist( center, tripoint( point( x + scentmap_minx - 1, y + scentmap_miny ),
                                                 g->get_levz() ) ) <= 8 ) ||
                    !get_map().has_flag( TFLAG_LIQUID, point( x + scentmap_minx - 1, y + scentmap_miny ) ) ) {
                    raw_scent_set( x + scentmap_minx - 1, y + scentmap_miny, cz, new_scent[y][x] );
                }
            }
        } );
    } else {
        for( int x = 1; x < SCENT_RADIUS * 2 + 2; ++x ) {
            for( int y = 0; y < SCENT_RADIUS * 2 + 1; ++y ) {
                // Don't spread scent into water unless the source is in water.
                // Keep scent trails in the water when we exit until rain disturbs them.
                if( ( get_map().has_flag( TFLAG_LIQUID, center ) &&
                      rl_dist( center, tripoint( point( x + scentmap_minx - 1, y + scentmap_miny ),
                                                 g->get_levz() ) ) <= 8 ) ||
                    !get_map().has_flag( TFLAG_LIQUID, point( x + scentmap_minx - 1, y + scentmap_miny ) ) ) {
                    raw_scent_set( x + scentmap_minx - 1, y + scentmap_miny, cz, new_scent[y][x] );
                }
            }
        }
    }
}

std::string scent_map::serialize( bool is_type ) const
{
    // Scent values now live on per-submap arrays and are not serialized.
    // Only typescent is retained for backward compatibility.
    if( is_type ) {
        return typescent.str();
    }
    return {};
}

void scent_map::deserialize( const std::string &data, bool is_type )
{
    // Scent values now live on per-submap arrays; old flat-array data is discarded.
    // Only typescent is loaded for backward compatibility.
    if( is_type && !data.empty() ) {
        typescent = scenttype_id( data );
    }
}

namespace
{
generic_factory<scent_type> scent_factory( "scent_type" );
} // namespace

template<>
const scent_type &string_id<scent_type>::obj() const
{
    return scent_factory.obj( *this );
}

template<>
bool string_id<scent_type>::is_valid() const
{
    return scent_factory.is_valid( *this );
}

void scent_type::load_scent_type( const JsonObject &jo, const std::string &src )
{
    scent_factory.load( jo, src );
}

void scent_type::load( const JsonObject &jo, const std::string & )
{
    assign( jo, "id", id );
    assign( jo, "receptive_species", receptive_species );
}

const std::vector<scent_type> &scent_type::get_all()
{
    return scent_factory.get_all();
}

void scent_type::check_scent_consistency()
{
    for( const scent_type &styp : get_all() ) {
        for( const species_id &spe : styp.receptive_species ) {
            if( !spe.is_valid() ) {
                debugmsg( "scent_type %s has invalid species_id %s in receptive_species", styp.id.c_str(),
                          spe.c_str() );
            }
        }
    }
}

void scent_type::reset()
{
    scent_factory.reset();
}

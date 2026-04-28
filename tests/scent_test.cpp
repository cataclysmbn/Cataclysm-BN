
#include "scent_map.h"
#include "catch/catch.hpp"
#include "map.h"
#include "map_helpers.h"
#include "game.h"
#include "state_helpers.h"

void old_scent_map_update( const tripoint &center, map &m,
                           std::array<std::array<int, MAPSIZE_Y>, MAPSIZE_X> &grscent );

static constexpr int SCENT_RADIUS = 40;
void old_scent_map_update( const tripoint &center, map &m,
                           std::array<std::array<int, MAPSIZE_Y>, MAPSIZE_X> &grscent )
{

    // note: the next four intermediate matrices need to be at least
    // [2*SCENT_RADIUS+3][2*SCENT_RADIUS+1] in size to hold enough data
    // The code I'm modifying used [MAPSIZE_X]. I'm staying with that to avoid new bugs.

    // These two matrices are transposed so that x addresses are contiguous in memory
    std::array<std::array<int, MAPSIZE_Y>, MAPSIZE_X> sum_3_scent_y;
    std::array<std::array<int, MAPSIZE_Y>, MAPSIZE_X> squares_used_y;

    // these are for caching flag lookups
    std::array<std::array<bool, MAPSIZE_Y>, MAPSIZE_X>
    blocks_scent; // currently only TFLAG_NO_SCENT blocks scent
    std::array<std::array<bool, MAPSIZE_Y>, MAPSIZE_X> reduces_scent;


    std::vector<char> monkey( MAPSIZE_X * MAPSIZE_Y, 0 );

    // for loop constants
    const int scentmap_minx = center.x - SCENT_RADIUS;
    const int scentmap_maxx = center.x + SCENT_RADIUS;
    const int scentmap_miny = center.y - SCENT_RADIUS;
    const int scentmap_maxy = center.y + SCENT_RADIUS;

    // decrease this to reduce gas spread. Keep it under 125 for
    // stability. This is essentially a decimal number * 1000.
    const int diffusivity = 100;

    // The new scent flag searching function. Should be wayyy faster than the old one.
    m.scent_blockers( monkey, MAPSIZE_Y, point_bub_ms( scentmap_minx - 1, scentmap_miny - 1 ),
                      point_bub_ms( scentmap_maxx + 1, scentmap_maxy + 1 ) );

    for( int x = 0; x < MAPSIZE_X; x++ ) {
        for( int y = 0; y < MAPSIZE_Y; y++ ) {
            if( monkey[x * MAPSIZE_Y + y] == 0 ) {
                blocks_scent[x][y] = true;
                reduces_scent[x][y] = false;
            } else if( monkey[x * MAPSIZE_Y + y] == 1 ) {
                blocks_scent[x][y] = false;
                reduces_scent[x][y] = true;
            } else {
                blocks_scent[x][y] = false;
                reduces_scent[x][y] = false;
            }
        }
    }
    // Sum neighbors in the y direction.  This way, each square gets called 3 times instead of 9
    // times. This cost us an extra loop here, but it also eliminated a loop at the end, so there
    // is a net performance improvement over the old code. Could probably still be better.
    // note: this method needs an array that is one square larger on each side in the x direction
    // than the final scent matrix. I think this is fine since SCENT_RADIUS is less than
    // MAPSIZE_X, but if that changes, this may need tweaking.
    for( int x = scentmap_minx - 1; x <= scentmap_maxx + 1; ++x ) {
        for( int y = scentmap_miny; y <= scentmap_maxy; ++y ) {
            // remember the sum of the scent val for the 3 neighboring squares that can defuse into
            sum_3_scent_y[y][x] = 0;
            squares_used_y[y][x] = 0;
            for( int i = y - 1; i <= y + 1; ++i ) {
                if( !blocks_scent[x][i] ) {
                    if( reduces_scent[x][i] ) {
                        // only 20% of scent can diffuse on REDUCE_SCENT squares
                        sum_3_scent_y[y][x] += 2 * grscent[x][i];
                        squares_used_y[y][x] += 2;
                    } else {
                        sum_3_scent_y[y][x] += 10 * grscent[x][i];
                        squares_used_y[y][x] += 10;
                    }
                }
            }
        }
    }

    // Rest of the scent map
    for( int x = scentmap_minx; x <= scentmap_maxx; ++x ) {
        for( int y = scentmap_miny; y <= scentmap_maxy; ++y ) {
            int &scent_here = grscent[x][y];
            if( !blocks_scent[x][y] ) {
                // to how many neighboring squares do we diffuse out? (include our own square
                // since we also include our own square when diffusing in)
                const int squares_used = squares_used_y[y][x - 1]
                                         + squares_used_y[y][x]
                                         + squares_used_y[y][x + 1];

                int this_diffusivity;
                if( !reduces_scent[x][y] ) {
                    this_diffusivity = diffusivity;
                } else {
                    this_diffusivity = diffusivity / 5; //less air movement for REDUCE_SCENT square
                }
                // take the old scent and subtract what diffuses out
                int temp_scent = scent_here * ( 10 * 1000 - squares_used * this_diffusivity );
                // neighboring REDUCE_SCENT squares absorb some scent
                temp_scent -= scent_here * this_diffusivity * ( 90 - squares_used ) / 5;

                // we've already summed neighboring scent values in the y direction in the previous
                // loop. Now we do it for the x direction, multiply by diffusion, and this is what
                // diffuses into our current square.
                scent_here =
                    ( temp_scent
                      + this_diffusivity * ( sum_3_scent_y[y][x - 1]
                                             + sum_3_scent_y[y][x]
                                             + sum_3_scent_y[y][x + 1] )
                    ) / ( 1000 * 10 );
            } else {
                // this cell blocks scent via NO_SCENT (in json)
                scent_here = 0;
            }
        }
    }
}

TEST_CASE( "scent_matches_old", "[.]" )
{
    clear_all_state();
    tripoint origin( 60, 60, 0 );

    g->place_player( origin );

    map &here = get_map();

    here.ter_set( origin + tripoint_south_west, t_brick_wall );
    here.ter_set( origin + tripoint_west, t_brick_wall );
    here.ter_set( origin + tripoint_north, t_rock_wall_half );
    here.ter_set( origin, t_rock_wall_half );
    g->scent.reset();

    g->scent.set( origin, 1000, scenttype_id( "sc_human" ) );

    g->scent.update( origin, here );
    g->scent.update( origin, here );
    g->scent.update( origin, here );

    std::array<std::array<int, MAPSIZE_Y>, MAPSIZE_X> old_scent;
    for( auto &elem : old_scent ) {
        for( auto &val : elem ) {
            val = 0;
        }
    }

    old_scent[origin.x][origin.y] = 1000;

    old_scent_map_update( origin, here, old_scent );
    old_scent_map_update( origin, here, old_scent );
    old_scent_map_update( origin, here, old_scent );
    int x = 0;
    for( auto &elem : old_scent ) {
        int y = 0;
        for( auto &val : elem ) {

            INFO( x );
            INFO( y );
            CHECK( val == g->scent.get( {x, y, 0} ) );
            y++;
        }
        x++;
    }
}


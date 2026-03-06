#include "catch/catch.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include <vector>

#include "game_constants.h"
#include "lightmap.h"
#include "line.h" // For rl_dist.
#include "map.h"
#include "point.h"
#include "rng.h"
#include "shadowcasting.h"
#include "state_helpers.h"
#include "string_formatter.h"

// Constants setting the ratio of set to unset tiles.
constexpr unsigned int NUMERATOR = 1;
constexpr unsigned int DENOMINATOR = 10;

// NOLINTNEXTLINE(cata-xy)
static void oldCastLight( float ( &output_cache )[MAPSIZE * SEEX][MAPSIZE * SEEY],
                          const float ( &input_array )[MAPSIZE * SEEX][MAPSIZE * SEEY],
                          const int xx, const int xy, const int yx, const int yy,
                          const int offsetX, const int offsetY, const int offsetDistance,
                          const int row = 1, float start = 1.0f, const float end = 0.0f )
{

    float newStart = 0.0f;
    const float radius = 60.0f - offsetDistance;
    if( start < end ) {
        return;
    }
    bool blocked = false;
    tripoint delta;
    for( int distance = row; distance <= radius && !blocked; distance++ ) {
        delta.y = -distance;
        for( delta.x = -distance; delta.x <= 0; delta.x++ ) {
            const point current( offsetX + delta.x * xx + delta.y * xy, offsetY + delta.x * yx + delta.y * yy );
            const float leftSlope = ( delta.x - 0.5f ) / ( delta.y + 0.5f );
            const float rightSlope = ( delta.x + 0.5f ) / ( delta.y - 0.5f );

            if( start < rightSlope ) {
                continue;
            } else if( end > leftSlope ) {
                break;
            }

            //check if it's within the visible area and mark visible if so
            if( rl_dist( tripoint_zero, delta ) <= radius ) {
                output_cache[current.x][current.y] = VISIBILITY_FULL;
            }

            if( blocked ) {
                //previous cell was a blocking one
                if( input_array[current.x][current.y] == LIGHT_TRANSPARENCY_SOLID ) {
                    //hit a wall
                    newStart = rightSlope;
                } else {
                    blocked = false;
                    start = newStart;
                }
            } else {
                if( input_array[current.x][current.y] == LIGHT_TRANSPARENCY_SOLID &&
                    distance < radius ) {
                    //hit a wall within sight line
                    blocked = true;
                    oldCastLight( output_cache, input_array, xx, xy, yx, yy,
                                  offsetX, offsetY, offsetDistance,  distance + 1, start, leftSlope );
                    newStart = rightSlope;
                }
            }
        }
    }
}

/*
 * This is checking whether bresenham visibility checks match shadowcasting (they don't).
 */
static bool bresenham_visibility_check(
    const point &offset, const point &p,
    const float ( &transparency_cache )[MAPSIZE * SEEX][MAPSIZE * SEEY] )
{
    if( offset == p ) {
        return true;
    }
    bool visible = true;
    const int junk = 0;
    bresenham( p, offset, junk,
    [&transparency_cache, &visible]( const point & new_point ) {
        if( transparency_cache[new_point.x][new_point.y] <=
            LIGHT_TRANSPARENCY_SOLID ) {
            visible = false;
            return false;
        }
        return true;
    } );
    return visible;
}

static void randomly_fill_transparency(
    float ( &transparency_cache )[MAPSIZE * SEEX][MAPSIZE * SEEY],
    const unsigned int numerator = NUMERATOR, const unsigned int denominator = DENOMINATOR )
{
    // Construct a rng that produces integers in a range selected to provide the probability
    // we want, i.e. if we want 1/4 tiles to be set, produce numbers in the range 0-3,
    // with 0 indicating the bit is set.
    std::uniform_int_distribution<unsigned int> distribution( 0, denominator );
    auto rng = std::bind( distribution, rng_get_engine() );

    // Initialize the transparency value of each square to a random value.
    for( auto &inner : transparency_cache ) {
        for( float &square : inner ) {
            if( rng() < numerator ) {
                square = LIGHT_TRANSPARENCY_SOLID;
            } else {
                square = LIGHT_TRANSPARENCY_OPEN_AIR;
            }
        }
    }
}

static bool is_nonzero( const float x )
{
    return x != 0;
}

static bool is_nonzero( const four_quadrants &x )
{
    return is_nonzero( x.max() );
}

template<typename Exp>
bool grids_are_equivalent( float control[MAPSIZE * SEEX][MAPSIZE * SEEY],
                           Exp experiment[MAPSIZE * SEEX][MAPSIZE * SEEY] )
{
    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
            // Check that both agree on the outcome, but not necessarily the same values.
            if( is_nonzero( control[x][y] ) != is_nonzero( experiment[x][y] ) ) {
                return false;
            }
        }
    }

    return true;
}

template<typename Exp>
void print_grid_comparison( const point &offset,
                            float ( &transparency_cache )[MAPSIZE * SEEX][MAPSIZE * SEEY],
                            float control[MAPSIZE * SEEX][MAPSIZE * SEEY],
                            Exp experiment[MAPSIZE * SEEX][MAPSIZE * SEEY] )
{
    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEX; ++y ) {
            char output = ' ';
            const bool shadowcasting_disagrees =
                is_nonzero( control[x][y] ) != is_nonzero( experiment[x][y] );
            const bool bresenham_disagrees =
                bresenham_visibility_check( offset, point( x, y ), transparency_cache ) !=
                is_nonzero( experiment[x][y] );

            if( shadowcasting_disagrees && bresenham_disagrees ) {
                if( is_nonzero( experiment[x][y] ) ) {
                    output = 'R'; // Old shadowcasting and bresenham can't see.
                } else {
                    output = 'N'; // New shadowcasting can't see.
                }
            } else if( shadowcasting_disagrees ) {
                if( is_nonzero( control[x][y] ) ) {
                    output = 'C'; // New shadowcasting & bresenham can't see.
                } else {
                    output = 'O'; // Old shadowcasting can't see.
                }
            } else if( bresenham_disagrees ) {
                if( is_nonzero( experiment[x][y] ) ) {
                    output = 'B'; // Bresenham can't see it.
                } else {
                    output = 'S'; // Shadowcasting can't see it.
                }
            }
            if( transparency_cache[x][y] == LIGHT_TRANSPARENCY_SOLID ) {
                output = '#';
            }
            if( x == offset.x && y == offset.y ) {
                output = '@';
            }
            cata_printf( "%c", output );
        }
        cata_printf( "\n" );
    }
    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEX; ++y ) {
            char output = ' ';
            if( transparency_cache[x][y] == LIGHT_TRANSPARENCY_SOLID ) {
                output = '#';
            } else if( control[x][y] > LIGHT_TRANSPARENCY_SOLID ) {
                output = 'X';
            }
            cata_printf( "%c", output );
        }
        cata_printf( "    " );
        for( int y = 0; y < MAPSIZE * SEEX; ++y ) {
            char output = ' ';
            if( transparency_cache[x][y] == LIGHT_TRANSPARENCY_SOLID ) {
                output = '#';
            } else if( is_nonzero( experiment[x][y] ) ) {
                output = 'X';
            }
            cata_printf( "%c", output );
        }
        cata_printf( "\n" );
    }
}

// Sight model for all shadowcasting test cases.  Both update_float and
// update_quadrants are non-null so this model works with castLightAll (float)
// and castLightAll_q (four_quadrants) alike.
static const light_model k_sight_model = {
    sight_calc, sight_check, update_light, update_light_quadrants, sight_from_lookup,
    accumulate_transparency
};

static void shadowcasting_runoff( const int iterations, const bool test_bresenham = false )
{
    // Static to avoid stack overflow: MAPSIZE*SEEX x MAPSIZE*SEEY = 420x420 floats (~689 KB each).
    static float seen_squares_control[MAPSIZE * SEEX][MAPSIZE * SEEY];
    static float seen_squares_experiment[MAPSIZE * SEEX][MAPSIZE * SEEY];
    static float transparency_cache[MAPSIZE * SEEX][MAPSIZE * SEEY];
    static diagonal_blocks blocked_cache[MAPSIZE * SEEX][MAPSIZE * SEEY];

    // Result arrays accumulate light; must be zeroed before each run.
    std::fill_n( &seen_squares_control[0][0], MAPSIZE * SEEX * MAPSIZE * SEEY, 0.0f );
    std::fill_n( &seen_squares_experiment[0][0], MAPSIZE * SEEX * MAPSIZE * SEEY, 0.0f );
    // transparency_cache fully overwritten by randomly_fill_transparency below.
    // blocked_cache fully overwritten by uninitialized_fill_n below.

    diagonal_blocks fill = {false, false};
    std::uninitialized_fill_n( &blocked_cache[0][0], MAPSIZE * SEEX * MAPSIZE * SEEY, fill );

    randomly_fill_transparency( transparency_cache );

    map dummy;

    const point offset( 65, 65 );

    const auto start1 = std::chrono::high_resolution_clock::now();
    for( int i = 0; i < iterations; i++ ) {
        // First the control algorithm.
        oldCastLight( seen_squares_control, transparency_cache, 0, 1, 1, 0, offset.x, offset.y, 0 );
        oldCastLight( seen_squares_control, transparency_cache, 1, 0, 0, 1, offset.x, offset.y, 0 );

        oldCastLight( seen_squares_control, transparency_cache, 0, -1, 1, 0, offset.x, offset.y, 0 );
        oldCastLight( seen_squares_control, transparency_cache, -1, 0, 0, 1, offset.x, offset.y, 0 );

        oldCastLight( seen_squares_control, transparency_cache, 0, 1, -1, 0, offset.x, offset.y, 0 );
        oldCastLight( seen_squares_control, transparency_cache, 1, 0, 0, -1, offset.x, offset.y, 0 );

        oldCastLight( seen_squares_control, transparency_cache, 0, -1, -1, 0, offset.x, offset.y, 0 );
        oldCastLight( seen_squares_control, transparency_cache, -1, 0, 0, -1, offset.x, offset.y, 0 );
    }
    const auto end1 = std::chrono::high_resolution_clock::now();

    const auto start2 = std::chrono::high_resolution_clock::now();
    for( int i = 0; i < iterations; i++ ) {
        // Then the current algorithm.
        castLightAll( &seen_squares_experiment[0][0], &transparency_cache[0][0],
                      &blocked_cache[0][0], MAPSIZE * SEEX, MAPSIZE * SEEY,
                      offset, 0, VISIBILITY_FULL, k_sight_model );
    }
    const auto end2 = std::chrono::high_resolution_clock::now();

    if( iterations > 1 ) {
        const long long diff1 = std::chrono::duration_cast<std::chrono::microseconds>
                                ( end1 - start1 ).count();
        const long long diff2 = std::chrono::duration_cast<std::chrono::microseconds>
                                ( end2 - start2 ).count();
        cata_printf( "oldCastLight() executed %d times in %lld microseconds.\n",
                     iterations, diff1 );
        cata_printf( "castLight() executed %d times in %lld microseconds.\n",
                     iterations, diff2 );
    }

    bool passed = grids_are_equivalent( seen_squares_control, seen_squares_experiment );
    for( int x = 0; test_bresenham && passed && x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEX; ++y ) {
            // Check that both agree on the outcome, but not necessarily the same values.
            if( bresenham_visibility_check( offset, point( x, y ), transparency_cache ) !=
                ( seen_squares_experiment[x][y] > LIGHT_TRANSPARENCY_SOLID ) ) {
                passed = false;
                break;
            }
        }
    }

    if( !passed ) {
        print_grid_comparison( offset, transparency_cache, seen_squares_control,
                               seen_squares_experiment );
    }

    REQUIRE( passed );
}

static void shadowcasting_float_quad(
    const int iterations, const unsigned int denominator = DENOMINATOR )
{
    // Static to avoid stack overflow: MAPSIZE*SEEX x MAPSIZE*SEEY arrays (~689 KB each).
    static float lit_squares_float[MAPSIZE * SEEX][MAPSIZE * SEEY];
    static four_quadrants lit_squares_quad[MAPSIZE * SEEX][MAPSIZE * SEEY];
    static float transparency_cache[MAPSIZE * SEEX][MAPSIZE * SEEY];
    static diagonal_blocks blocked_cache[MAPSIZE * SEEX][MAPSIZE * SEEY];

    // Result arrays accumulate light; must be zeroed before each run.
    std::fill_n( &lit_squares_float[0][0], MAPSIZE * SEEX * MAPSIZE * SEEY, 0.0f );
    std::fill_n( &lit_squares_quad[0][0], MAPSIZE * SEEX * MAPSIZE * SEEY, four_quadrants{} );
    // transparency_cache fully overwritten by randomly_fill_transparency below.
    // blocked_cache fully overwritten by uninitialized_fill_n below.

    diagonal_blocks fill = {false, false};
    std::uninitialized_fill_n( &blocked_cache[0][0], MAPSIZE * SEEX * MAPSIZE * SEEY, fill );

    randomly_fill_transparency( transparency_cache, denominator );

    map dummy;

    const point offset( 65, 65 );

    const auto start1 = std::chrono::high_resolution_clock::now();
    for( int i = 0; i < iterations; i++ ) {
        castLightAll_q( &lit_squares_quad[0][0], &transparency_cache[0][0],
                        &blocked_cache[0][0], MAPSIZE * SEEX, MAPSIZE * SEEY,
                        offset, 0, VISIBILITY_FULL, k_sight_model );
    }
    const auto end1 = std::chrono::high_resolution_clock::now();

    const auto start2 = std::chrono::high_resolution_clock::now();
    for( int i = 0; i < iterations; i++ ) {
        // Then the current algorithm.
        castLightAll( &lit_squares_float[0][0], &transparency_cache[0][0],
                      &blocked_cache[0][0], MAPSIZE * SEEX, MAPSIZE * SEEY,
                      offset, 0, VISIBILITY_FULL, k_sight_model );
    }
    const auto end2 = std::chrono::high_resolution_clock::now();

    if( iterations > 1 ) {
        const long long diff1 = std::chrono::duration_cast<std::chrono::microseconds>
                                ( end1 - start1 ).count();
        const long long diff2 = std::chrono::duration_cast<std::chrono::microseconds>
                                ( end2 - start2 ).count();
        cata_printf( "castLight on four_quadrants (denominator %u) "
                     "executed %d times in %lld microseconds.\n",
                     denominator, iterations, diff1 );
        cata_printf( "castLight on floats (denominator %u) "
                     "executed %d times in %lld microseconds.\n",
                     denominator, iterations, diff2 );
    }

    bool passed = grids_are_equivalent( lit_squares_float, lit_squares_quad );

    if( !passed ) {
        print_grid_comparison( offset, transparency_cache, lit_squares_float,
                               lit_squares_quad );
    }

    REQUIRE( passed );
}

static void shadowcasting_3d_2d( const int iterations )
{
    // Static to avoid stack overflow: MAPSIZE*SEEX x MAPSIZE*SEEY arrays (~689 KB each).
    static float seen_squares_control[MAPSIZE * SEEX][MAPSIZE * SEEY];
    static float seen_squares_experiment[MAPSIZE * SEEX][MAPSIZE * SEEY];
    static float transparency_cache[MAPSIZE * SEEX][MAPSIZE * SEEY];
    static bool floor_cache[MAPSIZE * SEEX][MAPSIZE *
                                            SEEY]; // zero-initialized once; never written by algorithms.
    static diagonal_blocks blocked_cache[MAPSIZE * SEEX][MAPSIZE * SEEY];

    // Result arrays accumulate light; must be zeroed before each run.
    std::fill_n( &seen_squares_control[0][0], MAPSIZE * SEEX * MAPSIZE * SEEY, 0.0f );
    std::fill_n( &seen_squares_experiment[0][0], MAPSIZE * SEEX * MAPSIZE * SEEY, 0.0f );
    // transparency_cache fully overwritten by randomly_fill_transparency below.
    // floor_cache is read-only by cast_zlight; stays all-false (static zero-init) for all calls.
    // blocked_cache fully overwritten by uninitialized_fill_n below.

    diagonal_blocks fill = {false, false};
    std::uninitialized_fill_n( &blocked_cache[0][0], MAPSIZE * SEEX * MAPSIZE * SEEY, fill );

    randomly_fill_transparency( transparency_cache );

    map dummy;

    const tripoint offset( 65, 65, 0 );

    const auto start1 = std::chrono::high_resolution_clock::now();
    for( int i = 0; i < iterations; i++ ) {
        // First the control algorithm.
        castLightAll( &seen_squares_control[0][0], &transparency_cache[0][0],
                      &blocked_cache[0][0], MAPSIZE * SEEX, MAPSIZE * SEEY,
                      offset.xy(), 0, VISIBILITY_FULL, k_sight_model );
    }
    const auto end1 = std::chrono::high_resolution_clock::now();

    const tripoint origin( offset );
    // TODO: Give some more proper values here (all z-levels share the same 2D test data).
    // Build cache_grid_ref wrappers so cast_zlight can index into the flat C arrays.
    array_of_grids_of<float>                 seen_caches;
    array_of_grids_of<const float>           transparency_caches;
    array_of_grids_of<const bool>            floor_caches;
    array_of_grids_of<const diagonal_blocks> blocked_caches;
    for( int z = -OVERMAP_DEPTH; z <= OVERMAP_HEIGHT; z++ ) {
        const int zi = z + OVERMAP_DEPTH;
        seen_caches[zi]         = { &seen_squares_experiment[0][0], MAPSIZE * SEEX, MAPSIZE * SEEY };
        transparency_caches[zi] = { &transparency_cache[0][0],      MAPSIZE * SEEX, MAPSIZE * SEEY };
        floor_caches[zi]        = { &floor_cache[0][0],             MAPSIZE * SEEX, MAPSIZE * SEEY };
        blocked_caches[zi]      = { &blocked_cache[0][0],           MAPSIZE * SEEX, MAPSIZE * SEEY };
    }

    const auto start2 = std::chrono::high_resolution_clock::now();
    for( int i = 0; i < iterations; i++ ) {
        // Then the newer algorithm.
        cast_zlight( seen_caches, transparency_caches, floor_caches, blocked_caches,
                     origin, 0, 1.0f, k_sight_model );
    }
    const auto end2 = std::chrono::high_resolution_clock::now();

    if( iterations > 1 ) {
        const long long diff1 =
            std::chrono::duration_cast<std::chrono::microseconds>( end1 - start1 ).count();
        const long long diff2 =
            std::chrono::duration_cast<std::chrono::microseconds>( end2 - start2 ).count();
        cata_printf( "castLight() executed %d times in %lld microseconds.\n",
                     iterations, diff1 );
        cata_printf( "cast_zlight() executed %d times in %lld microseconds.\n",
                     iterations, diff2 );
        cata_printf( "new/old execution time ratio: %.02f.\n", static_cast<double>( diff2 ) / diff1 );
    }

    bool passed = grids_are_equivalent( seen_squares_control, seen_squares_experiment );

    if( !passed ) {
        print_grid_comparison( offset.xy(), transparency_cache, seen_squares_control,
                               seen_squares_experiment );
    }

    REQUIRE( passed );
}

// T, O and V are 'T'ransparent, 'O'paque and 'V'isible.
// X marks the player location, which is not set to visible by this algorithm.
static constexpr float T = LIGHT_TRANSPARENCY_OPEN_AIR;
static constexpr float O = LIGHT_TRANSPARENCY_SOLID;
static constexpr float V = LIGHT_TRANSPARENCY_OPEN_AIR;
static constexpr float X = LIGHT_TRANSPARENCY_SOLID;

const point ORIGIN( 65, 65 );

struct grid_overlay {
    std::vector<std::vector<float>> data;
    point offset;
    float default_value;

    // origin_offset is specified as the coordinates of the "camera" within the overlay.
    grid_overlay( const point &origin_offset, const float default_value ) {
        this->offset = ORIGIN - origin_offset;
        this->default_value = default_value;
    }

    int height() const {
        return data.size();
    }
    int width() const {
        if( data.empty() ) {
            return 0;
        }
        return data[0].size();
    }

    float get_global( const point &p ) const {
        if( p.y >= offset.y && p.y < offset.y + height() &&
            p.x >= offset.x && p.x < offset.x + width() ) {
            return data[ p.y - offset.y ][ p.x - offset.x ];
        }
        return default_value;
    }

    float get_local( const point &p ) const {
        return data[ p.y ][ p.x ];
    }
};

static void run_spot_check( const grid_overlay &test_case, const grid_overlay &expected_result )
{
    // Static to avoid stack overflow: MAPSIZE*SEEX x MAPSIZE*SEEY arrays (~689 KB each).
    static float seen_squares[ MAPSIZE * SEEX ][ MAPSIZE * SEEY ];
    static float transparency_cache[ MAPSIZE * SEEX ][ MAPSIZE * SEEY ];
    static diagonal_blocks blocked_cache[MAPSIZE * SEEX][MAPSIZE * SEEY];

    // seen_squares accumulates light; must be zeroed before each run.
    std::fill_n( &seen_squares[0][0], MAPSIZE * SEEX * MAPSIZE * SEEY, 0.0f );
    // transparency_cache fully overwritten by the loop below.
    // blocked_cache fully overwritten by uninitialized_fill_n below.

    diagonal_blocks fill = {false, false};
    std::uninitialized_fill_n( &blocked_cache[0][0], MAPSIZE * SEEX * MAPSIZE * SEEY, fill );

    for( int x = 0; x < MAPSIZE * SEEX; ++x ) {
        for( int y = 0; y < MAPSIZE * SEEY; ++y ) {
            transparency_cache[x][y] = test_case.get_global( point( x, y ) );
        }
    }

    castLightAll( &seen_squares[0][0], &transparency_cache[0][0], &blocked_cache[0][0],
                  MAPSIZE * SEEX, MAPSIZE * SEEY, ORIGIN, 0, VISIBILITY_FULL, k_sight_model );
    // Compares the whole grid, but out-of-bounds compares will de-facto pass.
    for( int y = 0; y < expected_result.height(); ++y ) {
        for( int x = 0; x < expected_result.width(); ++x ) {
            INFO( "x:" << x << " y:" << y << " expected:" << expected_result.data[y][x] << " actual:" <<
                  seen_squares[expected_result.offset.x + x][expected_result.offset.y + y] );
            if( V == expected_result.get_local( point( x, y ) ) ) {
                CHECK( seen_squares[expected_result.offset.x + x][expected_result.offset.y + y] > 0 );
            } else {
                CHECK( seen_squares[expected_result.offset.x + x][expected_result.offset.y + y] == 0 );
            }
        }
    }
}

TEST_CASE( "shadowcasting_slope_inversion_regression_test", "[shadowcasting]" )
{
    clear_all_state();
    grid_overlay test_case( { 7, 8 }, LIGHT_TRANSPARENCY_OPEN_AIR );
    test_case.data = {
        {T, T, T, T, T, T, T, T, T, T},
        {T, O, T, T, T, T, T, T, T, T},
        {T, O, T, T, T, T, T, T, T, T},
        {T, O, O, T, O, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, O, T},
        {T, T, T, T, T, T, O, T, O, T},
        {T, T, T, T, T, T, O, O, O, T},
        {T, T, T, T, T, T, T, T, T, T}
    };

    grid_overlay expected_results( { 7, 8 }, LIGHT_TRANSPARENCY_OPEN_AIR );
    expected_results.data = {
        {O, O, O, V, V, V, V, V, V, V},
        {O, V, V, O, V, V, V, V, V, V},
        {O, O, V, V, V, V, V, V, V, V},
        {O, O, V, V, V, V, V, V, V, V},
        {O, O, V, V, V, V, V, V, V, V},
        {O, O, O, V, V, V, V, V, V, O},
        {O, O, O, O, V, V, V, V, V, O},
        {O, O, O, O, O, V, V, V, V, O},
        {O, O, O, O, O, O, V, X, V, O},
        {O, O, O, O, O, O, V, V, V, O},
        {O, O, O, O, O, O, O, O, O, O}
    };

    run_spot_check( test_case, expected_results );
}

TEST_CASE( "shadowcasting_pillar_behavior_cardinally_adjacent", "[shadowcasting]" )
{
    clear_all_state();
    grid_overlay test_case( { 1, 4 }, LIGHT_TRANSPARENCY_OPEN_AIR );
    test_case.data = {
        {T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T},
        {T, T, O, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T}
    };

    grid_overlay expected_results( { 1, 4 }, LIGHT_TRANSPARENCY_OPEN_AIR );
    expected_results.data = {
        {V, V, V, V, V, V, V, O, O},
        {V, V, V, V, V, V, O, O, O},
        {V, V, V, V, V, O, O, O, O},
        {V, V, V, V, O, O, O, O, O},
        {V, X, V, O, O, O, O, O, O},
        {V, V, V, V, O, O, O, O, O},
        {V, V, V, V, V, O, O, O, O},
        {V, V, V, V, V, V, O, O, O},
        {V, V, V, V, V, V, V, O, O}
    };

    run_spot_check( test_case, expected_results );
}

TEST_CASE( "shadowcasting_pillar_behavior_2_1_diagonal_gap", "[shadowcasting]" )
{
    clear_all_state();
    // NOLINTNEXTLINE(cata-use-named-point-constants)
    grid_overlay test_case( { 1, 1 }, LIGHT_TRANSPARENCY_OPEN_AIR );
    test_case.data = {
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, O, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T}
    };

    // NOLINTNEXTLINE(cata-use-named-point-constants)
    grid_overlay expected_results( { 1, 1 }, LIGHT_TRANSPARENCY_OPEN_AIR );
    expected_results.data = {
        {V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V},
        {V, X, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V},
        {V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V},
        {V, V, V, V, V, O, O, O, V, V, V, V, V, V, V, V, V, V},
        {V, V, V, V, V, V, O, O, O, O, O, O, O, V, V, V, V, V},
        {V, V, V, V, V, V, V, O, O, O, O, O, O, O, O, O, O, O},
        {V, V, V, V, V, V, V, V, O, O, O, O, O, O, O, O, O, O},
        {V, V, V, V, V, V, V, V, V, O, O, O, O, O, O, O, O, O},
        {V, V, V, V, V, V, V, V, V, V, O, O, O, O, O, O, O, O},
    };

    run_spot_check( test_case, expected_results );
}

TEST_CASE( "shadowcasting_vision_along_a_wall", "[shadowcasting]" )
{
    clear_all_state();
    grid_overlay test_case( { 8, 2 }, LIGHT_TRANSPARENCY_OPEN_AIR );
    test_case.data = {
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, O, O, O, O, O, O, O, O, O, O, O, O, O, O, O, O, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T},
        {T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T}
    };

    grid_overlay expected_results( { 8, 2 }, LIGHT_TRANSPARENCY_OPEN_AIR );
    expected_results.data = {
        {O, O, O, O, O, O, O, O, O, O, O, O, O, O, O, O, O, O},
        {V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V},
        {V, V, V, V, V, V, V, V, X, V, V, V, V, V, V, V, V, V},
        {V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V},
        {V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V},
        {V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V},
        {V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V},
        {V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V}
    };

    run_spot_check( test_case, expected_results );
}

// Some random edge cases aren't matching.
TEST_CASE( "shadowcasting_runoff", "[.]" )
{
    clear_all_state();
    shadowcasting_runoff( 1 );
}

TEST_CASE( "shadowcasting_performance", "[.]" )
{
    clear_all_state();
    shadowcasting_runoff( 100000 );
}

TEST_CASE( "shadowcasting_3d_2d", "[.]" )
{
    clear_all_state();
    shadowcasting_3d_2d( 1 );
}

TEST_CASE( "shadowcasting_3d_2d_performance", "[.]" )
{
    clear_all_state();
    shadowcasting_3d_2d( 100000 );
}

TEST_CASE( "shadowcasting_float_quad_equivalence", "[shadowcasting]" )
{
    clear_all_state();
    shadowcasting_float_quad( 1 );
}

TEST_CASE( "shadowcasting_float_quad_performance", "[.]" )
{
    clear_all_state();
    shadowcasting_float_quad( 1000000 );
    shadowcasting_float_quad( 1000000, 100 );
}

// I'm not sure this will ever work.
TEST_CASE( "bresenham_vs_shadowcasting", "[.]" )
{
    clear_all_state();
    shadowcasting_runoff( 1, true );
}

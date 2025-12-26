#include "catch/catch.hpp"

#include <algorithm>
#include <array>
#include <ranges>

#include "coordinates.h"
#include "map_iterator.h"
#include "point.h"

static std::array<tripoint, 9> range_1_2d_centered = {
    {   {tripoint_north_west}, { tripoint_north}, { tripoint_north_east},
        {tripoint_west}, { tripoint_zero}, { tripoint_east},
        {tripoint_south_west}, { tripoint_south}, { tripoint_south_east}
    }
};

TEST_CASE( "Radius one 2D square centered at origin." )
{
    for( const tripoint &candidate :
         tripoint_range<tripoint>( tripoint_north_west, tripoint_south_east ) ) {
        REQUIRE( std::ranges::contains( range_1_2d_centered, candidate ) );
    }
}

static std::array<tripoint, 9> range_1_2d_offset = {
    {   {-5, -5, 0}, {-4, -5, 0}, {-3, -5, 0},
        {-5, -4, 0}, {-4, -4, 0}, {-3, -4, 0},
        {-5, -3, 0}, {-4, -3, 0}, {-3, -3, 0}
    }
};

TEST_CASE( "Radius one 2D square centered at -4/-4/0." )
{
    for( const tripoint &candidate : tripoint_range<tripoint>( {-5, -5, 0}, {-3, -3, 0} ) ) {
        REQUIRE( std::ranges::contains( range_1_2d_offset, candidate ) );
    }
}

TEST_CASE( "Radius one 2D square centered at -4/-4/0 in abs_omt coords." )
{
    for( const tripoint_abs_omt &candidate :
         tripoint_range<tripoint_abs_omt>( {-5, -5, 0}, {-3, -3, 0} ) ) {
        REQUIRE( std::ranges::contains( range_1_2d_offset, candidate.raw() ) );
    }
}

static std::array<tripoint, 343> range_3_3d_offset = {
    {   { 5, 5, -2}, { 6, 5, -2}, { 7, 5, -2}, { 8, 5, -2}, { 9, 5, -2}, {10, 5, -2}, {11, 5, -2},
        { 5, 6, -2}, { 6, 6, -2}, { 7, 6, -2}, { 8, 6, -2}, { 9, 6, -2}, {10, 6, -2}, {11, 6, -2},
        { 5, 7, -2}, { 6, 7, -2}, { 7, 7, -2}, { 8, 7, -2}, { 9, 7, -2}, {10, 7, -2}, {11, 7, -2},
        { 5, 8, -2}, { 6, 8, -2}, { 7, 8, -2}, { 8, 8, -2}, { 9, 8, -2}, {10, 8, -2}, {11, 8, -2},
        { 5, 9, -2}, { 6, 9, -2}, { 7, 9, -2}, { 8, 9, -2}, { 9, 9, -2}, {10, 9, -2}, {11, 9, -2},
        { 5, 10, -2}, { 6, 10, -2}, { 7, 10, -2}, { 8, 10, -2}, { 9, 10, -2}, {10, 10, -2}, {11, 10, -2},
        { 5, 11, -2}, { 6, 11, -2}, { 7, 11, -2}, { 8, 11, -2}, { 9, 11, -2}, {10, 11, -2}, {11, 11, -2},

        { 5, 5, -1}, { 6, 5, -1}, { 7, 5, -1}, { 8, 5, -1}, { 9, 5, -1}, {10, 5, -1}, {11, 5, -1},
        { 5, 6, -1}, { 6, 6, -1}, { 7, 6, -1}, { 8, 6, -1}, { 9, 6, -1}, {10, 6, -1}, {11, 6, -1},
        { 5, 7, -1}, { 6, 7, -1}, { 7, 7, -1}, { 8, 7, -1}, { 9, 7, -1}, {10, 7, -1}, {11, 7, -1},
        { 5, 8, -1}, { 6, 8, -1}, { 7, 8, -1}, { 8, 8, -1}, { 9, 8, -1}, {10, 8, -1}, {11, 8, -1},
        { 5, 9, -1}, { 6, 9, -1}, { 7, 9, -1}, { 8, 9, -1}, { 9, 9, -1}, {10, 9, -1}, {11, 9, -1},
        { 5, 10, -1}, { 6, 10, -1}, { 7, 10, -1}, { 8, 10, -1}, { 9, 10, -1}, {10, 10, -1}, {11, 10, -1},
        { 5, 11, -1}, { 6, 11, -1}, { 7, 11, -1}, { 8, 11, -1}, { 9, 11, -1}, {10, 11, -1}, {11, 11, -1},

        { 5, 5, 0}, { 6, 5, 0}, { 7, 5, 0}, { 8, 5, 0}, { 9, 5, 0}, {10, 5, 0}, {11, 5, 0},
        { 5, 6, 0}, { 6, 6, 0}, { 7, 6, 0}, { 8, 6, 0}, { 9, 6, 0}, {10, 6, 0}, {11, 6, 0},
        { 5, 7, 0}, { 6, 7, 0}, { 7, 7, 0}, { 8, 7, 0}, { 9, 7, 0}, {10, 7, 0}, {11, 7, 0},
        { 5, 8, 0}, { 6, 8, 0}, { 7, 8, 0}, { 8, 8, 0}, { 9, 8, 0}, {10, 8, 0}, {11, 8, 0},
        { 5, 9, 0}, { 6, 9, 0}, { 7, 9, 0}, { 8, 9, 0}, { 9, 9, 0}, {10, 9, 0}, {11, 9, 0},
        { 5, 10, 0}, { 6, 10, 0}, { 7, 10, 0}, { 8, 10, 0}, { 9, 10, 0}, {10, 10, 0}, {11, 10, 0},
        { 5, 11, 0}, { 6, 11, 0}, { 7, 11, 0}, { 8, 11, 0}, { 9, 11, 0}, {10, 11, 0}, {11, 11, 0},

        { 5, 5, 1}, { 6, 5, 1}, { 7, 5, 1}, { 8, 5, 1}, { 9, 5, 1}, {10, 5, 1}, {11, 5, 1},
        { 5, 6, 1}, { 6, 6, 1}, { 7, 6, 1}, { 8, 6, 1}, { 9, 6, 1}, {10, 6, 1}, {11, 6, 1},
        { 5, 7, 1}, { 6, 7, 1}, { 7, 7, 1}, { 8, 7, 1}, { 9, 7, 1}, {10, 7, 1}, {11, 7, 1},
        { 5, 8, 1}, { 6, 8, 1}, { 7, 8, 1}, { 8, 8, 1}, { 9, 8, 1}, {10, 8, 1}, {11, 8, 1},
        { 5, 9, 1}, { 6, 9, 1}, { 7, 9, 1}, { 8, 9, 1}, { 9, 9, 1}, {10, 9, 1}, {11, 9, 1},
        { 5, 10, 1}, { 6, 10, 1}, { 7, 10, 1}, { 8, 10, 1}, { 9, 10, 1}, {10, 10, 1}, {11, 10, 1},
        { 5, 11, 1}, { 6, 11, 1}, { 7, 11, 1}, { 8, 11, 1}, { 9, 11, 1}, {10, 11, 1}, {11, 11, 1},

        { 5, 5, 2}, { 6, 5, 2}, { 7, 5, 2}, { 8, 5, 2}, { 9, 5, 2}, {10, 5, 2}, {11, 5, 2},
        { 5, 6, 2}, { 6, 6, 2}, { 7, 6, 2}, { 8, 6, 2}, { 9, 6, 2}, {10, 6, 2}, {11, 6, 2},
        { 5, 7, 2}, { 6, 7, 2}, { 7, 7, 2}, { 8, 7, 2}, { 9, 7, 2}, {10, 7, 2}, {11, 7, 2},
        { 5, 8, 2}, { 6, 8, 2}, { 7, 8, 2}, { 8, 8, 2}, { 9, 8, 2}, {10, 8, 2}, {11, 8, 2},
        { 5, 9, 2}, { 6, 9, 2}, { 7, 9, 2}, { 8, 9, 2}, { 9, 9, 2}, {10, 9, 2}, {11, 9, 2},
        { 5, 10, 2}, { 6, 10, 2}, { 7, 10, 2}, { 8, 10, 2}, { 9, 10, 2}, {10, 10, 2}, {11, 10, 2},
        { 5, 11, 2}, { 6, 11, 2}, { 7, 11, 2}, { 8, 11, 2}, { 9, 11, 2}, {10, 11, 2}, {11, 11, 2},

        { 5, 5, 3}, { 6, 5, 3}, { 7, 5, 3}, { 8, 5, 3}, { 9, 5, 3}, {10, 5, 3}, {11, 5, 3},
        { 5, 6, 3}, { 6, 6, 3}, { 7, 6, 3}, { 8, 6, 3}, { 9, 6, 3}, {10, 6, 3}, {11, 6, 3},
        { 5, 7, 3}, { 6, 7, 3}, { 7, 7, 3}, { 8, 7, 3}, { 9, 7, 3}, {10, 7, 3}, {11, 7, 3},
        { 5, 8, 3}, { 6, 8, 3}, { 7, 8, 3}, { 8, 8, 3}, { 9, 8, 3}, {10, 8, 3}, {11, 8, 3},
        { 5, 9, 3}, { 6, 9, 3}, { 7, 9, 3}, { 8, 9, 3}, { 9, 9, 3}, {10, 9, 3}, {11, 9, 3},
        { 5, 10, 3}, { 6, 10, 3}, { 7, 10, 3}, { 8, 10, 3}, { 9, 10, 3}, {10, 10, 3}, {11, 10, 3},
        { 5, 11, 3}, { 6, 11, 3}, { 7, 11, 3}, { 8, 11, 3}, { 9, 11, 3}, {10, 11, 3}, {11, 11, 3},

        { 5, 5, 4}, { 6, 5, 4}, { 7, 5, 4}, { 8, 5, 4}, { 9, 5, 4}, {10, 5, 4}, {11, 5, 4},
        { 5, 6, 4}, { 6, 6, 4}, { 7, 6, 4}, { 8, 6, 4}, { 9, 6, 4}, {10, 6, 4}, {11, 6, 4},
        { 5, 7, 4}, { 6, 7, 4}, { 7, 7, 4}, { 8, 7, 4}, { 9, 7, 4}, {10, 7, 4}, {11, 7, 4},
        { 5, 8, 4}, { 6, 8, 4}, { 7, 8, 4}, { 8, 8, 4}, { 9, 8, 4}, {10, 8, 4}, {11, 8, 4},
        { 5, 9, 4}, { 6, 9, 4}, { 7, 9, 4}, { 8, 9, 4}, { 9, 9, 4}, {10, 9, 4}, {11, 9, 4},
        { 5, 10, 4}, { 6, 10, 4}, { 7, 10, 4}, { 8, 10, 4}, { 9, 10, 4}, {10, 10, 4}, {11, 10, 4},
        { 5, 11, 4}, { 6, 11, 4}, { 7, 11, 4}, { 8, 11, 4}, { 9, 11, 4}, {10, 11, 4}, {11, 11, 4}
    }
};

TEST_CASE( "Radius three 3D square centered at 8/8/1." )
{
    for( const tripoint &candidate : tripoint_range<tripoint>( {5, 5, -2}, {11, 11, 4} ) ) {
        REQUIRE( std::ranges::contains( range_3_3d_offset, candidate ) );
    }
}

TEST_CASE( "tripoint_range is a C++20 forward_range" )
{
    STATIC_REQUIRE( std::forward_iterator<tripoint_range<tripoint>::iterator> );
    STATIC_REQUIRE( std::ranges::forward_range<tripoint_range<tripoint>> );
    STATIC_REQUIRE( std::ranges::view<tripoint_range<tripoint>> );
}

TEST_CASE( "tripoint_range works with std::ranges algorithms" )
{
    auto range = tripoint_range<tripoint>( tripoint_north_west, tripoint_south_east );

    // Test std::ranges::find
    auto it = std::ranges::find( range, tripoint_zero );
    REQUIRE( it != range.end() );
    REQUIRE( *it == tripoint_zero );

    // Test std::ranges::count
    auto count = std::ranges::count( range, tripoint_north );
    REQUIRE( count == 1 );

    // Test std::ranges::any_of
    bool has_zero = std::ranges::any_of( range, []( const tripoint & p ) {
        return p == tripoint_zero;
    } );
    REQUIRE( has_zero );

    // Test std::ranges::distance
    auto dist = std::ranges::distance( range );
    REQUIRE( dist == 9 );
}

TEST_CASE( "tripoint_range iterator is default constructible and copyable" )
{
    tripoint_range<tripoint>::iterator default_iter;
    tripoint_range<tripoint>::iterator another_iter;

    auto range = tripoint_range<tripoint>( tripoint_north_west, tripoint_south_east );
    default_iter = range.begin();
    another_iter = default_iter;

    REQUIRE( default_iter == another_iter );
    REQUIRE( *default_iter == *another_iter );
}

TEST_CASE( "tripoint_range works with std::ranges::views" )
{
    auto range = tripoint_range<tripoint>( {-1, -1, 0}, {1, 1, 0} );

    // Test with filter view
    auto filtered = range | std::views::filter( []( const tripoint & p ) { return p.x >= 0 && p.y >= 0; } );

    int filtered_count = 0;
    for( const auto &p : filtered ) {
        ( void )p;
        filtered_count++;
    }
    REQUIRE( filtered_count == 4 ); // (0,0), (1,0), (0,1), (1,1)

    // Test with transform view
    auto transformed = range | std::views::transform( []( const tripoint & p ) { return p.x + p.y; } );

    REQUIRE( std::ranges::distance( transformed ) == 9 );
}

TEST_CASE( "tripoint_range iterator survives temporary range (no dangling)" )
{
    // Critical safety test: Iterator must store bounds by value, not pointer to parent
    // This would crash with old implementation storing range*
    auto it = points_in_radius( tripoint_zero, 1 ).begin();
    // points_in_radius returns temporary, destroyed after this line
    // Iterator must still be valid
    REQUIRE( *it == tripoint( -1, -1, -1 ) );
    ++it; // Would crash if iterator stored dangling pointer
    REQUIRE( *it == tripoint( 0, -1, -1 ) );
}

TEST_CASE( "tripoint_range copied iterator remains valid" )
{
    auto range = tripoint_range<tripoint>( tripoint_zero, tripoint( 2, 2, 0 ) );
    auto it1 = range.begin();
    auto it2 = it1; // Copy iterator

    ++it1;
    REQUIRE( *it1 == tripoint( 1, 0, 0 ) );
    REQUIRE( *it2 == tripoint( 0, 0, 0 ) ); // Original position

    ++it2;
    REQUIRE( *it2 == tripoint( 1, 0, 0 ) );
}

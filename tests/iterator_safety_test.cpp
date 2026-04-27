#include "catch/catch.hpp"

#include "map_iterator.h"
#include "rect_range.h"
#include "vpart_range.h"
#include "vehicle.h"
#include "vpart_position.h"
#include "point.h"

#include <algorithm>
#include <ranges>
#include <vector>

namespace
{

struct test_rect {
    int x;
    int y;
    int w;
    int h;

    auto operator<=>( const test_rect & ) const = default; // *NOPAD*
};

auto make_temp_tripoint_range() -> tripoint_range<tripoint>
{
    return tripoint_range<tripoint>( tripoint( 0, 0, 0 ), tripoint( 1, 1, 0 ) );
}

auto make_temp_rect_range( const int width, const int height,
                           const point count ) -> rect_range<test_rect>
{
    return rect_range<test_rect>( width, height, count );
}

} // namespace

TEST_CASE( "tripoint_range_iterator_safety", "[iterator][safety]" )
{
    // Get iterator from a temporary range object.
    // The range object is destroyed at the end of this statement.
    auto it = make_temp_tripoint_range().begin();

    // If the iterator held a pointer to the range, accessing it would be undefined behavior.
    // Since it stores range_min and range_max by value, this is safe.
    REQUIRE( *it == tripoint( 0, 0, 0 ) );

    ++it;
    REQUIRE( *it == tripoint( 1, 0, 0 ) );

    ++it;
    REQUIRE( *it == tripoint( 0, 1, 0 ) );

    ++it;
    REQUIRE( *it == tripoint( 1, 1, 0 ) );
}

TEST_CASE( "rect_range_iterator_safety_numeric", "[iterator][safety]" )
{
    auto it = make_temp_rect_range( 10, 10, point( 2, 2 ) ).begin();

    // The dereference returns {x, y, width, height} struct where x,y are computed.
    // For this test we just verify we can dereference without crashing after
    // the temporary range is destroyed.
    auto rect = *it;
    REQUIRE( rect.x == 0 );
    REQUIRE( rect.y == 0 );

    ++it;
    auto rect2 = *it;
    REQUIRE( rect2.x == 10 ); // Second element (width=10)
}

TEST_CASE( "rect_range models a C++20 random_access_range", "[iterator][safety][ranges]" )
{
    using test_rect_range = rect_range<test_rect>;

    STATIC_REQUIRE( std::random_access_iterator<test_rect_range::iterator> );
    STATIC_REQUIRE( std::ranges::random_access_range<test_rect_range> );
    STATIC_REQUIRE( std::ranges::view<test_rect_range> );

    auto range = test_rect_range( 10, 20, point( 3, 2 ) );
    auto it = range.begin();
    auto end = range.end();

    REQUIRE( *it == test_rect{ 0, 0, 10, 20 } );
    REQUIRE( *( it + 4 ) == test_rect{ 10, 20, 10, 20 } );
    REQUIRE( *( 2 + it ) == test_rect{ 20, 0, 10, 20 } );
    REQUIRE( *( end - 2 ) == test_rect{ 10, 20, 10, 20 } );
    REQUIRE( it[5] == test_rect{ 20, 20, 10, 20 } );
    REQUIRE( end - it == 6 );
    REQUIRE( std::ranges::distance( range ) == 6 );
}

TEST_CASE( "rect_range iterator comparisons include range geometry", "[iterator][safety][ranges]" )
{
    const auto base_range = rect_range<test_rect>( 10, 10, point( 2, 2 ) );
    const auto different_step = rect_range<test_rect>( 20, 10, point( 2, 2 ) );
    const auto different_size = rect_range<test_rect>( 10, 10, point( 2, 3 ) );

    CHECK( base_range.begin() != different_step.begin() );
    CHECK( base_range.begin() + 1 != different_step.begin() + 1 );
    CHECK( base_range.end() != different_size.begin() + 4 );
    CHECK( *( base_range.begin() + 1 ) == test_rect{ 10, 0, 10, 10 } );
    CHECK( *( different_step.begin() + 1 ) == test_rect{ 20, 0, 20, 10 } );
}

TEST_CASE( "vehicle_part_range_iterator_safety", "[iterator][safety]" )
{
    vehicle v;

    // Add parts to distinct locations to ensure successful installation.
    v.install_part( point( 0, 0 ), vpart_id( "frame_vertical" ) );
    v.install_part( point( 1, 0 ), vpart_id( "frame_vertical" ) );
    v.install_part( point( 2, 0 ), vpart_id( "frame_vertical" ) );

    // Get an iterator from a temporary range object.
    // CRITICAL: This temporary object is destroyed at the end of this statement.
    auto it_begin = v.get_all_parts().begin();
    auto it_end = v.get_all_parts().end();

    // Before the fix: it_begin held a const pointer to the temporary range,
    // making it a dangling pointer after the temporary was destroyed.
    // After the fix: it_begin stores a copy of the range (lightweight: vehicle* + filter state),
    // so it remains valid even after the temporary is destroyed.

    REQUIRE( it_begin != it_end );
    // Part indices are not guaranteed to be 0, 1, 2 sequentially depending on installation internals,
    // but iteration count should be 3.
    int count = 0;
    while( it_begin != it_end ) {
        ++count;
        ++it_begin;
    }
    REQUIRE( count == 3 );
}


TEST_CASE( "vehicle_part_with_feature_range_iterator_safety", "[iterator][safety]" )
{
    vehicle v;

    // Add parts with different features at distinct locations.
    v.install_part( point( 0, 0 ), vpart_id( "frame_vertical" ) ); // structure
    v.install_part( point( 0, 0 ), vpart_id( "seat" ) );           // seat on structure
    v.install_part( point( 1, 0 ), vpart_id( "frame_vertical" ) ); // structure
    v.install_part( point( 1, 0 ), vpart_id( "seat" ) );           // seat on structure

    // Create a filtered range (temporary) and get iterators.
    // The range_type stores the feature filter by value in vehicle_part_iterator.
    // Even though the temporary range is destroyed, the iterator carries its own copy of the filter.
    auto it_begin = v.get_avail_parts( std::string( "SEAT" ) ).begin();
    auto it_end = v.get_avail_parts( std::string( "SEAT" ) ).end();

    // Verify we can safely dereference after the temporary range is gone.
    int count = 0;
    while( it_begin != it_end ) {
        ++count;
        ++it_begin;
    }
    // There should be 2 seats.
    REQUIRE( count == 2 );
}

TEST_CASE( "vehicle_part_iterator_copy_semantics", "[iterator][safety]" )
{
    vehicle v;

    v.install_part( point( 0, 0 ), vpart_id( "frame_vertical" ) );
    v.install_part( point( 1, 0 ), vpart_id( "frame_vertical" ) );
    v.install_part( point( 2, 0 ), vpart_id( "frame_vertical" ) );

    auto range = v.get_all_parts();
    auto it1 = range.begin();
    auto it2 = it1; // Copy construction

    // Advance it1
    ++it1;
    ++it1;

    // it2 should still point to the first element
    // it1 should point to the third element
    REQUIRE( it2->part_index() != it1->part_index() );
    REQUIRE( std::ranges::distance( range ) == 3 );
}

TEST_CASE( "vehicle_part_range_for_temporary_safety", "[iterator][safety]" )
{
    vehicle v;

    v.install_part( point( 0, 0 ), vpart_id( "frame_vertical" ) );
    v.install_part( point( 1, 0 ), vpart_id( "frame_vertical" ) );
    v.install_part( point( 2, 0 ), vpart_id( "frame_vertical" ) );

    std::vector<int> indices;

    // This range-for loop creates iterators from a temporary range.
    // Before the fix: Undefined behavior / potential dangling iterators.
    // After the fix: Safe, because each iterator owns its range state.
    for( const auto &part : v.get_all_parts() ) {
        indices.push_back( part.part_index() );
    }

    REQUIRE( indices.size() == 3 );
}

TEST_CASE( "vehicle_part_filtered_range_for_temporary_safety",
           "[iterator][safety]" )
{
    vehicle v;

    v.install_part( point( 0, 0 ), vpart_id( "frame_vertical" ) );
    v.install_part( point( 0, 0 ), vpart_id( "seat" ) );
    v.install_part( point( 1, 0 ), vpart_id( "frame_vertical" ) );
    v.install_part( point( 1, 0 ), vpart_id( "seat" ) );

    std::vector<int> indices;
    int count = 0;

    // Iterate over filtered temporary range.
    // The filter state is now safely copied into each iterator.
    for( const auto &part : v.get_avail_parts( std::string( "SEAT" ) ) ) {
        indices.push_back( part.part_index() );
        ++count;
    }

    // Should have found 2 seat parts
    REQUIRE( count == 2 );
}

TEST_CASE( "vehicle_part_range_empty_method", "[iterator][safety]" )
{
    vehicle v;

    // Empty vehicle should have empty range.
    REQUIRE( v.get_all_parts().empty() );

    v.install_part( point( 0, 0 ), vpart_id( "frame_vertical" ) );

    // Non-empty vehicle should have non-empty range.
    REQUIRE_FALSE( v.get_all_parts().empty() );

    // Filtered range for non-existent feature should be empty.
    REQUIRE( v.get_avail_parts( std::string( "SEAT" ) ).empty() );

    // Filtered range for existing feature should not be empty.
    v.install_part( point( 0, 0 ), vpart_id( "seat" ) );
    REQUIRE_FALSE( v.get_avail_parts( std::string( "SEAT" ) ).empty() );
}

TEST_CASE( "vehicle_part_range_std_ranges_algorithms", "[iterator][safety][ranges]" )
{
    vehicle v;

    v.install_part( point( 0, 0 ), vpart_id( "frame_vertical" ) );
    v.install_part( point( 0, 0 ), vpart_id( "seat" ) );
    v.install_part( point( 1, 0 ), vpart_id( "frame_vertical" ) );
    v.install_part( point( 1, 0 ), vpart_id( "seat" ) );

    // Test std::ranges::distance on temporary range (borrowed_range)
    // This tests that enable_borrowed_range allows ranges algorithms on temporaries
    const int seat_count = std::ranges::distance( v.get_avail_parts( std::string( "SEAT" ) ) );
    REQUIRE( seat_count == 2 );

    const int all_count = std::ranges::distance( v.get_all_parts() );
    REQUIRE( all_count == 4 );

    // Test iteration with std::ranges
    std::vector<int> indices;
    for( auto part : v.get_all_parts() ) {
        indices.push_back( part.part_index() );
    }
    REQUIRE( indices.size() == 4 );
}

TEST_CASE( "vehicle_part_range_concepts", "[iterator][safety][ranges]" )
{
    // Verify that our ranges satisfy std::ranges concepts
    static_assert( std::ranges::range<vehicle_part_range> );
    static_assert( std::ranges::input_range<vehicle_part_range> );
    static_assert( std::ranges::forward_range<vehicle_part_range> );
    static_assert( std::ranges::borrowed_range<vehicle_part_range> );

    using filtered_range = vehicle_part_with_feature_range<std::string>;
    static_assert( std::ranges::range<filtered_range> );
    static_assert( std::ranges::input_range<filtered_range> );
    static_assert( std::ranges::forward_range<filtered_range> );
    static_assert( std::ranges::borrowed_range<filtered_range> );
}

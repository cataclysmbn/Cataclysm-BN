#include "catch/catch.hpp"

#include "coordinates.h"
#include "translocator_utils.h"

TEST_CASE( "translocator local destination uses runtime bubble center",
           "[magic][translocator]" )
{
    const auto omt_local_dest = tripoint_bub_ms( 3, 4, 2 );
    const auto bubble_center = point_bub_ms( 84, 84 );

    CHECK( translocator::local_dest( omt_local_dest, bubble_center ) ==
           tripoint_bub_ms( 87, 88, 2 ) );
}

#include "catch/catch.hpp"
#include "travel/travel_destination.h"

TEST_CASE( "travel_line_overlay_is_drawn_for_known_targets", "[travel][tiles]" )
{
    CHECK( should_draw_travel_line_overlay( true, true ) );
    CHECK_FALSE( should_draw_travel_line_overlay( true, false ) );
    CHECK( should_draw_travel_line_overlay( false, false ) );
}

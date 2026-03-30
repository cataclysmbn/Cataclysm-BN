#include "catch/catch.hpp"

#include "procgen/proc_ui_navigation.h"

TEST_CASE( "proc_builder_wrap_cursor_wraps_at_top_and_bottom", "[proc][ui]" )
{
    CHECK( proc::wrap_cursor( 0, -1, 3 ) == 2 );
    CHECK( proc::wrap_cursor( 2, 1, 3 ) == 0 );
    CHECK( proc::wrap_cursor( 1, 1, 3 ) == 2 );
    CHECK( proc::wrap_cursor( 2, -1, 3 ) == 1 );
    CHECK( proc::wrap_cursor( 1, 5, 3 ) == 0 );
    CHECK( proc::wrap_cursor( 0, -4, 3 ) == 2 );
}

TEST_CASE( "proc_builder_wrap_cursor_handles_empty_lists", "[proc][ui]" )
{
    CHECK( proc::wrap_cursor( 0, 1, 0 ) == 0 );
    CHECK( proc::wrap_cursor( 0, -1, 0 ) == 0 );
    CHECK( proc::wrap_cursor( 0, 1, 1 ) == 0 );
    CHECK( proc::wrap_cursor( 0, -1, 1 ) == 0 );
    CHECK( proc::wrap_cursor( 4, -1, 0 ) == 0 );
}

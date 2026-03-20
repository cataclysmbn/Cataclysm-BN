#include "catch/catch.hpp"

#include "proc_ui_navigation.h"

TEST_CASE( "proc_builder_cursor_wraps_at_list_edges", "[proc][ui]" )
{
    CHECK( proc::wrap_cursor( 0, -1, 4 ) == 3 );
    CHECK( proc::wrap_cursor( 3, 1, 4 ) == 0 );
    CHECK( proc::wrap_cursor( 1, 1, 4 ) == 2 );
    CHECK( proc::wrap_cursor( 2, -1, 4 ) == 1 );
}

TEST_CASE( "proc_builder_cursor_wrap_handles_empty_and_single_entry_lists", "[proc][ui]" )
{
    CHECK( proc::wrap_cursor( 0, 1, 0 ) == 0 );
    CHECK( proc::wrap_cursor( 0, -1, 0 ) == 0 );
    CHECK( proc::wrap_cursor( 0, 1, 1 ) == 0 );
    CHECK( proc::wrap_cursor( 0, -1, 1 ) == 0 );
}

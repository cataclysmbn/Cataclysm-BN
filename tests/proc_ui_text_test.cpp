#include "catch/catch.hpp"

#include <string>
#include <vector>

#include "proc_ui_text.h"

TEST_CASE( "proc_ui_groups_duplicate_labels_in_first_seen_order", "[proc][ui]" )
{
    const auto grouped = proc::group_duplicate_labels( { "bread", "cheese", "bread", "meat", "cheese" } );

    REQUIRE( grouped.size() == 3 );
    CHECK( grouped[0].label == "bread" );
    CHECK( grouped[0].count == 2 );
    CHECK( grouped[1].label == "cheese" );
    CHECK( grouped[1].count == 2 );
    CHECK( grouped[2].label == "meat" );
    CHECK( grouped[2].count == 1 );
}

TEST_CASE( "proc_ui_formats_grouped_slot_and_preview_labels_with_x_counts", "[proc][ui]" )
{
    CHECK( proc::grouped_label_summary( {}, "empty" ) == "empty" );
    CHECK( proc::grouped_label_summary( { "bread", "bread", "cheese" }, "empty" ) ==
           "bread x2, cheese" );
    CHECK( proc::grouped_label_lines( { "bread", "bread", "cheese" } ) ==
           std::vector<std::string> { "- bread x2", "- cheese" } );
}

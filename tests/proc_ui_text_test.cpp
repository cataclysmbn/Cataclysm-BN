#include "catch/catch.hpp"

#include <string>
#include <vector>

#include "procgen/proc_ui_text.h"

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

TEST_CASE( "proc_ui_formats_builder_readiness_labels", "[proc][ui]" )
{
    CHECK( proc::builder_readiness_label( proc::builder_readiness::missing_required_slots ) ==
           "Status: [ MISSING REQUIRED SLOTS ]" );
    CHECK( proc::builder_readiness_label( proc::builder_readiness::missing_recipe_requirements ) ==
           "Status: [ MISSING TOOLS OR QUALITIES ]" );
    CHECK( proc::builder_readiness_label( proc::builder_readiness::ready_to_craft ) ==
           "Status: [ READY TO CRAFT ]" );
}

TEST_CASE( "proc_ui_compacts_missing_requirement_text_for_status_bar", "[proc][ui]" )
{
    CHECK( proc::compact_requirement_text( "" ).empty() );
    CHECK( proc::compact_requirement_text( "These tools are missing:\ncutting 1\nboiling 2\n" ) ==
           "These tools are missing: cutting 1; boiling 2" );
    CHECK( proc::compact_requirement_text(
               "These tools are missing:\ncutting 1\nThese tools are missing:\nboiling 2\n" ) ==
           "These tools are missing: cutting 1; boiling 2" );
}

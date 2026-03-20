#include "catch/catch.hpp"

#include <string>
#include <vector>

#include "proc_ui_candidates.h"

TEST_CASE( "proc_ui_groups_duplicate_candidates_in_first_seen_order", "[proc][ui]" )
{
    const auto grouped = proc::group_candidate_entries( {
        proc::candidate_label_entry{ .key = "bread", .label = "(I) bread  [+100 kcal]", .ix = 1, .count = 1 },
        proc::candidate_label_entry{ .key = "cheese", .label = "(I) cheese  [+80 kcal]", .ix = 2, .count = 1 },
        proc::candidate_label_entry{ .key = "bread", .label = "(I) bread  [+100 kcal]", .ix = 3, .count = 2 },
    } );

    REQUIRE( grouped.size() == 2 );
    CHECK( grouped[0].key == "bread" );
    CHECK( grouped[0].label == "(I) bread  [+100 kcal]" );
    CHECK( grouped[0].ixs == std::vector<proc::part_ix> { 1, 3 } );
    CHECK( grouped[0].total_count == 3 );
    CHECK( grouped[1].key == "cheese" );
    CHECK( grouped[1].ixs == std::vector<proc::part_ix> { 2 } );
    CHECK( grouped[1].total_count == 1 );
}

TEST_CASE( "proc_ui_formats_grouped_candidates_with_total_remaining_count", "[proc][ui]" )
{
    CHECK( proc::grouped_candidate_label( {
        .key = "bread",
        .label = "(I) bread  [+100 kcal]",
        .ixs = { 1 },
        .total_count = 1,
    } ) == "(I) bread  [+100 kcal]" );
    CHECK( proc::grouped_candidate_label( {
        .key = "bread",
        .label = "(I) bread  [+100 kcal]",
        .ixs = { 1, 2 },
        .total_count = 3,
    } ) == "(I) bread  [+100 kcal] x3" );
    CHECK( proc::first_grouped_candidate_ix( {
        .key = "empty",
        .label = "empty",
        .ixs = {},
        .total_count = 0,
    } ) == proc::invalid_part_ix );
    CHECK( proc::first_grouped_candidate_ix( {
        .key = "bread",
        .label = "(I) bread  [+100 kcal]",
        .ixs = { 4, 7 },
        .total_count = 2,
    } ) == 4 );
}

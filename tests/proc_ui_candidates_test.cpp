#include "catch/catch.hpp"

#include <string>
#include <vector>

#include "procgen/proc_ui_candidates.h"

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

TEST_CASE( "proc_ui_filters_grouped_candidates_by_search_and_remaining_uses", "[proc][ui]" )
{
    auto bread_pantry = proc::part_fact{};
    bread_pantry.ix = 1;
    bread_pantry.id = itype_id( "bread" );
    bread_pantry.tag = { "bread" };
    bread_pantry.mat = { material_id( "wheat" ) };
    bread_pantry.uses = 2;

    auto bread_bag = bread_pantry;
    bread_bag.ix = 2;
    bread_bag.uses = 1;

    auto carrot = proc::part_fact{};
    carrot.ix = 3;
    carrot.id = itype_id( "carrot" );
    carrot.tag = { "veg" };
    carrot.mat = { material_id( "veggy" ) };
    carrot.uses = 1;

    auto state = proc::builder_state{};
    state.facts = { bread_pantry, bread_bag, carrot };
    state.cand.emplace( proc::slot_id( "bread" ), std::vector<proc::part_ix> { 1, 2, 3 } );
    state.chosen.emplace( proc::slot_id( "bread" ), std::vector<proc::part_ix> { 1 } );

    const auto sources = std::vector<proc::candidate_source_entry> {
        {
            .label = "(I) bread  [+100 kcal]",
            .name = "Bread",
            .where = "pantry",
            .fact = bread_pantry,
        },
        {
            .label = "(I) bread  [+100 kcal]",
            .name = "Bread",
            .where = "mess kit",
            .fact = bread_bag,
        },
        {
            .label = "(I) carrot  [+30 kcal]",
            .name = "Carrot",
            .where = "pantry",
            .fact = carrot,
        },
    };

    const auto grouped = proc::filter_grouped_candidates( state, proc::slot_id( "bread" ), sources,
                         "tag:bread mat:wheat" );
    REQUIRE( grouped.size() == 1 );
    CHECK( grouped[0].label == "(I) bread  [+100 kcal]" );
    CHECK( grouped[0].ixs == std::vector<proc::part_ix> { 1, 2 } );
    CHECK( grouped[0].total_count == 2 );

    const auto pantry_only = proc::filter_grouped_candidates( state, proc::slot_id( "bread" ), sources,
                             "bread pantry" );
    REQUIRE( pantry_only.size() == 1 );
    CHECK( pantry_only[0].ixs == std::vector<proc::part_ix> { 1 } );
    CHECK( pantry_only[0].total_count == 1 );
}

#include "catch/catch.hpp"

#include <type_traits>
#include <vector>

#include "enum_conversions.h"
#include "procgen/proc_types.h"
#include "type_id.h"

TEST_CASE( "proc_hist_round_trip", "[proc]" )
{
    CHECK( io::enum_to_string( proc::hist::none ) == "none" );
    CHECK( io::enum_to_string( proc::hist::compact ) == "compact" );
    CHECK( io::enum_to_string( proc::hist::full ) == "full" );

    CHECK( io::string_to_enum<proc::hist>( "none" ) == proc::hist::none );
    CHECK( io::string_to_enum<proc::hist>( "compact" ) == proc::hist::compact );
    CHECK( io::string_to_enum<proc::hist>( "full" ) == proc::hist::full );
}

TEST_CASE( "proc_core_scaffold_types", "[proc]" )
{
    CHECK( proc::fast_blob{}.empty() );
    CHECK( proc::full_blob{} == proc::full_blob{} );

    const auto sandwich = proc::schema_id( "sandwich" );
    const auto bread = proc::slot_id( "bread" );
    auto fact = proc::part_fact{};
    fact.ix = 7;
    fact.id = itype_id( "2x4" );
    const auto pick = proc::pick{ .slot = bread, .parts = { 1, 4 } };

    CHECK( sandwich == proc::schema_id( "sandwich" ) );
    CHECK( bread == proc::slot_id( "bread" ) );
    CHECK( fact.valid() );
    CHECK_FALSE( proc::part_fact{}.valid() );
    CHECK_FALSE( pick.empty() );
    CHECK( pick.parts == std::vector<proc::part_ix> { 1, 4 } );
}

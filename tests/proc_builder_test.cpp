#include "catch/catch.hpp"

#include <sstream>
#include <string>
#include <vector>

#include "json.h"
#include "proc_builder.h"
#include "proc_schema.h"
#include "recipe_dictionary.h"

namespace
{

auto load_schema_for_test( const std::string &json ) -> proc::schema
{
    proc::reset();
    std::istringstream input( json );
    JsonIn jsin( input );
    const auto jo = jsin.get_object();
    jo.get_string( "type" );
    proc::load( jo, "test" );
    const auto loaded = proc::all().front();
    proc::reset();
    return loaded;
}

auto load_recipe_for_test( const std::string &json ) -> recipe
{
    std::istringstream input( json );
    JsonIn jsin( input );
    auto rec = recipe{};
    rec.load( jsin.get_object(), "test" );
    return rec;
}

} // namespace

TEST_CASE( "proc_builder_matches_query_atoms", "[proc][builder]" )
{
    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "knife_butcher" );
    fact.tag = { "blade", "knife" };
    fact.flag = { flag_id( "STAB" ) };
    fact.mat = { material_id( "steel" ) };
    fact.qual.emplace( quality_id( "CUT" ), 2 );

    CHECK( proc::matches_atom( fact, "tag:knife" ) );
    CHECK( proc::matches_atom( fact, "flag:STAB" ) );
    CHECK( proc::matches_atom( fact, "mat:steel" ) );
    CHECK( proc::matches_atom( fact, "itype:knife_butcher" ) );
    CHECK( proc::matches_atom( fact, "qual:CUT>=1" ) );
    CHECK_FALSE( proc::matches_atom( fact, "qual:CUT>=3" ) );
}

TEST_CASE( "proc_builder_builds_candidates_and_fast_preview", "[proc][builder]" )
{
    const auto sch = load_schema_for_test( R"(
{
  "type": "PROC",
  "id": "sandwich",
  "cat": "food",
  "res": "sandwich_generic",
  "slot": [
    { "id": "top", "role": "bread", "min": 1, "max": 1, "ok": [ "tag:bread" ] },
    { "id": "fill", "role": "meat", "min": 0, "max": 2, "rep": true, "ok": [ "tag:meat" ] }
  ]
}
    )" );

    auto bread = proc::part_fact{};
    bread.ix = 1;
    bread.id = itype_id( "bread" );
    bread.tag = { "bread" };
    bread.mass_g = 120;
    bread.volume_ml = 250;
    bread.kcal = 300;

    auto meat = proc::part_fact{};
    meat.ix = 2;
    meat.id = itype_id( "meat_cooked" );
    meat.tag = { "meat" };
    meat.mass_g = 80;
    meat.volume_ml = 125;
    meat.kcal = 180;

    const auto state0 = proc::build_state( sch, { bread, meat } );
    REQUIRE( state0.cand.at( proc::slot_id( "top" ) ).size() == 1 );
    REQUIRE( state0.cand.at( proc::slot_id( "fill" ) ).size() == 1 );

    auto state = state0;
    REQUIRE( proc::add_pick( state, sch, proc::slot_id( "top" ), 1 ) );
    REQUIRE( proc::add_pick( state, sch, proc::slot_id( "fill" ), 2 ) );

    CHECK( state.fast.mass_g == 200 );
    CHECK( state.fast.volume_ml == 375 );
    CHECK( state.fast.kcal == 480 );
    CHECK( proc::fast_fp( sch, state.fast, proc::selected_facts( state ) ).starts_with( "sandwich:" ) );
}

TEST_CASE( "proc_recipe_search_matches_builder_name_and_role", "[proc][builder][recipe]" )
{
    proc::reset();
    std::istringstream input( R"(
{
  "type": "PROC",
  "id": "sandwich",
  "cat": "food",
  "res": "sandwich_generic",
  "slot": [
    { "id": "top", "role": "bread", "min": 1, "max": 1, "ok": [ "tag:bread" ] }
  ]
}
    )" );
    JsonIn jsin( input );
    const auto jo = jsin.get_object();
    jo.get_string( "type" );
    proc::load( jo, "test" );

    const auto rec = load_recipe_for_test( R"(
{
  "type": "recipe",
  "result": "sandwich_generic",
  "time": 100,
  "difficulty": 0,
  "category": "CC_FOOD",
  "subcategory": "CSC_FOOD_MEAT",
  "description": "proc recipe test",
  "proc": true,
  "proc_id": "sandwich",
  "builder_name": "Sandwich",
  "builder_desc": "Pick bread and fillings.",
  "components": [],
  "qualities": [],
  "tools": []
}
    )" );

    auto subset = recipe_subset{};
    subset.include( &rec );
    CHECK( subset.search( "Sandwich" ).size() == 1 );
    CHECK( subset.search( "c:bread" ).size() == 1 );
    proc::reset();
}

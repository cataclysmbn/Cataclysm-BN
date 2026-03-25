#include "catch/catch.hpp"

#include <vector>

#include "catalua_impl.h"
#include "catalua_sol.h"
#include "procgen/proc_item.h"

TEST_CASE( "proc_lua_full_bridge_reads_named_function", "[proc][lua]" )
{
    auto state = cata::lua_state{};
    state.lua.open_libraries( sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::string,
                              sol::lib::table );
    state.lua.script( R"(
 procgen = {
  food = {
    full = function(params)
      return {
        kcal = 777,
        mass_g = 333,
        volume_ml = 222,
        name = "lua sandwich",
        description = "lua description",
        vit = { vitC = 12 }
      }
    end,
    make = function(params)
      return {
        name = params.blob.name,
        mode = "compact"
      }
    end
  }
}
    )" );

    auto sch = proc::schema{};
    sch.id = proc::schema_id( "sandwich" );
    sch.res = itype_id( "sandwich_generic" );
    sch.lua.full = "procgen.food.full";
    sch.lua.make = "procgen.food.make";

    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "bread" );
    fact.kcal = 50;

    auto full = proc::run_full( sch, { fact }, proc::fast_blob{}, { .state = &state, .picks = {} } );
    CHECK( full.data.kcal == 777 );
    CHECK( full.data.mass_g == 333 );
    CHECK( full.data.volume_ml == 222 );
    CHECK( full.data.name == "lua sandwich" );
    CHECK( full.data.description == "lua description" );
    CHECK( full.data.vit.at( vitamin_id( "vitC" ) ) == 12 );

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::none;
    opts.state = &state;
    const auto made = proc::make_item( sch, { fact }, opts );
    REQUIRE( proc::read_payload( *made ) );
    CHECK( proc::read_payload( *made )->mode == proc::hist::compact );
}

TEST_CASE( "proc_lua_missing_runtime_keeps_preview_blob", "[proc][lua]" )
{
    auto sch = proc::schema{};
    sch.id = proc::schema_id( "sandwich" );
    sch.res = itype_id( "sandwich_generic" );
    sch.lua.full = "procgen.food.full";

    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "bread" );
    fact.kcal = 90;

    auto preview = proc::fast_blob{};
    preview.kcal = 90;
    preview.mass_g = 60;
    preview.volume_ml = 125;
    preview.name = "fallback sandwich";
    preview.description = "fallback description";

    const auto full = proc::run_full( sch, { fact }, preview );
    CHECK( full.data.kcal == 90 );
    CHECK( full.data.mass_g == 60 );
    CHECK( full.data.volume_ml == 125 );
    CHECK( full.data.name == "fallback sandwich" );
    CHECK( full.data.description == "fallback description" );
}

TEST_CASE( "proc_lua_validate_returns_reason", "[proc][lua]" )
{
    auto state = cata::lua_state{};
    state.lua.open_libraries( sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::string,
                              sol::lib::table );
    state.lua.script( R"(
 procgen = {
  food = {
    validate = function(params)
      return { err = "needs more variety" }
    end
  }
 }
    )" );

    auto sch = proc::schema{};
    sch.id = proc::schema_id( "trail_mix" );
    sch.res = itype_id( "trail_mix_generic" );
    sch.lua.validate = "procgen.food.validate";

    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "chocolate" );
    fact.kcal = 100;

    const auto valid = proc::validate_selection( sch, { fact }, proc::fast_blob{}, { .state = &state } );
    CHECK_FALSE( valid.has_value() );
    CHECK( valid.error() == "needs more variety" );
}

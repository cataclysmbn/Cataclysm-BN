#include "catch/catch.hpp"

#include <vector>

#include "catalua_impl.h"
#include "catalua_sol.h"
#include "proc_item.h"

TEST_CASE( "proc_lua_full_bridge_reads_named_function", "[proc][lua]" )
{
    auto state = cata::lua_state{};
    state.lua.open_libraries( sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::string,
                              sol::lib::table );
    state.lua.script( R"(
proc = {
  food = {
    full = function(params)
      return {
        kcal = 777,
        mass_g = 333,
        volume_ml = 222,
        name = "lua sandwich",
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
    sch.lua.full = "proc.food.full";
    sch.lua.make = "proc.food.make";

    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "bread" );
    fact.kcal = 50;

    auto full = proc::run_full( sch, { fact }, proc::fast_blob{}, { .state = &state } );
    CHECK( full.data.kcal == 777 );
    CHECK( full.data.mass_g == 333 );
    CHECK( full.data.volume_ml == 222 );
    CHECK( full.data.name == "lua sandwich" );
    CHECK( full.data.vit.at( vitamin_id( "vitC" ) ) == 12 );

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::none;
    opts.state = &state;
    const auto made = proc::make_item( sch, { fact }, opts );
    REQUIRE( proc::read_payload( *made ) );
    CHECK( proc::read_payload( *made )->mode == proc::hist::compact );
}

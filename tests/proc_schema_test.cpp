#include "catch/catch.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "json.h"
#include "proc_schema.h"

namespace
{

auto load_test_schema( const std::string &json ) -> proc::schema
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

} // namespace

TEST_CASE( "proc_schema_terse_keys_parse", "[proc][schema]" )
{
    const auto loaded = load_test_schema( R"(
{
  "type": "PROC",
  "id": "sandwich",
  "cat": "food",
  "res": "2x4",
  "hist": {
    "def": "compact",
    "ok": [ "none", "compact", "full" ]
  },
  "slot": [
    {
      "id": "top",
      "role": "bread",
      "min": 1,
      "max": 1,
      "rep": false,
      "ok": [ "tag:bread" ],
      "no": [ "flag:LIQUID" ]
    }
  ]
}
    )" );

    CHECK( loaded.id == proc::schema_id( "sandwich" ) );
    CHECK( loaded.cat == "food" );
    CHECK( loaded.res == itype_id( "2x4" ) );
    CHECK( loaded.hist.def == proc::hist::compact );
    CHECK( loaded.hist.ok == std::vector<proc::hist> { proc::hist::none, proc::hist::compact, proc::hist::full } );
    REQUIRE( loaded.slots.size() == 1 );
    CHECK( loaded.slots.front().id == proc::slot_id( "top" ) );
    CHECK( loaded.slots.front().role == "bread" );
    CHECK( loaded.slots.front().min == 1 );
    CHECK( loaded.slots.front().max == 1 );
    CHECK_FALSE( loaded.slots.front().rep );
    CHECK( loaded.slots.front().ok == std::vector<std::string> { "tag:bread" } );
    CHECK( loaded.slots.front().no == std::vector<std::string> { "flag:LIQUID" } );
}

TEST_CASE( "proc_schema_registry_loads_by_id", "[proc][schema]" )
{
    proc::reset();

    std::istringstream input( R"(
{
  "type": "PROC",
  "id": "knife_spear",
  "cat": "melee",
  "res": "spear_knife",
  "hist": {
    "def": "full",
    "ok": [ "compact", "full" ]
  },
  "slot": []
}
    )" );
    JsonIn jsin( input );
    const auto jo = jsin.get_object();
    jo.get_string( "type" );
    proc::load( jo, "test" );

    REQUIRE( proc::has( proc::schema_id( "knife_spear" ) ) );
    CHECK( proc::get( proc::schema_id( "knife_spear" ) ).cat == "melee" );
    CHECK( proc::get( proc::schema_id( "knife_spear" ) ).hist.def == proc::hist::full );

    proc::reset();
}

TEST_CASE( "proc_sandwich_schema_allows_three_breads", "[proc][schema]" )
{
    proc::reset();

    auto file = std::ifstream( "data/json/proc/sandwich.json", std::ios::binary );
    REQUIRE( file.is_open() );

    auto jsin = JsonIn( file );
    for( JsonObject jo : jsin.get_array() ) {
        jo.allow_omitted_members();
        const auto type = jo.get_string( "type" );
        if( type != "PROC" || jo.get_string( "id" ) != "sandwich" ) {
            continue;
        }
        proc::load( jo, "data/json/proc/sandwich.json" );
    }

    REQUIRE( proc::has( proc::schema_id( "sandwich" ) ) );
    const auto &loaded = proc::get( proc::schema_id( "sandwich" ) );
    const auto slot = std::ranges::find_if( loaded.slots, []( const proc::slot_data & entry ) {
        return entry.id == proc::slot_id( "bread" );
    } );
    REQUIRE( slot != loaded.slots.end() );
    CHECK( slot->min == 2 );
    CHECK( slot->max == 3 );
    CHECK( slot->rep );

    proc::reset();
}

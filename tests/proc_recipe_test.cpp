#include "catch/catch.hpp"

#include <fstream>
#include <ranges>
#include <sstream>
#include <string>

#include "avatar.h"
#include "calendar.h"
#include "item.h"
#include "json.h"
#include "map.h"
#include "state_helpers.h"
#include "recipe.h"

namespace
{

auto load_test_recipe( const std::string &json ) -> recipe
{
    std::istringstream input( json );
    JsonIn jsin( input );

    auto rec = recipe();
    rec.load( jsin.get_object(), "test" );
    return rec;
}

} // namespace

TEST_CASE( "proc_stew_recipe_requires_cutting_and_boiling", "[proc][recipe]" )
{
    auto file = std::ifstream( "data/json/recipes/food/proc_stew.json", std::ios::binary );
    REQUIRE( file.is_open() );

    auto jsin = JsonIn( file );
    auto rec = recipe{};
    auto loaded = false;
    for( JsonObject jo : jsin.get_array() ) {
        jo.allow_omitted_members();
        if( jo.get_string( "type" ) != "recipe" || jo.get_string( "result" ) != "stew_generic" ) {
            continue;
        }
        rec.load( jo, "data/json/recipes/food/proc_stew.json" );
        loaded = true;
        break;
    }

    REQUIRE( loaded );
    rec.finalize();
    const auto &qualities = rec.simple_requirements().get_qualities();
    const auto has_quality = [&]( const quality_id & id, const int level ) {
        return std::ranges::any_of( qualities, [&]( const std::vector<quality_requirement> &alt ) {
            return std::ranges::any_of( alt, [&]( const quality_requirement & entry ) {
                return entry.type == id && entry.level == level;
            } );
        } );
    };

    CHECK( has_quality( quality_id( "CUT" ), 1 ) );
    CHECK( has_quality( quality_id( "BOIL" ), 2 ) );
}

TEST_CASE( "proc_stew_recipe_readiness_requires_cutting_and_boiling_tools", "[proc][recipe]" )
{
    clear_all_state();
    auto &who = get_avatar();
    auto &here = get_map();
    who.setpos( tripoint( 60, 60, 0 ) );

    auto file = std::ifstream( "data/json/recipes/food/proc_stew.json", std::ios::binary );
    REQUIRE( file.is_open() );

    auto jsin = JsonIn( file );
    auto rec = recipe{};
    auto loaded = false;
    for( JsonObject jo : jsin.get_array() ) {
        jo.allow_omitted_members();
        if( jo.get_string( "type" ) != "recipe" || jo.get_string( "result" ) != "stew_generic" ) {
            continue;
        }
        rec.load( jo, "data/json/recipes/food/proc_stew.json" );
        loaded = true;
        break;
    }

    REQUIRE( loaded );
    rec.finalize();

    const auto can_start = [&]() {
        who.invalidate_crafting_inventory();
        return rec.simple_requirements().can_make_with_inventory( who.crafting_inventory(),
                rec.get_component_filter(), 1, cost_adjustment::start_only );
    };

    CHECK_FALSE( can_start() );

    here.add_item( who.pos(), item::spawn( itype_id( "knife_butcher" ), calendar::turn ) );
    CHECK_FALSE( can_start() );

    here.add_item( who.pos(), item::spawn( itype_id( "pot" ), calendar::turn ) );
    CHECK( can_start() );
}

TEST_CASE( "proc_recipe_fields_parse", "[proc][recipe]" )
{
    const auto rec = load_test_recipe( R"(
{
  "type": "recipe",
  "result": "2x4",
  "time": 100,
  "difficulty": 0,
  "category": "CC_OTHER",
  "subcategory": "CSC_OTHER_MISC",
  "description": "proc recipe test",
  "proc": true,
  "proc_id": "sandwich",
  "builder_name": "Sandwich builder",
  "builder_desc": "Pick fillings.",
  "components": [],
  "qualities": [],
  "tools": []
}
    )" );

    CHECK( rec.is_proc() );
    CHECK( rec.proc_id() == proc::schema_id( "sandwich" ) );
    CHECK( rec.builder_name().translated() == "Sandwich builder" );
    CHECK( rec.builder_desc().translated() == "Pick fillings." );
    CHECK( rec.get_consistency_error().empty() );
}

TEST_CASE( "proc_recipe_requires_proc_id", "[proc][recipe]" )
{
    const auto rec = load_test_recipe( R"(
{
  "type": "recipe",
  "result": "2x4",
  "time": 100,
  "difficulty": 0,
  "category": "CC_OTHER",
  "subcategory": "CSC_OTHER_MISC",
  "description": "proc recipe test",
  "proc": true,
  "components": [],
  "qualities": [],
  "tools": []
}
    )" );

    CHECK( rec.get_consistency_error() == "is proc but missing proc_id" );
}

TEST_CASE( "proc_recipe_rejects_proc_id_without_proc", "[proc][recipe]" )
{
    const auto rec = load_test_recipe( R"(
{
  "type": "recipe",
  "result": "2x4",
  "time": 100,
  "difficulty": 0,
  "category": "CC_OTHER",
  "subcategory": "CSC_OTHER_MISC",
  "description": "proc recipe test",
  "proc_id": "sandwich",
  "components": [],
  "qualities": [],
  "tools": []
}
    )" );

    CHECK( rec.get_consistency_error() == "specifies proc_id but proc is false" );
}

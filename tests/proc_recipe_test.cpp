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
#include "proc_fact.h"
#include "proc_recipe.h"
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

auto load_recipe_from_file( const std::string &path, const std::string &result ) -> recipe
{
    auto file = std::ifstream( path, std::ios::binary );
    REQUIRE( file.is_open() );

    auto jsin = JsonIn( file );
    auto rec = recipe {};
    auto loaded = false;
    for( JsonObject jo : jsin.get_array() ) {
        jo.allow_omitted_members();
        if( jo.get_string( "type" ) != "recipe" || jo.get_string( "result" ) != result ) {
            continue;
        }
        rec.load( jo, path );
        loaded = true;
        break;
    }

    REQUIRE( loaded );
    rec.finalize();
    return rec;
}

auto has_quality_requirement( const requirement_data &reqs, const quality_id &id,
                              const int level ) -> bool
{
    const auto &qualities = reqs.get_qualities();
    return std::ranges::any_of( qualities, [&]( const std::vector<quality_requirement> &alt ) {
        return std::ranges::any_of( alt, [&]( const quality_requirement & entry ) {
            return entry.type == id && entry.level == level;
        } );
    } );
}

} // namespace

TEST_CASE( "proc_stew_recipe_requires_static_boiling_only", "[proc][recipe]" )
{
    const auto rec = load_recipe_from_file( "data/json/recipes/food/proc_stew.json", "stew_generic" );

    CHECK_FALSE( has_quality_requirement( rec.simple_requirements(), quality_id( "CUT" ), 1 ) );
    CHECK( has_quality_requirement( rec.simple_requirements(), quality_id( "BOIL" ), 2 ) );
}

TEST_CASE( "proc_stew_recipe_adds_cutting_for_raw_selected_ingredients", "[proc][recipe]" )
{
    const auto rec = load_recipe_from_file( "data/json/recipes/food/proc_stew.json", "stew_generic" );
    const auto broth = proc::normalize_part_fact( item( "broth" ), { .ix = 1 } );
    const auto carrot = proc::normalize_part_fact( item( "carrot" ), { .ix = 2 } );

    const auto reqs = proc::recipe_requirements( rec, { broth, carrot } );

    CHECK( has_quality_requirement( reqs, quality_id( "CUT" ), 1 ) );
    CHECK( has_quality_requirement( reqs, quality_id( "BOIL" ), 2 ) );
}

TEST_CASE( "proc_stew_recipe_skips_cutting_for_prepared_selected_ingredients", "[proc][recipe]" )
{
    const auto rec = load_recipe_from_file( "data/json/recipes/food/proc_stew.json", "stew_generic" );
    const auto broth = proc::normalize_part_fact( item( "broth" ), { .ix = 1 } );
    const auto cooked_beans = proc::normalize_part_fact( item( "beans_cooked" ), { .ix = 2 } );
    const auto cooked_meat = proc::normalize_part_fact( item( "meat_cooked" ), { .ix = 3 } );

    const auto reqs = proc::recipe_requirements( rec, { broth, cooked_beans, cooked_meat } );

    CHECK_FALSE( has_quality_requirement( reqs, quality_id( "CUT" ), 1 ) );
    CHECK( has_quality_requirement( reqs, quality_id( "BOIL" ), 2 ) );
}

TEST_CASE( "proc_stew_recipe_readiness_depends_on_selected_ingredient_tools", "[proc][recipe]" )
{
    clear_all_state();
    auto &who = get_avatar();
    auto &here = get_map();
    who.setpos( tripoint( 60, 60, 0 ) );

    const auto rec = load_recipe_from_file( "data/json/recipes/food/proc_stew.json", "stew_generic" );
    const auto raw_requirements = proc::recipe_requirements( rec, {
        proc::normalize_part_fact( item( "broth" ), { .ix = 1 } ),
        proc::normalize_part_fact( item( "carrot" ), { .ix = 2 } )
    } );
    const auto prepared_requirements = proc::recipe_requirements( rec, {
        proc::normalize_part_fact( item( "broth" ), { .ix = 3 } ),
        proc::normalize_part_fact( item( "beans_cooked" ), { .ix = 4 } )
    } );

    const auto can_start = [&]( const requirement_data & reqs ) {
        who.invalidate_crafting_inventory();
        return reqs.can_make_with_inventory( who.crafting_inventory(),
                                             rec.get_component_filter(), 1, cost_adjustment::start_only );
    };

    CHECK_FALSE( can_start( raw_requirements ) );
    CHECK_FALSE( can_start( prepared_requirements ) );

    here.add_item( who.pos(), item::spawn( itype_id( "knife_butcher" ), calendar::turn ) );
    CHECK_FALSE( can_start( raw_requirements ) );
    CHECK_FALSE( can_start( prepared_requirements ) );

    here.add_item( who.pos(), item::spawn( itype_id( "pot" ), calendar::turn ) );
    CHECK( can_start( raw_requirements ) );
    CHECK( can_start( prepared_requirements ) );

    clear_all_state();
    who.setpos( tripoint( 60, 60, 0 ) );

    here.add_item( who.pos(), item::spawn( itype_id( "pot" ), calendar::turn ) );
    CHECK_FALSE( can_start( raw_requirements ) );
    CHECK( can_start( prepared_requirements ) );
}

TEST_CASE( "proc_sword_recipe_requires_selected_material_tools", "[proc][recipe][weapon]" )
{
    const auto rec = load_recipe_from_file( "data/json/recipes/weapon/proc_sword.json",
                                            "proc_sword_generic" );
    const auto steel_chunk = proc::normalize_part_fact( item( "steel_chunk" ), { .ix = 1 } );
    const auto stick_long = proc::normalize_part_fact( item( "stick_long" ), { .ix = 2 } );
    const auto rag = proc::normalize_part_fact( item( "rag" ), { .ix = 3 } );

    CHECK_FALSE( has_quality_requirement( rec.simple_requirements(), quality_id( "CUT" ), 1 ) );
    CHECK_FALSE( has_quality_requirement( rec.simple_requirements(), quality_id( "HAMMER" ), 1 ) );

    const auto metal_requirements = proc::recipe_requirements( rec, { steel_chunk } );
    CHECK_FALSE( has_quality_requirement( metal_requirements, quality_id( "CUT" ), 1 ) );
    CHECK( has_quality_requirement( metal_requirements, quality_id( "HAMMER" ), 1 ) );

    const auto wood_requirements = proc::recipe_requirements( rec, { stick_long, rag } );
    CHECK( has_quality_requirement( wood_requirements, quality_id( "CUT" ), 1 ) );
    CHECK_FALSE( has_quality_requirement( wood_requirements, quality_id( "HAMMER" ), 1 ) );

    const auto mixed_requirements = proc::recipe_requirements( rec, { steel_chunk, stick_long, rag } );
    CHECK( has_quality_requirement( mixed_requirements, quality_id( "CUT" ), 1 ) );
    CHECK( has_quality_requirement( mixed_requirements, quality_id( "HAMMER" ), 1 ) );
}

TEST_CASE( "proc_sword_recipe_readiness_depends_on_selected_material_tools",
           "[proc][recipe][weapon]" )
{
    clear_all_state();
    auto &who = get_avatar();
    auto &here = get_map();
    who.setpos( tripoint( 60, 60, 0 ) );

    const auto rec = load_recipe_from_file( "data/json/recipes/weapon/proc_sword.json",
                                            "proc_sword_generic" );
    const auto metal_requirements = proc::recipe_requirements( rec, {
        proc::normalize_part_fact( item( "steel_chunk" ), { .ix = 1 } )
    } );
    const auto wood_requirements = proc::recipe_requirements( rec, {
        proc::normalize_part_fact( item( "stick_long" ), { .ix = 2 } ),
        proc::normalize_part_fact( item( "rag" ), { .ix = 3 } )
    } );
    const auto mixed_requirements = proc::recipe_requirements( rec, {
        proc::normalize_part_fact( item( "steel_chunk" ), { .ix = 4 } ),
        proc::normalize_part_fact( item( "stick_long" ), { .ix = 5 } ),
        proc::normalize_part_fact( item( "rag" ), { .ix = 6 } )
    } );

    const auto can_start = [&]( const requirement_data & reqs ) {
        who.invalidate_crafting_inventory();
        return reqs.can_make_with_inventory( who.crafting_inventory(),
                                             rec.get_component_filter(), 1, cost_adjustment::start_only );
    };

    CHECK_FALSE( can_start( metal_requirements ) );
    CHECK_FALSE( can_start( wood_requirements ) );
    CHECK_FALSE( can_start( mixed_requirements ) );

    here.add_item( who.pos(), item::spawn( itype_id( "knife_butcher" ), calendar::turn ) );
    CHECK_FALSE( can_start( metal_requirements ) );
    CHECK( can_start( wood_requirements ) );
    CHECK_FALSE( can_start( mixed_requirements ) );

    here.add_item( who.pos(), item::spawn( itype_id( "hammer" ), calendar::turn ) );
    CHECK( can_start( metal_requirements ) );
    CHECK( can_start( wood_requirements ) );
    CHECK( can_start( mixed_requirements ) );
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

TEST_CASE( "proc_recipe_preview_description_avoids_placeholder_copy_from_text", "[proc][recipe]" )
{
    const auto sandwich = load_recipe_from_file( "data/json/recipes/food/proc_sandwich.json",
                          "sandwich_generic" );
    const auto sword = load_recipe_from_file( "data/json/recipes/weapon/proc_sword.json",
                       "proc_sword_generic" );

    CHECK_FALSE( proc::recipe_preview_description( sandwich ).contains( "If you can see this" ) );
    CHECK_FALSE( proc::recipe_preview_description( sword ).contains( "If you can see this" ) );
    CHECK_FALSE( proc::recipe_preview_description( sandwich ).empty() );
    CHECK_FALSE( proc::recipe_preview_description( sword ).empty() );
}

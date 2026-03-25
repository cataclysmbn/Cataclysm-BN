#include "catch/catch.hpp"

#include <fstream>
#include <iterator>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

#include "calendar.h"
#include "item.h"
#include "json.h"
#include "procgen/proc_builder.h"
#include "procgen/proc_fact.h"
#include "procgen/proc_item.h"
#include "procgen/proc_schema.h"
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

auto load_schema_from_file( const std::string &path, const std::string &id ) -> proc::schema
{
    proc::reset();

    auto file = std::ifstream( path, std::ios::binary );
    REQUIRE( file.is_open() );

    auto jsin = JsonIn( file );
    for( JsonObject jo : jsin.get_array() ) {
        jo.allow_omitted_members();
        if( jo.get_string( "type" ) != "PROC" || jo.get_string( "id" ) != id ) {
            continue;
        }
        proc::load( jo, path );
    }

    REQUIRE( proc::has( proc::schema_id( id ) ) );
    const auto loaded = proc::get( proc::schema_id( id ) );
    proc::reset();
    return loaded;
}

auto make_proc_test_item( const itype_id &id, const proc::schema_id &schema,
                          const std::string &fp ) -> detached_ptr<item>
{
    auto crafted = item::spawn( id, calendar::turn );
    auto payload = proc::payload{};
    payload.id = schema;
    payload.fp = fp;
    payload.blob.name = crafted->type_name();
    proc::write_payload( *crafted, payload );
    return crafted;
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

TEST_CASE( "proc_builder_rejects_malformed_quality_atoms_without_throwing", "[proc][builder]" )
{
    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "knife_butcher" );
    fact.qual.emplace( quality_id( "CUT" ), 2 );

    CHECK_FALSE( proc::matches_atom( fact, "qual:CUT>=" ) );
    CHECK_FALSE( proc::matches_atom( fact, "qual:CUT>=x" ) );
    CHECK_FALSE( proc::matches_atom( fact, "qual:>=2" ) );
    CHECK_FALSE( proc::matches_atom( fact, "qual:CUT>" ) );
}

TEST_CASE( "proc_builder_search_matches_name_location_and_fact_tokens", "[proc][builder]" )
{
    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "knife_butcher" );
    fact.tag = { "blade", "knife" };
    fact.flag = { flag_id( "STAB" ) };
    fact.mat = { material_id( "steel" ) };
    fact.qual.emplace( quality_id( "CUT" ), 2 );

    const auto opts = proc::part_search_options{
        .name = "Combat Knife",
        .where = "backpack"
    };

    CHECK( proc::part_matches_search( fact, opts, "combat" ) );
    CHECK( proc::part_matches_search( fact, opts, "backpack" ) );
    CHECK( proc::part_matches_search( fact, opts, "itype:knife_butcher" ) );
    CHECK( proc::part_matches_search( fact, opts, "tag:knife" ) );
    CHECK( proc::part_matches_search( fact, opts, "mat:steel" ) );
    CHECK( proc::part_matches_search( fact, opts, "flag:STAB" ) );
    CHECK( proc::part_matches_search( fact, opts, "qual:CUT>=2" ) );
    CHECK_FALSE( proc::part_matches_search( fact, opts, "mat:wheat" ) );
    CHECK_FALSE( proc::part_matches_search( fact, opts, "qual:CUT>=3" ) );
}

TEST_CASE( "proc_builder_search_requires_all_query_terms", "[proc][builder]" )
{
    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "knife_butcher" );
    fact.tag = { "blade", "knife" };
    fact.flag = { flag_id( "STAB" ) };
    fact.mat = { material_id( "steel" ) };
    fact.qual.emplace( quality_id( "CUT" ), 2 );

    const auto opts = proc::part_search_options{
        .name = "Combat Knife",
        .where = "backpack"
    };

    CHECK( proc::part_matches_search( fact, opts, "combat backpack" ) );
    CHECK( proc::part_matches_search( fact, opts, "tag:knife backpack" ) );
    CHECK( proc::part_matches_search( fact, opts, "itype:knife_butcher qual:CUT>=2" ) );
    CHECK_FALSE( proc::part_matches_search( fact, opts, "combat pantry" ) );
    CHECK_FALSE( proc::part_matches_search( fact, opts, "tag:knife mat:wheat" ) );
}

TEST_CASE( "proc_builder_search_rejects_malformed_quality_terms_without_throwing",
           "[proc][builder]" )
{
    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "knife_butcher" );
    fact.qual.emplace( quality_id( "CUT" ), 2 );

    const auto opts = proc::part_search_options{
        .name = "Combat Knife",
        .where = "backpack"
    };

    CHECK_FALSE( proc::part_matches_search( fact, opts, "qual:CUT>=" ) );
    CHECK_FALSE( proc::part_matches_search( fact, opts, "qual:CUT>=x" ) );
    CHECK_FALSE( proc::part_matches_search( fact, opts, "combat qual:>=2" ) );
    CHECK( proc::part_matches_search( fact, opts, "qual:CUT>=2" ) );
    CHECK_FALSE( proc::part_matches_search( fact, opts, "qual:CUT>=3" ) );
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

TEST_CASE( "proc_builder_limits_repeated_charge_picks", "[proc][builder]" )
{
    const auto sch = load_schema_for_test( R"(
{
  "type": "PROC",
  "id": "stew",
  "cat": "food",
  "res": "stew_generic",
  "slot": [
    { "id": "base", "role": "base", "min": 1, "max": 2, "rep": true, "ok": [ "itype:broth" ] }
  ]
}
    )" );

    auto broth = proc::part_fact{};
    broth.ix = 1;
    broth.id = itype_id( "broth" );
    broth.mass_g = 250;
    broth.volume_ml = 250;
    broth.kcal = 13;
    broth.chg = 1;
    broth.uses = 2;

    auto state = proc::build_state( sch, { broth } );
    REQUIRE( proc::add_pick( state, sch, proc::slot_id( "base" ), 1 ) );
    REQUIRE( proc::add_pick( state, sch, proc::slot_id( "base" ), 1 ) );
    CHECK_FALSE( proc::add_pick( state, sch, proc::slot_id( "base" ), 1 ) );
}

TEST_CASE( "proc_builder_rejects_picks_that_do_not_match_the_target_slot", "[proc][builder]" )
{
    const auto sch = load_schema_for_test( R"(
{
  "type": "PROC",
  "id": "sandwich",
  "cat": "food",
  "res": "sandwich_generic",
  "slot": [
    { "id": "top", "role": "bread", "min": 1, "max": 1, "ok": [ "tag:bread" ] },
    { "id": "fill", "role": "meat", "min": 0, "max": 1, "ok": [ "tag:meat" ] }
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

    auto state = proc::build_state( sch, { bread, meat } );
    CHECK_FALSE( proc::add_pick( state, sch, proc::slot_id( "top" ), 2 ) );
    CHECK_FALSE( proc::add_pick( state, sch, proc::slot_id( "fill" ), 1 ) );
    CHECK( state.chosen.empty() );
    CHECK( state.fast.mass_g == 0 );
    CHECK( state.fast.volume_ml == 0 );
    CHECK( state.fast.kcal == 0 );
}

TEST_CASE( "proc_builder_filters_out_exhausted_candidates", "[proc][builder]" )
{
    const auto sch = load_schema_for_test( R"(
{
  "type": "PROC",
  "id": "battery_pack",
  "cat": "other",
  "res": "battery",
  "slot": [
    { "id": "cell", "role": "cell", "min": 1, "max": 3, "rep": true, "ok": [] }
  ]
}
    )" );

    auto repeatable = proc::part_fact{};
    repeatable.ix = 1;
    repeatable.id = itype_id( "battery" );
    repeatable.uses = 2;

    auto spare = proc::part_fact{};
    spare.ix = 2;
    spare.id = itype_id( "battery" );
    spare.uses = 1;

    auto state = proc::build_state( sch, { repeatable, spare } );
    const auto candidates = state.cand.at( proc::slot_id( "cell" ) );
    CHECK( proc::remaining_uses( state, 1 ) == 2 );
    CHECK( proc::filter_available_candidates( state, candidates ) == candidates );

    REQUIRE( proc::add_pick( state, sch, proc::slot_id( "cell" ), 1 ) );
    CHECK( proc::remaining_uses( state, 1 ) == 1 );

    REQUIRE( proc::add_pick( state, sch, proc::slot_id( "cell" ), 1 ) );
    CHECK( proc::remaining_uses( state, 1 ) == 0 );
    CHECK( proc::filter_available_candidates( state, candidates ) == std::vector<proc::part_ix> { 2 } );
}

TEST_CASE( "proc_builder_required_slots_count_total_uses_toward_minimum", "[proc][builder]" )
{
    const auto sch = load_schema_for_test( R"(
{
  "type": "PROC",
  "id": "double_bread",
  "cat": "food",
  "res": "sandwich_generic",
  "slot": [
    { "id": "bread", "role": "bread", "min": 2, "max": 3, "rep": true, "ok": [ "tag:bread" ] }
  ]
}
    )" );

    auto single_use_bread = proc::part_fact{};
    single_use_bread.ix = 1;
    single_use_bread.id = itype_id( "bread" );
    single_use_bread.tag = { "bread" };
    single_use_bread.uses = 1;

    auto double_use_bread = single_use_bread;
    double_use_bread.ix = 2;
    double_use_bread.uses = 2;

    const auto insufficient = proc::build_state( sch, { single_use_bread } );
    CHECK_FALSE( proc::slot_can_meet_minimum( insufficient, sch, proc::slot_id( "bread" ) ) );

    const auto sufficient = proc::build_state( sch, { double_use_bread } );
    CHECK( proc::slot_can_meet_minimum( sufficient, sch, proc::slot_id( "bread" ) ) );
}

TEST_CASE( "proc_builder_debug_facts_skip_items_that_match_no_slots", "[proc][builder]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sandwich.json", "sandwich" );

    CHECK_FALSE( proc::debug_part_fact( sch, item( "rock" ), 1 ).has_value() );
}

TEST_CASE( "proc_builder_debug_facts_sum_matching_slot_capacity", "[proc][builder]" )
{
    const auto sch = load_schema_for_test( R"(
{
  "type": "PROC",
  "id": "debug_overlap",
  "cat": "weapon",
  "res": "proc_sword_generic",
  "slot": [
    { "id": "blade", "role": "blade", "min": 1, "max": 2, "rep": true, "ok": [ "mat:steel" ] },
    { "id": "guard", "role": "guard", "min": 0, "max": 1, "ok": [ "mat:steel" ] }
  ]
}
    )" );

    const auto fact = proc::debug_part_fact( sch, item( "steel_chunk" ), 7 );
    REQUIRE( fact.has_value() );
    CHECK( fact->ix == 7 );
    CHECK( fact->uses == 3 );
}

TEST_CASE( "proc_builder_debug_facts_follow_food_candidate_filtering", "[proc][builder][food]" )
{
    const auto sandwich = load_schema_from_file( "data/json/proc/sandwich.json", "sandwich" );
    const auto stew = load_schema_from_file( "data/json/proc/stew.json", "stew" );

    CHECK_FALSE( proc::debug_part_fact( stew, item( "soup_veggy" ), 1 ).has_value() );

    const auto carrot = proc::debug_part_fact( stew, item( "carrot" ), 2 );
    REQUIRE( carrot.has_value() );
    CHECK( carrot->uses == 4 );

    const auto proc_bread_item = make_proc_test_item( itype_id( "bread" ),
                                 proc::schema_id( "sandwich" ),
                                 "sandwich:bread" );
    CHECK_FALSE( proc::debug_part_fact( sandwich, *proc_bread_item, 3 ).has_value() );
}

TEST_CASE( "proc_builder_sandwich_bread_slot_requires_two_total_bread_uses",
           "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sandwich.json", "sandwich" );

    auto bread = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    bread.uses = 1;

    const auto one_bread = proc::build_state( sch, { bread } );
    CHECK_FALSE( proc::slot_can_meet_minimum( one_bread, sch, proc::slot_id( "bread" ) ) );

    const auto two_breads = proc::build_state( sch, { bread, proc::normalize_part_fact( item( "bread" ), { .ix = 2 } ) } );
    CHECK( proc::slot_can_meet_minimum( two_breads, sch, proc::slot_id( "bread" ) ) );
}

TEST_CASE( "proc_builder_stew_excludes_finished_dishes_from_candidates", "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/stew.json", "stew" );

    const auto broth = proc::normalize_part_fact( item( "broth" ), { .ix = 1 } );
    const auto carrot = proc::normalize_part_fact( item( "carrot" ), { .ix = 2 } );
    const auto cooked_meat = proc::normalize_part_fact( item( "meat_cooked" ), { .ix = 3 } );
    const auto veggie_soup = proc::normalize_part_fact( item( "soup_veggy" ), { .ix = 4 } );
    const auto meat_curry = proc::normalize_part_fact( item( "curry_meat" ), { .ix = 5 } );

    const auto state = proc::build_state( sch, { broth, carrot, cooked_meat, veggie_soup, meat_curry } );
    const auto &veg_candidates = state.cand.at( proc::slot_id( "veg" ) );
    const auto &meat_candidates = state.cand.at( proc::slot_id( "meat" ) );

    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 2 ) ) != veg_candidates.end() );
    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 4 ) ) == veg_candidates.end() );
    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 5 ) ) == veg_candidates.end() );
    CHECK( std::ranges::find( meat_candidates, proc::part_ix( 3 ) ) != meat_candidates.end() );
    CHECK( std::ranges::find( meat_candidates, proc::part_ix( 5 ) ) == meat_candidates.end() );
}

TEST_CASE( "proc_builder_stew_rejects_nonfood_material_matches", "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/stew.json", "stew" );

    const auto broth = proc::normalize_part_fact( item( "broth" ), { .ix = 1 } );
    const auto carrot = proc::normalize_part_fact( item( "carrot" ), { .ix = 2 } );
    const auto plant_fiber = proc::normalize_part_fact( item( "plant_fibre" ), { .ix = 3 } );
    const auto cooked_meat = proc::normalize_part_fact( item( "meat_cooked" ), { .ix = 4 } );
    const auto sinew = proc::normalize_part_fact( item( "sinew" ), { .ix = 5 } );

    const auto state = proc::build_state( sch, { broth, carrot, plant_fiber, cooked_meat, sinew } );
    const auto &veg_candidates = state.cand.at( proc::slot_id( "veg" ) );
    const auto &meat_candidates = state.cand.at( proc::slot_id( "meat" ) );

    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 2 ) ) != veg_candidates.end() );
    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 3 ) ) == veg_candidates.end() );
    CHECK( std::ranges::find( meat_candidates, proc::part_ix( 4 ) ) != meat_candidates.end() );
    CHECK( std::ranges::find( meat_candidates, proc::part_ix( 5 ) ) == meat_candidates.end() );
}

TEST_CASE( "proc_builder_stew_excludes_condiment_spreads_from_veg_candidates",
           "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/stew.json", "stew" );

    const auto broth = proc::normalize_part_fact( item( "broth" ), { .ix = 1 } );
    const auto carrot = proc::normalize_part_fact( item( "carrot" ), { .ix = 2 } );
    const auto ketchup = proc::normalize_part_fact( item( "ketchup" ), { .ix = 3 } );

    const auto state = proc::build_state( sch, { broth, carrot, ketchup } );
    const auto &veg_candidates = state.cand.at( proc::slot_id( "veg" ) );

    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 2 ) ) != veg_candidates.end() );
    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 3 ) ) == veg_candidates.end() );
}

TEST_CASE( "proc_builder_stew_keeps_liquid_bases_out_of_veg_candidates",
           "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/stew.json", "stew" );

    const auto broth = proc::normalize_part_fact( item( "broth" ), { .ix = 1 } );
    const auto bone_broth = proc::normalize_part_fact( item( "broth_bone" ), { .ix = 2 } );
    const auto carrot = proc::normalize_part_fact( item( "carrot" ), { .ix = 3 } );

    const auto state = proc::build_state( sch, { broth, bone_broth, carrot } );
    const auto &base_candidates = state.cand.at( proc::slot_id( "base" ) );
    const auto &veg_candidates = state.cand.at( proc::slot_id( "veg" ) );

    CHECK( std::ranges::find( base_candidates, proc::part_ix( 1 ) ) != base_candidates.end() );
    CHECK( std::ranges::find( base_candidates, proc::part_ix( 2 ) ) != base_candidates.end() );
    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 1 ) ) == veg_candidates.end() );
    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 2 ) ) == veg_candidates.end() );
    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 3 ) ) != veg_candidates.end() );
}

TEST_CASE( "proc_builder_sandwich_accepts_supported_condiments_in_cond_slot",
           "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sandwich.json", "sandwich" );

    const auto bread_a = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    const auto bread_b = proc::normalize_part_fact( item( "bread" ), { .ix = 2 } );
    const auto butter = proc::normalize_part_fact( item( "butter" ), { .ix = 3 } );
    const auto horseradish = proc::normalize_part_fact( item( "horseradish" ), { .ix = 4 } );
    const auto sauerkraut = proc::normalize_part_fact( item( "sauerkraut" ), { .ix = 5 } );

    const auto state = proc::build_state( sch, { bread_a, bread_b, butter, horseradish, sauerkraut } );
    const auto &cond_candidates = state.cand.at( proc::slot_id( "cond" ) );

    CHECK( std::ranges::find( cond_candidates, proc::part_ix( 3 ) ) != cond_candidates.end() );
    CHECK( std::ranges::find( cond_candidates, proc::part_ix( 4 ) ) != cond_candidates.end() );
    CHECK( std::ranges::find( cond_candidates, proc::part_ix( 5 ) ) != cond_candidates.end() );
}

TEST_CASE( "proc_builder_sandwich_accepts_supported_spreads_in_cond_slot",
           "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sandwich.json", "sandwich" );

    const auto bread_a = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    const auto bread_b = proc::normalize_part_fact( item( "bread" ), { .ix = 2 } );
    const auto honey = proc::normalize_part_fact( item( "honey_bottled" ), { .ix = 3 } );
    const auto jam = proc::normalize_part_fact( item( "jam_fruit" ), { .ix = 4 } );
    const auto peanut_butter = proc::normalize_part_fact( item( "peanutbutter" ), { .ix = 5 } );
    const auto syrup = proc::normalize_part_fact( item( "syrup" ), { .ix = 6 } );

    const auto state = proc::build_state( sch, { bread_a, bread_b, honey, jam, peanut_butter, syrup } );
    const auto &cond_candidates = state.cand.at( proc::slot_id( "cond" ) );

    CHECK( std::ranges::find( cond_candidates, proc::part_ix( 3 ) ) != cond_candidates.end() );
    CHECK( std::ranges::find( cond_candidates, proc::part_ix( 4 ) ) != cond_candidates.end() );
    CHECK( std::ranges::find( cond_candidates, proc::part_ix( 5 ) ) != cond_candidates.end() );
    CHECK( std::ranges::find( cond_candidates, proc::part_ix( 6 ) ) != cond_candidates.end() );
}

TEST_CASE( "proc_builder_sandwich_excludes_non_cheese_dairy_from_cheese_slot",
           "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sandwich.json", "sandwich" );

    const auto bread_a = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    const auto bread_b = proc::normalize_part_fact( item( "bread" ), { .ix = 2 } );
    const auto butter = proc::normalize_part_fact( item( "butter" ), { .ix = 3 } );
    const auto milk = proc::normalize_part_fact( item( "milk" ), { .ix = 4 } );
    const auto cheese = proc::normalize_part_fact( item( "cheese" ), { .ix = 5 } );

    const auto state = proc::build_state( sch, { bread_a, bread_b, butter, milk, cheese } );
    const auto &cheese_candidates = state.cand.at( proc::slot_id( "cheese" ) );

    CHECK( std::ranges::find( cheese_candidates, proc::part_ix( 3 ) ) == cheese_candidates.end() );
    CHECK( std::ranges::find( cheese_candidates, proc::part_ix( 4 ) ) == cheese_candidates.end() );
    CHECK( std::ranges::find( cheese_candidates, proc::part_ix( 5 ) ) != cheese_candidates.end() );
}

TEST_CASE( "proc_builder_sandwich_excludes_payload_marked_raw_food_candidates",
           "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sandwich.json", "sandwich" );

    const auto bread_a = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    const auto bread_b = proc::normalize_part_fact( item( "bread" ), { .ix = 2 } );
    const auto proc_bread_item = make_proc_test_item( itype_id( "bread" ),
                                 proc::schema_id( "sandwich" ),
                                 "sandwich:bread" );
    const auto proc_bread = proc::normalize_part_fact( *proc_bread_item, { .ix = 3 } );
    const auto cheese = proc::normalize_part_fact( item( "cheese" ), { .ix = 4 } );
    const auto proc_cheese_item = make_proc_test_item( itype_id( "cheese" ),
                                  proc::schema_id( "sandwich" ),
                                  "sandwich:cheese" );
    const auto proc_cheese = proc::normalize_part_fact( *proc_cheese_item, { .ix = 5 } );

    const auto state = proc::build_state( sch, { bread_a, bread_b, proc_bread, cheese, proc_cheese } );
    const auto &bread_candidates = state.cand.at( proc::slot_id( "bread" ) );
    const auto &cheese_candidates = state.cand.at( proc::slot_id( "cheese" ) );

    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 1 ) ) != bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 2 ) ) != bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 3 ) ) == bread_candidates.end() );
    CHECK( std::ranges::find( cheese_candidates, proc::part_ix( 4 ) ) != cheese_candidates.end() );
    CHECK( std::ranges::find( cheese_candidates, proc::part_ix( 5 ) ) == cheese_candidates.end() );
}

TEST_CASE( "proc_builder_stew_excludes_payload_marked_raw_food_candidates",
           "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/stew.json", "stew" );

    const auto broth = proc::normalize_part_fact( item( "broth" ), { .ix = 1 } );
    const auto carrot = proc::normalize_part_fact( item( "carrot" ), { .ix = 2 } );
    const auto proc_carrot_item = make_proc_test_item( itype_id( "carrot" ), proc::schema_id( "stew" ),
                                  "stew:carrot" );
    const auto proc_carrot = proc::normalize_part_fact( *proc_carrot_item, { .ix = 3 } );
    const auto cooked_meat = proc::normalize_part_fact( item( "meat_cooked" ), { .ix = 4 } );
    const auto proc_meat_item = make_proc_test_item( itype_id( "meat_cooked" ),
                                proc::schema_id( "stew" ),
                                "stew:meat" );
    const auto proc_meat = proc::normalize_part_fact( *proc_meat_item, { .ix = 5 } );

    const auto state = proc::build_state( sch, { broth, carrot, proc_carrot, cooked_meat, proc_meat } );
    const auto &veg_candidates = state.cand.at( proc::slot_id( "veg" ) );
    const auto &meat_candidates = state.cand.at( proc::slot_id( "meat" ) );

    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 2 ) ) != veg_candidates.end() );
    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 3 ) ) == veg_candidates.end() );
    CHECK( std::ranges::find( meat_candidates, proc::part_ix( 4 ) ) != meat_candidates.end() );
    CHECK( std::ranges::find( meat_candidates, proc::part_ix( 5 ) ) == meat_candidates.end() );
}

TEST_CASE( "proc_builder_sandwich_excludes_prepared_meals_from_raw_slots",
           "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sandwich.json", "sandwich" );

    const auto bread_a = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    const auto bread_b = proc::normalize_part_fact( item( "bread" ), { .ix = 2 } );
    const auto veggie_pie = proc::normalize_part_fact( item( "pie_veggy" ), { .ix = 3 } );
    const auto cheese_fries = proc::normalize_part_fact( item( "cheese_fries" ), { .ix = 4 } );
    const auto burrito = proc::normalize_part_fact( item( "homemade_burrito" ), { .ix = 5 } );
    const auto cheese = proc::normalize_part_fact( item( "cheese" ), { .ix = 6 } );

    const auto state = proc::build_state( sch,
    { bread_a, bread_b, veggie_pie, cheese_fries, burrito, cheese } );
    const auto &bread_candidates = state.cand.at( proc::slot_id( "bread" ) );
    const auto &veg_candidates = state.cand.at( proc::slot_id( "veg" ) );
    const auto &cheese_candidates = state.cand.at( proc::slot_id( "cheese" ) );
    const auto &meat_candidates = state.cand.at( proc::slot_id( "meat" ) );

    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 1 ) ) != bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 2 ) ) != bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 3 ) ) == bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 5 ) ) == bread_candidates.end() );
    CHECK( std::ranges::find( veg_candidates, proc::part_ix( 3 ) ) == veg_candidates.end() );
    CHECK( std::ranges::find( cheese_candidates, proc::part_ix( 4 ) ) == cheese_candidates.end() );
    CHECK( std::ranges::find( cheese_candidates, proc::part_ix( 6 ) ) != cheese_candidates.end() );
    CHECK( std::ranges::find( meat_candidates, proc::part_ix( 5 ) ) == meat_candidates.end() );
}

TEST_CASE( "proc_builder_sandwich_excludes_breakfast_foods_from_bread_slot",
           "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sandwich.json", "sandwich" );

    const auto bread_a = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    const auto bread_b = proc::normalize_part_fact( item( "bread" ), { .ix = 2 } );
    const auto french_toast = proc::normalize_part_fact( item( "frenchtoast" ), { .ix = 3 } );
    const auto toast_em = proc::normalize_part_fact( item( "toastem" ), { .ix = 4 } );
    const auto toaster_pastry = proc::normalize_part_fact( item( "toasterpastry" ), { .ix = 5 } );

    const auto state = proc::build_state( sch,
    { bread_a, bread_b, french_toast, toast_em, toaster_pastry } );
    const auto &bread_candidates = state.cand.at( proc::slot_id( "bread" ) );

    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 1 ) ) != bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 2 ) ) != bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 3 ) ) == bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 4 ) ) == bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 5 ) ) == bread_candidates.end() );
}

TEST_CASE( "proc_builder_sandwich_limits_bread_slot_to_sandwichable_bases",
           "[proc][builder][food]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sandwich.json", "sandwich" );

    const auto bread_a = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    const auto bread_b = proc::normalize_part_fact( item( "bread" ), { .ix = 2 } );
    const auto brioche = proc::normalize_part_fact( item( "brioche" ), { .ix = 3 } );
    const auto sweetbread = proc::normalize_part_fact( item( "sweetbread" ), { .ix = 4 } );
    const auto pancakes = proc::normalize_part_fact( item( "pancakes" ), { .ix = 5 } );
    const auto waffles = proc::normalize_part_fact( item( "waffles" ), { .ix = 6 } );
    const auto crackers = proc::normalize_part_fact( item( "crackers" ), { .ix = 7 } );
    const auto pretzels = proc::normalize_part_fact( item( "pretzels" ), { .ix = 8 } );
    const auto snack_cake = proc::normalize_part_fact( item( "cake2" ), { .ix = 9 } );

    const auto state = proc::build_state( sch,
    { bread_a, bread_b, brioche, sweetbread, pancakes, waffles, crackers, pretzels, snack_cake } );
    const auto &bread_candidates = state.cand.at( proc::slot_id( "bread" ) );

    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 1 ) ) != bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 2 ) ) != bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 3 ) ) != bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 4 ) ) == bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 5 ) ) == bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 6 ) ) == bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 7 ) ) == bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 8 ) ) == bread_candidates.end() );
    CHECK( std::ranges::find( bread_candidates, proc::part_ix( 9 ) ) == bread_candidates.end() );
}

TEST_CASE( "proc_builder_excludes_breakfast_dishes_and_entrees_from_food_slots",
           "[proc][builder][food]" )
{
    const auto sandwich = load_schema_from_file( "data/json/proc/sandwich.json", "sandwich" );
    const auto stew = load_schema_from_file( "data/json/proc/stew.json", "stew" );

    const auto bread_a = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    const auto bread_b = proc::normalize_part_fact( item( "bread" ), { .ix = 2 } );
    const auto cheese = proc::normalize_part_fact( item( "cheese" ), { .ix = 3 } );
    const auto carrot = proc::normalize_part_fact( item( "carrot" ), { .ix = 4 } );
    const auto broth = proc::normalize_part_fact( item( "broth" ), { .ix = 5 } );
    const auto omelette = proc::normalize_part_fact( item( "wild_veggy_omelette" ), { .ix = 6 } );
    const auto mre_entree = proc::normalize_part_fact( item( "mre_cheesetort" ), { .ix = 7 } );

    const auto sandwich_state = proc::build_state( sandwich,
    { bread_a, bread_b, cheese, omelette, mre_entree } );
    const auto &sandwich_veg_candidates = sandwich_state.cand.at( proc::slot_id( "veg" ) );
    const auto &sandwich_cheese_candidates = sandwich_state.cand.at( proc::slot_id( "cheese" ) );

    CHECK( std::ranges::find( sandwich_cheese_candidates, proc::part_ix( 3 ) ) !=
           sandwich_cheese_candidates.end() );
    CHECK( std::ranges::find( sandwich_veg_candidates, proc::part_ix( 6 ) ) ==
           sandwich_veg_candidates.end() );
    CHECK( std::ranges::find( sandwich_veg_candidates, proc::part_ix( 7 ) ) ==
           sandwich_veg_candidates.end() );
    CHECK( std::ranges::find( sandwich_cheese_candidates, proc::part_ix( 7 ) ) ==
           sandwich_cheese_candidates.end() );

    const auto stew_state = proc::build_state( stew, { broth, carrot, omelette, mre_entree } );
    const auto &stew_veg_candidates = stew_state.cand.at( proc::slot_id( "veg" ) );

    CHECK( std::ranges::find( stew_veg_candidates, proc::part_ix( 4 ) ) != stew_veg_candidates.end() );
    CHECK( std::ranges::find( stew_veg_candidates, proc::part_ix( 6 ) ) == stew_veg_candidates.end() );
    CHECK( std::ranges::find( stew_veg_candidates, proc::part_ix( 7 ) ) == stew_veg_candidates.end() );
}

TEST_CASE( "proc_builder_previews_sword_stats_from_materials", "[proc][builder][weapon]" )
{
    const auto sch = load_schema_for_test( R"(
{
  "type": "PROC",
  "id": "sword",
  "cat": "weapon",
  "res": "proc_sword_generic",
  "slot": [
    { "id": "blade", "role": "blade", "min": 1, "max": 1, "ok": [ "itype:steel_chunk" ] },
    { "id": "handle", "role": "handle", "min": 1, "max": 1, "ok": [ "itype:stick_long" ] },
    { "id": "grip", "role": "grip", "min": 1, "max": 2, "rep": true, "ok": [ "itype:rag" ] }
  ]
}
    )" );

    auto blade = proc::part_fact{};
    blade.ix = 1;
    blade.id = itype_id( "steel_chunk" );
    blade.mat = { material_id( "steel" ) };
    blade.mass_g = 1200;
    blade.volume_ml = 500;

    auto handle = proc::part_fact{};
    handle.ix = 2;
    handle.id = itype_id( "stick_long" );
    handle.mat = { material_id( "wood" ) };
    handle.mass_g = 400;
    handle.volume_ml = 500;

    auto grip = proc::part_fact{};
    grip.ix = 3;
    grip.id = itype_id( "rag" );
    grip.mat = { material_id( "cotton" ) };
    grip.mass_g = 50;
    grip.volume_ml = 50;

    auto state = proc::build_state( sch, { blade, handle, grip } );
    REQUIRE( proc::add_pick( state, sch, proc::slot_id( "blade" ), 1 ) );
    REQUIRE( proc::add_pick( state, sch, proc::slot_id( "handle" ), 2 ) );
    REQUIRE( proc::add_pick( state, sch, proc::slot_id( "grip" ), 3 ) );

    CHECK( state.fast.melee.bash > 0 );
    CHECK( state.fast.melee.cut > 0 );
    CHECK( state.fast.melee.stab > 0 );
    CHECK( state.fast.melee.dur > 0 );
    CHECK( state.fast.name == "hand-forged sword" );
}

TEST_CASE( "proc_builder_supports_sword_reinforcements_and_legacy_like_names",
           "[proc][builder][weapon]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sword.json", "sword" );

    auto wood_blade = proc::part_fact{};
    wood_blade.ix = 1;
    wood_blade.id = itype_id( "stick_long" );
    wood_blade.mat = { material_id( "wood" ) };
    wood_blade.mass_g = 450;
    wood_blade.volume_ml = 450;

    auto handle = proc::part_fact{};
    handle.ix = 2;
    handle.id = itype_id( "stick_long" );
    handle.mat = { material_id( "wood" ) };
    handle.mass_g = 180;
    handle.volume_ml = 200;

    auto grip = proc::part_fact{};
    grip.ix = 3;
    grip.id = itype_id( "rag" );
    grip.mat = { material_id( "cotton" ) };
    grip.mass_g = 30;
    grip.volume_ml = 40;

    auto nail = proc::part_fact{};
    nail.ix = 4;
    nail.id = itype_id( "nail" );
    nail.mat = { material_id( "iron" ) };
    nail.mass_g = 20;
    nail.volume_ml = 5;
    nail.uses = 4;

    auto scrap = proc::part_fact{};
    scrap.ix = 5;
    scrap.id = itype_id( "scrap" );
    scrap.mat = { material_id( "steel" ) };
    scrap.mass_g = 120;
    scrap.volume_ml = 80;

    const auto base_state = [&]() {
        auto state = proc::build_state( sch, { wood_blade, handle, grip, nail, scrap } );
        REQUIRE( proc::add_pick( state, sch, proc::slot_id( "blade" ), 1 ) );
        REQUIRE( proc::add_pick( state, sch, proc::slot_id( "handle" ), 2 ) );
        REQUIRE( proc::add_pick( state, sch, proc::slot_id( "grip" ), 3 ) );
        return state;
    };

    const auto plain = base_state();
    CHECK( plain.fast.name == "2-by-sword" );
    CHECK( plain.fast.description ==
           "A club-like sword carved from a sturdy length of wood. "
           "The hilt uses no guard and a rag-wrapped grip." );

    auto nailed = base_state();
    REQUIRE( proc::add_pick( nailed, sch, proc::slot_id( "reinforcement" ), 4 ) );
    REQUIRE( proc::add_pick( nailed, sch, proc::slot_id( "reinforcement" ), 4 ) );
    CHECK( nailed.fast.name == "nail sword" );
    CHECK( nailed.fast.description ==
           "A rough wooden sword stiffened with driven nails. "
           "The hilt uses no guard and a rag-wrapped grip. "
           "Driven nails add stiffness and a little puncturing power." );
    CHECK( nailed.fast.melee.cut > plain.fast.melee.cut );
    CHECK( nailed.fast.melee.dur >= plain.fast.melee.dur );

    auto crude = base_state();
    REQUIRE( proc::add_pick( crude, sch, proc::slot_id( "reinforcement" ), 5 ) );
    CHECK( crude.fast.name == "crude sword" );
    CHECK( crude.fast.description ==
           "A crude sword pieced together from wood and scavenged scrap. "
           "The hilt uses no guard and a rag-wrapped grip. "
           "Scrap reinforcement adds weight and stiffness." );
    CHECK( crude.fast.melee.cut > plain.fast.melee.cut );
    CHECK( crude.fast.mass_g > plain.fast.mass_g );
}

TEST_CASE( "proc_builder_ranks_sword_candidates_by_role_quality", "[proc][builder][weapon]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sword.json", "sword" );

    const auto wood_blade = proc::normalize_part_fact( item( "stick_long" ), { .ix = 1 } );
    const auto steel_blade = proc::normalize_part_fact( item( "steel_chunk" ), { .ix = 2 } );
    const auto bone_blade = proc::normalize_part_fact( item( "bone" ), { .ix = 3 } );
    const auto leather_grip = proc::normalize_part_fact( item( "leather" ), { .ix = 4 } );
    const auto rag_grip = proc::normalize_part_fact( item( "rag" ), { .ix = 5 } );

    const auto state = proc::build_state( sch, {
        wood_blade,
        steel_blade,
        bone_blade,
        leather_grip,
        rag_grip,
    } );

    const auto position_in = []( const std::vector<proc::part_ix> &values,
    const proc::part_ix target ) {
        const auto iter = std::ranges::find( values, target );
        REQUIRE( iter != values.end() );
        return static_cast<int>( std::distance( values.begin(), iter ) );
    };

    const auto &blade_candidates = state.cand.at( proc::slot_id( "blade" ) );
    CHECK( position_in( blade_candidates, 2 ) < position_in( blade_candidates, 3 ) );
    CHECK( position_in( blade_candidates, 3 ) < position_in( blade_candidates, 1 ) );

    const auto &grip_candidates = state.cand.at( proc::slot_id( "grip" ) );
    CHECK( position_in( grip_candidates, 4 ) < position_in( grip_candidates, 5 ) );
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
    CHECK( subset.search( "Sandwich bread" ).size() == 1 );
    CHECK( subset.search( "c:Sandwich bread" ).size() == 1 );
    CHECK( subset.search( "Sandwich stew" ).empty() );
    proc::reset();
}

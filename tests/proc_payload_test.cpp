#include "catch/catch.hpp"

#include <sstream>
#include <string>
#include <vector>

#include "fstream_utils.h"
#include "item.h"
#include "item_factory.h"
#include "json.h"
#include "json_assertion_helpers.h"
#include "proc_item.h"
#include "recipe_dictionary.h"
#include "units_mass.h"
#include "units_volume.h"

namespace
{

auto round_trip( const item &src ) -> detached_ptr<item>
{
    std::ostringstream out;
    JsonOut jsout( out );
    src.serialize( jsout );

    std::istringstream in( out.str() );
    JsonIn jsin( in );
    auto restored = item::spawn();
    restored->deserialize( jsin );
    return restored;
}

auto craft_plan_json( const proc::craft_plan &plan ) -> std::string
{
    return serialize_wrapper( [&]( JsonOut & jsout ) {
        proc::to_json( jsout, plan );
    } );
}

auto part_count( const proc::payload &payload, const itype_id &id ) -> int
{
    auto total = 0;
    std::ranges::for_each( payload.parts, [&]( const proc::compact_part & part ) {
        if( part.id == id ) {
            total += part.n;
        }
    } );
    return total;
}

auto role_count( const proc::payload &payload, const std::string &role ) -> int
{
    auto total = 0;
    std::ranges::for_each( payload.parts, [&]( const proc::compact_part & part ) {
        if( part.role == role ) {
            total += part.n;
        }
    } );
    return total;
}

} // namespace

TEST_CASE( "proc_payload_round_trips_through_item_save", "[proc][payload]" )
{
    auto sandwich = item( "sandwich_generic", calendar::turn );
    auto payload = proc::payload{};
    payload.id = proc::schema_id( "sandwich" );
    payload.mode = proc::hist::compact;
    payload.fp = "sandwich:abcd1234";
    payload.blob.kcal = 420;
    payload.blob.mass_g = 220;
    payload.blob.volume_ml = 330;
    payload.blob.name = "meat sandwich";
    payload.blob.melee.stab = 9;
    payload.servings = 6;
    auto part = proc::compact_part{};
    part.role = "bread";
    part.id = itype_id( "bread" );
    part.n = 2;
    part.hp = 1.0f;
    payload.parts.push_back( part );
    proc::write_payload( sandwich, payload );

    const auto restored = round_trip( sandwich );
    REQUIRE( proc::read_payload( *restored ) );
    CHECK( *proc::read_payload( *restored ) == payload );
}

TEST_CASE( "proc_craft_plan_round_trips_through_item_save", "[proc][payload]" )
{
    auto craft = item( "sandwich_generic", calendar::turn );
    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "bread" );
    fact.tag = { "bread" };
    fact.mat = { material_id( "wheat" ) };
    fact.kcal = 120;
    fact.mass_g = 60;
    fact.volume_ml = 125;
    proc::write_craft_plan( craft, {
        .mode = proc::hist::compact,
        .slots = { proc::slot_id( "blade" ), proc::slot_id( "grip" ) },
        .facts = { fact }
    } );

    const auto restored = round_trip( craft );
    REQUIRE( proc::read_craft_plan( *restored ) );
    CHECK( proc::read_craft_plan( *restored )->mode == proc::hist::compact );
    REQUIRE( proc::read_craft_plan( *restored )->slots.size() == 2 );
    CHECK( proc::read_craft_plan( *restored )->slots[0] == proc::slot_id( "blade" ) );
    REQUIRE( proc::read_craft_plan( *restored )->facts.size() == 1 );
    CHECK( proc::read_craft_plan( *restored )->facts[0].id == itype_id( "bread" ) );
}

TEST_CASE( "proc_payload_json_matches_snapshot", "[proc][payload][snapshot]" )
{
    auto payload = proc::payload{};
    payload.id = proc::schema_id( "sandwich" );
    payload.mode = proc::hist::compact;
    payload.fp = "sandwich:abcd1234";
    payload.blob.kcal = 420;
    payload.blob.mass_g = 220;
    payload.blob.volume_ml = 330;
    payload.blob.name = "meat sandwich";
    payload.blob.melee.stab = 9;
    payload.blob.vit.emplace( vitamin_id( "vitC" ), 4 );
    payload.servings = 6;
    payload.parts.push_back( proc::compact_part{
        .role = "bread",
        .id = itype_id( "bread" ),
        .n = 2,
        .hp = 1.0f,
        .dmg = 0,
        .chg = 0,
        .mat = { material_id( "wheat" ) },
        .proc = ""
    } );

    json_snapshot::check_json_snapshot(
        proc::payload_json( payload ),
        "tests/data/json_snapshots/proc_payload_test/payload.json" );
}

TEST_CASE( "proc_craft_plan_json_matches_snapshot", "[proc][payload][snapshot]" )
{
    json_snapshot::check_json_snapshot(
    craft_plan_json( {
        .mode = proc::hist::compact,
        .slots = { proc::slot_id( "blade" ), proc::slot_id( "grip" ) },
        .facts = {
            proc::part_fact{
                .ix = 1,
                .id = itype_id( "bread" ),
                .tag = { "bread" },
                .flag = {},
                .qual = {},
                .mat = { material_id( "wheat" ) },
                .vit = {},
                .mass_g = 60,
                .volume_ml = 125,
                .kcal = 120,
                .hp = 1.0f,
                .chg = 0,
                .uses = 1,
                .proc = ""
            }
        }
    } ),
    "tests/data/json_snapshots/proc_payload_test/craft_plan.json" );
}

TEST_CASE( "proc_payload_participates_in_stacking", "[proc][payload]" )
{
    auto a = item( "sandwich_generic", calendar::turn );
    auto b = item( "sandwich_generic", calendar::turn );

    auto payload = proc::payload{};
    payload.id = proc::schema_id( "sandwich" );
    payload.mode = proc::hist::none;
    payload.fp = "sandwich:a";
    payload.blob.kcal = 100;
    proc::write_payload( a, payload );
    proc::write_payload( b, payload );
    CHECK( a.stacks_with( b ) );

    payload.fp = "sandwich:b";
    proc::write_payload( b, payload );
    CHECK_FALSE( a.stacks_with( b ) );
}

TEST_CASE( "proc_payload_restores_nested_compact_parts", "[proc][payload]" )
{
    auto child = proc::payload{};
    child.id = proc::schema_id( "sandwich" );
    child.mode = proc::hist::none;
    child.fp = "sandwich:child";
    child.blob.name = "mini sandwich";

    auto payload = proc::payload{};
    payload.id = proc::schema_id( "knife_spear" );
    payload.mode = proc::hist::compact;
    payload.parts.push_back( proc::compact_part{
        .role = "head",
        .id = itype_id( "knife_butcher" ),
        .n = 1,
        .hp = 0.5f,
        .dmg = 1,
        .chg = 0,
        .mat = { material_id( "steel" ) },
        .proc = proc::payload_json( child )
    } );

    const auto restored = proc::restore_parts( payload );
    REQUIRE( restored.size() == 1 );
    REQUIRE( proc::read_payload( *restored.front() ) );
    CHECK( proc::read_payload( *restored.front() )->fp == "sandwich:child" );
}

TEST_CASE( "legacy_sandwiches_gain_proc_payload_on_save_load", "[proc][payload][migration]" )
{
    struct legacy_case {
        itype_id id;
        int bread = 0;
        int meat = 0;
        int bacon = 0;
        int fish = 0;
        int cheese = 0;
        int veg = 0;
        int cucumber = 0;
        int tomato = 0;
        int mustard = 0;
        int honey = 0;
        int jam = 0;
        int peanut_butter = 0;
        int syrup = 0;
    };

    const auto cases = std::vector<legacy_case> {
        legacy_case{ .id = itype_id( "sandwich_t" ), .bread = 2, .meat = 1 },
        legacy_case{ .id = itype_id( "sandwich_veggy" ), .bread = 2, .veg = 1 },
        legacy_case{ .id = itype_id( "sandwich_cheese" ), .bread = 2, .cheese = 1 },
        legacy_case{ .id = itype_id( "sandwich_sauce" ), .bread = 2, .mustard = 1 },
        legacy_case{ .id = itype_id( "sandwich_honey" ), .bread = 2, .honey = 1 },
        legacy_case{ .id = itype_id( "sandwich_jam" ), .bread = 2, .jam = 1 },
        legacy_case{ .id = itype_id( "sandwich_pb" ), .bread = 2, .peanut_butter = 1 },
        legacy_case{ .id = itype_id( "sandwich_pbj" ), .bread = 2, .jam = 1, .peanut_butter = 1 },
        legacy_case{ .id = itype_id( "sandwich_pbh" ), .bread = 2, .honey = 1, .peanut_butter = 1 },
        legacy_case{ .id = itype_id( "sandwich_pbm" ), .bread = 2, .peanut_butter = 1, .syrup = 1 },
        legacy_case{ .id = itype_id( "blt" ), .bread = 2, .bacon = 1, .veg = 1, .tomato = 1 },
        legacy_case{ .id = itype_id( "fish_sandwich" ), .bread = 2, .fish = 1, .veg = 1, .mustard = 1 },
        legacy_case{ .id = itype_id( "sandwich_deluxe" ), .bread = 2, .meat = 1, .cheese = 2, .veg = 1, .mustard = 1 },
        legacy_case{ .id = itype_id( "sandwich_okay" ), .bread = 2, .meat = 1, .veg = 1 },
        legacy_case{ .id = itype_id( "sandwich_deluxe_nocheese" ), .bread = 2, .meat = 1, .veg = 1, .mustard = 1 },
        legacy_case{ .id = itype_id( "sandwich_cucumber" ), .bread = 2, .cucumber = 1 }
    };

    std::ranges::for_each( cases, [&]( const legacy_case & test_case ) {
        const auto legacy = item( test_case.id, calendar::turn );
        const auto restored = round_trip( legacy );

        INFO( test_case.id.str() );
        const auto payload = proc::read_payload( *restored );
        REQUIRE( payload );
        CHECK( payload->id == proc::schema_id( "sandwich" ) );
        CHECK( payload->mode == proc::hist::compact );
        CHECK( payload->fp == "sandwich:legacy:" + test_case.id.str() );
        CHECK( item_controller->migrate_id( test_case.id ) == itype_id( "sandwich_generic" ) );
        CHECK( restored->typeId() == itype_id( "sandwich_generic" ) );
        CHECK( restored->type_name() == legacy.type_name() );
        CHECK( payload->blob.name == legacy.type_name() );
        CHECK( payload->blob.mass_g == units::to_gram( legacy.weight() ) );
        CHECK( payload->blob.volume_ml == units::to_milliliter( legacy.volume() ) );
        CHECK( payload->servings == std::max( legacy.charges, 1 ) );
        CHECK( restored->weight() == legacy.weight() );
        CHECK( restored->volume() == legacy.volume() );
        CHECK( part_count( *payload, itype_id( "bread" ) ) == test_case.bread );
        CHECK( part_count( *payload, itype_id( "meat_cooked" ) ) == test_case.meat );
        CHECK( part_count( *payload, itype_id( "bacon" ) ) == test_case.bacon );
        CHECK( part_count( *payload, itype_id( "fish_cooked" ) ) == test_case.fish );
        CHECK( part_count( *payload, itype_id( "cheese" ) ) == test_case.cheese );
        CHECK( part_count( *payload, itype_id( "lettuce" ) ) == test_case.veg );
        CHECK( part_count( *payload, itype_id( "cucumber" ) ) == test_case.cucumber );
        CHECK( part_count( *payload, itype_id( "tomato" ) ) == test_case.tomato );
        CHECK( part_count( *payload, itype_id( "mustard" ) ) == test_case.mustard );
        CHECK( part_count( *payload, itype_id( "honey_bottled" ) ) == test_case.honey );
        CHECK( part_count( *payload, itype_id( "jam_fruit" ) ) == test_case.jam );
        CHECK( part_count( *payload, itype_id( "peanutbutter" ) ) == test_case.peanut_butter );
        CHECK( part_count( *payload, itype_id( "syrup" ) ) == test_case.syrup );
    } );
}

TEST_CASE( "legacy_sandwiches_migrate_to_proc_sandwich_items", "[proc][payload][migration]" )
{
    const auto cases = std::vector<itype_id> {
        itype_id( "blt" ),
        itype_id( "sandwich_t" ),
        itype_id( "sandwich_honey" ),
        itype_id( "sandwich_jam" ),
        itype_id( "sandwich_pbj" ),
        itype_id( "sandwich_pbh" ),
        itype_id( "sandwich_pbm" ),
        itype_id( "fish_sandwich" ),
    };

    std::ranges::for_each( cases, [&]( const itype_id & test_case ) {
        auto legacy = item( test_case, calendar::turn );
        proc::clear_payload( legacy );
        legacy.convert( item_controller->migrate_id( test_case ) );

        item_controller->migrate_item( test_case, legacy );

        INFO( test_case.str() );
        const auto payload = proc::read_payload( legacy );
        REQUIRE( payload );
        CHECK( payload->id == proc::schema_id( "sandwich" ) );
        CHECK( payload->mode == proc::hist::compact );
        CHECK( payload->fp == "sandwich:legacy:" + test_case.str() );
        CHECK( legacy.typeId() == itype_id( "sandwich_generic" ) );
        CHECK( payload->blob.name == legacy.type_name() );
    } );
}

TEST_CASE( "legacy_swords_gain_proc_payload_on_save_load", "[proc][payload][migration]" )
{
    struct legacy_weapon_case {
        itype_id id;
        int blades = 0;
        int guards = 0;
        int handles = 0;
        int grips = 0;
        int reinforcements = 0;
        int steel_chunks = 0;
        int sticks = 0;
        int bones = 0;
        int leather = 0;
        int rags = 0;
        int nails = 0;
        int scraps = 0;
    };

    const auto cases = std::vector<legacy_weapon_case> {
        legacy_weapon_case{ .id = itype_id( "sword_metal" ), .blades = 1, .guards = 1, .handles = 1, .grips = 1, .steel_chunks = 2, .sticks = 1, .leather = 1 },
        legacy_weapon_case{ .id = itype_id( "sword_wood" ), .blades = 1, .guards = 1, .handles = 1, .grips = 1, .sticks = 3, .rags = 1 },
        legacy_weapon_case{ .id = itype_id( "sword_nail" ), .blades = 1, .guards = 1, .handles = 1, .grips = 1, .reinforcements = 1, .sticks = 3, .rags = 1, .nails = 1 },
        legacy_weapon_case{ .id = itype_id( "sword_crude" ), .blades = 1, .guards = 1, .handles = 1, .grips = 1, .reinforcements = 1, .sticks = 3, .rags = 1, .scraps = 1 },
        legacy_weapon_case{ .id = itype_id( "sword_bone" ), .blades = 1, .handles = 1, .grips = 1, .sticks = 1, .bones = 1, .leather = 1 }
    };

    std::ranges::for_each( cases, [&]( const legacy_weapon_case & test_case ) {
        const auto legacy = item( test_case.id, calendar::turn );
        const auto restored = round_trip( legacy );

        INFO( test_case.id.str() );
        const auto payload = proc::read_payload( *restored );
        REQUIRE( payload );
        CHECK( payload->id == proc::schema_id( "sword" ) );
        CHECK( payload->mode == proc::hist::compact );
        CHECK( payload->fp == "sword:legacy:" + test_case.id.str() );
        CHECK( item_controller->migrate_id( test_case.id ) == itype_id( "proc_sword_generic" ) );
        CHECK( restored->typeId() == itype_id( "proc_sword_generic" ) );
        CHECK( restored->type_name() == legacy.type_name() );
        CHECK( payload->blob.name == legacy.type_name() );
        CHECK( payload->blob.mass_g == units::to_gram( legacy.weight() ) );
        CHECK( payload->blob.volume_ml == units::to_milliliter( legacy.volume() ) );
        CHECK( units::to_gram( restored->weight() ) == units::to_gram( legacy.weight() ) );
        CHECK( restored->volume() == legacy.volume() );
        CHECK( payload->blob.melee.bash == legacy.damage_melee( DT_BASH ) );
        CHECK( payload->blob.melee.cut == legacy.damage_melee( DT_CUT ) );
        CHECK( payload->blob.melee.stab == legacy.damage_melee( DT_STAB ) );
        CHECK( role_count( *payload, "blade" ) == test_case.blades );
        CHECK( role_count( *payload, "guard" ) == test_case.guards );
        CHECK( role_count( *payload, "handle" ) == test_case.handles );
        CHECK( role_count( *payload, "grip" ) == test_case.grips );
        CHECK( role_count( *payload, "reinforcement" ) == test_case.reinforcements );
        CHECK( part_count( *payload, itype_id( "steel_chunk" ) ) == test_case.steel_chunks );
        CHECK( part_count( *payload, itype_id( "stick_long" ) ) == test_case.sticks );
        CHECK( part_count( *payload, itype_id( "bone" ) ) == test_case.bones );
        CHECK( part_count( *payload, itype_id( "leather" ) ) == test_case.leather );
        CHECK( part_count( *payload, itype_id( "rag" ) ) == test_case.rags );
        CHECK( part_count( *payload, itype_id( "nail" ) ) == test_case.nails );
        CHECK( part_count( *payload, itype_id( "scrap" ) ) == test_case.scraps );
    } );
}

TEST_CASE( "legacy_sword_ids_migrate_to_proc_uncraft_recipe", "[proc][payload][migration]" )
{
    CHECK( recipe_dictionary::get_uncraft( itype_id( "proc_sword_generic" ) ) );

    const auto cases = std::vector<itype_id> {
        itype_id( "sword_metal" ),
        itype_id( "sword_wood" ),
        itype_id( "sword_nail" ),
        itype_id( "sword_crude" ),
        itype_id( "sword_bone" )
    };

    std::ranges::for_each( cases, [&]( const itype_id & test_case ) {
        INFO( test_case.str() );
        CHECK( item_controller->migrate_id( test_case ) == itype_id( "proc_sword_generic" ) );
        CHECK( recipe_dictionary::get_uncraft( item_controller->migrate_id( test_case ) ) );
    } );
}

TEST_CASE( "legacy_sandwich_ids_migrate_to_proc_uncraft_recipe", "[proc][payload][migration]" )
{
    CHECK( recipe_dictionary::get_uncraft( itype_id( "sandwich_generic" ) ) );

    const auto cases = std::vector<itype_id> {
        itype_id( "blt" ),
        itype_id( "sandwich_t" ),
        itype_id( "sandwich_veggy" ),
        itype_id( "sandwich_cheese" ),
        itype_id( "sandwich_sauce" ),
        itype_id( "sandwich_honey" ),
        itype_id( "sandwich_jam" ),
        itype_id( "sandwich_pb" ),
        itype_id( "sandwich_pbj" ),
        itype_id( "sandwich_pbh" ),
        itype_id( "sandwich_pbm" ),
        itype_id( "sandwich_okay" ),
        itype_id( "fish_sandwich" ),
        itype_id( "sandwich_deluxe" ),
        itype_id( "sandwich_deluxe_nocheese" ),
        itype_id( "sandwich_cucumber" )
    };

    std::ranges::for_each( cases, [&]( const itype_id & test_case ) {
        INFO( test_case.str() );
        CHECK( item_controller->migrate_id( test_case ) == itype_id( "sandwich_generic" ) );
        CHECK( recipe_dictionary::get_uncraft( item_controller->migrate_id( test_case ) ) );
    } );
}

#include "catch/catch.hpp"

#include <sstream>
#include <string>

#include "fstream_utils.h"
#include "item.h"
#include "json.h"
#include "json_assertion_helpers.h"
#include "proc_item.h"

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

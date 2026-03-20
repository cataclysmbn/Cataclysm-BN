#include "catch/catch.hpp"

#include <sstream>
#include <string>

#include "item.h"
#include "json.h"
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

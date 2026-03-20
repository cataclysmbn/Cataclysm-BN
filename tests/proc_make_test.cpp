#include "catch/catch.hpp"

#include <vector>

#include "item.h"
#include "proc_item.h"
#include "proc_schema.h"

TEST_CASE( "proc_make_item_applies_food_blob_to_item", "[proc][make][food]" )
{
    auto sch = proc::schema{};
    sch.id = proc::schema_id( "sandwich" );
    sch.cat = "food";
    sch.res = itype_id( "sandwich_generic" );

    auto bread = proc::part_fact{};
    bread.ix = 1;
    bread.id = itype_id( "bread" );
    bread.kcal = 300;
    bread.mass_g = 120;
    bread.volume_ml = 250;
    bread.vit.emplace( vitamin_id( "vitC" ), 2 );

    auto meat = proc::part_fact{};
    meat.ix = 2;
    meat.id = itype_id( "meat_cooked" );
    meat.kcal = 180;
    meat.mass_g = 80;
    meat.volume_ml = 125;

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::none;
    const auto made = proc::make_item( sch, { bread, meat }, opts );
    REQUIRE( proc::read_payload( *made ) );
    CHECK( proc::read_payload( *made )->blob.kcal == 480 );
    CHECK( made->type_name() == proc::read_payload( *made )->blob.name );
    CHECK( made->weight() == units::from_gram( 200 ) );
    CHECK( made->volume() == units::from_milliliter( 375 ) );
}

TEST_CASE( "proc_food_uses_blob_nutrition_and_component_hash", "[proc][make][food]" )
{
    auto sch = proc::schema{};
    sch.id = proc::schema_id( "sandwich" );
    sch.cat = "food";
    sch.res = itype_id( "sandwich_generic" );

    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "bread" );
    fact.kcal = 250;
    fact.mass_g = 100;
    fact.volume_ml = 200;

    auto opts2 = proc::make_opts{};
    opts2.mode = proc::hist::none;
    const auto made = proc::make_item( sch, { fact }, opts2 );
    CHECK( proc::blob_kcal( *made ) == std::optional<int>( 250 ) );
    CHECK( made->make_component_hash() == std::hash<std::string> {}( proc::read_payload(
                *made )->fp ) );
}

TEST_CASE( "proc_compact_restore_preserves_spear_part_damage", "[proc][make][compact]" )
{
    auto payload = proc::payload{};
    payload.id = proc::schema_id( "knife_spear" );
    payload.mode = proc::hist::compact;
    auto head = proc::compact_part{};
    head.role = "head";
    head.id = itype_id( "knife_butcher" );
    head.n = 1;
    head.hp = 0.5f;
    head.dmg = 1;
    head.chg = 0;
    head.mat = { material_id( "steel" ) };

    auto shaft = proc::compact_part{};
    shaft.role = "shaft";
    shaft.id = itype_id( "stick_long" );
    shaft.n = 1;
    shaft.hp = 0.6f;
    shaft.dmg = 0;
    shaft.chg = 0;
    shaft.mat = { material_id( "wood" ) };

    auto bind = proc::compact_part{};
    bind.role = "bind";
    bind.id = itype_id( "rag" );
    bind.n = 1;
    bind.hp = 1.0f;
    bind.dmg = 0;
    bind.chg = 0;
    bind.mat = { material_id( "cotton" ) };

    payload.parts = { head, shaft, bind };

    const auto restored = proc::restore_parts( payload );
    REQUIRE( restored.size() == 3 );
    CHECK( restored[0]->damage() >= 1 );
    CHECK( restored[1]->damage() >= 0 );
}

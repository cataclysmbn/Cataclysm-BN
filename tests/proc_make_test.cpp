#include "catch/catch.hpp"

#include <vector>

#include "item.h"
#include "proc_fact.h"
#include "proc_item.h"
#include "proc_schema.h"

TEST_CASE( "proc_make_item_applies_food_blob_to_item", "[proc][make][food]" )
{
    auto sch = proc::schema{};
    sch.id = proc::schema_id( "sandwich" );
    sch.cat = "food";
    sch.res = itype_id( "sandwich_generic" );
    sch.slots = {
        proc::slot_data{ .id = proc::slot_id( "bread" ), .role = "bread", .min = 2, .max = 2, .rep = true, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "meat" ), .role = "meat", .min = 0, .max = 2, .rep = true, .ok = {}, .no = {} }
    };

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
    opts.slots = { proc::slot_id( "bread" ), proc::slot_id( "meat" ) };
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

TEST_CASE( "proc_make_item_full_mode_rebuilds_components_from_facts", "[proc][make][food]" )
{
    auto sch = proc::schema{};
    sch.id = proc::schema_id( "sandwich" );
    sch.cat = "food";
    sch.res = itype_id( "sandwich_generic" );

    auto bread = proc::part_fact{};
    bread.ix = 1;
    bread.id = itype_id( "bread" );
    bread.kcal = 120;
    bread.mass_g = 60;
    bread.volume_ml = 125;

    auto meat = proc::part_fact{};
    meat.ix = 2;
    meat.id = itype_id( "meat_cooked" );
    meat.kcal = 180;
    meat.mass_g = 80;
    meat.volume_ml = 125;

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::full;
    const auto made = proc::make_item( sch, { bread, meat }, opts );
    const auto &components = made->get_components();
    REQUIRE( components.size() == 2 );
    CHECK( components.as_vector()[0]->typeId() == itype_id( "bread" ) );
    CHECK( components.as_vector()[1]->typeId() == itype_id( "meat_cooked" ) );
}

TEST_CASE( "proc_make_item_applies_weapon_blob_to_item", "[proc][make][weapon]" )
{
    auto sch = proc::schema{};
    sch.id = proc::schema_id( "sword" );
    sch.cat = "weapon";
    sch.res = itype_id( "proc_sword_generic" );
    sch.slots = {
        proc::slot_data{ .id = proc::slot_id( "blade" ), .role = "blade", .min = 1, .max = 1, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "handle" ), .role = "handle", .min = 1, .max = 1, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "grip" ), .role = "grip", .min = 1, .max = 2, .rep = true, .ok = {}, .no = {} }
    };

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

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::compact;
    opts.slots = { proc::slot_id( "blade" ), proc::slot_id( "handle" ), proc::slot_id( "grip" ) };
    const auto made = proc::make_item( sch, { blade, handle, grip }, opts );
    REQUIRE( proc::read_payload( *made ) );
    REQUIRE( proc::blob_melee( *made ) );
    CHECK( proc::blob_melee( *made )->stab > 0 );
    CHECK( proc::blob_melee( *made )->bash > 0 );
    CHECK( made->damage_melee( DT_STAB ) == proc::blob_melee( *made )->stab );
    CHECK( made->type_name() == "forged sword" );
    REQUIRE( proc::read_payload( *made )->parts.size() == 3 );
    CHECK( proc::read_payload( *made )->parts[0].role == "blade" );
}

TEST_CASE( "proc_make_item_names_stews_from_selected_raw_ingredients", "[proc][make][food]" )
{
    auto sch = proc::schema{};
    sch.id = proc::schema_id( "stew" );
    sch.cat = "food";
    sch.res = itype_id( "stew_generic" );
    sch.slots = {
        proc::slot_data{ .id = proc::slot_id( "base" ), .role = "base", .min = 1, .max = 1, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "veg" ), .role = "veg", .min = 1, .max = 4, .rep = true, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "meat" ), .role = "meat", .min = 0, .max = 3, .rep = true, .ok = {}, .no = {} }
    };

    const auto broth = proc::normalize_part_fact( item( "broth" ), { .ix = 1 } );
    const auto carrot = proc::normalize_part_fact( item( "carrot" ), { .ix = 2 } );
    const auto cooked_meat = proc::normalize_part_fact( item( "meat_cooked" ), { .ix = 3 } );

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::compact;
    opts.slots = { proc::slot_id( "base" ), proc::slot_id( "veg" ) };
    const auto vegetable_stew = proc::make_item( sch, { broth, carrot }, opts );
    CHECK( vegetable_stew->type_name() == "vegetable stew" );

    opts.slots = { proc::slot_id( "base" ), proc::slot_id( "veg" ), proc::slot_id( "meat" ) };
    const auto meat_stew = proc::make_item( sch, { broth, carrot, cooked_meat }, opts );
    CHECK( meat_stew->type_name() == "meat stew" );
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

#include "catch/catch.hpp"

#include <vector>

#include "item.h"
#include "proc_fact.h"
#include "proc_item.h"
#include "proc_schema.h"

namespace
{

auto sandwich_schema_for_test() -> proc::schema
{
    auto sch = proc::schema{};
    sch.id = proc::schema_id( "sandwich" );
    sch.cat = "food";
    sch.res = itype_id( "sandwich_generic" );
    sch.slots = {
        proc::slot_data{ .id = proc::slot_id( "bread" ), .role = "bread", .min = 2, .max = 3, .rep = true, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "veg" ), .role = "veg", .min = 0, .max = 4, .rep = true, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "cheese" ), .role = "cheese", .min = 0, .max = 2, .rep = true, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "cond" ), .role = "cond", .min = 0, .max = 2, .rep = true, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "meat" ), .role = "meat", .min = 0, .max = 3, .rep = true, .ok = {}, .no = {} }
    };
    return sch;
}

auto make_sandwich_name_for_test( const std::vector<proc::part_fact> &facts,
                                  const std::vector<proc::slot_id> &slots ) -> std::string
{
    const auto made = proc::make_item( sandwich_schema_for_test(), facts, {
        .mode = proc::hist::compact,
        .rec = nullptr,
        .used = {},
        .slots = slots,
        .state = nullptr,
    } );
    return made->type_name();
}

} // namespace

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

TEST_CASE( "proc_make_item_scales_food_servings_with_total_size", "[proc][make][food]" )
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
    bread.kcal = 1200;
    bread.mass_g = 2400;
    bread.volume_ml = 2400;

    auto meat = proc::part_fact{};
    meat.ix = 2;
    meat.id = itype_id( "meat_cooked" );
    meat.kcal = 1800;
    meat.mass_g = 2600;
    meat.volume_ml = 2600;

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::none;
    opts.slots = { proc::slot_id( "bread" ), proc::slot_id( "meat" ) };
    const auto made = proc::make_item( sch, { bread, meat }, opts );

    const auto base = item( "sandwich_generic", calendar::turn );
    const auto ceil_div = []( const int value, const int divisor ) {
        return value <= 0 ? 0 : ( value + divisor - 1 ) / divisor;
    };
    const auto base_servings = std::max( base.charges, 1 );
    const auto expected_servings = std::max( {
        base_servings,
        ceil_div( ( bread.mass_g + meat.mass_g ) * base_servings,
                  std::max( units::to_gram( base.weight() ), 1L ) ),
        ceil_div( ( bread.volume_ml + meat.volume_ml ) * base_servings,
                  std::max( units::to_milliliter( base.volume() ), 1 ) )
    } );

    REQUIRE( proc::read_payload( *made ) );
    CHECK( made->charges == expected_servings );
    CHECK( made->charges == proc::read_payload( *made )->servings );
    CHECK( made->charges > base_servings );
    CHECK( proc::blob_kcal( *made ) == std::optional<int>( 75 ) );
    CHECK( made->weight() == units::from_gram( 5000 ) );
    CHECK( made->volume() == units::from_milliliter( 5000 ) );
}

TEST_CASE( "proc_food_remaining_size_tracks_servings", "[proc][make][food]" )
{
    auto sch = proc::schema{};
    sch.id = proc::schema_id( "sandwich" );
    sch.cat = "food";
    sch.res = itype_id( "sandwich_generic" );

    auto fact = proc::part_fact{};
    fact.ix = 1;
    fact.id = itype_id( "bread" );
    fact.kcal = 1200;
    fact.mass_g = 1200;
    fact.volume_ml = 1200;

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::none;
    const auto made = proc::make_item( sch, { fact }, opts );
    REQUIRE( proc::read_payload( *made ) );
    REQUIRE( made->charges == proc::read_payload( *made )->servings );

    const auto starting_weight = made->weight();
    const auto starting_volume = made->volume();
    const auto starting_kcal = proc::blob_kcal( *made );

    made->mod_charges( -1 );

    CHECK( made->weight() < starting_weight );
    CHECK( made->volume() < starting_volume );
    CHECK( proc::blob_kcal( *made ) == starting_kcal );
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
    REQUIRE( proc::read_payload( *made ) );
    CHECK( proc::blob_kcal( *made ) == std::optional<int>( 63 ) );
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
        proc::slot_data{ .id = proc::slot_id( "grip" ), .role = "grip", .min = 1, .max = 2, .rep = true, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "reinforcement" ), .role = "reinforcement", .min = 0, .max = 4, .rep = true, .ok = {}, .no = {} }
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
    CHECK( made->type_name() == "hand-forged sword" );
    REQUIRE( proc::read_payload( *made )->parts.size() == 3 );
    CHECK( proc::read_payload( *made )->parts[0].role == "blade" );
}

TEST_CASE( "proc_make_item_names_reinforced_wood_swords", "[proc][make][weapon]" )
{
    auto sch = proc::schema{};
    sch.id = proc::schema_id( "sword" );
    sch.cat = "weapon";
    sch.res = itype_id( "proc_sword_generic" );
    sch.slots = {
        proc::slot_data{ .id = proc::slot_id( "blade" ), .role = "blade", .min = 1, .max = 1, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "handle" ), .role = "handle", .min = 1, .max = 1, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "grip" ), .role = "grip", .min = 1, .max = 2, .rep = true, .ok = {}, .no = {} },
        proc::slot_data{ .id = proc::slot_id( "reinforcement" ), .role = "reinforcement", .min = 0, .max = 4, .rep = true, .ok = {}, .no = {} }
    };

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

    auto scrap = proc::part_fact{};
    scrap.ix = 5;
    scrap.id = itype_id( "scrap" );
    scrap.mat = { material_id( "steel" ) };
    scrap.mass_g = 120;
    scrap.volume_ml = 80;

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::compact;

    opts.slots = { proc::slot_id( "blade" ), proc::slot_id( "handle" ), proc::slot_id( "grip" ) };
    CHECK( proc::make_item( sch, { wood_blade, handle, grip }, opts )->type_name() == "2-by-sword" );

    opts.slots = { proc::slot_id( "blade" ), proc::slot_id( "handle" ), proc::slot_id( "grip" ),
                   proc::slot_id( "reinforcement" )
                 };
    CHECK( proc::make_item( sch, { wood_blade, handle, grip, nail }, opts )->type_name() ==
           "nail sword" );
    CHECK( proc::make_item( sch, { wood_blade, handle, grip, scrap }, opts )->type_name() ==
           "crude sword" );
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
    const auto cooked_fish = proc::normalize_part_fact( item( "fish_cooked" ), { .ix = 4 } );

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::compact;
    opts.slots = { proc::slot_id( "base" ), proc::slot_id( "veg" ) };
    const auto vegetable_stew = proc::make_item( sch, { broth, carrot }, opts );
    CHECK( vegetable_stew->type_name() == "vegetable stew" );

    opts.slots = { proc::slot_id( "base" ), proc::slot_id( "veg" ), proc::slot_id( "meat" ) };
    const auto meat_stew = proc::make_item( sch, { broth, carrot, cooked_meat }, opts );
    CHECK( meat_stew->type_name() == "meat stew" );

    const auto fish_stew = proc::make_item( sch, { broth, carrot, cooked_fish }, opts );
    CHECK( fish_stew->type_name() == "fish stew" );
}

TEST_CASE( "proc_make_item_names_sandwiches_from_selected_ingredients", "[proc][make][food]" )
{
    const auto bread_a = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    const auto bread_b = proc::normalize_part_fact( item( "bread" ), { .ix = 2 } );
    const auto meat = proc::normalize_part_fact( item( "meat_cooked" ), { .ix = 3 } );
    const auto cheese = proc::normalize_part_fact( item( "cheese" ), { .ix = 4 } );
    const auto lettuce = proc::normalize_part_fact( item( "lettuce" ), { .ix = 5 } );
    const auto mustard = proc::normalize_part_fact( item( "mustard" ), { .ix = 6 } );
    const auto fish = proc::normalize_part_fact( item( "fish_cooked" ), { .ix = 7 } );
    const auto cucumber = proc::normalize_part_fact( item( "cucumber" ), { .ix = 8 } );
    const auto bread_c = proc::normalize_part_fact( item( "bread" ), { .ix = 9 } );
    const auto bacon = proc::normalize_part_fact( item( "bacon" ), { .ix = 10 } );
    const auto tomato = proc::normalize_part_fact( item( "tomato" ), { .ix = 11 } );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, meat }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "meat" ),
    } ) == "meat sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, fish, lettuce, mustard }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "meat" ),
        proc::slot_id( "veg" ),
        proc::slot_id( "cond" ),
    } ) == "fish sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, bacon, lettuce, tomato }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "meat" ),
        proc::slot_id( "veg" ),
        proc::slot_id( "veg" ),
    } ) == "BLT" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, bread_c, bacon, lettuce, tomato, mustard }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "meat" ),
        proc::slot_id( "veg" ),
        proc::slot_id( "veg" ),
        proc::slot_id( "cond" ),
    } ) == "club sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, bacon, cheese, lettuce, tomato, mustard }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "meat" ),
        proc::slot_id( "cheese" ),
        proc::slot_id( "veg" ),
        proc::slot_id( "veg" ),
        proc::slot_id( "cond" ),
    } ) == "deluxe sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, bread_c, meat, lettuce, mustard }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "meat" ),
        proc::slot_id( "veg" ),
        proc::slot_id( "cond" ),
    } ) == "club sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, meat, cheese, lettuce, mustard }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "meat" ),
        proc::slot_id( "cheese" ),
        proc::slot_id( "veg" ),
        proc::slot_id( "cond" ),
    } ) == "deluxe sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, cucumber }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "veg" ),
    } ) == "cucumber sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, lettuce }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "veg" ),
    } ) == "vegetable sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, mustard }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "cond" ),
    } ) == "mustard sandwich" );
}

TEST_CASE( "proc_make_item_names_supported_condiment_sandwiches", "[proc][make][food]" )
{
    const auto bread_a = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    const auto bread_b = proc::normalize_part_fact( item( "bread" ), { .ix = 2 } );
    const auto butter = proc::normalize_part_fact( item( "butter" ), { .ix = 3 } );
    const auto horseradish = proc::normalize_part_fact( item( "horseradish" ), { .ix = 4 } );
    const auto sauerkraut = proc::normalize_part_fact( item( "sauerkraut" ), { .ix = 5 } );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, butter }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "cond" ),
    } ) == "butter sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, horseradish }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "cond" ),
    } ) == "horseradish sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, sauerkraut }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "cond" ),
    } ) == "sauerkraut sandwich" );
}

TEST_CASE( "proc_make_item_names_supported_spread_sandwiches", "[proc][make][food]" )
{
    const auto bread_a = proc::normalize_part_fact( item( "bread" ), { .ix = 1 } );
    const auto bread_b = proc::normalize_part_fact( item( "bread" ), { .ix = 2 } );
    const auto honey = proc::normalize_part_fact( item( "honey_bottled" ), { .ix = 3 } );
    const auto jam = proc::normalize_part_fact( item( "jam_fruit" ), { .ix = 4 } );
    const auto peanut_butter = proc::normalize_part_fact( item( "peanutbutter" ), { .ix = 5 } );
    const auto syrup = proc::normalize_part_fact( item( "syrup" ), { .ix = 6 } );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, honey }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "cond" ),
    } ) == "honey sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, jam }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "cond" ),
    } ) == "jam sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, peanut_butter }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "cond" ),
    } ) == "peanut butter sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, peanut_butter, jam }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "cond" ),
        proc::slot_id( "cond" ),
    } ) == "PB&J sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, peanut_butter, honey }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "cond" ),
        proc::slot_id( "cond" ),
    } ) == "PB&H sandwich" );

    CHECK( make_sandwich_name_for_test( { bread_a, bread_b, peanut_butter, syrup }, {
        proc::slot_id( "bread" ),
        proc::slot_id( "bread" ),
        proc::slot_id( "cond" ),
        proc::slot_id( "cond" ),
    } ) == "PB&M sandwich" );
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

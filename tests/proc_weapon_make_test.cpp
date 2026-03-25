#include "catch/catch.hpp"

#include <fstream>

#include "item.h"
#include "json.h"
#include "procgen/proc_item.h"
#include "procgen/proc_schema.h"
#include "proc_test_utils.h"

namespace
{

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

auto make_fact( const proc::part_ix ix, const itype_id &id,
                const std::vector<material_id> &mats,
                const int mass_g, const int volume_ml ) -> proc::part_fact
{
    return proc::part_fact {
        .ix = ix,
        .id = id,
        .mat = mats,
        .mass_g = mass_g,
        .volume_ml = volume_ml,
    };
}

} // namespace

TEST_CASE( "proc_make_item_generates_proc_gear_from_selected_parts", "[proc][make][weapon][lua]" )
{
    auto state = cata::lua_state{};
    proc_test::load_procgen_runtime( state );

    const auto steel_blade = make_fact( 1, itype_id( "steel_chunk" ), { material_id( "steel" ) }, 1200,
                                        500 );
    const auto wood_blade = make_fact( 2, itype_id( "stick_long" ), { material_id( "wood" ) }, 450,
                                       450 );
    const auto stone_head = make_fact( 3, itype_id( "rock" ), { material_id( "stone" ) }, 1400, 700 );
    const auto steel_head = make_fact( 4, itype_id( "steel_chunk" ), { material_id( "steel" ) }, 1100,
                                       450 );
    const auto shaft = make_fact( 5, itype_id( "stick_long" ), { material_id( "wood" ) }, 700, 600 );
    const auto handle = make_fact( 6, itype_id( "stick_long" ), { material_id( "wood" ) }, 180, 200 );
    const auto grip = make_fact( 7, itype_id( "rag" ), { material_id( "cotton" ) }, 30, 40 );
    const auto binding = make_fact( 8, itype_id( "string_36" ), { material_id( "cotton" ) }, 20, 15 );
    const auto bone_tip = make_fact( 9, itype_id( "bone" ), { material_id( "bone" ) }, 350, 120 );
    const auto steel_tip = make_fact( 10, itype_id( "knife_hunting" ), { material_id( "steel" ) }, 390,
                                      250 );

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::compact;
    opts.state = &state;

    SECTION( "steel swords get generated stats and stored components" ) {
        const auto sch = load_schema_from_file( "data/json/proc/sword.json", "sword" );
        opts.slots = { proc::slot_id( "blade" ), proc::slot_id( "handle" ), proc::slot_id( "grip" ) };
        const auto made = proc::make_item( sch, { steel_blade, handle, grip }, opts );
        REQUIRE( proc::read_payload( *made ) );
        REQUIRE( proc::blob_melee( *made ) );
        CHECK( made->typeId() == itype_id( "proc_sword_generic" ) );
        CHECK( made->type_name() == "steel sword" );
        CHECK( proc::read_payload( *made )->mode == proc::hist::full );
        CHECK( made->get_components().size() == 3 );
        CHECK( proc::blob_melee( *made )->cut > 0 );
        CHECK( proc::blob_melee( *made )->moves > 0 );
    }

    SECTION( "axe heads change generated axe damage" ) {
        const auto sch = load_schema_from_file( "data/json/proc/axe.json", "axe" );
        opts.slots = { proc::slot_id( "head" ), proc::slot_id( "handle" ), proc::slot_id( "binding" ) };
        const auto stone_axe = proc::make_item( sch, { stone_head, handle, binding }, opts );
        const auto steel_axe = proc::make_item( sch, { steel_head, handle, binding }, opts );
        REQUIRE( proc::blob_melee( *stone_axe ) );
        REQUIRE( proc::blob_melee( *steel_axe ) );
        CHECK( stone_axe->typeId() == itype_id( "proc_axe_generic" ) );
        CHECK( steel_axe->typeId() == itype_id( "proc_axe_generic" ) );
        CHECK( stone_axe->type_name() == "stone axe" );
        CHECK( steel_axe->type_name() == "steel axe" );
        CHECK( proc::blob_melee( *steel_axe )->cut >= proc::blob_melee( *stone_axe )->cut );
        CHECK( stone_axe->get_components().size() == 3 );
    }

    SECTION( "spear tips change generated spear damage" ) {
        const auto sch = load_schema_from_file( "data/json/proc/spear.json", "spear" );
        opts.slots = { proc::slot_id( "tip" ), proc::slot_id( "shaft" ), proc::slot_id( "binding" ) };
        const auto bone_spear = proc::make_item( sch, { bone_tip, shaft, binding }, opts );
        const auto steel_spear = proc::make_item( sch, { steel_tip, shaft, binding }, opts );
        REQUIRE( proc::blob_melee( *bone_spear ) );
        REQUIRE( proc::blob_melee( *steel_spear ) );
        CHECK( bone_spear->type_name() == "bone spear" );
        CHECK( steel_spear->type_name() == "steel spear" );
        CHECK( proc::blob_melee( *steel_spear )->stab >= proc::blob_melee( *bone_spear )->stab );
        CHECK( steel_spear->get_components().size() == 3 );
    }

    SECTION( "knife materials change generated knife damage and speed" ) {
        const auto sch = load_schema_from_file( "data/json/proc/knife.json", "knife" );
        opts.slots = { proc::slot_id( "blade" ), proc::slot_id( "handle" ), proc::slot_id( "binding" ) };
        const auto steel_knife = proc::make_item( sch, { steel_blade, handle, binding }, opts );
        const auto wood_knife = proc::make_item( sch, { wood_blade, handle, binding }, opts );
        REQUIRE( proc::blob_melee( *steel_knife ) );
        REQUIRE( proc::blob_melee( *wood_knife ) );
        CHECK( steel_knife->typeId() == itype_id( "proc_knife_generic" ) );
        CHECK( steel_knife->type_name() == "steel knife" );
        CHECK( proc::blob_melee( *steel_knife )->cut >= proc::blob_melee( *wood_knife )->cut );
        CHECK( proc::blob_melee( *steel_knife )->moves >= 65 );
        CHECK( steel_knife->get_components().size() == 3 );
    }
}

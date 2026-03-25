#include "catch/catch.hpp"

#include <fstream>
#include <sstream>

#include "catalua_impl.h"
#include "catalua_sol.h"
#include "flag.h"
#include "item.h"
#include "json.h"
#include "martialarts.h"
#include "proc_item.h"
#include "proc_schema.h"

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

auto load_procgen_runtime( cata::lua_state &state ) -> void
{
    state.lua.open_libraries( sol::lib::base, sol::lib::package, sol::lib::math, sol::lib::string,
                              sol::lib::table );

    auto file = std::ifstream( "data/json/proc/gear.lua", std::ios::binary );
    REQUIRE( file.is_open() );

    auto script = std::ostringstream {};
    script << file.rdbuf();

    auto procgen = state.lua.create_table();
    procgen["gear"] = state.lua.script( script.str() );
    state.lua["procgen"] = procgen;
}

} // namespace

TEST_CASE( "proc_make_item_converts_proc_swords_to_legacy_variants", "[proc][make][weapon][lua]" )
{
    const auto sch = load_schema_from_file( "data/json/proc/sword.json", "sword" );
    CHECK( sch.lua.make == "procgen.gear.make" );

    auto state = cata::lua_state{};
    load_procgen_runtime( state );

    auto steel_blade = proc::part_fact{};
    steel_blade.ix = 1;
    steel_blade.id = itype_id( "steel_chunk" );
    steel_blade.mat = { material_id( "steel" ) };
    steel_blade.mass_g = 1200;
    steel_blade.volume_ml = 500;

    auto wood_blade = proc::part_fact{};
    wood_blade.ix = 2;
    wood_blade.id = itype_id( "stick_long" );
    wood_blade.mat = { material_id( "wood" ) };
    wood_blade.mass_g = 450;
    wood_blade.volume_ml = 450;

    auto bone_blade = proc::part_fact{};
    bone_blade.ix = 3;
    bone_blade.id = itype_id( "bone" );
    bone_blade.mat = { material_id( "bone" ) };
    bone_blade.mass_g = 900;
    bone_blade.volume_ml = 400;

    auto handle = proc::part_fact{};
    handle.ix = 4;
    handle.id = itype_id( "stick_long" );
    handle.mat = { material_id( "wood" ) };
    handle.mass_g = 180;
    handle.volume_ml = 200;

    auto grip = proc::part_fact{};
    grip.ix = 5;
    grip.id = itype_id( "rag" );
    grip.mat = { material_id( "cotton" ) };
    grip.mass_g = 30;
    grip.volume_ml = 40;

    auto nail = proc::part_fact{};
    nail.ix = 6;
    nail.id = itype_id( "nail" );
    nail.mat = { material_id( "iron" ) };
    nail.mass_g = 20;
    nail.volume_ml = 5;

    auto scrap = proc::part_fact{};
    scrap.ix = 7;
    scrap.id = itype_id( "scrap" );
    scrap.mat = { material_id( "steel" ) };
    scrap.mass_g = 120;
    scrap.volume_ml = 80;

    auto opts = proc::make_opts{};
    opts.mode = proc::hist::compact;
    opts.state = &state;

    SECTION( "steel blades use hand-forged sword data" ) {
        opts.slots = { proc::slot_id( "blade" ), proc::slot_id( "handle" ), proc::slot_id( "grip" ) };
        const auto made = proc::make_item( sch, { steel_blade, handle, grip }, opts );
        CHECK( made->typeId() == itype_id( "sword_metal" ) );
        CHECK( made->type_name() == "hand-forged sword" );
        CHECK( made->has_technique( matec_id( "WBLOCK_2" ) ) );
    }

    SECTION( "nails use nail sword data" ) {
        opts.slots = { proc::slot_id( "blade" ), proc::slot_id( "handle" ), proc::slot_id( "grip" ),
                       proc::slot_id( "reinforcement" ), proc::slot_id( "reinforcement" )
                     };
        const auto made = proc::make_item( sch, { wood_blade, handle, grip, nail, nail }, opts );
        CHECK( made->typeId() == itype_id( "sword_nail" ) );
        CHECK( made->type_name() == "nail sword" );
        CHECK( made->has_flag( flag_id( "STAB" ) ) );
    }

    SECTION( "scrap uses crude sword data" ) {
        opts.slots = { proc::slot_id( "blade" ), proc::slot_id( "handle" ), proc::slot_id( "grip" ),
                       proc::slot_id( "reinforcement" )
                     };
        const auto made = proc::make_item( sch, { wood_blade, handle, grip, scrap }, opts );
        CHECK( made->typeId() == itype_id( "sword_crude" ) );
        CHECK( made->type_name() == "crude sword" );
        CHECK_FALSE( made->has_flag( flag_id( "STAB" ) ) );
    }

    SECTION( "bone blades use bone sword data while keeping proc name" ) {
        opts.slots = { proc::slot_id( "blade" ), proc::slot_id( "handle" ), proc::slot_id( "grip" ) };
        const auto made = proc::make_item( sch, { bone_blade, handle, grip }, opts );
        CHECK( made->typeId() == itype_id( "sword_bone" ) );
        CHECK( made->type_name() == "bone sword" );
    }

    SECTION( "result variant does not depend on localized sword names" ) {
        state.lua.script( R"(
            procgen.test = procgen.test or {}
            function procgen.test.rename(params)
              local blob = params.blob or {}
              blob.name = "mystery sword"
              return blob
            end
        )" );

        auto renamed = sch;
        renamed.lua.full = "procgen.test.rename";

        opts.slots = { proc::slot_id( "blade" ), proc::slot_id( "handle" ), proc::slot_id( "grip" ) };
        const auto made = proc::make_item( renamed, { steel_blade, handle, grip }, opts );
        CHECK( made->typeId() == itype_id( "sword_metal" ) );
        CHECK( made->type_name() == "mystery sword" );
    }
}

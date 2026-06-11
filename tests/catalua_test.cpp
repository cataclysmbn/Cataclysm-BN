#include "catch/catch.hpp"

#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "catalua.h"
#include "catalua_coord.h"
#include "catalua_hooks.h"
#include "catalua_impl.h"
#include "catalua_serde.h"
#include "catalua_sol.h"
#include "clzones.h"
#include "coordinates.h"
#include "debug.h"
#include "faction.h"
#include "flag.h"
#include "fstream_utils.h"
#include "game.h"
#include "iexamine.h"
#include "init.h"
#include "json.h"
#include "map.h"
#include "map_helpers.h"
#include "mapdata.h"
#include "effect.h"
#include "monster.h"
#include "npc.h"
#include "options.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "string_formatter.h"
#include "stringmaker.h"
#include "type_id.h"
#include "units_angle.h"
#include "units_energy.h"
#include "units_mass.h"
#include "units_utility.h"
#include "units_volume.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "veh_type.h"

#include <memory>
#include <optional>
#include <string>
#include <stdexcept>
#include <vector>

// workaround for https://github.com/llvm/llvm-project/issues/113087
#define CHECK_TUPLE(...) CHECK((__VA_ARGS__))

static void run_lua_test_script( sol::state &lua, const std::string &script_name )
{
    std::string full_script_name = "tests/lua/" + script_name;

    run_lua_script( lua, full_script_name );
}

TEST_CASE( "lua_class_members", "[lua]" )
{
    sol::state lua = make_lua_state();

    // Create global table for test
    sol::table test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    // Set input
    test_data["in"] = point( -10, 10 );

    // Run Lua script
    run_lua_test_script( lua, "class_members_test.lua" );

    // Get test output
    std::string res = test_data["out"];

    REQUIRE( res == "result is Point(12,13)" );
}

TEST_CASE( "lua_global_functions", "[lua]" )
{
    sol::state lua = make_lua_state();

    // Create global table for test
    sol::table test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    // Randomize avatar name
    get_avatar().pick_name();
    std::string expected_name = get_avatar().name;

    // Run Lua script
    run_lua_test_script( lua, "global_functions_test.lua" );

    // Get test output
    std::string lua_avatar_name = test_data["avatar_name"];
    std::string lua_creature_avatar_name = test_data["creature_avatar_name"];
    std::string lua_monster_avatar_name = test_data["monster_avatar_name"];
    std::string lua_character_avatar_name = test_data["character_avatar_name"];
    std::string lua_npc_avatar_name = test_data["npc_avatar_name"];

    REQUIRE( lua_avatar_name == expected_name );
    REQUIRE( lua_creature_avatar_name == expected_name );
    REQUIRE( lua_monster_avatar_name == "nil" );
    REQUIRE( lua_character_avatar_name == expected_name );
    REQUIRE( lua_npc_avatar_name == "nil" );
}

TEST_CASE( "lua_map_create_item_at_places_without_returning_owned_item", "[lua][map]" )
{
    clear_all_state();
    auto lua = make_lua_state();
    auto test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    auto &here = get_map();
    const auto pos = get_avatar().bub_pos();
    here.i_clear( pos );
    test_data["pos"] = pos;

    const auto script_res = lua.safe_script( R"(
local map = gapi.get_map()
local placed = map:create_item_at(test_data["pos"], ItypeId.new("rock"), 1)
test_data["return_type"] = type(placed)
test_data["item_count"] = #map:get_items_at(test_data["pos"])
)", sol::script_pass_on_error );
    REQUIRE( script_res.valid() );

    CHECK( test_data.get<std::string>( "return_type" ) == "nil" );
    CHECK( test_data.get<int>( "item_count" ) == 1 );

    here.i_clear( pos );
}

TEST_CASE( "lua_activity_bindings", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    cata::init_global_state_tables( state, {} );
    sol::state &lua = state.lua;

    auto test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    run_lua_test_script( lua, "activity_binding_test.lua" );

    REQUIRE( test_data.get<bool>( "has_examine_functions" ) );
    REQUIRE( test_data.get<bool>( "has_activity_functions" ) );
    REQUIRE( test_data.get<std::string>( "activity_id" ) == "ACT_WAIT" );
    REQUIRE( test_data.get<std::string>( "activity_name" ) == "test wash" );
    CHECK( test_data.get<int>( "activity_moves_total" ) == to_moves<int>( 5_minutes ) );
    CHECK( test_data.get<bool>( "activity_interruptable" ) );
    CHECK( test_data.get<std::string>( "activity_coord" ).starts_with( "TripointAbsMs" ) );

    get_avatar().activity->moves_left = 0;
    get_avatar().activity->do_turn( get_avatar() );

    CHECK( get_avatar().activity->is_null() );
    CHECK( test_data.get<bool>( "turn_called" ) );
    CHECK( test_data.get<std::string>( "turn_name" ) == "test wash" );
    CHECK( test_data.get<bool>( "finish_called" ) );
    CHECK( test_data.get<std::string>( "finish_name" ) == "test wash" );
    CHECK( test_data.get<std::string>( "finish_pos_type" ) == "TripointAbsMs" );
    CHECK( test_data.get<std::string>( "finish_mode" ) == "test_shower" );
    CHECK( test_data.get<bool>( "finish_is_warm" ) );
    CHECK( test_data.get<std::string>( "finish_cleaner_label" ) == "soap" );
    CHECK( test_data.get<int>( "finish_nested_charges" ) == 7 );
}

TEST_CASE( "lua_activity_without_callback_finishes", "[lua]" )
{
    clear_all_state();
    auto act = std::make_unique<player_activity>( activity_id( "ACT_WASH_SELF" ), 0 );
    get_avatar().assign_activity( std::move( act ) );

    get_avatar().activity->do_turn( get_avatar() );

    CHECK( get_avatar().activity->is_null() );
}

TEST_CASE( "robofac_authorization_updates_real_active_creatures", "[lua][robofac]" )
{
    clear_all_state();
    auto lua = make_lua_state();

    auto test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    auto &security = spawn_npc( point_bub_ms{ 50, 50 }, "hub_security" );
    security.set_attitude( NPCATT_KILL );
    auto &turret = spawn_test_monster( "mon_robofac_turret_light", tripoint_bub_ms{ 51, 50, 0 } );
    test_data["security"] = &security;
    test_data["turret"] = &turret;

    run_lua_test_script( lua, "robofac_actual_authorization_test.lua" );

    CHECK( test_data.get<std::string>( "security_faction" ) == "robofac_auxiliaries" );
    CHECK( test_data.get<npc_attitude>( "security_attitude" ) == NPCATT_NULL );
    CHECK( test_data.get<bool>( "turret_authorized" ) );
}

TEST_CASE( "lua_nearby_omt_creature_queries_return_active_creatures", "[lua][creature]" )
{
    clear_all_state();
    auto lua = make_lua_state();

    auto test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    auto &nearby_npc = spawn_npc( point_bub_ms{ 50, 50 }, "test_talker" );
    auto &nearby_monster = spawn_test_monster( "mon_zombie", tripoint_bub_ms{ 51, 50, 0 } );
    test_data["center"] = nearby_npc.abs_omt_pos();
    test_data["expected_npc"] = &nearby_npc;
    test_data["expected_monster"] = &nearby_monster;

    run_lua_test_script( lua, "nearby_omt_creature_query_test.lua" );

    CHECK( test_data.get<int>( "npc_count" ) == 1 );
    CHECK( test_data.get<int>( "monster_count" ) == 1 );
    CHECK( test_data.get<bool>( "found_expected_npc" ) );
    CHECK( test_data.get<bool>( "found_expected_monster" ) );
}

TEST_CASE( "lua_npc_move_to_binding_moves_real_npc", "[lua][npc]" )
{
    clear_all_state();
    auto lua = make_lua_state();

    auto test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    map &here = get_map();
    const auto start = tripoint_bub_ms{ 50, 50, 0 };
    const auto destination = tripoint_bub_ms{ 51, 50, 0 };
    for( const tripoint_bub_ms &pos : { start, destination } ) {
        here.ter_set( pos, ter_id( "t_dirt" ) );
        here.furn_set( pos, furn_id( "f_null" ) );
    }

    auto &moving_npc = spawn_npc( start.xy(), "test_talker" );
    moving_npc.set_moves( 1000 );
    test_data["npc"] = &moving_npc;
    test_data["destination"] = destination;

    run_lua_test_script( lua, "npc_move_to_test.lua" );

    CHECK( test_data.get<bool>( "moved" ) );
    CHECK( moving_npc.bub_pos() == destination );
}

TEST_CASE( "lua_place_monster_pins_upgrade_time", "[lua][monster]" )
{
    const auto restore_turn = restore_on_out_of_scope<time_point>( calendar::turn );
    clear_map();
    put_player_underground();
    calendar::turn = calendar::start_of_cataclysm + 2 * calendar::season_length();

    auto lua = make_lua_state();
    auto test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;
    test_data["monster_id"] = mtype_id( "mon_zombie" );
    test_data["pos"] = tripoint_bub_ms{ 5, 5, 0 };

    run_lua_test_script( lua, "place_monster_upgrade_time_test.lua" );

    const auto current_day = to_days<int>( calendar::turn - calendar::turn_zero );
    REQUIRE( test_data.get<bool>( "monster_spawned" ) );
    CHECK( test_data.get<std::string>( "monster_type" ) == "mon_zombie" );
    CHECK( test_data.get<int>( "upgrade_time" ) > current_day );
}

TEST_CASE( "lua_typed_coords_projection", "[lua]" )
{
    auto lua = make_lua_state();

    auto test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;
    lua["accept_abs_omt"] = []( const tripoint_abs_omt & p ) {
        return p.to_string();
    };

    run_lua_test_script( lua, "typed_coords_projection_test.lua" );

    CHECK( test_data.get<std::string>( "to_omt" ) == "TripointAbsOmt(1,1,2)" );
    CHECK( test_data.get<std::string>( "named_to_omt" ) == "TripointAbsOmt(1,1,2)" );
    CHECK( test_data.get<std::string>( "remain_quotient" ) == "TripointAbsOm(1,0,-1)" );
    CHECK( test_data.get<std::string>( "remain_remainder" ) == "PointOmSm(1,2)" );
    CHECK( test_data.get<std::string>( "combined" ) == "TripointAbsSm(361,2,-1)" );
    CHECK( test_data.get<int>( "distance" ) == 3 );
    CHECK( test_data.get<std::string>( "named_bub_point" ) == "PointBubMs(3,4)" );
    CHECK( test_data.get<std::string>( "named_abs_tripoint" ) == "TripointAbsMs(5,6,7)" );
    CHECK( test_data.get<std::string>( "named_abs_tripoint_from_typed_point" ) ==
           "TripointAbsMs(8,9,10)" );

    // Validate project_remain_omt example from the typed-coordinates documentation.
    CHECK( test_data.get<std::string>( "doc_remain_omt_quotient" ) == "TripointAbsOmt(1,1,2)" );
    CHECK( test_data.get<std::string>( "doc_remain_omt_remainder" ) == "PointOmtMs(1,2)" );
    CHECK( test_data.get<std::string>( "doc_remain_omt_combined" ) == "TripointAbsMs(25,26,2)" );
    CHECK( test_data.get<std::string>( "doc_remain_omt_method_combined" ) ==
           "TripointAbsMs(25,26,2)" );

    CHECK( test_data.get<std::string>( "typed_param" ) == "(1,2,3)" );
    CHECK_FALSE( test_data.get<bool>( "raw_param_ok" ) );
    CHECK_FALSE( test_data.get<bool>( "wrong_coord_ok" ) );
    CHECK( test_data.get<std::string>( "raw_reinterpreted" ) == "TripointAbsOmt(1,2,3)" );
    CHECK( test_data.get<std::string>( "raw_reinterpreted_param" ) == "(1,2,3)" );
    CHECK( test_data.get<std::string>( "typed_reinterpreted" ) == "TripointAbsOmt(1,2,3)" );
    CHECK( test_data.get<std::string>( "raw_delta_arithmetic" ) == "TripointAbsSm(364,2,-1)" );
}

TEST_CASE( "lua_coord_cpp_helpers", "[lua]" )
{
    auto lua = make_lua_state();
    const auto cpp_pos = tripoint_abs_omt( 1, 2, 3 );
    const auto lua_pos = cata::detail::lua_coords::to_lua( cpp_pos );

    CHECK( lua_pos.raw == cpp_pos.raw() );
    CHECK( lua_pos.origin == coords::origin::abs );
    CHECK( lua_pos.scale == coords::scale::overmap_terrain );

    const auto round_trip = cata::detail::lua_coords::as_cpp<tripoint_abs_omt>( lua_pos );
    REQUIRE( round_trip );
    CHECK( *round_trip == cpp_pos );

    const auto lua_obj = sol::make_object( lua, lua_pos );
    const auto object_round_trip = cata::detail::lua_coords::as_cpp<tripoint_abs_omt>( lua_obj );
    REQUIRE( object_round_trip );
    CHECK( *object_round_trip == cpp_pos );

    CHECK_FALSE( cata::detail::lua_coords::as_cpp<tripoint_bub_ms>( lua_pos ) );
    CHECK( cata::detail::lua_coords::expect_cpp<tripoint_abs_omt>( lua_pos ) == cpp_pos );
    CHECK_THROWS_AS( cata::detail::lua_coords::expect_cpp<tripoint_bub_ms>( lua_pos ),
                     std::runtime_error );
}

TEST_CASE( "plumbing_lua_tripoint_migration", "[lua][plumbing]" )
{
    clear_all_state();
    auto lua = make_lua_state();

    auto fake_map = lua.create_table();
    auto blood_intensity = 0;
    auto removed_blood_fields = 0;
    fake_map["bub_to_abs"] = []( const sol::object &, const tripoint_bub_ms & ) -> tripoint_abs_ms {
        return tripoint_abs_ms( 48, 48, 0 );
    };
    fake_map["has_vehicle_part_with_feature_at"] = []( const sol::object &, const tripoint_bub_ms &,
    const std::string &, bool ) -> bool {
        return false;
    };
    fake_map["points_in_radius"] = []( const sol::object &, const tripoint_bub_ms &, int,
    int ) -> std::vector<tripoint_bub_ms> {
        return { tripoint_bub_ms( 10, 10, 0 ) };
    };
    fake_map["get_field_int_at"] = [&blood_intensity]( const sol::object &, const tripoint_bub_ms &,
    const sol::object & ) -> int {
        return blood_intensity;
    };
    fake_map["remove_field_at"] = [&blood_intensity, &removed_blood_fields]( const sol::object &,
    const tripoint_bub_ms &, const sol::object & ) -> void {
        if( blood_intensity > 0 )
        {
            removed_blood_fields++;
        }
        blood_intensity = 0;
    };
    auto empty_item_stack = lua.create_table();
    empty_item_stack["items"] = [&lua]() -> sol::table { return lua.create_table(); };
    fake_map["get_items_at"] = [&empty_item_stack]( const sol::object &,
    const tripoint_bub_ms & ) -> sol::table {
        return empty_item_stack;
    };
    fake_map["get_temperature_c"] = []( const sol::object &, const tripoint_bub_ms & ) -> double {
        return 20.0;
    };

    auto fake_user = lua.create_table();
    fake_user["get_pos_ms"] = []( const sol::object & ) -> tripoint_bub_ms {
        return tripoint_bub_ms( 9, 10, 0 );
    };
    auto fake_user_on_fixture = lua.create_table();
    fake_user_on_fixture["get_pos_ms"] = []( const sol::object & ) -> tripoint_bub_ms {
        return tripoint_bub_ms( 10, 10, 0 );
    };

    auto grid = lua.create_table();
    grid["is_valid"] = []( const sol::object & ) -> bool { return true; };
    grid["get_resource"] = []( const sol::object & ) -> int { return 1000; };
    grid["mod_resource"] = []( const sol::object &, int ) -> void {};

    auto tracker = lua.create_table();
    tracker["grid_at"] = [&grid]( const sol::object &, const tripoint_abs_ms & ) -> sol::table {
        return grid;
    };

    auto gapi_table = lua.create_table();
    gapi_table["get_map"] = [&fake_map]() -> sol::table { return fake_map; };
    gapi_table["get_distribution_grid_tracker"] = [&tracker]() -> sol::table { return tracker; };
    gapi_table["add_msg"] = []( const sol::variadic_args & ) -> void {};

    auto used_grid_pos = std::optional<tripoint_abs_omt>();
    auto overmapbuffer_table = lua.create_table();
    overmapbuffer_table["fluid_grid_liquid_charges_at"] = [&used_grid_pos](
    const tripoint_abs_omt & pos, const itype_id & ) -> int {
        used_grid_pos = pos;
        return 100;
    };
    overmapbuffer_table["drain_fluid_grid_liquid_charges"] = []( const tripoint_abs_omt &,
    const itype_id &, int ) -> int {
        return 24;
    };

    auto menu_choices = std::vector<int> { 1, 2 };
    auto menu_query_count = size_t{ 0 };
    auto menu = lua.create_table();
    menu["title"] = []( const sol::object &, const std::string & ) -> void {};
    menu["add"] = []( const sol::object &, int, const std::string & ) -> void {};
    menu["query"] = [&menu_choices, &menu_query_count]( const sol::object & ) -> int {
        return menu_choices.at( menu_query_count++ );
    };

    auto ui_list = lua.create_table();
    ui_list["new"] = [&menu]() -> sol::table { return menu; };

    auto env = sol::environment( lua, sol::create, lua.globals() );
    env["gapi"] = gapi_table;
    env["overmapbuffer"] = overmapbuffer_table;
    env["UiList"] = ui_list;

    auto load_res = lua.load_file( "data/json/lua/plumbing.lua" );
    REQUIRE( load_res.valid() );
    auto exec = sol::protected_function( load_res );
    sol::set_environment( env, exec );
    auto exec_res = exec();
    REQUIRE( exec_res.valid() );
    auto plumbing = exec_res.get<sol::table>();

    auto params = lua.create_table();
    params["user"] = fake_user;
    params["pos"] = cata::detail::lua_coords::to_lua( tripoint_bub_ms( 10, 10, 0 ) );
    auto examine = plumbing["examine_shower"].get<sol::protected_function>();
    auto examine_res = examine( params );
    REQUIRE( examine_res.valid() );

    REQUIRE( used_grid_pos.has_value() );
    CHECK( used_grid_pos->raw() == tripoint( 2, 2, 0 ) );

    blood_intensity = 1;
    params["user"] = fake_user_on_fixture;
    auto clean_res = examine( params );
    REQUIRE( clean_res.valid() );
    CHECK( removed_blood_fields > 0 );
    CHECK( blood_intensity == 0 );

    auto &soap = get_avatar().add_item_with_id( itype_id( "soap" ), 10 );
    REQUIRE( soap.charges > 1 );
    params["user"] = get_avatar().as_character();
    params["pos"] = cata::detail::lua_coords::to_lua( get_avatar().bub_pos() );
    auto consume_res = examine( params );
    REQUIRE( consume_res.valid() );
    CHECK( get_avatar().activity->id() == activity_id( "ACT_WASH_SELF" ) );
    get_avatar().cancel_activity();
}

TEST_CASE( "plumbing_lua_morale_refreshes_without_stacking", "[lua][plumbing]" )
{
    clear_all_state();
    auto lua = make_lua_state();

    auto empty_item_stack = lua.create_table();
    empty_item_stack["items"] = [&lua]() -> sol::table { return lua.create_table(); };

    auto fake_map = lua.create_table();
    fake_map["points_in_radius"] = []( const sol::object &, const tripoint_bub_ms &, int,
    int ) -> std::vector<tripoint_bub_ms> {
        return { tripoint_bub_ms( 10, 10, 0 ) };
    };
    fake_map["get_items_at"] = [&empty_item_stack]( const sol::object &,
    const tripoint_bub_ms & ) -> sol::table {
        return empty_item_stack;
    };
    fake_map["has_vehicle_part_with_feature_at"] = []( const sol::object &, const tripoint_bub_ms &,
    const std::string & feature, bool ) -> bool {
        return feature == "TOWEL";
    };

    auto last_message = std::string{};
    auto gapi_table = lua.create_table();
    gapi_table["get_map"] = [&fake_map]() -> sol::table { return fake_map; };
    gapi_table["add_msg"] = [&last_message]( const sol::object &,
    const std::string & message ) -> void {
        last_message = message;
    };

    auto env = sol::environment( lua, sol::create, lua.globals() );
    env["gapi"] = gapi_table;

    auto load_res = lua.load_file( "data/json/lua/plumbing.lua" );
    REQUIRE( load_res.valid() );
    auto exec = sol::protected_function( load_res );
    sol::set_environment( env, exec );
    auto exec_res = exec();
    REQUIRE( exec_res.valid() );
    auto plumbing = exec_res.get<sol::table>();
    auto finish = plumbing["finish_wash"].get<sol::protected_function>();

    auto data = lua.create_table();
    data["mode"] = "shower";
    data["is_warm"] = false;
    data["used_hygiene"] = false;
    data["is_cold_wash"] = false;

    auto params = lua.create_table();
    params["user"] = get_avatar().as_character();
    params["data"] = data;

    REQUIRE( finish( params ).valid() );
    REQUIRE( finish( params ).valid() );
    REQUIRE( finish( params ).valid() );
    CHECK( get_avatar().get_morale( morale_type( "morale_shower" ) ) == 6 );
    CHECK( last_message.find( "vehicle towel hanger" ) != std::string::npos );
}

TEST_CASE( "plumbing_lua_data_hooks", "[lua]" )
{
    const auto &shower = furn_id( "f_shower" ).obj();
    const auto &bathtub = furn_id( "f_bathtub" ).obj();
    const auto lua_examine = iexamine_function_from_string( "lua_examine" );

    REQUIRE( shower.examine == lua_examine );
    REQUIRE( bathtub.examine == lua_examine );
    REQUIRE( shower.examine_action_id == "PLUMBING_SHOWER_EXAMINE" );
    REQUIRE( bathtub.examine_action_id == "PLUMBING_BATHTUB_EXAMINE" );

    const auto body_cleanser_flag = flag_id( "BODY_CLEANSER" );
    REQUIRE( body_cleanser_flag.is_valid() );
    REQUIRE( itype_id( "soap" ).obj().has_flag( body_cleanser_flag ) );
    REQUIRE( itype_id( "soapy_water" ).obj().has_flag( body_cleanser_flag ) );
    REQUIRE( itype_id( "soap_flakes" ).obj().has_flag( body_cleanser_flag ) );
    CHECK_FALSE( itype_id( "bleach" ).obj().has_flag( body_cleanser_flag ) );
    CHECK_FALSE( itype_id( "detergent" ).obj().has_flag( body_cleanser_flag ) );
    CHECK_FALSE( itype_id( "ammonia" ).obj().has_flag( body_cleanser_flag ) );

    REQUIRE( morale_type( "morale_shower" ).is_valid() );
    REQUIRE( morale_type( "morale_bath" ).is_valid() );
    REQUIRE( morale_type( "morale_cleansed_self" ).is_valid() );

    const auto &vehicle_shower = vpart_id( "vehicle_shower" ).obj();
    REQUIRE( vehicle_shower.has_flag( "SHOWER" ) );
    REQUIRE( vehicle_shower.has_flag( "FAUCET" ) );
}

TEST_CASE( "lua_called_from_cpp", "[lua]" )
{
    sol::state lua = make_lua_state();

    // Create global table for test
    sol::table test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    // Run Lua script
    run_lua_test_script( lua, "called_from_cpp_test.lua" );

    // Get Lua function
    REQUIRE( test_data["func"].valid() );
    sol::protected_function lua_func = test_data["func"];

    // Get test output
    REQUIRE( test_data["out"].valid() );
    sol::table out_data = test_data["out"];
    int ret = 0;

    REQUIRE( out_data["i"].valid() );
    REQUIRE( out_data["s"].valid() );

    CHECK_TUPLE( out_data["i"] == 0 );
    CHECK( out_data.get<std::string>( "s" ).empty() );

    // Execute function
    ret = lua_func( 4, "Bright " );

    CHECK( ret == 8 );
    CHECK_TUPLE( out_data["i"] == 4 );
    CHECK( out_data.get<std::string>( "s" ) == "Bright " );

    // Execute function again
    ret = lua_func( 6, "Nights" );

    CHECK( ret == 12 );
    CHECK_TUPLE( out_data["i"] == 10 );
    CHECK( out_data.get<std::string>( "s" ) == "Bright Nights" );

    // And again, but this time with 1 parameter
    ret = lua_func( 1 );

    CHECK( ret == 2 );
    CHECK_TUPLE( out_data["i"] == 11 );
    CHECK( out_data.get<std::string>( "s" ) == "Bright Nights" );
}

TEST_CASE( "lua_runtime_error", "[lua]" )
{
    sol::state lua = make_lua_state();

    // Running Lua script that has a runtime error
    // ends up throwing std::runtime_error on C++ side

    const std::string expected =
        "Script runtime error in tests/lua/runtime_error.lua: "
        "tests/lua/runtime_error.lua:2: attempt to index a nil value (global 'table_with_typo')\n"
        "stack traceback:\n"
        "\ttests/lua/runtime_error.lua:2: in main chunk";

    REQUIRE_THROWS_MATCHES(
        run_lua_test_script( lua, "runtime_error.lua" ),
        std::runtime_error,
        Catch::Message( expected )
    );
}

TEST_CASE( "lua_called_error_on_lua_side", "[lua]" )
{
    sol::state lua = make_lua_state();

    // Running Lua script that calls error()
    // ends up throwing std::runtime_error on C++ side

    const std::string expected =
        "Script runtime error in tests/lua/called_error_on_lua_side.lua: "
        "tests/lua/called_error_on_lua_side.lua:2: Error called on Lua side!\n"
        "stack traceback:\n"
        "\t[C]: in function 'base.error'\n"
        "\ttests/lua/called_error_on_lua_side.lua:2: in main chunk";

    REQUIRE_THROWS_MATCHES(
        run_lua_test_script( lua, "called_error_on_lua_side.lua" ),
        std::runtime_error,
        Catch::Message( expected )
    );
}

static void cpp_call_error( sol::this_state L )
{
    luaL_error( L.lua_state(), "Error called on Cpp side!" );
}

TEST_CASE( "lua_called_error_on_cpp_side", "[lua]" )
{
    sol::state lua = make_lua_state();

    lua.globals()["cpp_call_error"] = cpp_call_error;

    // Running Lua script that calls C++ function that calls error()
    // ends up throwing std::runtime_error on C++ side

    const std::string expected =
        "Script runtime error in tests/lua/called_error_on_cpp_side.lua: "
        "tests/lua/called_error_on_cpp_side.lua:2: Error called on Cpp side!\n"
        "stack traceback:\n"
        "\t[C]: in function 'base.cpp_call_error'\n"
        "\ttests/lua/called_error_on_cpp_side.lua:2: in main chunk";

    REQUIRE_THROWS_MATCHES(
        run_lua_test_script( lua, "called_error_on_cpp_side.lua" ),
        std::runtime_error,
        Catch::Message( expected )
    );
}

[[ noreturn ]]
static void cpp_throw_exception()
{
    throw std::runtime_error( "Exception thrown on Cpp side!" );
}

TEST_CASE( "lua_called_cpp_func_throws", "[lua]" )
{
    sol::state lua = make_lua_state();

    lua.globals()["cpp_throw_exception"] = cpp_throw_exception;

    // Running Lua script that calls C++ function that throws std::runtime_error
    // ends up throwing another std::runtime_error

    const std::string expected =
        "Script runtime error in tests/lua/called_cpp_func_throws.lua: "
        "Exception thrown on Cpp side!\n"
        "stack traceback:\n"
        "\t[C]: in function 'base.cpp_throw_exception'\n"
        "\ttests/lua/called_cpp_func_throws.lua:2: in main chunk";

    REQUIRE_THROWS_MATCHES(
        run_lua_test_script( lua, "called_cpp_func_throws.lua" ),
        std::runtime_error,
        Catch::Message( expected )
    );
}

struct custom_udata {
    int unused = 0;
};

TEST_CASE( "lua_get_luna_type", "[lua]" )
{
    sol::state lua = make_lua_state();

    SECTION( "number" ) {
        sol::table st = lua.create_table();
        st["k"] = 3;
        CHECK( get_luna_type( st["k"] ) == std::nullopt );
    }
    SECTION( "string" ) {
        sol::table st = lua.create_table();
        st["k"] = "abc";
        CHECK( get_luna_type( st["k"] ) == std::nullopt );
    }
    SECTION( "table" ) {
        sol::table st = lua.create_table();
        st["k"] = lua.create_table();
        CHECK( get_luna_type( st["k"] ) == std::nullopt );
    }
    SECTION( "registered userdata" ) {
        sol::table st = lua.create_table();
        st["k"] = tripoint( 1, 2, 3 );
        CHECK( get_luna_type( st["k"] ) == std::optional( "Tripoint" ) );
    }
    SECTION( "unknown userdata" ) {
        sol::table st = lua.create_table();
        st["k"] = custom_udata{};
        CHECK( get_luna_type( st["k"] ) == std::nullopt );
    }
}

TEST_CASE( "lua_map_vehicle_replacement", "[lua]" )
{
    clear_all_state();

    auto &here = get_map();
    const auto origin = tripoint_bub_ms( 60, 60, 0 );
    const auto original_facing = -90_degrees;
    const auto overridden_facing = 180_degrees;
    auto *vehicle_ptr = here.add_vehicle( vproto_id( "bicycle" ), origin, original_facing, 0, 0 );
    REQUIRE( vehicle_ptr != nullptr );

    sol::state lua = make_lua_state();
    auto test_data = lua.create_table();
    test_data["map"] = &here;
    lua.globals()["test_data"] = test_data;

    run_lua_test_script( lua, "map_vehicle_replacement_test.lua" );

    CHECK( test_data.get<int>( "vehicle_count_before" ) == 1 );
    CHECK( test_data.get<std::string>( "vehicle_type_before" ) == "bicycle" );
    CHECK( test_data.get<bool>( "replace_ok" ) );
    CHECK( test_data.get<bool>( "replace_with_opts_ok" ) );
    CHECK( test_data.get<int>( "vehicle_count_after" ) == 1 );
    CHECK( test_data.get<std::string>( "vehicle_type_after" ) == "swivel_chair" );

    const auto vehicles = here.get_vehicles();
    REQUIRE( vehicles.size() == 1 );
    CHECK( vehicles.front().pos == origin );
    REQUIRE( vehicles.front().v != nullptr );
    CHECK( vehicles.front().v->type == vproto_id( "swivel_chair" ) );
    CHECK( normalize( vehicles.front().v->face.dir() ) == normalize( overridden_facing ) );
    CHECK( vehicles.front().v->static_drag() == vehicles.front().v->static_drag( false ) );
    const auto part_count = vehicles.front().v->part_count();
    auto has_lock = false;
    for( auto index = 0; index < part_count; ++index ) {
        CHECK( vehicles.front().v->part( index ).damage_percent() == Approx( 0.0 ) );
        has_lock = has_lock ||
                   vehicles.front().v->part_with_feature( index, "DOOR_LOCKING", false ) == index;
    }
    CHECK( !has_lock );
}

TEST_CASE( "lua_table_serde", "[lua]" )
{
    sol::state lua = make_lua_state();

    sol::table st = lua.create_table();
    st["inner_val"] = 4;

    sol::table t = lua.create_table();
    t["member_bool"] = false;
    t["member_float"] = 16.0;
    t["member_int"] = 11;
    t["member_string"] = "fuckoff";
    t["member_usertype"] = tripoint( 7, 5, 3 );
    t["member_point_coord"] = cata::detail::lua_coords::to_lua( point_bub_ms( 8, 9 ) );
    t["member_tripoint_coord"] = cata::detail::lua_coords::to_lua( tripoint_abs_omt( 1, 2, 3 ) );
    t["subtable"] = st;

    std::string data = serialize_wrapper( [&]( JsonOut & jsout ) {
        cata::serialize_lua_table( t, jsout );
    } );

    sol::table nt = lua.create_table();
    deserialize_wrapper( [&]( JsonIn & jsin ) {
        JsonObject jsobj = jsin.get_object();
        cata::deserialize_lua_table( nt, jsobj );
    }, data );

    // Sanity check: field does not exist
    sol::object mem_none = nt["member_the_best"];
    REQUIRE( !mem_none.valid() );

    sol::object mem_bool = nt["member_bool"];
    REQUIRE( mem_bool.valid() );
    REQUIRE( mem_bool.is<bool>() );
    CHECK( mem_bool.as<bool>() == false );

    sol::object mem_float = nt["member_float"];
    REQUIRE( mem_float.valid() );
    REQUIRE( mem_float.is<double>() );
    CHECK( mem_float.as<double>() == Approx( 16.0 ) );

    sol::object mem_int = nt["member_int"];
    REQUIRE( mem_int.valid() );
    CHECK( mem_int.is<double>() );
    REQUIRE( mem_int.is<int>() );
    CHECK( mem_int.as<int>() == 11 );

    sol::object mem_string = nt["member_string"];
    REQUIRE( mem_string.valid() );
    REQUIRE( mem_string.is<std::string>() );
    CHECK( mem_string.as<std::string>() == "fuckoff" );

    sol::object mem_usertype = nt["member_usertype"];
    REQUIRE( mem_usertype.valid() );
    REQUIRE( mem_usertype.is<tripoint>() );
    CHECK( mem_usertype.as<tripoint>() == tripoint( 7, 5, 3 ) );

    auto mem_point_coord = sol::object( nt["member_point_coord"] );
    REQUIRE( mem_point_coord.valid() );
    REQUIRE( mem_point_coord.is<cata::detail::lua_coords::lua_point_coord>() );
    const auto point_coord = mem_point_coord.as<cata::detail::lua_coords::lua_point_coord>();
    CHECK( point_coord.raw == point( 8, 9 ) );
    CHECK( point_coord.origin == coords::origin::bubble );
    CHECK( point_coord.scale == coords::scale::map_square );

    auto mem_tripoint_coord = sol::object( nt["member_tripoint_coord"] );
    REQUIRE( mem_tripoint_coord.valid() );
    REQUIRE( mem_tripoint_coord.is<cata::detail::lua_coords::lua_tripoint_coord>() );
    const auto tripoint_coord = mem_tripoint_coord.as<cata::detail::lua_coords::lua_tripoint_coord>();
    CHECK( tripoint_coord.raw == tripoint( 1, 2, 3 ) );
    CHECK( tripoint_coord.origin == coords::origin::abs );
    CHECK( tripoint_coord.scale == coords::scale::overmap_terrain );

    sol::object mem_table = nt["subtable"];
    REQUIRE( mem_table.valid() );
    REQUIRE( mem_table.is<sol::table>() );

    // Subtable
    sol::table nts = mem_table;
    sol::object inner_val = nts["inner_val"];
    REQUIRE( inner_val.valid() );
    REQUIRE( inner_val.is<int>() );
    CHECK( inner_val.as<int>() == 4 );
}

TEST_CASE( "lua_table_serde_error_no_reg", "[lua]" )
{
    sol::state lua = make_lua_state();

    sol::table t = lua.create_table();
    t["my_member"] = custom_udata{};

    // Trying to serialize unregistered type results in error
    std::string data;
    std::string dmsg = capture_debugmsg_during( [&]() {
        data = serialize_wrapper( [&]( JsonOut & jsout ) {
            cata::serialize_lua_table( t, jsout );
        } );
    } );

    CHECK( dmsg == "Tried to serialize usertype that was not registered with luna." );
}

TEST_CASE( "lua_table_serde_error_no_luna", "[lua]" )
{
    sol::state lua = make_lua_state();

    lua.new_usertype<custom_udata>( "CustomUData" );

    sol::table t = lua.create_table();
    t["my_member"] = custom_udata{};

    // Trying to serialize type that was not registered with luna results in error
    std::string data;
    std::string dmsg = capture_debugmsg_during( [&]() {
        data = serialize_wrapper( [&]( JsonOut & jsout ) {
            cata::serialize_lua_table( t, jsout );
        } );
    } );

    CHECK( dmsg == "Tried to serialize usertype that was not registered with luna." );
}

TEST_CASE( "lua_table_serde_error_no_ser", "[lua]" )
{
    sol::state lua = make_lua_state();

    sol::table t = lua.create_table();
    avatar *av_ptr = &get_avatar();
    t["my_member"] = av_ptr;

    // Trying to serialize unserializable type results in error
    std::string data;
    std::string dmsg = capture_debugmsg_during( [&]() {
        data = serialize_wrapper( [&]( JsonOut & jsout ) {
            cata::serialize_lua_table( t, jsout );
        } );
    } );

    CHECK( dmsg == "Tried to serialize usertype that does not allow serialization." );
}

TEST_CASE( "lua_table_serde_error_rec_table", "[lua]" )
{
    sol::state lua = make_lua_state();

    sol::table t1 = lua.create_table();
    sol::table t2 = lua.create_table();
    sol::table t3 = lua.create_table();
    sol::table t4 = lua.create_table();
    sol::table t5 = lua.create_table();

    /*
        t1 -> t2 -> t3 -> t5
        ^      |
        |      \--> t4 -\
        |               |
        \---------------/
    */
    t1["t2"] = t2;
    t2["t3"] = t3;
    t2["t4"] = t4;
    t3["t5"] = t5;
    t4["t1"] = t1;

    // Trying to serialize recursive table results in error
    std::string data;
    std::string dmsg = capture_debugmsg_during( [&]() {
        data = serialize_wrapper( [&]( JsonOut & jsout ) {
            cata::serialize_lua_table( t1, jsout );
        } );
    } );

    CHECK( dmsg == "Tried to serialize recursive table structure." );
}

TEST_CASE( "id_conversions", "[lua]" )
{
    sol::state lua = make_lua_state();

    sol::table t = lua.create_table();

    // The functions don't need to do anything, we're just checking type conversion
    t["func_raw"] = []( const ter_t & ) {

    };
    t["func_int_id"] = []( const ter_id & ) {

    };
    t["func_str_id"] = []( const ter_str_id & ) {

    };

    static const ter_str_id t_fragile_roof( "t_fragile_roof" );
    REQUIRE( t_fragile_roof.is_valid() );

    const ter_t *raw_ptr = &t_fragile_roof.obj();

    t["str_id"] = t_fragile_roof;
    t["int_id"] = t_fragile_roof.id();
    t["raw_ptr"] = raw_ptr;

    lua.globals()["test_data"] = t;

    run_lua_test_script( lua, "id_conversions.lua" );
}

TEST_CASE( "id_conversions_no_int_id", "[lua]" )
{
    sol::state lua = make_lua_state();

    sol::table t = lua.create_table();

    // The functions don't need to do anything, we're just checking type conversion
    t["func_raw"] = []( const faction & ) {

    };
    t["func_str_id"] = []( const faction_id & ) {

    };

    REQUIRE( your_fac.is_valid() );

    const faction *raw_ptr = &your_fac.obj();

    t["str_id"] = your_fac;
    t["raw_ptr"] = raw_ptr;

    lua.globals()["test_data"] = t;

    run_lua_test_script( lua, "id_conversions_no_int_id.lua" );
}

TEST_CASE( "catalua_regression_sol_1444", "[lua]" )
{
    // Regression test for https://github.com/ThePhD/sol2/issues/1444
    sol::state lua = make_lua_state();
    run_lua_test_script( lua, "regression_sol_1444.lua" );
}

TEST_CASE( "catalua_table_compare", "[lua]" )
{
    sol::state lua = make_lua_state();
    sol::table a = lua.create_table();
    sol::table b = lua.create_table();
    SECTION( "empty tables" ) {
        CHECK( compare_tables( a, b ) );
        CHECK( compare_tables( b, a ) );
    }
    SECTION( "one table has values, the other is empty" ) {
        a["my_key"] = "my_val";
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables have identical keys and values" ) {
        a["my_key"] = "my_val";
        b["my_key"] = "my_val";
        CHECK( compare_tables( a, b ) );
        CHECK( compare_tables( b, a ) );
    }
    SECTION( "tables have different values" ) {
        a["my_key"] = "my_val";
        b["my_key"] = "ANOTHER_VAL";
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables have different keys and values" ) {
        a["my_key"] = "my_val";
        b["best_cata"] = "bn";
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables have different keys and values" ) {
        a["my_key"] = "my_val";
        b["best_cata"] = "bn";
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "can't compare tables with functions" ) {
        a["my_key"] = &compare_tables;
        b["my_key"] = &compare_tables;
        CHECK_THROWS( compare_tables( a, b ) );
        CHECK_THROWS( compare_tables( b, a ) );
    }
    SECTION( "can't compare tables with lambdas" ) {
        a["my_key"] = [&]( int ) {
            debugmsg( "Function A" );
        };
        b["my_key"] = [&]( int ) {
            debugmsg( "Function B" );
        };
        CHECK_THROWS( compare_tables( a, b ) );
        CHECK_THROWS( compare_tables( b, a ) );
    }
    SECTION( "tables have different values" ) {
        a["my_key"] = 1;
        b["my_key"] = 2;
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables have different value types" ) {
        a["my_key"] = 1;
        b["my_key"] = "2";
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables have different number types" ) {
        a["my_key"] = 1;
        b["my_key"] = 1.0;
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables have different key types" ) {
        a["1"] = "abc";
        b[1] = "abc";
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables have identical subtables" ) {
        sol::table a_sub = lua.create_table();
        sol::table b_sub = lua.create_table();
        a_sub["my_key"] = "my_val";
        b_sub["my_key"] = "my_val";
        a["sub"] = a_sub;
        b["sub"] = b_sub;
        CHECK( compare_tables( a, b ) );
        CHECK( compare_tables( b, a ) );
    }
    SECTION( "tables have different subtables" ) {
        sol::table a_sub = lua.create_table();
        sol::table b_sub = lua.create_table();
        a_sub["my_key"] = "my_val";
        b_sub["my_key"] = "ANOTHER_VAL";
        a["sub"] = a_sub;
        b["sub"] = b_sub;
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables have same userdata" ) {
        a["my_key"] = tripoint( 1, 2, 3 );
        b["my_key"] = tripoint( 1, 2, 3 );
        CHECK( compare_tables( a, b ) );
        CHECK( compare_tables( b, a ) );
    }
    SECTION( "tables have different userdata" ) {
        a["my_key"] = tripoint( 1, 2, 3 );
        b["my_key"] = tripoint( 12, 34, 56 );
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables have different userdata types" ) {
        a["my_key"] = tripoint( 1, 2, 3 );
        b["my_key"] = point( 12, 34 );
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables with same userdata as keys" ) {
        a[tripoint( 1, 3, 37 )] = "my_val";
        b[tripoint( 1, 3, 37 )] = "my_val";
        CHECK( compare_tables( a, b ) );
        CHECK( compare_tables( b, a ) );
    }
    SECTION( "tables have different userdata types in keys" ) {
        a[tripoint( 1, 3, 37 )] = "my_val";
        b[point( 12, 34 )] = "my_val";
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables have different userdata values in keys" ) {
        a[tripoint( 1, 3, 37 )] = "my_val";
        b[tripoint( 1, 2, 3 )] = "my_val";
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
    SECTION( "tables have equivalent tables as keys" ) {
        sol::table key_a = lua.create_table();
        key_a["hello"] = "world";
        sol::table key_b = lua.create_table();
        key_b["hello"] = "world";
        a[key_a] = "my_val";
        b[key_b] = "my_val";
        CHECK( compare_tables( a, b ) );
        CHECK( compare_tables( b, a ) );
    }
    SECTION( "tables have different tables as keys" ) {
        sol::table key_a = lua.create_table();
        key_a["hello"] = "world";
        sol::table key_b = lua.create_table();
        key_b["hello"] = "BRIGHT NIGHTS";
        a[key_a] = "my_val";
        b[key_b] = "my_val";
        CHECK_FALSE( compare_tables( a, b ) );
        CHECK_FALSE( compare_tables( b, a ) );
    }
}

static std::string serialize_table( sol::table t )
{
    return serialize_wrapper( [&]( JsonOut & jsout ) {
        cata::serialize_lua_table( t, jsout );
    } );
}

static sol::table deserialize_table( sol::state &lua, const std::string &data )
{
    sol::table res = lua.create_table();
    deserialize_wrapper( [&]( JsonIn & jsin ) {
        JsonObject jsobj = jsin.get_object();
        cata::deserialize_lua_table( res, jsobj );
    }, data );
    return res;
}

static void run_serde_test( sol::state &lua, sol::table original )
{
    std::string data = serialize_table( original );
    sol::table got = deserialize_table( lua, data );
    bool eq = compare_tables( original, got );
    if( !eq ) {
        std::string data2 = serialize_table( got );
        CHECK( data == data2 );
    }
    REQUIRE( eq );
}

TEST_CASE( "catalua_table_serde", "[lua]" )
{
    sol::state lua = make_lua_state();
    sol::table t = lua.create_table();
    SECTION( "empty table" ) {
        run_serde_test( lua, t );
    }
    SECTION( "empty table from JSON" ) {
        // This is a short notation for an empty table
        std::string data = "{}";
        sol::table got = deserialize_table( lua, data );
        bool eq = compare_tables( t, got );
        if( !eq ) {
            std::string data2 = serialize_table( got );
            CHECK( data == data2 );
        }
        REQUIRE( eq );
    }
    SECTION( "table with string keys and values" ) {
        t["my_key"] = "my_val";
        t["another_key"] = "another_val";
        run_serde_test( lua, t );
    }
    SECTION( "table with integer values" ) {
        t["my_key"] = 1337;
        t["another_key"] = 1234;
        run_serde_test( lua, t );
    }
    SECTION( "table with floating values" ) {
        t["my_key"] = 13.37;
        t["another_key"] = 1.234;
        run_serde_test( lua, t );
    }
    SECTION( "table with integer keys" ) {
        t.add( "abc" );
        t.add( "def" );
        run_serde_test( lua, t );
    }
    SECTION( "table with userdata values" ) {
        t["my_key"] = point( 13, 37 );
        t["another_key"] = tripoint( 12, 34, 56 );
        run_serde_test( lua, t );
    }
    SECTION( "table with typed coordinate values" ) {
        t["my_key"] = cata::detail::lua_coords::to_lua( point_bub_ms( 13, 37 ) );
        t["another_key"] = cata::detail::lua_coords::to_lua( tripoint_abs_omt( 12, 34, 56 ) );
        run_serde_test( lua, t );
    }
    SECTION( "table with userdata keys" ) {
        t[point( 13, 37 )] = "leet";
        t[tripoint( 12, 34, 56 )] = "numbers";
        run_serde_test( lua, t );
    }
    SECTION( "table with typed coordinate keys" ) {
        t[cata::detail::lua_coords::to_lua( point_bub_ms( 13, 37 ) )] = "leet";
        t[cata::detail::lua_coords::to_lua( tripoint_abs_omt( 12, 34, 56 ) )] = "numbers";
        run_serde_test( lua, t );
    }
    SECTION( "table with userdata keys and values" ) {
        t[point( 13, 37 )] = tripoint( 1, 3, 37 );
        t[tripoint( 12, 34, 56 )] = point( 98765, 43210 );
        run_serde_test( lua, t );
    }
    SECTION( "table with typed coordinate keys and values" ) {
        t[cata::detail::lua_coords::to_lua( point_bub_ms( 13, 37 ) )] =
            cata::detail::lua_coords::to_lua( tripoint_abs_omt( 1, 3, 37 ) );
        t[cata::detail::lua_coords::to_lua( tripoint_abs_omt( 12, 34, 56 ) )] =
            cata::detail::lua_coords::to_lua( point_rel_omt( 98765, 43210 ) );
        run_serde_test( lua, t );
    }
    SECTION( "table with tables as keys" ) {
        sol::table key1 = lua.create_table();
        key1["my_key"] = "my_val";
        sol::table key2 = lua.create_table();
        key2[13] = 37;
        t[key1] = "hello";
        t[key2] = "world";
        run_serde_test( lua, t );
    }
}

TEST_CASE( "lua_units_functions", "[lua]" )
{
    sol::state lua = make_lua_state();

    // Test variables
    const double angle_degrees = 32.0; // Multiple of 2 in case of floating-point error
    const int energy_kilojoules = 128;
    const std::int64_t mass_kilograms = 64;
    const int volume_liters = 16;

    // Create global table for test
    sol::table test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    // Set global table keys
    test_data["angle_degrees"] = angle_degrees;
    test_data["energy_kilojoules"] = energy_kilojoules;
    test_data["mass_kilograms"] = mass_kilograms;
    test_data["volume_liters"] = volume_liters;

    // Run Lua script
    run_lua_test_script( lua, "units_test.lua" );

    // Get test output
    double lua_angle_arcmins = test_data["angle_arcmins"];
    int lua_energy_joules = test_data["energy_joules"];
    std::int64_t lua_mass_grams = test_data["mass_grams"];
    int lua_volume_milliliters = test_data["volume_milliliters"];

    // Check if match
    REQUIRE( lua_angle_arcmins == units::to_arcmin( units::from_degrees( angle_degrees ) ) );
    REQUIRE( lua_energy_joules == units::to_joule( units::from_kilojoule( energy_kilojoules ) ) );
    REQUIRE( lua_mass_grams == units::to_gram( units::from_kilogram( mass_kilograms ) ) );
    REQUIRE( lua_volume_milliliters == units::to_milliliter( units::from_liter( volume_liters ) ) );
}

TEST_CASE( "lua_require_relative", "[lua]" )
{
    sol::state lua = make_lua_state();

    // Create global table for test
    sol::table test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    // Run Lua script that uses relative require
    run_lua_test_script( lua, "require_test_relative.lua" );

    // Check results
    int result_add = test_data["result_add"];
    int result_mul = test_data["result_mul"];

    REQUIRE( result_add == 5 );   // 2 + 3
    REQUIRE( result_mul == 20 );  // 4 * 5
}

TEST_CASE( "lua_require_dotted", "[lua]" )
{
    sol::state lua = make_lua_state();

    // Create global table for test
    sol::table test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    // Run Lua script that uses dotted require
    run_lua_test_script( lua, "require_test_dotted.lua" );

    // Check results
    int result_add = test_data["result_add"];
    int result_mul = test_data["result_mul"];

    REQUIRE( result_add == 30 );  // 10 + 20
    REQUIRE( result_mul == 21 );  // 3 * 7
}

TEST_CASE( "robofac_authorization_scans_nearby_hub01_tiles", "[lua][robofac]" )
{
    auto lua = make_lua_state();
    auto test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    run_lua_test_script( lua, "robofac_authorization_scan_test.lua" );

    CHECK( test_data.get<bool>( "npc_authorized" ) );
    CHECK( test_data.get<bool>( "npc_attitude_cleared" ) );
    CHECK( test_data.get<bool>( "monster_authorized" ) );
    CHECK( test_data.get<std::string>( "hub01_prefix" ) == "robofachq" );
    CHECK( test_data.get<int>( "npc_omt_queries" ) == 1 );
    CHECK( test_data.get<int>( "monster_omt_queries" ) == 1 );
    CHECK( test_data.get<int>( "npc_query_radius" ) == 4 );
    CHECK( test_data.get<int>( "monster_query_radius" ) == 4 );
    CHECK( test_data.get<bool>( "npc_query_ignores_z" ) );
    CHECK( test_data.get<bool>( "monster_query_ignores_z" ) );
}

TEST_CASE( "lua_cooking_enjoy_bonus_applies_to_unheated_comestibles", "[lua][cooking]" )
{
    auto lua = make_lua_state();
    auto test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    run_lua_test_script( lua, "cooking_enjoy_bonus_test.lua" );

    CHECK( test_data.get<std::string>( "high_skill_var_name" ) == "comestible_fun" );
    CHECK( test_data.get<int>( "high_skill_fun" ) == 15 );
    CHECK( test_data.get<int>( "high_skill_bad_fun" ) == -5 );
    CHECK( test_data.get<std::string>( "zero_skill_var_name" ) == "comestible_fun" );
    CHECK( test_data.get<int>( "zero_skill_fun" ) == 10 );
}

static auto init_test_lua_hook_state( cata::lua_state &state ) -> void
{
    state.lua = make_lua_state();
    sol::state &lua = state.lua;

    sol::table game = lua.create_table();
    sol::table hooks = lua.create_table();
    sol::table internal = lua.create_table();

    game["hooks"] = hooks;
    game["cata_internal"] = internal;
    game["current_mod"] = "test_mod";
    lua.globals()["game"] = game;

    game["add_hook"] = [&lua]( const std::string & hook_name, const sol::object & entry ) {
        auto *L = lua.lua_state();
        sol::table hooks_table = lua["game"]["hooks"];
        sol::optional<sol::table> maybe_hook_list = hooks_table[hook_name];

        if( !maybe_hook_list ) {
            debugmsg( "Invalid hook name: %s", hook_name );
            return;
        }

        sol::table hook_list = *maybe_hook_list;

        const auto current_mod = lua["game"]["current_mod"];
        const auto mod_id = current_mod.valid() && current_mod.get_type() == sol::type::string
                            ? current_mod.get<std::string>()
                            : "<unknown>";

        const auto is_function = entry.is<sol::function>() || entry.is<sol::protected_function>();
        if( is_function ) {
            auto new_entry = lua.create_table();
            new_entry["mod_id"] = mod_id;
            new_entry["priority"] = 0;
            new_entry["fn"] = entry;

            sol::stack::push( L, hook_list );
            const auto next_index = static_cast<int>( lua_rawlen( L, -1 ) ) + 1;
            lua_pop( L, 1 );

            hook_list.set( next_index, new_entry );
            return;
        }

        if( entry.is<sol::table>() ) {
            auto tbl = entry.as<sol::table>();
            const auto has_mod_id = tbl["mod_id"].valid() && tbl["mod_id"].get_type() != sol::type::lua_nil;
            if( !has_mod_id ) {
                tbl["mod_id"] = mod_id;
            }

            sol::stack::push( L, hook_list );
            const auto next_index = static_cast<int>( lua_rawlen( L, -1 ) ) + 1;
            lua_pop( L, 1 );

            hook_list.set( next_index, tbl );
            return;
        }

        debugmsg( "add_hook expects function or table entry, got type: %s for hook: %s",
                  sol::type_name( lua, entry.get_type() ).c_str(), hook_name.c_str() );
    };

    sol::table cata_tbl = lua.create_table();
    cata_tbl.set_function( "run_hooks", [&state]( const std::string & name ) -> sol::table {
        return cata::run_hooks( name, nullptr, { .state = &state } );
    } );
    cata_tbl.set_function( "run_hooks_exit_early", [&state]( const std::string & name ) -> sol::table {
        return cata::run_hooks( name, nullptr, { .exit_early = true, .state = &state } );
    } );
    lua.globals()["cata"] = cata_tbl;
}

TEST_CASE( "lua_has_hooks_tracks_registered_entries", "[lua]" )
{
    cata::lua_state state;
    init_test_lua_hook_state( state );
    sol::state &lua = state.lua;

    auto hook_list = lua.create_table();
    lua.globals()["game"]["hooks"]["on_creature_do_turn"] = hook_list;

    CHECK_FALSE( cata::has_hooks( "on_creature_do_turn", { .state = &state } ) );
    CHECK_FALSE( cata::has_hooks( "on_invalid_hook_for_test", { .state = &state } ) );

    hook_list[1] = []( sol::table ) {};
    CHECK( cata::has_hooks( "on_creature_do_turn", { .state = &state } ) );
}

TEST_CASE( "lua_hooks_order_and_chaining", "[lua]" )
{
    cata::lua_state state;
    init_test_lua_hook_state( state );
    sol::state &lua = state.lua;

    lua.globals()["game"]["hooks"]["on_game_load"] = lua.create_table();

    run_lua_script( lua, "tests/lua/hooks_order_and_chaining_test.lua" );

    sol::table results_tbl = lua.globals()["game"]["cata_internal"]["hook_test_results"];
    const sol::table log_tbl = results_tbl["log"];

    REQUIRE( log_tbl.valid() );

    // Order should be priority 10 -> 5 -> legacy(0)
    CHECK( log_tbl.get<std::string>( 1 ) == "p10" );
    CHECK( log_tbl.get<std::string>( 2 ) == "p5" );
    CHECK( log_tbl.get<std::string>( 3 ) == "legacy" );

    // Ensure hooks can override params.prev and affect downstream hooks.
    CHECK( results_tbl.get<std::string>( "prev_seen" ) == "p5_ret" );
}

TEST_CASE( "lua_hooks_exit_early", "[lua]" )
{
    cata::lua_state state;
    init_test_lua_hook_state( state );
    sol::state &lua = state.lua;

    lua.globals()["game"]["hooks"]["on_game_save"] = lua.create_table();

    run_lua_script( lua, "tests/lua/hooks_exit_early_test.lua" );

    sol::table results_tbl = lua.globals()["game"]["cata_internal"]["hook_test_results"];
    const sol::table log_tbl = results_tbl["log"];

    REQUIRE( log_tbl.valid() );

    CHECK( log_tbl.get<std::string>( 1 ) == "p10" );
    CHECK( results_tbl.get<bool>( "allowed" ) == false );
    CHECK( log_tbl.get<sol::optional<std::string>>( 2 ) == sol::nullopt );
}

// ─── Hook wiring tests ───────────────────────────────────────────────────────
// Each test registers a callback on the global Lua state, triggers the
// corresponding C++ event, asserts the callback fired, then removes the entry.
//
// The cleanup struct guarantees removal even when a REQUIRE inside a helper
// throws and unwinds the stack.

namespace
{

static const efftype_id effect_test_lua_effect( "test_lua_effect" );

struct hook_cleanup {
    sol::table list;
    int idx;
    hook_cleanup( sol::table l, int i ) : list( l ), idx( i ) {}
    ~hook_cleanup() {
        list[idx] = sol::lua_nil;
    }
};

// Append an entry backed by a C++ callable to a global hook list.
// Returns the table index so the caller can build a hook_cleanup.
template<typename Fn>
static auto push_hook( sol::state &lua, const std::string &name,
                       Fn &&fn ) -> std::pair<sol::table, int>
{
    sol::table list = lua["game"]["hooks"][name];
    auto *L = lua.lua_state();
    sol::stack::push( L, list );
    const auto idx = static_cast<int>( lua_rawlen( L, -1 ) ) + 1;
    lua_pop( L, 1 );

    auto entry = lua.create_table();
    entry["mod_id"] = "test";
    entry["priority"] = 0;
    entry["fn"] = std::forward<Fn>( fn );
    list[idx] = entry;
    return { list, idx };
}

} // namespace

// ── Trivial ──────────────────────────────────────────────────────────────────

TEST_CASE( "lua_hook_wiring_character_reset_stats", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    const auto char_ptr = std::make_shared<Character *>( nullptr );
    const auto [list, idx] = push_hook( lua, "on_character_reset_stats",
    [char_ptr]( sol::table params ) {
        *char_ptr = params["character"].get<sol::optional<Character *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup{ list, idx };

    get_avatar().reset_stats();

    CHECK( *char_ptr == &get_avatar() );
}

TEST_CASE( "lua_hook_wiring_monster_loaded", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    const auto mon_ptr = std::make_shared<monster *>( nullptr );
    const auto cre_ptr = std::make_shared<Creature *>( nullptr );

    const auto [ml, mi] = push_hook( lua, "on_monster_loaded",
    [mon_ptr]( sol::table params ) {
        *mon_ptr = params["monster"].get<sol::optional<monster *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup_ml{ ml, mi };

    const auto [cl, ci] = push_hook( lua, "on_creature_loaded",
    [cre_ptr]( sol::table params ) {
        auto *m = params["creature"].get<sol::optional<monster *>>().value_or( nullptr );
        *cre_ptr = static_cast<Creature *>( m );
    } );
    hook_cleanup cleanup_cl{ cl, ci };

    monster &mon = spawn_test_monster( "mon_zombie", tripoint_bub_ms{ 5, 5, 0 } );
    mon.on_load();

    CHECK( *mon_ptr == &mon );
    CHECK( *cre_ptr == static_cast<Creature *>( &mon ) );
}

// ── Easy ─────────────────────────────────────────────────────────────────────

TEST_CASE( "lua_hook_wiring_monster_spawn", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    const auto mon_ptr = std::make_shared<monster *>( nullptr );
    const auto cre_ptr = std::make_shared<Creature *>( nullptr );

    const auto [ms, msi] = push_hook( lua, "on_monster_spawn",
    [mon_ptr]( sol::table params ) {
        *mon_ptr = params["monster"].get<sol::optional<monster *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup_ms{ ms, msi };

    const auto [cs, csi] = push_hook( lua, "on_creature_spawn",
    [cre_ptr]( sol::table params ) {
        auto *m = params["creature"].get<sol::optional<monster *>>().value_or( nullptr );
        *cre_ptr = static_cast<Creature *>( m );
    } );
    hook_cleanup cleanup_cs{ cs, csi };

    monster &mon = spawn_test_monster( "mon_zombie", tripoint_bub_ms{ 5, 5, 0 } );

    CHECK( *mon_ptr == &mon );
    CHECK( *cre_ptr == static_cast<Creature *>( &mon ) );
}

TEST_CASE( "lua_hook_wiring_mon_death", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    monster &mon = spawn_test_monster( "mon_zombie", tripoint_bub_ms{ 5, 5, 0 } );

    const auto mon_ptr = std::make_shared<monster *>( nullptr );
    const auto [list, idx] = push_hook( lua, "on_mon_death",
    [mon_ptr]( sol::table params ) {
        *mon_ptr = params["mon"].get<sol::optional<monster *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup{ list, idx };

    mon.die( nullptr );

    CHECK( *mon_ptr == &mon );
}

TEST_CASE( "lua_hook_wiring_creature_melee_attacked", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    // avatar is at {60,60,0} after clear_all_state; place target adjacent
    monster &mon = spawn_test_monster( "mon_zombie", tripoint_bub_ms{ 61, 60, 0 } );

    const auto char_ptr = std::make_shared<Character *>( nullptr );
    const auto tgt_ptr  = std::make_shared<Creature *>( nullptr );
    const auto success  = std::make_shared<sol::optional<bool>>( sol::nullopt );

    const auto [list, idx] = push_hook( lua, "on_creature_melee_attacked",
    [char_ptr, tgt_ptr, success]( sol::table params ) {
        *char_ptr = params["char"].get<sol::optional<Character *>>().value_or( nullptr );
        *tgt_ptr  = params["target"].get<sol::optional<Creature *>>().value_or( nullptr );
        *success  = params["success"].get<sol::optional<bool>>();
    } );
    hook_cleanup cleanup{ list, idx };

    get_avatar().melee_attack( mon, false );

    CHECK( *char_ptr == &get_avatar() );
    CHECK( *tgt_ptr == static_cast<Creature *>( &mon ) );
    CHECK( success->has_value() );
}

TEST_CASE( "lua_hook_wiring_npc_loaded", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    const auto npc_ptr = std::make_shared<npc *>( nullptr );
    const auto cre_ptr = std::make_shared<Creature *>( nullptr );

    const auto [nl, ni] = push_hook( lua, "on_npc_loaded",
    [npc_ptr]( sol::table params ) {
        *npc_ptr = params["npc"].get<sol::optional<npc *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup_nl{ nl, ni };

    const auto [cl, ci] = push_hook( lua, "on_creature_loaded",
    [cre_ptr]( sol::table params ) {
        *cre_ptr = params["creature"].get<sol::optional<Creature *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup_cl{ cl, ci };

    // spawn_npc calls g->load_npcs() which calls npc::on_load() for the new NPC
    npc &spawned = spawn_npc( point_bub_ms{ 50, 50 }, "test_talker" );

    CHECK( *npc_ptr == &spawned );
    CHECK( *cre_ptr == static_cast<Creature *>( &spawned ) );
}

// ── Easy with test data (requires test_lua_effect in TEST_DATA/effects.json) ─

TEST_CASE( "lua_hook_wiring_character_effect_added", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    REQUIRE( effect_test_lua_effect.is_valid() );

    const auto char_ptr = std::make_shared<Character *>( nullptr );
    const auto eff_ptr  = std::make_shared<effect *>( nullptr );
    const auto [list, idx] = push_hook( lua, "on_character_effect_added",
    [char_ptr, eff_ptr]( sol::table params ) {
        *char_ptr = params["char"].get<sol::optional<Character *>>().value_or( nullptr );
        *eff_ptr  = params["effect"].get<sol::optional<effect *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup{ list, idx };

    get_avatar().add_effect( effect_test_lua_effect, 1_turns );

    CHECK( *char_ptr == &get_avatar() );
    CHECK( *eff_ptr != nullptr );
}

TEST_CASE( "lua_hook_wiring_character_effect_tick", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    REQUIRE( effect_test_lua_effect.is_valid() );

    const auto char_ptr = std::make_shared<Character *>( nullptr );
    const auto eff_ptr  = std::make_shared<effect *>( nullptr );
    const auto [list, idx] = push_hook( lua, "on_character_effect",
    [char_ptr, eff_ptr]( sol::table params ) {
        *char_ptr = params["char"].get<sol::optional<Character *>>().value_or( nullptr );
        *eff_ptr  = params["effect"].get<sol::optional<effect *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup{ list, idx };

    // add_effect triggers process_one_effect(is_new=true) which fires the tick hook
    get_avatar().add_effect( effect_test_lua_effect, 1_turns );

    CHECK( *char_ptr == &get_avatar() );
    CHECK( *eff_ptr != nullptr );
}

TEST_CASE( "lua_hook_wiring_character_effect_removed", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    REQUIRE( effect_test_lua_effect.is_valid() );

    get_avatar().add_effect( effect_test_lua_effect, 1_turns );

    const auto char_ptr = std::make_shared<Character *>( nullptr );
    const auto eff_ptr  = std::make_shared<effect *>( nullptr );
    const auto [list, idx] = push_hook( lua, "on_character_effect_removed",
    [char_ptr, eff_ptr]( sol::table params ) {
        *char_ptr = params["character"].get<sol::optional<Character *>>().value_or( nullptr );
        *eff_ptr  = params["effect"].get<sol::optional<effect *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup{ list, idx };

    get_avatar().remove_effect( effect_test_lua_effect );

    CHECK( *char_ptr == &get_avatar() );
    CHECK( *eff_ptr != nullptr );
}

TEST_CASE( "lua_hook_wiring_mon_effect_added", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    REQUIRE( effect_test_lua_effect.is_valid() );

    monster &mon = spawn_test_monster( "mon_zombie", tripoint_bub_ms{ 5, 5, 0 } );

    const auto mon_ptr = std::make_shared<monster *>( nullptr );
    const auto eff_ptr = std::make_shared<effect *>( nullptr );
    const auto [list, idx] = push_hook( lua, "on_mon_effect_added",
    [mon_ptr, eff_ptr]( sol::table params ) {
        *mon_ptr = params["mon"].get<sol::optional<monster *>>().value_or( nullptr );
        *eff_ptr = params["effect"].get<sol::optional<effect *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup{ list, idx };

    mon.add_effect( effect_test_lua_effect, 1_turns );

    CHECK( *mon_ptr == &mon );
    CHECK( *eff_ptr != nullptr );
}

TEST_CASE( "lua_hook_wiring_mon_effect_tick", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    REQUIRE( effect_test_lua_effect.is_valid() );

    monster &mon = spawn_test_monster( "mon_zombie", tripoint_bub_ms{ 5, 5, 0 } );

    const auto mon_ptr = std::make_shared<monster *>( nullptr );
    const auto eff_ptr = std::make_shared<effect *>( nullptr );
    const auto [list, idx] = push_hook( lua, "on_mon_effect",
    [mon_ptr, eff_ptr]( sol::table params ) {
        *mon_ptr = params["mon"].get<sol::optional<monster *>>().value_or( nullptr );
        *eff_ptr = params["effect"].get<sol::optional<effect *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup{ list, idx };

    mon.add_effect( effect_test_lua_effect, 1_turns );

    CHECK( *mon_ptr == &mon );
    CHECK( *eff_ptr != nullptr );
}

TEST_CASE( "lua_hook_wiring_mon_effect_removed", "[lua]" )
{
    clear_all_state();
    auto &state = *DynamicDataLoader::get_instance().lua;
    sol::state &lua = state.lua;

    REQUIRE( effect_test_lua_effect.is_valid() );

    monster &mon = spawn_test_monster( "mon_zombie", tripoint_bub_ms{ 5, 5, 0 } );
    mon.add_effect( effect_test_lua_effect, 1_turns );

    const auto cre_ptr = std::make_shared<Creature *>( nullptr );
    const auto eff_ptr = std::make_shared<effect *>( nullptr );
    const auto [list, idx] = push_hook( lua, "on_mon_effect_removed",
    [cre_ptr, eff_ptr]( sol::table params ) {
        *cre_ptr = params["mon"].get<sol::optional<Creature *>>().value_or( nullptr );
        *eff_ptr = params["effect"].get<sol::optional<effect *>>().value_or( nullptr );
    } );
    hook_cleanup cleanup{ list, idx };

    mon.remove_effect( effect_test_lua_effect );

    CHECK( *cre_ptr == static_cast<Creature *>( &mon ) );
    CHECK( *eff_ptr != nullptr );
}

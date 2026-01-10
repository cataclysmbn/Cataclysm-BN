#include "catch/catch.hpp"

#include "avatar.h"
#include "catacharset.h"
#include "catalua_impl.h"
#include "catalua_serde.h"
#include "catalua_sol.h"
#include "clzones.h"
#include "debug.h"
#include "faction.h"
#include "fstream_utils.h"
#include "json.h"
#include "mapdata.h"
#include "options.h"
#include "point.h"
#include "string_formatter.h"
#include "stringmaker.h"
#include "type_id.h"
#include "units_angle.h"
#include "units_energy.h"
#include "units_mass.h"
#include "units_volume.h"

#include <optional>
#include <string>
#include <stdexcept>

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
    SECTION( "table with userdata keys" ) {
        t[point( 13, 37 )] = "leet";
        t[tripoint( 12, 34, 56 )] = "numbers";
        run_serde_test( lua, t );
    }
    SECTION( "table with userdata keys and values" ) {
        t[point( 13, 37 )] = tripoint( 1, 3, 37 );
        t[tripoint( 12, 34, 56 )] = point( 98765, 43210 );
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

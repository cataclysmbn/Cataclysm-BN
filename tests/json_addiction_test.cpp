#include "addiction.h"
#include "avatar.h"
#include "calendar.h"
#include "catalua.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "character.h"
#include "init.h"
#include "json.h"
#include "morale_types.h"
#include "pldata.h"
#include "state_helpers.h"
#include "type_id.h"

#include <sstream>
#include <string>

TEST_CASE("json_addiction_types", "[addiction]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });

    avatar& who = get_avatar();

    SECTION("addiction types are loaded from json") {
        const addiction_type_id marloss_r = addiction_type_id("marloss_r");
        REQUIRE(marloss_r.is_valid());
        const addiction_type_data& data = marloss_r.obj();
        CHECK(data.name.translated() == "Marloss berries");
        CHECK(data.description.translated() == "You should try some of those pink berries.");
        CHECK(data.craving_morale == MORALE_CRAVING_MARLOSS);
        CHECK(data.legacy_enum == add_type::MARLOSS_R);
        CHECK(data.on_withdrawal.empty());

        CHECK(addiction_id_from_legacy(add_type::ALCOHOL) == addiction_type_id("alcohol"));
        CHECK(legacy_add_type(addiction_type_id("opiate")) == add_type::PKILLER);
        CHECK(addiction_type_name(addiction_type_id("sleeping pill")) == "sleeping pills");
        CHECK(addiction_craving(addiction_type_id("crack")) == MORALE_CRAVING_CRACK);
    }

    SECTION("lua on_withdrawal replaces the hardcoded behavior") {
        auto& state = *DynamicDataLoader::get_instance().lua;
        cata::init_global_state_tables(state, {});
        sol::state& lua = state.lua;

        auto test_data = lua.create_table();
        lua.globals()["test_data"] = test_data;

        const auto script = lua.safe_script(
            R"(
            game.addiction_functions["test_addiction_withdrawal"] = function( params )
                test_data.withdrawal_called = true
                test_data.withdrawal_char = params.char ~= nil
                test_data.withdrawal_addiction = params.addiction
                test_data.withdrawal_intensity = params.intensity
                test_data.withdrawal_sated = params.sated
            end
        )",
            sol::script_pass_on_error);
        REQUIRE(script.valid());

        addiction lua_addiction(addiction_type_id("test_lua_addiction"), 10);
        lua_addiction.sated = -1_turns;
        addict_effect(who, lua_addiction);

        CHECK(test_data.get<bool>("withdrawal_called"));
        CHECK(test_data.get<bool>("withdrawal_char"));
        CHECK(test_data.get<std::string>("withdrawal_addiction") == "test_lua_addiction");
        CHECK(test_data.get<int>("withdrawal_intensity") == 10);
        CHECK(test_data.get<int>("withdrawal_sated") == -1);

        // Lua-defined addictions work through the normal api by id string,
        // from both Lua and C++.
        lua["test_avatar"] = &who;
        const auto add_via_lua = lua.safe_script(
            R"(
            test_avatar:add_addiction( "test_lua_addiction", 200 )
            assert( test_avatar:addiction_level( "test_lua_addiction" ) == 1 )
            assert( test_avatar:has_addiction( "marloss_b" ) == false )
        )",
            sol::script_pass_on_error);
        CHECK(add_via_lua.valid());
        CHECK(who.addiction_level(addiction_type_id("test_lua_addiction")) == 1);
        who.rem_addiction(addiction_type_id("test_lua_addiction"));
        CHECK(who.addiction_level(addiction_type_id("test_lua_addiction")) == 0);
    }

    SECTION("legacy withdrawal behavior still runs for hardcoded types") {
        // No on_withdrawal on core types: the C++ switch runs, keyed on
        // legacy_enum. Must not crash or fire any Lua.
        addiction legacy_addiction(addiction_type_id("marloss_b"), 10);
        legacy_addiction.sated = -1_turns;
        addict_effect(who, legacy_addiction);
        addiction alcohol(addiction_type_id("alcohol"), 10);
        addict_effect(who, alcohol);
    }

    SECTION("addiction serialization roundtrips old and new save formats") {
        // Old saves store the legacy enum as an int ("type_enum": 12 == MARLOSS_B)
        auto old_buffer = std::istringstream(R"({"type_enum": 12, "intensity": 3, "sated": 100})");
        JsonIn old_jsin(old_buffer);
        addiction old_format;
        old_format.deserialize(old_jsin);
        CHECK(old_format.type == addiction_type_id("marloss_b"));
        CHECK(old_format.intensity == 3);

        auto new_buffer = std::istringstream(
            R"({"type": "marloss_y", "intensity": 5, "sated": 100})");
        JsonIn new_jsin(new_buffer);
        addiction new_format;
        new_format.deserialize(new_jsin);
        CHECK(new_format.type == addiction_type_id("marloss_y"));
        CHECK(new_format.intensity == 5);

        std::ostringstream out;
        JsonOut jsout(out);
        new_format.serialize(jsout);
        const std::string serialized = out.str();
        CHECK(serialized.find("\"type\"") != std::string::npos);
        CHECK(serialized.find("marloss_y") != std::string::npos);
        CHECK(serialized.find("type_enum") == std::string::npos);
    }
}

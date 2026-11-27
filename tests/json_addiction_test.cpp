#include "addiction.h"
#include "avatar.h"
#include "calendar.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "character.h"
#include "event.h"
#include "event_bus.h"
#include "init.h"
#include "json.h"
#include "morale_types.h"
#include "pldata.h"
#include "state_helpers.h"
#include "type_id.h"

#include <sstream>
#include <string>
#include <vector>

namespace {

/// Records every event the bus dispatches while subscribed.
class addiction_event_recorder: public event_subscriber {
public:
    void notify(const cata::event& e) override { events.push_back(e); }

    std::vector<cata::event> events;
};

/// Reads one `addiction` from @p json, as a save file stores it.
auto addiction_from_json(const std::string& json) -> addiction {
    auto buffer = std::istringstream(json);
    auto jsin = JsonIn(buffer);
    auto result = addiction();
    result.deserialize(jsin);
    return result;
}

} // namespace

/// Tests the JSON `addiction_type` registry that replaced the hardcoded
/// addiction tables: the 13 core types load with the same names, withdrawal
/// names and craving morale as before, every legacy enum maps to its id and
/// back (old saves store the enum), and addictions save, load and fire the
/// gains/loses events by id.
TEST_CASE("json_addiction_types", "[addiction]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });

    auto& who = get_avatar();

    SECTION("addiction types are loaded from json") {
        const auto marloss_r = addiction_type_id("marloss_r");
        REQUIRE(marloss_r.is_valid());
        const auto& data = marloss_r.obj();
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

    SECTION("the status UI keeps the old withdrawal names") {
        // Before the registry these were a separate table in addiction.cpp
        // ("Nicotine Withdrawal" and so on), distinct from the short names.
        CHECK(addiction_name(addiction(addiction_type_id("nicotine"))) == "Nicotine Withdrawal");
        CHECK(addiction_name(addiction(addiction_type_id("sleeping pill")))
              == "Sleeping Pill Dependence");
        CHECK(addiction_name(addiction(addiction_type_id("crack"))) == "Crack Cocaine Withdrawal");
        CHECK(addiction_name(addiction(addiction_type_id("marloss_r"))) == "Marloss Longing");
        CHECK(addiction_name(addiction(addiction_type_id("marloss_b"))) == "Marloss Desire");
        CHECK(addiction_name(addiction(addiction_type_id("marloss_y"))) == "Marloss Cravings");
        // A type without withdrawal_name falls back to its name.
        CHECK(addiction_name(addiction(addiction_type_id("test_lua_addiction")))
              == "TEST lua addiction");
    }

    SECTION("every legacy enum maps to a json type and back") {
        for (auto i = 1; i < static_cast<int>(add_type::NUM_ADD_TYPES); ++i) {
            const auto legacy = static_cast<add_type>(i);
            CAPTURE(io::enum_to_string(legacy));
            const auto id = addiction_id_from_legacy(legacy);
            REQUIRE(id.is_valid());
            CHECK(legacy_add_type(id) == legacy);
            // Old saves store the enum as an int.
            const auto loaded = addiction_from_json(
                R"({"type_enum": )" + std::to_string(i) + R"(, "intensity": 3, "sated": 100})");
            CHECK(loaded.type == id);
        }
    }

    SECTION("addiction serialization roundtrips old and new save formats") {
        // Old saves store the legacy enum as an int ("type_enum": 12 == MARLOSS_B)
        const auto old_format = addiction_from_json(
            R"({"type_enum": 12, "intensity": 3, "sated": 100})");
        CHECK(old_format.type == addiction_type_id("marloss_b"));
        CHECK(old_format.intensity == 3);

        const auto new_format = addiction_from_json(
            R"({"type": "marloss_y", "intensity": 5, "sated": 100})");
        CHECK(new_format.type == addiction_type_id("marloss_y"));
        CHECK(new_format.intensity == 5);

        std::ostringstream out;
        JsonOut jsout(out);
        new_format.serialize(jsout);
        const auto serialized = out.str();
        CHECK(serialized.find("\"type\"") != std::string::npos);
        CHECK(serialized.find("marloss_y") != std::string::npos);
        CHECK(serialized.find("type_enum") == std::string::npos);
    }

    SECTION("gaining and losing an addiction sends events with its id") {
        // JSON-only types send the events too, so the memorial logs them.
        addiction_event_recorder recorder;
        get_event_bus().subscribe(&recorder);

        who.add_addiction(addiction_type_id("test_lua_addiction"), 200);
        who.rem_addiction(addiction_type_id("test_lua_addiction"));

        REQUIRE(recorder.events.size() == 2);
        CHECK(recorder.events[0].type() == event_type::gains_addiction);
        CHECK(recorder.events[0].get<std::string>("addiction") == "test_lua_addiction");
        CHECK(recorder.events[0].get<character_id>("character") == who.getID());
        CHECK(recorder.events[1].type() == event_type::loses_addiction);
        CHECK(recorder.events[1].get<std::string>("addiction") == "test_lua_addiction");
    }

    SECTION("loading a character drops addictions whose type no longer exists") {
        // For example an addiction from a mod that was removed from the world.
        auto saved = avatar();
        saved.addictions.emplace_back(addiction_type_id("removed_mod_addiction"), 5);
        saved.addictions.emplace_back(addiction_type_id("alcohol"), 5);
        std::ostringstream out;
        JsonOut jsout(out);
        saved.serialize(jsout);

        auto buffer = std::istringstream(out.str());
        JsonIn jsin(buffer);
        auto loaded = avatar();
        loaded.deserialize(jsin);
        REQUIRE(loaded.addictions.size() == 1);
        CHECK(loaded.addictions.front().type == addiction_type_id("alcohol"));
    }
}

/// Tests the `on_withdrawal = "lua:NAME"` dispatch of `addict_effect`: a JSON
/// type with a Lua callback runs `game.addiction_functions["NAME"]` instead of
/// the hardcoded withdrawal behavior, and the core types still run the C++
/// behavior without calling Lua. The callback is added to the live `game`
/// table and removed afterwards, so other tests keep their Lua state.
TEST_CASE("json_addiction_lua_withdrawal", "[addiction][lua]") {
    clear_all_state();
    sol::state& lua = DynamicDataLoader::get_instance().lua->lua;
    const auto cleanup = on_out_of_scope([&]() {
        lua.safe_script(R"(
            game.addiction_functions["test_addiction_withdrawal"] = nil
            test_data = nil
            test_avatar = nil
        )");
        clear_all_state();
    });

    auto& who = get_avatar();
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

    SECTION("lua on_withdrawal replaces the hardcoded behavior") {
        auto lua_addiction = addiction(addiction_type_id("test_lua_addiction"), 10);
        lua_addiction.sated = -1_turns;
        addict_effect(who, lua_addiction);

        CHECK(test_data.get<bool>("withdrawal_called"));
        CHECK(test_data.get<bool>("withdrawal_char"));
        CHECK(test_data.get<std::string>("withdrawal_addiction") == "test_lua_addiction");
        CHECK(test_data.get<int>("withdrawal_intensity") == 10);
        CHECK(test_data.get<int>("withdrawal_sated") == -1);
    }

    SECTION("core types run the C++ behavior and don't call lua") {
        auto marloss = addiction(addiction_type_id("marloss_b"), 10);
        marloss.sated = -1_turns;
        addict_effect(who, marloss);
        auto alcohol = addiction(addiction_type_id("alcohol"), 10);
        addict_effect(who, alcohol);
        CHECK_FALSE(test_data.get<sol::object>("withdrawal_called").valid());
    }

    SECTION("lua-defined addictions work through the character api by id string") {
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
}

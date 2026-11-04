#include "catalua_bindings_coords_common.h"
#include "catalua_impl.h"
#include "catalua_serde.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "debug.h"
#include "flag.h"
#include "json.h"
#include "point.h"
#include "rng.h"
#include "stringmaker.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

auto serialize_table(const sol::table& table, bool pretty = false) -> std::string {
    auto stream = std::ostringstream{};
    auto jsout = JsonOut(stream, pretty);
    cata::serialize_lua_table(table, jsout);
    return stream.str();
}

auto deserialize_table(sol::state& lua, const std::string& data) -> sol::table {
    auto stream = std::istringstream{data};
    auto jsin = JsonIn(stream);
    auto object = jsin.get_object();
    auto table = lua.create_table();
    cata::deserialize_lua_table(table, object);
    return table;
}

auto rehash_table(sol::table table) -> void {
    for (const auto index : std::views::iota(0, 256)) {
        table["discarded_" + std::to_string(index)] = index;
    }
    for (const auto index : std::views::iota(0, 256)) {
        table["discarded_" + std::to_string(index)] = sol::nil;
    }
}

struct lua_entry {
    sol::object key;
    sol::object value;
};

auto make_mixed_table(sol::state& lua, int history) -> sol::table {
    auto nested = lua.create_table();
    auto table = lua.create_table();
    if (history % 2 != 0) {
        rehash_table(nested);
        rehash_table(table);
        nested["z"] = 16.0;
        nested["a"] = 16;
    } else {
        nested["a"] = 16;
        nested["z"] = 16.0;
    }
    auto entries = std::vector<lua_entry>{
        {.key = sol::make_object(lua, false), .value = sol::make_object(lua, "false key")},
        {.key = sol::make_object(lua, true), .value = sol::make_object(lua, "true key")},
        {.key = sol::make_object(lua, -8), .value = sol::make_object(lua, false)},
        {.key = sol::make_object(lua, 7), .value = sol::make_object(lua, 91)},
        {.key = sol::make_object(lua, -1.25), .value = sol::make_object(lua, -2.5)},
        {.key = sol::make_object(lua, 2.5), .value = sol::make_object(lua, nested)},
        {.key = sol::make_object(lua, "7"), .value = sol::make_object(lua, "integer spelling")},
        {.key = sol::make_object(lua, "2.5"), .value = sol::make_object(lua, "float spelling")},
        {.key = sol::make_object(lua, "true"), .value = sol::make_object(lua, 3)},
        {.key = sol::make_object(lua, "false"), .value = sol::make_object(lua, 4)},
        {.key = sol::make_object(lua, std::string{"quote\"\\\ntail"}),
         .value = sol::make_object(lua, std::string{"value\ntail"})},
        {.key = sol::make_object(lua, "nested"), .value = sol::make_object(lua, nested)},
    };
    if (history % 3 == 1) {
        std::ranges::reverse(entries);
    } else if (history % 3 == 2) {
        std::ranges::rotate(entries, entries.begin() + 5);
    }
    for (const auto& entry : entries) { table.set(entry.key, entry.value); }
    return table;
}

auto make_identity_table(sol::state& lua, int history) -> sol::table {
    auto table = lua.create_table();
    if (history % 2 != 0) { rehash_table(table); }
    // Separate identity keys with identical structures, two with identical values too.
    const auto values = history % 2 == 0 ? std::array{30, 10, 10} : std::array{10, 10, 30};
    for (const auto value : values) {
        auto key = lua.create_table();
        key["same"] = "structure";
        table[key] = value;
    }
    auto shared = lua.create_table();
    shared["payload"] = point(4, 5);
    table["alias_a"] = shared;
    table["alias_b"] = shared;
    table[shared] = shared;
    return table;
}

auto serialized_data(const JsonArray& entries, size_t index) -> JsonValue {
    auto object = entries.get_object(index);
    object.allow_omitted_members();
    return object.get_member("data");
}

auto serialize_nested_table(const sol::table& table, bool pretty, int depth) -> std::string {
    auto stream = std::ostringstream{};
    auto jsout = JsonOut(stream, pretty, depth);
    jsout.start_object(true);
    jsout.member("before", 11);
    jsout.member("lua");
    cata::serialize_lua_table(table, jsout);
    jsout.member("after", 22);
    jsout.member("tail");
    jsout.start_array(true);
    jsout.write("done");
    jsout.end_array();
    jsout.end_object();
    return stream.str();
}

} // namespace

TEST_CASE("lua_serde_preserves_exact_caller_pretty_context", "[lua][lua_serde][json]") {
    auto lua = make_lua_state();
    auto table = lua.create_table();
    auto nested = lua.create_table();
    nested["coord"] = cata::detail::lua_coords::to_lua(tripoint_abs_omt(1, 2, 3));
    nested["flag"] = flag_id("MC_USED");
    nested["number"] = 16.0;
    nested["text"] = " two words\t\n\"\\ ";
    table["a"] = nested;
    table["z"] = point(3, 4);
    // Literal expected formatting, including nested real userdata and string whitespace.
    const auto entries = std::string{
        R"([ { "type": "string", "data": "a" }, { "type": "lua_table", "data": { "entries": [ { "type": "string", "data": "coord" }, { "type": "userdata", "kind": "TripointCoord", "data": { "origin": "abs", "scale": "omt", "raw": [ 1, 2, 3 ] } }, { "type": "string", "data": "flag" }, { "type": "userdata", "kind": "JsonFlagId", "data": "MC_USED" }, { "type": "string", "data": "number" }, { "type": "float", "data": 16.000000000000000 }, { "type": "string", "data": "text" }, { "type": "string", "data": " two words\t\n\"\\ " } ] } }, { "type": "string", "data": "z" }, { "type": "userdata", "kind": "Point", "data": [ 3, 4 ] } ])"};
    CHECK(serialize_table(table, true) == "{\n  \"entries\": " + entries + "\n}");
    CHECK(serialize_nested_table(table, true, 0)
          == "{\n  \"before\": 11,\n  \"lua\": { \"entries\": " + entries
                 + " },\n  \"after\": 22,\n  \"tail\": [\n    \"done\"\n  ]\n}");
    CHECK(
        serialize_nested_table(table, true, 2)
        == "{\n      \"before\": 11,\n      \"lua\": { \"entries\": " + entries
               + " },\n      \"after\": 22,\n      \"tail\": [\n        \"done\"\n      ]\n    }");
}

TEST_CASE(
    "lua_serde_pair_order_is_independent_of_pretty_mode_and_depth", "[lua][lua_serde][json]") {
    auto expected = std::string{};
    for (const auto history : std::views::iota(0, 6)) {
        auto lua = make_lua_state();
        auto table = lua.create_table();
        if (history % 2 != 0) { rehash_table(table); }
        const auto numbers = history % 2 == 0 ? std::array{2, 20} : std::array{20, 2};
        for (const auto number : numbers) {
            auto key = lua.create_table();
            key["number"] = number;
            table[key] = flag_id("MC_USED");
            table[point(1, number)] = point(3, 4);
        }
        table["x y"] = 30;
        table["xy"] = 10;
        const auto compact = serialize_table(table);
        if (history == 0) { expected = compact; }
        CHECK(compact == expected);
        for (const auto pretty : {false, true}) {
            const auto root = serialize_table(table, pretty);
            CHECK(serialize_table(deserialize_table(lua, root)) == expected);
            for (const auto depth : {0, 2, 5}) {
                CAPTURE(history, pretty, depth);
                const auto output = serialize_nested_table(table, pretty, depth);
                auto stream = std::istringstream{output};
                auto jsin = JsonIn(stream);
                auto wrapper = jsin.get_object();
                CHECK(wrapper.get_int("before") == 11);
                CHECK(wrapper.get_int("after") == 22);
                CHECK(wrapper.get_array("tail").get_string(0) == "done");
                auto object = wrapper.get_object("lua");
                const auto entries = object.get_array("entries");
                // Compact [1,20] sorts before [1,2]. Comparing pretty text directly
                // reverses this because a closing space sorts before the digit '0'.
                CHECK(entries.size() == 12);
                CHECK(
                    serialized_data(serialized_data(entries, 0).get_object().get_array("entries"), 1)
                        .get_int()
                    == 20);
                CHECK(
                    serialized_data(serialized_data(entries, 2).get_object().get_array("entries"), 1)
                        .get_int()
                    == 2);
                CHECK(serialized_data(entries, 4).get_string() == "x y");
                CHECK(serialized_data(entries, 6).get_string() == "xy");
                CHECK(serialized_data(entries, 8).get_array().get_int(1) == 20);
                CHECK(serialized_data(entries, 10).get_array().get_int(1) == 2);
                auto restored = lua.create_table();
                cata::deserialize_lua_table(restored, object);
                CHECK(serialize_table(restored) == expected);
            }
        }
    }
}

TEST_CASE(
    "jsonout_fragment_context_is_independent_and_has_no_caller_separator", "[json][lua_serde]") {
    for (const auto pretty : {false, true}) {
        auto stream = std::ostringstream{};
        auto parent = JsonOut(stream, pretty, 2);
        parent.start_array(true);
        parent.write(11);
        auto fragment_stream = std::ostringstream{};
        auto fragment = JsonOut(fragment_stream, parent);
        CHECK(parent.get_need_separator());
        CHECK_FALSE(fragment.get_need_separator());
        fragment.start_object(true);
        fragment.member("value", 16.0);
        fragment.end_object();
        CHECK(fragment_stream.str()
              == (pretty ? "{\n        \"value\": 16.000000000000000\n      }"
                         : "{\"value\":16.000000000000000}"));
        parent.write(22);
        parent.end_array();
        CHECK(stream.str() == (pretty ? "[\n      11,\n      22\n    ]" : "[11,22]"));
    }
}

TEST_CASE("lua_serde_orders_complete_typed_pairs_recursively", "[lua][lua_serde]") {
    auto expected = std::string{};
    for (const auto history : std::views::iota(0, 12)) {
        CAPTURE(history);
        auto lua = make_lua_state();
        auto table = make_mixed_table(lua, history);
        const auto data = serialize_table(table);
        if (history == 0) { expected = data; }
        CHECK(data == expected);
        auto restored = deserialize_table(lua, data);
        CHECK(serialize_table(restored) == expected);
        CHECK(restored.get<std::string>(false) == "false key");
        CHECK(restored.get<std::string>(true) == "true key");
        CHECK(restored.get<int>(7) == 91);
        CHECK(restored.get<std::string>("7") == "integer spelling");
        CHECK(restored.get<std::string>("2.5") == "float spelling");
        CHECK(restored.get<double>(-1.25) == -2.5);
        const auto nested = restored.get<sol::table>("nested");
        CHECK(nested.get<int>("a") == 16);
        CHECK(nested.get<double>("z") == 16.0);
        CHECK(is_number_integer(lua, nested.get<sol::object>("a")));
        CHECK_FALSE(is_number_integer(lua, nested.get<sol::object>("z")));
        CHECK(serialize_table(deserialize_table(lua, serialize_table(table, true))) == expected);
        CHECK(restored.get<std::string>(std::string{"quote\"\\\ntail"})
              == std::string{"value\ntail"});
    }
}

TEST_CASE("lua_serde_keeps_scalar_encoding_without_numeric_coercion", "[lua][lua_serde]") {
    auto lua = make_lua_state();
    auto table = lua.create_table();
    table["large"] = std::numeric_limits<int64_t>::max();
    table["small"] = std::numeric_limits<int64_t>::min();
    table["integer"] = 16;
    table["float"] = 16.0;
    table["negative_zero"] = -0.0;
    const auto data = serialize_table(table);
    CHECK(
        data
        == R"({"entries":[{"type":"string","data":"float"},{"type":"float","data":16.000000000000000},{"type":"string","data":"integer"},{"type":"int","data":16},{"type":"string","data":"large"},{"type":"int","data":9223372036854775807},{"type":"string","data":"negative_zero"},{"type":"float","data":-0.000000000000000},{"type":"string","data":"small"},{"type":"int","data":-9223372036854775808}]})");
    CHECK(serialize_table(table, true) == R"({
  "entries": [ { "type": "string", "data": "float" }, { "type": "float", "data": 16.000000000000000 }, { "type": "string", "data": "integer" }, { "type": "int", "data": 16 }, { "type": "string", "data": "large" }, { "type": "int", "data": 9223372036854775807 }, { "type": "string", "data": "negative_zero" }, { "type": "float", "data": -0.000000000000000 }, { "type": "string", "data": "small" }, { "type": "int", "data": -9223372036854775808 } ]
})");
    // The legacy decoder's int range/float precision is not changed by ordering.
    auto strings = lua.create_table();
    strings.set(sol::make_object(lua, std::string{"a\0b", 3}),
                sol::make_object(lua, std::string{"v\0x", 3}));
    strings["ab"] = "vx";
    CHECK(
        serialize_table(strings)
        == R"({"entries":[{"type":"string","data":"a\u0000b"},{"type":"string","data":"v\u0000x"},{"type":"string","data":"ab"},{"type":"string","data":"vx"}]})");
    CHECK(serialize_table(strings, true) == R"({
  "entries": [ { "type": "string", "data": "a\u0000b" }, { "type": "string", "data": "v\u0000x" }, { "type": "string", "data": "ab" }, { "type": "string", "data": "vx" } ]
})");
    // JsonIn's legacy Unicode decoder drops U+0000. Ordering must not parse these
    // fragments and thereby introduce that loss/collision into the producer too.
}

TEST_CASE(
    "lua_serde_keeps_identity_key_multiplicity_and_existing_alias_semantics", "[lua][lua_serde]") {
    auto expected = std::string{};
    for (const auto history : std::views::iota(0, 8)) {
        auto lua = make_lua_state();
        auto table = make_identity_table(lua, history);
        const auto data = serialize_table(table);
        if (history == 0) { expected = data; }
        CHECK(data == expected);
        auto stream = std::istringstream{data};
        auto jsin = JsonIn(stream);
        CHECK(jsin.get_object().get_array("entries").size() == 12);
        auto restored = deserialize_table(lua, data);
        CHECK(serialize_table(restored) == expected);
        auto structural_values = std::vector<int>{};
        auto alias_key_found = false;
        restored.for_each([&](const sol::object& key, const sol::object& value) {
            if (key.get_type() != sol::type::table) { return; }
            auto key_table = key.as<sol::table>();
            if (key_table["same"].valid()) {
                structural_values.push_back(value.as<int>());
            } else {
                alias_key_found = true;
                CHECK_FALSE(key_table == value.as<sol::table>());
            }
        });
        std::ranges::sort(structural_values);
        CHECK(structural_values == std::vector{10, 10, 30});
        CHECK(alias_key_found);
        const auto original_a = table.get<sol::table>("alias_a");
        const auto original_b = table.get<sol::table>("alias_b");
        CHECK(original_a == original_b);
        const auto restored_a = restored.get<sol::table>("alias_a");
        const auto restored_b = restored.get<sol::table>("alias_b");
        CHECK_FALSE(restored_a == restored_b);
        CHECK(serialize_table(restored_a) == serialize_table(restored_b));
    }
}

TEST_CASE("lua_serde_orders_real_userdata_keys_and_roundtrips_coordinates", "[lua][lua_serde]") {
    auto expected = std::string{};
    for (const auto history : std::views::iota(0, 8)) {
        auto lua = make_lua_state();
        auto table = lua.create_table();
        if (history % 2 != 0) { rehash_table(table); }
        const auto values = history % 2 == 0 ? std::array{30, 10, 10} : std::array{10, 10, 30};
        for (const auto value : values) { table[point(1, 2)] = value; }
        table["flag"] = flag_id("MC_USED");
        table["point"] = point(7, 8);
        table["tripoint"] = tripoint(7, 8, 9);
        table["point_coord"] = cata::detail::lua_coords::to_lua(point_bub_ms(8, 9));
        table["tripoint_coord"] = cata::detail::lua_coords::to_lua(tripoint_abs_omt(1, 2, 3));
        table[cata::detail::lua_coords::to_lua(point_bub_ms(1, 2))] = "typed point key";
        table[cata::detail::lua_coords::to_lua(tripoint_abs_omt(1, 2, 3))] = "typed tripoint key";
        const auto rng_before = rng_get_engine();
        const auto data = serialize_table(table);
        CHECK(rng_get_engine() == rng_before);
        CHECK(table.get<flag_id>("flag") == flag_id("MC_USED"));
        CHECK(table.get<point>("point") == point(7, 8));
        CHECK(table.get<tripoint>("tripoint") == tripoint(7, 8, 9));
        CHECK(
            table.get<cata::detail::lua_coords::lua_point_coord>("point_coord").raw == point(8, 9));
        CHECK(table.get<cata::detail::lua_coords::lua_tripoint_coord>("tripoint_coord").raw
              == tripoint(1, 2, 3));
        if (history == 0) { expected = data; }
        CHECK(data == expected);
        auto restored = deserialize_table(lua, data);
        CHECK(serialize_table(restored) == expected);
        CHECK(restored.get<flag_id>("flag") == flag_id("MC_USED"));
        CHECK(restored.get<point>("point") == point(7, 8));
        CHECK(restored.get<tripoint>("tripoint") == tripoint(7, 8, 9));
        const auto point_coord = restored.get<cata::detail::lua_coords::lua_point_coord>(
            "point_coord");
        CHECK(point_coord.raw == point(8, 9));
        CHECK(point_coord.origin == coords::origin::bubble);
        CHECK(point_coord.scale == coords::scale::map_square);
        const auto tripoint_coord = restored.get<cata::detail::lua_coords::lua_tripoint_coord>(
            "tripoint_coord");
        CHECK(tripoint_coord.raw == tripoint(1, 2, 3));
        CHECK(tripoint_coord.origin == coords::origin::abs);
        CHECK(tripoint_coord.scale == coords::scale::overmap_terrain);
        auto point_key_values = std::vector<int>{};
        auto typed_keys = 0;
        restored.for_each([&](const sol::object& key, const sol::object& value) {
            if (key.is<point>()) {
                CHECK(key.as<point>() == point(1, 2));
                point_key_values.push_back(value.as<int>());
            } else if (key.is<cata::detail::lua_coords::lua_point_coord>()) {
                CHECK(value.as<std::string>() == "typed point key");
                ++typed_keys;
            } else if (key.is<cata::detail::lua_coords::lua_tripoint_coord>()) {
                CHECK(value.as<std::string>() == "typed tripoint key");
                ++typed_keys;
            }
        });
        std::ranges::sort(point_key_values);
        CHECK(point_key_values == std::vector{10, 10, 30});
        CHECK(typed_keys == 2);
    }
}

TEST_CASE(
    "lua_serde_captures_effectful_userdata_once_in_normal_traversal_order", "[lua][lua_serde]") {
    const auto pretty = GENERATE(false, true);
    auto lua = make_lua_state();
    auto calls = std::vector<point>{};
    auto point_type = lua.get<sol::table>("Point");
    point_type["serialize"] = [&](const point& value, JsonOut& jsout) {
        calls.push_back(value);
        value.serialize(jsout);
    };
    auto numeric_calls = 0;
    auto math = lua.get<sol::table>("math");
    auto original_math_type = math.get<sol::protected_function>("type");
    math["type"] = [&](const sol::object& number) -> std::string {
        ++numeric_calls;
        auto result = original_math_type(number);
        check_func_result(result);
        return result.get<std::string>();
    };
    auto table = lua.create_table();
    table[17] = 42;
    auto shared = lua.create_table();
    shared["point"] = point(5, 6);
    table[point(1, 2)] = point(3, 4);
    table["z"] = shared;
    table["a"] = shared;
    auto expected_calls = std::vector<point>{};
    table.for_each([&](const sol::object& key, const sol::object& value) {
        if (key.is<point>()) { expected_calls.push_back(key.as<point>()); }
        if (value.is<point>()) {
            expected_calls.push_back(value.as<point>());
        } else if (value.get_type() == sol::type::table) {
            expected_calls.push_back(point(5, 6));
        }
    });
    REQUIRE(expected_calls.size() == 4);
    const auto data = serialize_table(table, pretty);
    CHECK(calls == expected_calls);
    CHECK(numeric_calls == 2);
    calls.clear();
    auto restored = deserialize_table(lua, data);
    CHECK(serialize_table(restored, pretty) == data);
    CHECK(calls.size() == 4);
    CHECK(numeric_calls == 4);
    calls.clear();
    numeric_calls = 0;
    const auto nested = serialize_nested_table(table, pretty, 2);
    CHECK_FALSE(nested.empty());
    CHECK(calls == expected_calls);
    CHECK(numeric_calls == 2);
}

TEST_CASE(
    "lua_serde_userdata_failures_preserve_stack_and_allow_native_retry",
    "[lua][lua_serde][lua_serde_error]") {
    const auto pretty = GENERATE(false, true);
    const auto lua_error = GENERATE(false, true);
    const auto write_before_failure = GENERATE(false, true);
    CAPTURE(pretty, lua_error, write_before_failure);
    auto lua = make_lua_state();
    auto calls = 0;
    auto point_type = lua.get<sol::table>("Point");
    if (lua_error) {
        lua["record_serializer_call"] = [&]() { ++calls; };
        lua["write_point"] = [](const point& value, JsonOut& jsout) { value.serialize(jsout); };
        lua["write_before_failure"] = write_before_failure;
        auto script = lua.safe_script(
            R"(
            ---@param value Point
            ---@param writer userdata
            return function(value, writer)
                record_serializer_call()
                if write_before_failure then write_point(value, writer) end
                error("expected Lua serializer failure", 0)
            end
            )",
            sol::script_pass_on_error);
        REQUIRE(script.valid());
        point_type["serialize"] = script.get<sol::protected_function>();
    } else {
        point_type["serialize"] = [&](const point& value, JsonOut& jsout) -> void {
            ++calls;
            if (write_before_failure) { value.serialize(jsout); }
            throw std::runtime_error("expected C++ serializer failure");
        };
    }
    auto table = lua.create_table();
    table["point"] = point(3, 4);
    REQUIRE(table.get<sol::object>("point").is<point>());
    const auto stack_top = lua_gettop(lua.lua_state());
    auto failed_output = std::string{};
    for (const auto attempt : {1, 2}) {
        auto output = std::string{};
        const auto diagnostic = capture_debugmsg_during([&]() {
            output = serialize_nested_table(table, pretty, 2);
        });
        CHECK(diagnostic.find("Failed to serialize type 'Point'") != std::string::npos);
        CHECK(diagnostic.find(
                  lua_error ? "expected Lua serializer failure" : "expected C++ serializer failure")
              != std::string::npos);
        CHECK(calls == attempt);
        CHECK(lua_gettop(lua.lua_state()) == stack_top);
        CHECK(table.get<point>("point") == point(3, 4));
        if (attempt == 1) { failed_output = output; }
        CHECK(output == failed_output);
    }
    // Replacing a Sol usertype binding invalidates its old closure; rebind the native method.
    // Error fragments are never accepted as saved data.
    point_type["serialize"] = &point::serialize;
    const auto recovered = serialize_table(table, pretty);
    auto restored = deserialize_table(lua, recovered);
    CHECK(restored.get<point>("point") == point(3, 4));
    CHECK(serialize_table(restored, pretty) == recovered);
    const auto nested = serialize_nested_table(table, pretty, 2);
    auto stream = std::istringstream{nested};
    auto input = JsonIn(stream);
    auto wrapper = input.get_object();
    CHECK(wrapper.get_int("before") == 11);
    CHECK(wrapper.get_int("after") == 22);
    CHECK(wrapper.get_array("tail").get_string(0) == "done");
    wrapper.allow_omitted_members();
    CHECK(calls == 2);
    CHECK(lua_gettop(lua.lua_state()) == stack_top);
}

TEST_CASE("lua_serde_does_not_change_traversal_state_or_rng", "[lua][lua_serde]") {
    auto lua = make_lua_state();
    auto table = make_mixed_table(lua, 5);
    lua["subject"] = table;
    auto script = lua.safe_script(
        R"(
        calls = 0
        local function unexpected()
            calls = calls + 1
            error("serialization must not invoke this metamethod")
        end
        math.random = unexpected
        math.randomseed = unexpected
        setmetatable(subject, { __pairs = unexpected, __len = unexpected,
            __lt = unexpected, __tostring = unexpected })
    )",
        sol::script_pass_on_error);
    REQUIRE(script.valid());
    auto before_keys = std::vector<sol::object>{};
    auto before_values = std::vector<sol::object>{};
    table.for_each([&](const sol::object& key, const sol::object& value) {
        before_keys.push_back(key);
        before_values.push_back(value);
    });
    const auto stack_top = lua_gettop(lua.lua_state());
    const auto rng_before = rng_get_engine();
    const auto data = serialize_table(table);
    CHECK(serialize_table(table) == data);
    const auto pretty_data = serialize_table(table, true);
    CHECK(serialize_table(table, true) == pretty_data);
    CHECK(lua_gettop(lua.lua_state()) == stack_top);
    CHECK(rng_get_engine() == rng_before);
    CHECK(lua.get<int>("calls") == 0);
    auto index = size_t{0};
    table.for_each([&](const sol::object& key, const sol::object& value) {
        REQUIRE(index < before_keys.size());
        CHECK(key == before_keys[index]);
        CHECK(value == before_values[index]);
        ++index;
    });
    CHECK(index == before_keys.size());
}

TEST_CASE("lua_serde_preserves_cycle_rejection_in_keys_and_values", "[lua][lua_serde]") {
    auto lua = make_lua_state();
    auto table = lua.create_table();
    SECTION("value cycle") { table["self"] = table; }
    SECTION("key cycle") { table[table] = "self"; }
    SECTION("indirect cycle") {
        auto child = lua.create_table();
        child["parent"] = table;
        table["child"] = child;
    }
    auto data = std::string{};
    const auto diagnostic = capture_debugmsg_during([&]() { data = serialize_table(table); });
    CHECK(diagnostic == "Tried to serialize recursive table structure.");
    CHECK(data.find(R"("type":"lua_table","data":null)") != std::string::npos);
    const auto repeated = capture_debugmsg_during([&]() { CHECK(serialize_table(table) == data); });
    CHECK(repeated == diagnostic);
}

TEST_CASE("lua_serde_preserves_unsupported_value_diagnostics", "[lua][lua_serde]") {
    auto lua = make_lua_state();
    auto table = lua.create_table();
    table["function"] = []() { return 1; };
    auto data = std::string{};
    const auto diagnostic = capture_debugmsg_during([&]() { data = serialize_table(table); });
    CHECK(diagnostic == "Unsupported type encountered when serializing Lua table.");
    CHECK(data == R"({"entries":[{"type":"string","data":"function"},{}]})");
}

TEST_CASE(
    "lua_serde_accepts_legacy_unsorted_pairs_without_changing_decoder_errors", "[lua][lua_serde]") {
    auto lua = make_lua_state();
    const auto legacy = std::string{
        R"({"entries":[{"type":"string","data":"z"},{"type":"float","data":16.0},{"type":"bool","data":true},{"type":"string","data":"bool key"},{"type":"string","data":"a"},{"type":"int","data":16}]})"};
    auto restored = deserialize_table(lua, legacy);
    CHECK(restored.get<std::string>(true) == "bool key");
    CHECK(restored.get<int>("a") == 16);
    CHECK(is_number_integer(lua, restored.get<sol::object>("a")));
    CHECK_FALSE(is_number_integer(lua, restored.get<sol::object>("z")));
    CHECK(
        serialize_table(restored)
        == R"({"entries":[{"type":"bool","data":true},{"type":"string","data":"bool key"},{"type":"string","data":"a"},{"type":"int","data":16},{"type":"string","data":"z"},{"type":"float","data":16.000000000000000}]})");
    CHECK(serialize_table(deserialize_table(lua, "{}")) == "{}");
    const auto diagnostic = capture_debugmsg_during([&]() {
        CHECK(serialize_table(
                  deserialize_table(lua, R"({"entries":[{"type":"string","data":"orphan"}]})"))
              == "{}");
    });
    CHECK(diagnostic == "invalid array size 1");
}

// Explicit-only consumer for immutable ordinary-replay exports. It uses the real
// decoder, bound objects and producer; it does not normalize any acceptance input.
TEST_CASE("lua_serde_saved_record_consumer", "[.lua_serde_record]") {
    const auto* input_path = std::getenv("CATA_LUA_SERDE_INPUT");
    const auto* output_path = std::getenv("CATA_LUA_SERDE_OUTPUT");
    REQUIRE(input_path != nullptr);
    REQUIRE(output_path != nullptr);
    auto lua = make_lua_state();
    auto input = std::ifstream{input_path};
    REQUIRE(input.good());
    auto jsin = JsonIn(input);
    auto record = jsin.get_object();
    auto mods = lua.create_table();
    for (const auto member : record) {
        auto table = lua.create_table();
        auto object = member.get_object();
        cata::deserialize_lua_table(table, object);
        mods[member.name()] = table;
    }
    const auto tablet = mods.get<sol::table>("tablet_ebook");
    const auto flags = tablet.get<sol::table>("flags");
    for (const auto* name : {"MC_USED", "TRADER_AVOID", "ETHEREAL_ITEM"}) {
        CHECK(flags.get<flag_id>(name) == flag_id(name));
    }
    CHECK(
        serialize_table(flags)
        == R"({"entries":[{"type":"string","data":"ETHEREAL_ITEM"},{"type":"userdata","kind":"JsonFlagId","data":"ETHEREAL_ITEM"},{"type":"string","data":"MC_USED"},{"type":"userdata","kind":"JsonFlagId","data":"MC_USED"},{"type":"string","data":"TRADER_AVOID"},{"type":"userdata","kind":"JsonFlagId","data":"TRADER_AVOID"}]})");
    auto output = std::ofstream{output_path};
    REQUIRE(output.good());
    auto jsout = JsonOut(output);
    jsout.start_object();
    for (const auto member : record) {
        jsout.member(member.name());
        cata::serialize_lua_table(mods.get<sol::table>(member.name()), jsout);
    }
    jsout.end_object();
    output.flush();
    REQUIRE(output.good());
}

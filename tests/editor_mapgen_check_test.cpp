#include "catch/catch.hpp"
#include "editor/json_pointer.h"
#include "editor/mapgen_check.h"
#include "json_formatter.h"

#include <nlohmann/json.hpp>
#include <ranges>
#include <string>
#include <vector>

namespace {

using nlohmann::ordered_json;

/// A 24x24 mapgen entry of floor, as the data files write it.
auto floor_entry() -> ordered_json {
    auto rows = ordered_json::array();
    for (auto row = 0; row < 24; ++row) { rows.push_back(std::string(24, '.')); }
    return {
        {"type", "mapgen"},
        {"method", "json"},
        {"om_terrain", "field"},
        {"object", {{"fill_ter", "t_floor"}, {"rows", rows}, {"terrain", {{".", "t_floor"}}}}},
    };
}

struct pointed {
    std::string text;
    std::string pointer;
};

/// What the loader says of `entry`, each message with the pointer of the value it names ("" for
/// none).
auto check(const ordered_json& entry) -> std::vector<pointed> {
    const auto text = format_json(entry.dump()).value();
    return editor::check_mapgen(text) | std::views::transform([&](const auto& message) {
               return pointed{
                   .text = message.text,
                   .pointer = message.offset ? editor::pointer_at(text, *message.offset) : ""};
           })
         | std::ranges::to<std::vector>();
}

auto names(const std::vector<pointed>& messages, const std::string& words) -> bool {
    return std::ranges::any_of(messages, [&](const auto& message) {
        return message.text.find(words) != std::string::npos;
    });
}

} // namespace

TEST_CASE("the pointer of a byte is the innermost value holding it", "[editor_mapgen]") {
    const auto text = std::string(R"({"a": [1, {"b/c": "xyz"}], "d~": 2})");
    CHECK(editor::pointer_at(text, 0).empty());
    CHECK(editor::pointer_at(text, text.find('1')) == "/a/0");
    CHECK(editor::pointer_at(text, text.find('y')) == "/a/1/b~1c");
    CHECK(editor::pointer_at(text, text.find('2')) == "/d~0");
}

TEST_CASE("a well-formed mapgen loads without a word from the loader", "[editor_mapgen]") {
    CHECK(check(floor_entry()).empty());
}

TEST_CASE("a row of the wrong length points at that row", "[editor_mapgen]") {
    auto entry = floor_entry();
    entry["object"]["rows"][2] = std::string(23, '.');
    const auto messages = check(entry);
    REQUIRE(messages.size() == 1);
    CHECK(messages[0].text.find("row 3 must have") != std::string::npos);
    CHECK(messages[0].pointer == "/object/rows/2");
}

TEST_CASE("a symbol without a definition points at its row", "[editor_mapgen]") {
    auto entry = floor_entry();
    entry["object"].erase("fill_ter");
    entry["object"]["rows"][5] = std::string(10, '.') + "x" + std::string(13, '.');
    const auto messages = check(entry);
    REQUIRE_FALSE(messages.empty());
    CHECK(names(messages, "'x' is not in 'terrain'"));
    CHECK(messages[0].pointer == "/object/rows/5");
}

TEST_CASE("a misspelt key is named where it is", "[editor_mapgen]") {
    auto entry = floor_entry();
    entry["object"]["place_itemz"] = ordered_json::array();
    const auto messages = check(entry);
    REQUIRE(messages.size() == 1);
    CHECK(names(messages, "\"place_itemz\""));
    CHECK(messages[0].pointer == "/object/place_itemz");
}

TEST_CASE("a placement outside the map points at the placement", "[editor_mapgen]") {
    auto entry = floor_entry();
    entry["object"]["place_monster"] = {{{"monster", "mon_zombie"}, {"x", 30}, {"y", 3}}};
    const auto messages = check(entry);
    REQUIRE_FALSE(messages.empty());
    CHECK(messages[0].pointer.starts_with("/object/place_monster/0"));
}

TEST_CASE("an unknown terrain id is reported", "[editor_mapgen]") {
    auto entry = floor_entry();
    entry["object"]["terrain"]["."] = "t_no_such_terrain";
    const auto messages = check(entry);
    REQUIRE_FALSE(messages.empty());
    CHECK(names(messages, "t_no_such_terrain"));
}

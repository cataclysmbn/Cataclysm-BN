#include "catch/catch.hpp"
#include "editor/document.h"
#include "editor/json_pointer.h"
#include "editor/mapgen_check.h"
#include "json_formatter.h"

#include <filesystem>
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

TEST_CASE("a symbol on every square is checked once", "[editor_mapgen]") {
    auto entry = floor_entry();
    entry["object"].erase("fill_ter");
    entry["object"]["terrain"]["."] = {{"param", "no_such_parameter"}};
    const auto messages = check(entry);
    CHECK(
        std::ranges::count_if(
            messages,
            [](const auto& message) {
                return message.text.find("undefined parameter") != std::string::npos;
            })
        == 1);
}

TEST_CASE("an unknown overmap terrain points at its id", "[editor_mapgen]") {
    auto entry = floor_entry();
    entry["om_terrain"] = "no_such_terrain";
    const auto messages = check(entry);
    REQUIRE_FALSE(messages.empty());
    CHECK(names(messages, "no_such_terrain"));
    CHECK(messages[0].pointer == "/om_terrain");
}

TEST_CASE("an unknown palette points at its id", "[editor_mapgen]") {
    auto entry = floor_entry();
    entry["object"]["palettes"] = {"no_such_palette"};
    const auto messages = check(entry);
    REQUIRE_FALSE(messages.empty());
    CHECK(names(messages, "no_such_palette"));
    CHECK(messages[0].pointer == "/object/palettes/0");
}

/// The data still places a few things outside the map, which the game drops; nothing else is said.
TEST_CASE(
    "every json mapgen in the data loads with no word but its dropped placements",
    "[editor_mapgen]") {
    auto checked = 0;
    for (const auto& file : std::filesystem::recursive_directory_iterator("data/json")) {
        if (file.path().extension() != ".json") { continue; }
        const auto doc = editor::document::open(file.path());
        REQUIRE(doc);
        for (const auto& object : doc->objects()) {
            const auto& value = object.value;
            if (!value.is_object() || value.value("type", "") != "mapgen"
                || value.value("method", "") != "json") {
                continue;
            }
            CAPTURE(file.path().string(), object.begin);
            for (const auto& message : editor::check_mapgen(value.dump())) {
                CAPTURE(message.text);
                CHECK(message.text.find(" map") != std::string::npos);
                CHECK(message.text.find("is outside the") != std::string::npos);
            }
            ++checked;
        }
    }
    CHECK(checked > 1000);
}

TEST_CASE("a palette loads with no word from the loader", "[editor_mapgen][editor_palette]") {
    CHECK(
        check({{"type", "palette"},
               {"id", "editor_check_palette"},
               {"palettes", {"cabin_palette"}},
               {"terrain", {{".", "t_floor"}}}})
            .empty());
}

TEST_CASE(
    "a palette that a palette it names names again is a loop", "[editor_mapgen][editor_palette]") {
    // cabin_palette names cabin_palette_common, which here names cabin_palette.
    const auto looped = ordered_json{
        {"type", "palette"}, {"id", "cabin_palette_common"}, {"palettes", {"cabin_palette"}}};
    const auto messages = check(looped);
    CHECK(names(messages, "loop in palette references: cabin_palette_common > cabin_palette"));
    // The palette the game loaded is back afterwards, so that nothing loops through it any more.
    CHECK(
        check(
            {{"type", "palette"}, {"id", "editor_check_palette"}, {"palettes", {"cabin_palette"}}})
            .empty());
}

#include "cata_utility.h"
#include "catch/catch.hpp"
#include "editor/map_session.h"
#include "editor/session_file.h"
#include "path_info.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

namespace {

auto read_json(const std::filesystem::path& path) -> nlohmann::ordered_json {
    auto in = std::ifstream(path, std::ios::binary);
    auto text = std::ostringstream{};
    text << in.rdbuf();
    return nlohmann::ordered_json::parse(text.str());
}

} // namespace

TEST_CASE(
    "the editors make a new file only in a mod's folder of the player's mods",
    "[editor_session][editor_vehicle]") {
    const auto mods = std::filesystem::path(PATH_INFO::user_moddir());
    // A file loose in the mods is no mod's: no world loads it.
    CHECK_FALSE(editor::is_mod_file(mods / "loose.json"));
    CHECK(editor::is_mod_file(mods / "some_mod" / "loose.json"));
    CHECK(editor::is_mod_file(mods / "some_mod" / "vehicles" / "car.json"));
    CHECK_FALSE(editor::is_mod_file(mods / "some_mod" / "car.txt"));
    const auto loose = editor::map_editor::check_new_path((mods / "loose.json").string());
    REQUIRE_FALSE(loose.has_value());
    CHECK(loose.error().contains("folder"));
    CHECK(editor::map_editor::check_new_path((mods / "some_mod" / "house.json").string()));
}

TEST_CASE(
    "a file the editors write makes its folder a mod unless a mod holds it already",
    "[editor_session][editor_vehicle]") {
    const auto mods = std::filesystem::path(PATH_INFO::user_moddir());
    const auto made = mods / "editor_mod_folder_made";
    const auto kept = mods / "editor_mod_folder_kept";
    const auto forget = [&] {
        std::filesystem::remove_all(made);
        std::filesystem::remove_all(kept);
    };
    forget();
    const auto cleanup = on_out_of_scope(forget);
    const auto entries = nlohmann::ordered_json::array(
        {{{"type", "palette"}, {"id", "editor_mod_folder_palette"}}});
    REQUIRE(editor::map_editor::write_new_file((made / "maps" / "house.json").string(), entries));
    const auto info = read_json(made / "modinfo.json");
    REQUIRE(info.is_array());
    REQUIRE(info.size() == 1);
    CHECK(info[0]["type"] == "MOD_INFO");
    CHECK(info[0]["id"] == "editor_mod_folder_made");
    CHECK_FALSE(std::filesystem::exists(made / "maps" / "modinfo.json"));
    // A mod whose modinfo.json is deeper than the folder holds the file already.
    std::filesystem::create_directories(kept / "inner");
    std::ofstream(kept / "inner" / "modinfo.json")
        << R"([{"type":"MOD_INFO","id":"inner","name":"Inner","category":"content"}])";
    REQUIRE(editor::map_editor::write_new_file((kept / "inner" / "house.json").string(), entries));
    CHECK_FALSE(std::filesystem::exists(kept / "modinfo.json"));
    CHECK(read_json(kept / "inner" / "modinfo.json")[0]["id"] == "inner");
}

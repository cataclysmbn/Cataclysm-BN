#include "cata_utility.h"
#include "catch/catch.hpp"
#include "editor/map_session.h"
#include "editor/recent_documents.h"
#include "editor/session_file.h"
#include "mod_manager.h"
#include "path_info.h"
#include "worldfactory.h"

#include <algorithm>
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

TEST_CASE(
    "the editors make no mod of a folder whose name is another mod's id or no mod id",
    "[editor_session][editor_vehicle]") {
    const auto mods = std::filesystem::path(PATH_INFO::user_moddir());
    const auto taken = mods / "bn";
    const auto spaced = mods / "editor mod folder";
    const auto forget = [&] {
        std::filesystem::remove_all(taken);
        std::filesystem::remove_all(spaced);
    };
    forget();
    const auto cleanup = on_out_of_scope(forget);
    // The folder's name would be the new mod's id, and bn is the game's own mod.
    const auto game_mod = editor::map_editor::check_new_path((taken / "house.json").string());
    REQUIRE_FALSE(game_mod.has_value());
    CHECK(game_mod.error().contains("bn_2/"));
    const auto no_id = editor::map_editor::check_new_path((spaced / "house.json").string());
    REQUIRE_FALSE(no_id.has_value());
    CHECK(no_id.error().contains("editor_mod_folder/"));
    // Save as refuses before it writes anything.
    auto doc = editor::document::create(mods / "editor_mod_folder_unsaved" / "house.json", "[]");
    REQUIRE(doc.has_value());
    CHECK_FALSE(editor::save_document_as(*doc, taken / "house.json").has_value());
    CHECK_FALSE(std::filesystem::exists(taken));
}

TEST_CASE(
    "the editors make no mod of a folder that holds other mods below it",
    "[editor_session][editor_vehicle]") {
    const auto mods = std::filesystem::path(PATH_INFO::user_moddir());
    const auto pack = mods / "editor_mod_folder_pack";
    const auto forget = [&] { std::filesystem::remove_all(pack); };
    forget();
    const auto cleanup = on_out_of_scope(forget);
    std::filesystem::create_directories(pack / "inner");
    std::ofstream(pack / "inner" / "modinfo.json")
        << R"([{"type":"MOD_INFO","id":"editor_mod_folder_inner","name":"Inner"}])";
    // A mod of the pack would load the inner mod's files a second time.
    const auto checked = editor::map_editor::check_new_path((pack / "house.json").string());
    REQUIRE_FALSE(checked.has_value());
    CHECK(checked.error().contains("inner"));
    auto doc = editor::document::create(pack / "house.json", "[]");
    REQUIRE(doc.has_value());
    CHECK_FALSE(editor::save_document(*doc).has_value());
    CHECK_FALSE(std::filesystem::exists(pack / "house.json"));
    CHECK_FALSE(std::filesystem::exists(pack / "modinfo.json"));
    // The inner mod's own folder takes a new file.
    CHECK(editor::map_editor::check_new_path((pack / "inner" / "house.json").string()));
}

TEST_CASE(
    "the editors save an edit to a file already in a folder that cannot be a mod",
    "[editor_session][editor_vehicle]") {
    const auto mods = std::filesystem::path(PATH_INFO::user_moddir());
    const auto spaced = mods / "editor mod folder old";
    const auto forget = [&] { std::filesystem::remove_all(spaced); };
    forget();
    const auto cleanup = on_out_of_scope(forget);
    std::filesystem::create_directories(spaced);
    std::ofstream(spaced / "house.json") << "[]";
    auto doc = editor::document::open(spaced / "house.json");
    REQUIRE(doc.has_value());
    // The file is there already: refusing would only lose the edit.
    CHECK(editor::save_document(*doc).has_value());
    CHECK_FALSE(std::filesystem::exists(spaced / "modinfo.json"));
}

TEST_CASE(
    "the editors look past a folder they cannot read for mods below a new mod's folder",
    "[editor_session][editor_vehicle]") {
    const auto mods = std::filesystem::path(PATH_INFO::user_moddir());
    const auto top = mods / "editor_mod_folder_locked";
    const auto forget = [&] {
        std::filesystem::permissions(
            top / "locked", std::filesystem::perms::owner_all, std::filesystem::perm_options::add);
        std::filesystem::remove_all(top);
    };
    std::filesystem::remove_all(top);
    std::filesystem::create_directories(top / "locked" / "deeper");
    const auto cleanup = on_out_of_scope(forget);
    std::filesystem::permissions(top / "locked", std::filesystem::perms::none);
    CHECK(editor::map_editor::check_new_path((top / "house.json").string()));
}

TEST_CASE(
    "an editor world opens with every mod of the player's mods after the mods it starts from",
    "[editor_session][editor_vehicle]") {
    const auto mods = std::filesystem::path(PATH_INFO::user_moddir());
    const auto own = mods / "editor_world_mods_own";
    const auto old = mods / "editor_world_mods_old";
    const auto forget = [&] {
        std::filesystem::remove_all(own);
        std::filesystem::remove_all(old);
        world_generator->get_mod_manager().refresh_mod_list();
    };
    forget();
    const auto cleanup = on_out_of_scope(forget);
    std::filesystem::create_directories(own);
    std::ofstream(own / "modinfo.json")
        << R"([{"type":"MOD_INFO","id":"editor_world_mods_own","name":"Own",)"
           R"("category":"content","dependencies":["bn","desertpack"]}])";
    std::filesystem::create_directories(old);
    std::ofstream(old / "modinfo.json")
        << R"([{"type":"MOD_INFO","id":"editor_world_mods_old","name":"Old",)"
           R"("category":"content","dependencies":["bn"],"obsolete":true}])";
    world_generator->get_mod_manager().refresh_mod_list();
    const auto defaults = world_generator->get_mod_manager().get_default_mods();
    const auto opened = editor::editor_world_mods({});
    REQUIRE(opened.size() >= defaults.size());
    CHECK(std::ranges::equal(opened | std::views::take(defaults.size()), defaults));
    const auto own_at = std::ranges::find(opened, mod_id("editor_world_mods_own"));
    const auto needed = std::ranges::find(opened, mod_id("desertpack"));
    REQUIRE(own_at != opened.end());
    REQUIRE(needed != opened.end());
    CHECK(needed < own_at);
    // An obsolete mod starts no new world.
    CHECK_FALSE(std::ranges::contains(opened, mod_id("editor_world_mods_old")));
    // A document's mods come first; none of them left starts from the default mods.
    const auto core = mod_management::get_default_core_content_pack();
    const auto reopened = editor::editor_world_mods({core, mod_id("gone")});
    REQUIRE_FALSE(reopened.empty());
    CHECK(reopened.front() == core);
    CHECK_FALSE(std::ranges::contains(reopened, mod_id("gone")));
    CHECK(std::ranges::contains(reopened, mod_id("editor_world_mods_own")));
    CHECK(editor::editor_world_mods({mod_id("gone")}) == opened);
}

TEST_CASE(
    "an editor world leaves out a mod of the player's that needs another core than the game's",
    "[editor_session][editor_vehicle]") {
    const auto mods = std::filesystem::path(PATH_INFO::user_moddir());
    const auto core = mods / "editor_world_mods_core";
    const auto needy = mods / "editor_world_mods_needy";
    const auto forget = [&] {
        std::filesystem::remove_all(core);
        std::filesystem::remove_all(needy);
        world_generator->get_mod_manager().refresh_mod_list();
    };
    forget();
    const auto cleanup = on_out_of_scope(forget);
    std::filesystem::create_directories(core);
    std::ofstream(core / "modinfo.json")
        << R"([{"type":"MOD_INFO","id":"editor_world_mods_core","name":"Core",)"
           R"("category":"content","core":true}])";
    std::filesystem::create_directories(needy);
    std::ofstream(needy / "modinfo.json")
        << R"([{"type":"MOD_INFO","id":"editor_world_mods_needy","name":"Needy",)"
           R"("category":"content","dependencies":["editor_world_mods_core"]}])";
    world_generator->get_mod_manager().refresh_mod_list();
    const auto defaults = world_generator->get_mod_manager().get_default_mods();
    // Adding it would put its core in place of the game's and drop the default mods.
    const auto opened = editor::editor_world_mods({});
    CHECK(std::ranges::equal(opened | std::views::take(defaults.size()), defaults));
    CHECK_FALSE(std::ranges::contains(opened, mod_id("editor_world_mods_needy")));
}

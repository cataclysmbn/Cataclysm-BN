#include "cata_utility.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "filesystem.h"
#include "json.h"
#include "options.h"
#include "path_info.h"
#include "world.h"
#include "worldfactory.h"

#include <algorithm>
#include <ostream>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

TEST_CASE(
    "world reset preserves the save format without retaining saved data",
    "[world][save][world_reset]") {
    const auto format = GENERATE(save_format::V1, save_format::V2_COMPRESSED_SQLITE3);
    CAPTURE(format);

    const auto original_savedir = PATH_INFO::savedir();
    const auto test_savedir = original_savedir + "world_reset/";
    REQUIRE_FALSE(dir_exist(test_savedir));
    REQUIRE(assure_dir_exist(test_savedir));
    const auto cleanup = on_out_of_scope([&]() {
        PATH_INFO::set_savedir(original_savedir);
        CHECK(remove_tree(test_savedir));
    });
    PATH_INFO::set_savedir(test_savedir);

    auto info = WORLDINFO{};
    info.world_name = "reset_world";
    info.world_save_format = format;
    info.WORLD_OPTIONS["WORLD_END"].setValue("reset");
    info.active_mod_order = {mod_id("test_data")};
    REQUIRE(info.save());

    const auto save = save_t::from_save_id("Reset survivor");
    const auto omt = tripoint_abs_omt{1, 2, 0};
    const auto read_map = [](JsonIn& json) { CHECK(json.get_string() == "old map"); };
    {
        auto saved_world = world{&info};
        REQUIRE(saved_world.write_to_file(save.base_path() + ".sav", [](std::ostream& out) {
            out << "{}";
        }));
        REQUIRE(saved_world.write_map_omt("", omt, [](std::ostream& out) {
            out << R"("old map")";
        }));
        REQUIRE(saved_world.read_map_omt("", omt, read_map));
    }

    auto factory = worldfactory{};
    factory.init();
    auto* loaded = factory.get_world(info.world_name);
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->world_save_format == format);
    REQUIRE(loaded->save_exists(save));

    SECTION("reset retains metadata and removes saved data after rediscovery") {
        factory.delete_world(info.world_name, false);
        CHECK(loaded->world_saves.empty());
        CHECK(dir_exist(info.folder_path()));
        CHECK(file_exist(info.folder_path() + "/map.sqlite3")
              == (format == save_format::V2_COMPRESSED_SQLITE3));

        factory.init();
        loaded = factory.get_world(info.world_name);
        REQUIRE(loaded != nullptr);
        CHECK(loaded->world_save_format == format);
        CHECK(loaded->world_saves.empty());
        CHECK(loaded->WORLD_OPTIONS.at("WORLD_END").getValue() == "reset");
        CHECK(loaded->active_mod_order == info.active_mod_order);

        auto reset_world = world{loaded};
        CHECK_FALSE(reset_world.read_map_omt("", omt, read_map));
        REQUIRE(reset_world.write_map_omt("", omt, [](std::ostream& out) {
            out << R"("new map")";
        }));
        CHECK(reset_world.read_map_omt("", omt, [](JsonIn& json) {
            CHECK(json.get_string() == "new map");
        }));
    }

    SECTION("deleting a world does not recreate its metadata") {
        factory.delete_world(info.world_name, true);
        CHECK_FALSE(dir_exist(info.folder_path()));
        CHECK_FALSE(factory.has_world(info.world_name));
        factory.init();
        CHECK_FALSE(factory.has_world(info.world_name));
    }
}

TEST_CASE(
    "world option saves preserve every record in stable name order",
    "[world][save][world_options]") {
    namespace ranges = std::ranges;
    using namespace std::views;

    const auto original_savedir = PATH_INFO::savedir();
    const auto test_savedir = original_savedir + "world_options_order/";
    REQUIRE_FALSE(dir_exist(test_savedir));
    REQUIRE(assure_dir_exist(test_savedir));
    const auto cleanup = on_out_of_scope([&]() {
        PATH_INFO::set_savedir(original_savedir);
        CHECK(remove_tree(test_savedir));
    });
    PATH_INFO::set_savedir(test_savedir);

    auto info = WORLDINFO{};
    info.world_name = "ordered_options";
    info.world_save_format = GENERATE(save_format::V1, save_format::V2_COMPRESSED_SQLITE3);
    info.WORLD_OPTIONS.at("WORLD_END").setValue("reset");
    info.WORLD_OPTIONS.emplace("test_hidden_world_option", options_manager::cOpt{});
    const auto original = info.WORLD_OPTIONS;
    auto names = original | keys | ranges::to<std::vector>();
    ranges::sort(names);
    const auto visible_names =
        names
        | filter([&](const auto& name) { return !original.at(name).getDefaultText().empty(); })
        | ranges::to<std::vector>();
    REQUIRE_FALSE(visible_names.empty());
    REQUIRE(visible_names.size() < original.size());

    const auto save_options = [&]() -> std::string {
        REQUIRE(info.save());
        return read_entire_file(info.folder_path() + "/" + PATH_INFO::worldoptions());
    };
    const auto first = save_options();

    auto reordered = options_manager::options_container{};
    reordered.rehash(original.bucket_count() * 4 + 1);
    for (const auto& name : names | reverse) { reordered.emplace(name, original.at(name)); }
    info.WORLD_OPTIONS = std::move(reordered);
    CHECK(save_options() == first);
    CHECK(info.WORLD_OPTIONS == original);

    auto saved_names = std::vector<std::string>{};
    REQUIRE(
        read_from_file_json(info.folder_path() + "/" + PATH_INFO::worldoptions(), [&](JsonIn& json) {
            for (auto record : json.get_array()) {
                auto option = record.get_object();
                const auto name = option.get_string("name");
                REQUIRE(original.contains(name));
                const auto& expected = original.at(name);
                CHECK(option.get_string("info") == expected.getTooltip());
                CHECK(option.get_string("default") == expected.getDefaultText(false));
                CHECK(option.get_string("value") == expected.getValue(true));
                saved_names.push_back(name);
            }
        }));
    CHECK(saved_names == visible_names);

    auto loaded = WORLDINFO{};
    loaded.world_name = info.world_name;
    REQUIRE(loaded.load_options());
    for (const auto& name : visible_names) {
        CHECK(loaded.WORLD_OPTIONS.at(name).getValue(true) == original.at(name).getValue(true));
    }
    CHECK_FALSE(loaded.WORLD_OPTIONS.contains("test_hidden_world_option"));
}

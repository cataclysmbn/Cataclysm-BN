#include "cata_utility.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "filesystem.h"
#include "path_info.h"
#include "world.h"
#include "worldfactory.h"

#include <ostream>
#include <string>

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

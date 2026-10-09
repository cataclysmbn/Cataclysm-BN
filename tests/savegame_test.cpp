#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "debug.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "game.h"
#include "json.h"
#include "recipe.h"
#include "state_helpers.h"
#include "type_id.h"
#include "world.h"
#include "worldfactory.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

TEST_CASE("failed save load blocks saving over the broken save", "[save][load]") {
    clear_all_state();
    g->clear_failed_load_save_block();
    const auto cleanup = on_out_of_scope([]() {
        clear_all_state();
        g->clear_failed_load_save_block();
    });

    const auto fixture_path = fs::path("tests/data/save/broken_load_save/#QnJva2VuU2F2ZQ==.sav");
    const auto world_path = fs::path(g->get_active_world()->info->folder_path());
    const auto save_path = world_path / "#QnJva2VuU2F2ZQ==.sav";
    CHECK(fs::copy_file(fixture_path, save_path, fs::copy_options::overwrite_existing));

    const auto before_load = read_entire_file(save_path.string());
    auto loaded = true;
    const auto debug_message = capture_debugmsg_during([&]() {
        loaded = g->load(save_t::from_save_id("BrokenSave"));
    });

    CHECK_FALSE(loaded);
    CHECK(debug_message.find("Bad save json") != std::string::npos);
    CHECK_FALSE(g->save(false));
    CHECK(read_entire_file(save_path.string()) == before_load);
}

TEST_CASE("manual combat mode is serialized in save data", "[save]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });

    g->manual_combat_mode = true;

    std::ostringstream save_data;
    g->serialize(save_data);

    CHECK(save_data.str().find(R"("manual_combat_mode": true)") != std::string::npos);
}

TEST_CASE("learned recipes serialize in stable recipe ID order", "[save][recipe]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });

    const auto recipe_ids = std::array{
        recipe_id("brew_rum"),
        recipe_id("meat_cooked"),
        recipe_id("sandwich_pb"),
    };
    REQUIRE(std::ranges::all_of(recipe_ids, &recipe_id::is_valid));

    auto first_allocations =
        std::array<recipe, 3>{recipe_ids[1].obj(), recipe_ids[2].obj(), recipe_ids[0].obj()};
    auto second_allocations =
        std::array<recipe, 3>{recipe_ids[2].obj(), recipe_ids[0].obj(), recipe_ids[1].obj()};
    auto first_character = avatar{};
    auto second_character = avatar{};
    first_character.learn_recipe(&first_allocations[2]);
    first_character.learn_recipe(&first_allocations[0]);
    first_character.learn_recipe(&first_allocations[1]);
    second_character.learn_recipe(&second_allocations[2]);
    second_character.learn_recipe(&second_allocations[0]);
    second_character.learn_recipe(&second_allocations[1]);

    const auto first_save = serialize(first_character);
    const auto second_save = serialize(second_character);
    const auto learned_recipe_ids = [](const std::string& save) {
        auto input = std::istringstream(save);
        auto json = JsonIn(input);
        auto object = json.get_object();
        object.allow_omitted_members();
        auto result = std::vector<std::string>{};
        object.read("learned_recipes", result);
        return result;
    };
    const auto expected_order = std::vector<std::string>{"brew_rum", "meat_cooked", "sandwich_pb"};

    CHECK(learned_recipe_ids(first_save) == expected_order);
    CHECK(learned_recipe_ids(second_save) == expected_order);

    auto loaded_character = avatar{};
    deserialize(loaded_character, first_save);
    for (const auto& recipe_id : recipe_ids) {
        CHECK(loaded_character.knows_recipe(&recipe_id.obj()));
    }
}

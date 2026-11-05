#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "crafting.h"
#include "enums.h"
#include "game.h"
#include "item.h"
#include "json.h"
#include "map/map.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"
#include "state_helpers.h"
#include "units_temperature.h"
#include "weather/weather.h"

#include <cmath>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct ingredient_test_state {
    restore_on_out_of_scope<time_point> time{calendar::turn};
    restore_on_out_of_scope<units::temperature> temperature{get_weather().temperature};

    ingredient_test_state() {
        clear_all_state();
        calendar::turn = calendar::start_of_cataclysm + 1_hours;
        get_weather().temperature = 18_c;
        get_weather().clear_temp_cache();
    }
    ~ingredient_test_state() {
        clear_all_state();
        get_weather().clear_temp_cache();
    }
};

auto finish_recipe(const recipe_id& id, std::vector<detached_ptr<item>> ingredients)
    -> item& // *NOPAD*
{
    auto& who = get_avatar();
    who.learn_recipe(&id.obj());
    auto craft = item::spawn(&id.obj(), 1, std::move(ingredients), std::vector<item_comp>{});
    complete_craft(who, *craft);
    const auto results = who.items_with([&](const auto& it) {
        return it.typeId() == id->result();
    });
    REQUIRE(results.size() == 1);
    return *results.front();
}

} // namespace

TEST_CASE(
    "Nonperishable ingredients stay unspoiled when cooked on day 22",
    "[rot][crafting][ingredients][issue_10181]") {
    const auto state = ingredient_test_state();
    const auto recipes = restore_on_out_of_scope(
        const_cast<recipe_subset&>(get_avatar().get_learned_recipes()));
    const auto recipe_name = GENERATE("cornbread", "pemmican", "sausagegravy");
    const auto elapsed = GENERATE(0_days, 21_days);
    auto ingredients = std::vector<detached_ptr<item>>();
    const auto add = [&](const char* id, int charges) {
        auto ingredient = item::spawn(itype_id(id), calendar::turn, charges);
        ingredient->set_relative_rot(0.0);
        ingredients.push_back(std::move(ingredient));
    };
    const auto name = std::string_view(recipe_name);
    if (name == "cornbread") {
        add("cornmeal", 3);
        add("water_clean", 2);
    } else if (name == "pemmican") {
        add("lard", 1);
        add("dry_meat", 2);
        add("dry_mushroom", 1);
    } else {
        add("dry_meat", 1);
        add("lard", 1);
        add("cornmeal", 1);
        add("mushroom", 2);
    }

    const auto freezer = tripoint_bub_ms(65, 65, 0);
    get_map().furn_set(freezer, furn_str_id("f_minifreezer_on"));
    for (auto& ingredient : ingredients) { get_map().add_item(freezer, std::move(ingredient)); }
    ingredients.clear();
    calendar::turn += elapsed;
    const auto stored = get_map().i_at(freezer) | std::ranges::to<std::vector>();
    for (auto* ingredient : stored) {
        REQUIRE_FALSE(ingredient->rotten());
        REQUIRE(ingredient->get_rot() == 0_turns);
        ingredients.push_back(ingredient->detach());
    }

    auto& food = finish_recipe(recipe_id(recipe_name), std::move(ingredients));
    CHECK_FALSE(food.rotten());
    REQUIRE_FALSE(food.get_components().empty());
    for (const auto* component : food.get_components()) {
        CAPTURE(recipe_name, to_days<int>(elapsed), component->typeId().str());
        CHECK(std::isfinite(component->get_relative_rot()));
        CHECK(component->get_relative_rot() == 0.0);
        CHECK_FALSE(component->rotten());
    }
    CHECK(food.components_to_string().find("(rotten)") == std::string::npos);
}

TEST_CASE(
    "Loading an old save does not make nonperishable ingredients rotten",
    "[rot][ingredients][save][issue_10181]") {
    const auto state = ingredient_test_state();
    // Rot-related fields from the sausage gravy in the issue's attached save.
    // No positive rot is stored on the three nonperishable ingredients.
    auto input = std::istringstream(R"({
        "typeid": "sausagegravy", "rot": 240, "last_rot_check": 1854224,
        "active": true, "charges": 3,
        "components": [
            { "typeid": "dry_meat", "last_rot_check": 0,
              "item_tags": ["COOKED", "COMPONENT"], "charges": 1 },
            { "typeid": "lard", "last_rot_check": 0,
              "item_tags": ["COOKED", "COMPONENT"], "charges": 1 },
            { "typeid": "cornmeal", "last_rot_check": 0,
              "item_tags": ["COOKED", "COMPONENT"], "charges": 1 }
        ]
    })");
    auto json = JsonIn(input);
    auto food = item::spawn(json);
    REQUIRE(food);
    REQUIRE(food->get_components().size() == 3);
    for (const auto* component : food->get_components()) {
        CAPTURE(component->typeId().str());
        REQUIRE_FALSE(component->goes_bad());
        CHECK(std::isfinite(component->get_relative_rot()));
        CHECK(component->get_relative_rot() == 0.0);
        CHECK_FALSE(component->rotten());
    }
    CHECK(food->components_to_string().find("(rotten)") == std::string::npos);
}

TEST_CASE(
    "Nonperishable food stays unspoiled before and after it becomes an ingredient",
    "[rot][ingredients][issue_10181]") {
    const auto state = ingredient_test_state();
    const auto type =
        GENERATE("test_rot_nonperishable", "flour", "cornmeal", "lard", "water_clean");
    auto ingredient = item::spawn(itype_id(type), calendar::turn, 1);
    REQUIRE_FALSE(ingredient->goes_bad());
    calendar::turn += 21_days;
    ingredient = item::process_rot(std::move(ingredient), tripoint_bub_ms::zero());
    REQUIRE(ingredient);
    CHECK_FALSE(ingredient->rotten());
    CHECK(ingredient->get_rot() == 0_turns);

    auto food = item::spawn("test_rot_food");
    food->add_component(std::move(ingredient));
    const auto* component = food->get_components().front();
    CHECK(std::isfinite(component->get_relative_rot()));
    CHECK(component->get_relative_rot() == 0.0);
    CHECK_FALSE(component->rotten());
    CHECK(food->components_to_string().find("(rotten)") == std::string::npos);
}

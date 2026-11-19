#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "crafting.h"
#include "flag.h"
#include "item.h"
#include "item_factory.h"
#include "itype.h"
#include "map/map.h"
#include "output.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "rot_test_state.h"
#include "type_id.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <string>
#include <vector>

// Regression tests for the open rot-related issues.
// Each test names the issue it comes from, so a still-failing test points at an unfixed issue.

namespace {

constexpr auto station = tripoint_bub_ms(65, 65, 0);

struct machine_run {
    std::string furniture;
    std::string fake;
    itype_id input;
    itype_id product;
};

/// Every smokable and millable item, with the machine that processes it.
auto machine_runs() -> std::vector<machine_run> {
    auto runs = std::vector<machine_run>();
    for (const auto* type : item_controller->all()) {
        if (type->comestible && type->has_flag(flag_id("SMOKABLE"))
            && !type->comestible->smoking_result.is_empty()) {
            runs.push_back(
                {.furniture = "f_smoking_rack_active",
                 .fake = "fake_smoke_plume",
                 .input = type->get_id(),
                 .product = type->comestible->smoking_result});
        }
        if (type->milling_data) {
            runs.push_back(
                {.furniture = "f_wind_mill_active",
                 .fake = "fake_milling_item",
                 .input = type->get_id(),
                 .product = type->milling_data->into_});
        }
    }
    std::ranges::sort(runs, {}, [](const auto& run) { return run.input.str(); });
    return runs;
}

auto nonperishable_ids() -> std::vector<std::string> {
    using namespace std::views;
    namespace ranges = std::ranges;
    auto ids =
        item_controller->all() | filter([](const auto* type) {
            return type->comestible
                && !item::spawn(type->get_id(), calendar::start_of_cataclysm)->goes_bad();
        })
        | transform([](const auto* type) { return type->get_id().str(); })
        | ranges::to<std::vector>();
    ranges::sort(ids);
    return ids;
}

/// Items that appeared in the machine as its product, found by type.
auto products_at(const tripoint_bub_ms& pos, const itype_id& product) -> std::vector<item*> {
    return get_map().i_at(pos)
         | std::views::filter([&](const item* it) { return it->typeId() == product; })
         | std::ranges::to<std::vector>();
}

/// The player sees "(rotten)" in the ingredient list and eats half the kcal when rot exceeds 1.
auto check_not_made_of_rotten_food(const item& food, const bool result_is_fresh) -> void {
    CAPTURE(food.typeId().str());
    CHECK(std::isfinite(food.get_relative_rot()));
    if (result_is_fresh) {
        CHECK_FALSE(food.rotten());
        CHECK(food.get_relative_rot() <= 1.0);
    }
    CHECK(food.components_to_string().find("(rotten)") == std::string::npos);
    for (const auto* component : food.get_components()) {
        CAPTURE(component->typeId().str());
        CHECK(std::isfinite(component->get_relative_rot()));
        CHECK_FALSE(component->rotten());
    }
}

auto finish_recipe(const recipe_id& id, std::vector<detached_ptr<item>> ingredients) -> item& {
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
    "Smoking rack and mill output is not made of rotten food",
    "[rot][open_issue][smoking][issue_10689][issue_1499][!mayfail]") {
    const auto state = rot_test_state(1_hours);
    const auto start = calendar::turn;
    auto& here = get_map();
    const auto runs = machine_runs();
    REQUIRE_FALSE(runs.empty());
    for (const auto& run : runs) {
        CAPTURE(run.furniture, run.input.str());
        calendar::turn = start;
        here.i_clear(station);

        // Mirrors iexamine::smoker_activate and mill_activate: food is flagged and a fake item
        // times the process.
        here.furn_set(station, furn_str_id(run.furniture));
        auto food = item::spawn(run.input, calendar::turn);
        food->set_flag(flag_id("PROCESSING"));
        here.add_item(station, std::move(food));
        auto timer = item::spawn(itype_id(run.fake), calendar::turn);
        timer->activate();
        timer->set_counter(to_turns<int>(6_hours));
        here.add_item(station, std::move(timer));

        calendar::turn += 6_hours;
        here.process_items();

        const auto products = products_at(station, run.product);
        REQUIRE_FALSE(products.empty());
        CHECK(products_at(station, run.input).empty());
        for (const auto later : {0_hours, 12_hours}) {
            CAPTURE(to_hours<int>(later));
            calendar::turn = start + 6_hours + later;
            for (const auto* product : products) {
                check_not_made_of_rotten_food(*product, later == 0_hours);
            }
        }
    }
}

TEST_CASE(
    "Preserved food recipes do not list rotten ingredients",
    "[rot][open_issue][crafting][issue_10757][!mayfail]") {
    const auto state = rot_test_state(1_hours);
    const auto recipes = restore_on_out_of_scope(
        const_cast<recipe_subset&>(get_avatar().get_learned_recipes()));
    const auto [recipe_name, inputs] = GENERATE(table<std::string, std::vector<std::string>>({
        {"potted_meat", {"lard", "meat", "salt"}},
        {"dry_veggy", {"veggy"}},
        {"dry_meat", {"meat"}},
        {"sausage_wasteland_smoke", {"sausage_wasteland_raw"}},
    }));
    const auto elapsed = GENERATE(0_days, 4_days);
    CAPTURE(recipe_name, to_days<int>(elapsed));
    auto ingredients =
        inputs | std::views::transform([](const auto& id) {
            return item::spawn(itype_id(id), calendar::turn);
        })
        | std::ranges::to<std::vector>();

    auto& food = finish_recipe(recipe_id(recipe_name), std::move(ingredients));
    calendar::turn += elapsed;
    REQUIRE_FALSE(food.get_components().empty());
    CHECK(food.components_to_string().find("(rotten)") == std::string::npos);
    for (const auto* component : food.get_components()) {
        CAPTURE(component->typeId().str());
        CHECK(std::isfinite(component->get_relative_rot()));
        CHECK_FALSE(component->rotten());
        if (!component->goes_bad()) { CHECK(component->get_relative_rot() == 0.0); }
    }
}

TEST_CASE(
    "A nonperishable ingredient is never listed as rotten",
    "[rot][open_issue][ingredients][issue_10757][!mayfail]") {
    const auto state = rot_test_state(1_hours);
    const auto start = calendar::turn;
    const auto ids = nonperishable_ids();
    REQUIRE_FALSE(ids.empty());
    for (const auto& ingredient_id : ids) {
        CAPTURE(ingredient_id);
        auto owner = item::spawn("test_rot_food");
        owner->add_component(item::spawn(itype_id(ingredient_id), start));
        for (const auto elapsed : {0_days, 22_days}) {
            CAPTURE(to_days<int>(elapsed));
            calendar::turn = start + elapsed;
            const auto& component = *owner->get_components().front();
            CHECK(std::isfinite(component.get_relative_rot()));
            CHECK(component.get_relative_rot() == 0.0);
            CHECK_FALSE(component.rotten());
            CHECK(owner->components_to_string().find("(rotten)") == std::string::npos);
        }
    }
}

TEST_CASE(
    "Storage information for perishable food follows the item's location",
    "[rot][open_issue][info][issue_2362][!mayfail]") {
    const auto state = rot_test_state(1_hours);
    const auto text = [](const std::vector<iteminfo>& info) {
        auto joined = std::string();
        for (const auto& entry : info) { joined += entry.sName; }
        return joined;
    };
    const auto has = [](const std::string& haystack, const std::string& part) {
        return remove_color_tags(haystack).find(part) != std::string::npos;
    };

    SECTION("food on a tile describes that tile's protection") {
        const auto [furniture, protection] = GENERATE(table<std::string, std::string>({
            {"f_minifreezer_on", "Current storage conditions fully protect"},
            {"f_fridge_on", "Current storage conditions partially protect"},
            {"f_null", "Current storage conditions do not protect"},
        }));
        CAPTURE(furniture);
        auto& here = get_map();
        here.furn_set(station, furn_str_id(furniture));
        here.add_item(station, item::spawn("test_rot_food", calendar::turn));
        const auto& food = here.i_at(station).only_item();
        // Advanced inventory and inventory selectors call info() without a temperature.
        CHECK(has(text(food.info()), protection));
        CHECK(has(food.info_string(), protection));
    }

    SECTION("food that is nowhere yet has no storage line") {
        const auto food = item::spawn("test_rot_food", calendar::turn);
        CHECK_FALSE(has(text(food->info()), "Current storage conditions"));
    }
}

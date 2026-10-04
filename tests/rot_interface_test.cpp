#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "crafting.h"
#include "debug.h"
#include "item.h"
#include "iteminfo_query.h"
#include "map/map.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"
#include "state_helpers.h"
#include "units_temperature.h"
#include "weather/weather.h"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

struct rot_interface_state {
    restore_on_out_of_scope<time_point> time{calendar::turn};

    rot_interface_state() {
        clear_all_state();
        calendar::turn = calendar::start_of_cataclysm + 91_days;
        get_weather().temperature = 18_c;
        get_weather().clear_temp_cache();
    }
    ~rot_interface_state() { clear_all_state(); }
};

auto fresh_food() -> detached_ptr<item> {
    auto food = item::spawn("test_rot_food", calendar::turn);
    food->set_rot(0_turns);
    return food;
}

auto freshness_info(item& food) -> std::string {
    const auto query = iteminfo_query(
        std::vector<iteminfo_parts>{iteminfo_parts::FOOD_ROT, iteminfo_parts::FOOD_ROT_STORAGE});
    return food.info_string(query, 1, temperature_flag::TEMP_NORMAL);
}

} // namespace

TEST_CASE("Food freshness labels match how much the food has spoiled", "[rot][interface]") {
    const auto state = rot_interface_state();
    auto food = fresh_food();

    CHECK(food->goes_bad());
    CHECK(food->get_shelf_life() == 1_days);
    CHECK(food->get_relative_rot() == 0.0);
    CHECK(food->is_fresh());
    CHECK_FALSE(food->is_going_bad());
    CHECK_FALSE(food->rotten());

    food->set_rot(1_hours);
    CHECK(food->get_rot() == 1_hours);
    food->mod_rot(1_hours);
    CHECK(food->get_rot() == 2_hours);
    food->set_relative_rot(0.5);
    CHECK(food->get_relative_rot() == Approx(0.5));
    CHECK_FALSE(food->is_fresh());
    CHECK_FALSE(food->is_going_bad());
    CHECK_FALSE(food->rotten());

    food->set_relative_rot(0.1);
    CHECK_FALSE(food->is_fresh());
    food->set_relative_rot(0.9);
    CHECK_FALSE(food->is_going_bad());
    food->set_relative_rot(0.9001);
    CHECK(food->is_going_bad());
    food->set_relative_rot(1.0);
    CHECK_FALSE(food->rotten());
    food->set_relative_rot(1.0001);
    CHECK(food->rotten());

    auto component = item::spawn(*food);
    component->set_flag(flag_id("COMPONENT"));
    component->set_rot(component->get_shelf_life() / 2);
    CHECK(component->get_relative_rot() == Approx(0.5));

    auto nonperishable = item::spawn("test_rot_nonperishable");
    nonperishable->set_rot(2_days);
    nonperishable->set_relative_rot(0.5);
    CHECK_FALSE(nonperishable->goes_bad());
    CHECK(nonperishable->get_shelf_life() == 0_turns);
    CHECK(nonperishable->get_relative_rot() == 0.0);
    CHECK_FALSE(nonperishable->is_fresh());
    CHECK_FALSE(nonperishable->is_going_bad());
    CHECK_FALSE(nonperishable->rotten());

    auto processing = fresh_food();
    processing->set_flag(flag_id("PROCESSING"));
    CHECK_FALSE(processing->goes_bad());
    CHECK_FALSE(processing->is_fresh());
    processing->mod_last_rot_check(1_hours);

    const auto diagnostic = capture_debugmsg_during([&]() { food->mod_last_rot_check(1_hours); });
    CHECK(diagnostic.find("non smoking item") != std::string::npos);
}

TEST_CASE(
    "Smoked or milled food ages from when it was made, not when it was checked",
    "[rot][interface]") {
    const auto state = rot_interface_state();
    const auto processing_result = GENERATE(false, true);
    auto food = fresh_food();
    if (processing_result) { food->set_flag(flag_id("PROCESSING_RESULT")); }
    calendar::turn += 20_minutes;
    food->set_relative_rot(0.5);
    CHECK(food->get_rot() == 12_hours + (processing_result ? 20_minutes : 0_turns));
}

TEST_CASE("An unfinished recipe does not preserve food in an open container", "[rot][interface]") {
    const auto state = rot_interface_state();
    auto& here = get_map();
    const auto pos = tripoint_bub_ms(65, 65, 0);
    auto food = fresh_food();
    food->set_relative_rot(3.0);
    auto container = item::spawn("test_rot_open");
    container->put_in(std::move(food));
    auto* const material = container.get();
    auto components = std::vector<detached_ptr<item>>();
    components.push_back(std::move(container));
    auto craft = item::
        spawn(&recipe_id("meat_cooked").obj(), 1, std::move(components), std::vector<item_comp>{});
    here.add_item(pos, std::move(craft));
    material->prepare_for_location_removal();
    CHECK(material->contents.empty());
}

TEST_CASE("Missing container data does not crash food removal", "[rot][boundary]") {
    const auto state = rot_interface_state();
    auto& here = get_map();
    const auto pos = tripoint_bub_ms(65, 65, 0);
    here.set_temperature(pos, 0);
    auto parent = item::spawn("test_rot_open");
    auto child = item::spawn("test_rot_open");
    child->put_in(fresh_food());
    auto* const nested = child.get();
    parent->put_in(std::move(child));
    auto* const ancestor = parent.get();
    here.add_item(pos, std::move(parent));
    const auto restore_type = restore_on_out_of_scope(ancestor->type);
    // Preserve the same invalid-type recovery contract as rot actualization.
    // The child and its location remain valid while ancestor metadata is unavailable.
    ancestor->type = nullptr;
    calendar::turn += 20_minutes;
    nested->prepare_for_location_removal();
    REQUIRE(nested->contents.num_item_stacks() == 1);
    CHECK(nested->contents.front().get_rot() == 20_minutes);
}

TEST_CASE("Food storage affects when it spoils and how it is sorted", "[rot][interface]") {
    const auto state = rot_interface_state();
    const auto bottom = std::numeric_limits<int>::max();

    auto plain_food = fresh_food();
    CHECK_FALSE(plain_food->goes_bad_after_opening(true));
    CHECK(plain_food->goes_bad_after_opening());

    auto generic = item::spawn("test_rot_remains");
    CHECK_FALSE(generic->goes_bad_after_opening());

    auto open = item::spawn("test_rot_open");
    CHECK_FALSE(open->goes_bad_after_opening(true));
    CHECK_FALSE(open->goes_bad_after_opening());
    CHECK(open->spoilage_sort_order() == bottom);
    open->put_in(item::spawn("test_rot_food"));
    CHECK_FALSE(open->goes_bad_after_opening(true));
    CHECK_FALSE(open->goes_bad_after_opening());

    auto preserving_empty = item::spawn("test_rot_preserving");
    CHECK_FALSE(preserving_empty->goes_bad_after_opening(true));
    CHECK_FALSE(preserving_empty->goes_bad_after_opening());
    auto preserving_nonperishable = item::spawn("test_rot_preserving");
    preserving_nonperishable->put_in(item::spawn("test_rot_nonperishable"));
    CHECK_FALSE(preserving_nonperishable->goes_bad_after_opening(true));
    CHECK_FALSE(preserving_nonperishable->goes_bad_after_opening());

    auto preserving = item::spawn("test_rot_preserving");
    preserving->put_in(item::spawn("test_rot_food"));
    CHECK(preserving->goes_bad_after_opening(true));
    CHECK(preserving->goes_bad_after_opening());
    CHECK(preserving->spoilage_sort_order() == bottom - 3);

    auto sealed = item::spawn("test_rot_sealed");
    sealed->put_in(item::spawn("test_rot_open"));

    auto food = fresh_food();
    food->set_rot(2_hours);
    CHECK(food->spoilage_sort_order() == to_turns<int>(1_days - 2_hours));

    auto food_container = item::spawn("test_rot_open");
    food_container->put_in(std::move(food));
    CHECK(food_container->spoilage_sort_order() == to_turns<int>(1_days - 2_hours));

    CHECK(item::spawn("test_rot_nonperishable")->spoilage_sort_order() == bottom - 3);
    CHECK(item::spawn("stick_fiber")->spoilage_sort_order() == bottom - 1);
    CHECK(item::spawn("aspirin")->spoilage_sort_order() == bottom - 2);
    CHECK(item::spawn("test_rot_remains")->spoilage_sort_order() == bottom);

    auto rotten_food = fresh_food();
    rotten_food->set_relative_rot(2.01);
    CHECK(rotten_food->has_rotten_away());
    rotten_food->set_relative_rot(2.0);
    CHECK_FALSE(rotten_food->has_rotten_away());
    auto corpse = item::make_corpse(mtype_id("mon_test_rot_corpse"), calendar::turn);
    corpse->set_rot(10_days + 1_turns);
    CHECK(corpse->has_rotten_away());
    auto reviver = item::make_corpse(mtype_id("mon_test_rot_reviver"), calendar::turn);
    reviver->set_rot(10_days + 1_turns);
    CHECK_FALSE(reviver->has_rotten_away());
}

TEST_CASE(
    "Food descriptions explain its freshness based on the player's cooking skill",
    "[rot][freshness]") {
    const auto state = rot_interface_state();
    auto& you = get_avatar();
    you.clear_skills();

    auto food = fresh_food();
    const auto fresh_info = freshness_info(*food);
    CAPTURE(fresh_info);
    CHECK(fresh_info.find("fresh</color> as") != std::string::npos);
    food->set_relative_rot(0.95);
    CHECK(freshness_info(*food).find("brink") != std::string::npos);
    food->set_relative_rot(0.5);
    CHECK(freshness_info(*food).find("fine") != std::string::npos);

    you.set_skill_level(skill_id("cooking"), 3);
    for (const auto progress : {-0.1, 0.05}) {
        food->set_relative_rot(progress);
        CHECK(freshness_info(*food).find("still has") != std::string::npos);
    }
    for (const auto progress : {0.2, 0.4, 0.6, 0.8}) {
        food->set_relative_rot(progress);
        const auto info = freshness_info(*food);
        CHECK(
            info.find(
                progress < 0.3   ? "quite fresh"
                : progress < 0.5 ? "midlife"
                : progress < 0.7
                    ? "passed its midlife"
                    : "will be old soon")
            != std::string::npos);
    }
    food->set_relative_rot(0.95);
    CHECK(freshness_info(*food).find("just") != std::string::npos);
}

TEST_CASE(
    "Combining food portions averages their spoilage according to quantity", "[rot][interface]") {
    const auto state = rot_interface_state();

    auto first = item::spawn("test_rot_food", calendar::turn, 2);
    auto second = item::spawn("test_rot_food", calendar::turn, 1);
    first->set_rot(2_hours);
    second->set_rot(6_hours);
    REQUIRE(first->merge_charges(std::move(second), true));
    CHECK(first->charges == 3);
    CHECK(first->get_rot() == 3_hours + 20_minutes);

    auto empty_first = item::spawn("test_rot_food", calendar::turn, 0);
    auto empty_second = item::spawn("test_rot_food", calendar::turn, 0);
    empty_first->set_rot(2_hours);
    empty_second->set_rot(6_hours);
    REQUIRE(empty_first->merge_charges(std::move(empty_second), true));
    CHECK(empty_first->get_rot() == 0_turns);
}

TEST_CASE("Cooked food is as spoiled as its most spoiled ingredient", "[rot][crafting]") {
    const auto state = rot_interface_state();
    const auto recipes = restore_on_out_of_scope(
        const_cast<recipe_subset&>(get_avatar().get_learned_recipes()));
    const auto recipe_name = recipe_id("meat_cooked");
    const auto first_rot = GENERATE(0.25, 0.75);
    auto first_component = item::spawn("test_rot_food");
    first_component->set_relative_rot(first_rot);
    auto second_component = item::spawn("test_rot_food");
    second_component->set_relative_rot(1.0 - first_rot);
    auto components = std::vector<detached_ptr<item>>();
    components.push_back(std::move(first_component));
    components.push_back(std::move(second_component));
    components.push_back(item::spawn("test_rot_nonperishable"));

    auto craft =
        item::spawn(&recipe_name.obj(), 1, std::move(components), std::vector<item_comp>{});
    REQUIRE(craft->get_components().size() == 3);
    CHECK(craft->get_components().front()->minimum_freshness_duration(temperature_flag::TEMP_NORMAL)
          < calendar::INDEFINITELY_LONG_DURATION);
    auto& you = get_avatar();
    complete_craft(you, *craft);
    const auto results = you.items_with([](const item& it) {
        return it.typeId() == itype_id("meat_cooked");
    });
    REQUIRE(results.size() == 1);
    CHECK(results.front()->get_relative_rot() == Approx(0.75));

    // Test the empty-input boundary independently of configurable stack merging.
    you.inv_clear();
    auto component_free_craft = item::
        spawn(&recipe_name.obj(), 1, std::vector<detached_ptr<item>>{}, std::vector<item_comp>{});
    complete_craft(you, *component_free_craft);
    const auto fresh_results = you.items_with([](const auto& it) {
        return it.typeId() == itype_id("meat_cooked");
    });
    REQUIRE(fresh_results.size() == 1);
    CHECK(fresh_results.front()->get_relative_rot() == 0.0);
}

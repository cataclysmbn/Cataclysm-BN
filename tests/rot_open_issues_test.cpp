#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "crafting.h"
#include "debug.h"
#include "flag.h"
#include "game.h"
#include "harvest.h"
#include "item.h"
#include "item_factory.h"
#include "item_group.h"
#include "itype.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/mapbuffer_registry.h"
#include "map/submap.h"
#include "mapgen/mapgen_constructor.h"
#include "mongroup.h"
#include "monstergenerator.h"
#include "mtype.h"
#include "options_helpers.h"
#include "output.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "state_helpers.h"
#include "type_id.h"
#include "weather/weather.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <string>
#include <vector>

// Regression tests for the open rot-related issues.
// Each test names the issue it comes from, so a still-failing test points at an unfixed issue.

namespace {

struct open_issue_state {
    restore_on_out_of_scope<time_point> time{calendar::turn};
    restore_on_out_of_scope<units::temperature> temperature{get_weather().temperature};

    open_issue_state() {
        clear_all_state();
        calendar::turn = calendar::start_of_cataclysm + 1_hours;
        get_weather().temperature = 18_c;
        get_weather().clear_temp_cache();
    }
    ~open_issue_state() {
        clear_all_state();
        get_weather().clear_temp_cache();
    }
};

constexpr auto station = tripoint_bub_ms(65, 65, 0);

auto comestible_ids(const auto& wanted) -> std::vector<std::string> {
    auto ids = std::vector<std::string>();
    for (const auto* type : item_controller->all()) {
        if (type->comestible && wanted(*type)) { ids.push_back(type->get_id().str()); }
    }
    std::ranges::sort(ids);
    return ids;
}

auto smokable_ids() -> std::vector<std::string> {
    return comestible_ids([](const itype& type) {
        return type.has_flag(flag_id("SMOKABLE")) && !type.comestible->smoking_result.is_empty();
    });
}

auto millable_ids() -> std::vector<std::string> {
    return item_controller->all() | std::views::filter([](const itype* type) {
               return static_cast<bool>(type->milling_data);
           })
         | std::views::transform([](const itype* type) { return type->get_id().str(); })
         | std::ranges::to<std::vector>();
}

auto nonperishable_ids() -> std::vector<std::string> {
    return comestible_ids([](const itype& type) {
        return !item::spawn(type.get_id(), calendar::start_of_cataclysm)->goes_bad();
    });
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
    "Smoking rack output is not made of rotten food",
    "[rot][open_issue][smoking][issue_10689][issue_1499]") {
    const auto state = open_issue_state();
    const auto start = calendar::turn;
    auto& here = get_map();
    const auto ids = smokable_ids();
    REQUIRE_FALSE(ids.empty());
    for (const auto& input_id : ids) {
        CAPTURE(input_id);
        calendar::turn = start;
        here.i_clear(station);
        const auto& smoking_result = itype_id(input_id)->comestible->smoking_result;

        // Mirrors iexamine::smoker_activate: food is flagged and a plume times the process.
        here.furn_set(station, furn_str_id("f_smoking_rack_active"));
        auto food = item::spawn(itype_id(input_id), calendar::turn);
        food->set_flag(flag_id("PROCESSING"));
        here.add_item(station, std::move(food));
        auto plume = item::spawn("fake_smoke_plume", calendar::turn);
        plume->activate();
        plume->set_counter(to_turns<int>(6_hours));
        here.add_item(station, std::move(plume));

        calendar::turn += 6_hours;
        here.process_items();

        const auto products = products_at(station, smoking_result);
        REQUIRE_FALSE(products.empty());
        CHECK(products_at(station, itype_id(input_id)).empty());
        for (const auto later : {0_hours, 12_hours}) {
            CAPTURE(to_hours<int>(later));
            calendar::turn = start + 6_hours + later;
            for (const auto* product : products) {
                check_not_made_of_rotten_food(*product, later == 0_hours);
            }
        }
    }
}

TEST_CASE("Mill output is not made of rotten food", "[rot][open_issue][smoking][issue_10689]") {
    const auto state = open_issue_state();
    const auto start = calendar::turn;
    auto& here = get_map();
    const auto ids = millable_ids();
    REQUIRE_FALSE(ids.empty());
    for (const auto& input_id : ids) {
        CAPTURE(input_id);
        calendar::turn = start;
        here.i_clear(station);
        const auto& product_id = itype_id(input_id)->milling_data->into_;

        here.furn_set(station, furn_str_id("f_wind_mill_active"));
        auto grain = item::spawn(itype_id(input_id), calendar::turn);
        grain->set_flag(flag_id("PROCESSING"));
        here.add_item(station, std::move(grain));
        auto fake = item::spawn("fake_milling_item", calendar::turn);
        fake->activate();
        fake->set_counter(to_turns<int>(6_hours));
        here.add_item(station, std::move(fake));

        calendar::turn += 6_hours;
        here.process_items();

        const auto products = products_at(station, product_id);
        REQUIRE_FALSE(products.empty());
        for (const auto* product : products) { check_not_made_of_rotten_food(*product, true); }
    }
}

TEST_CASE(
    "Preserved food recipes do not list rotten ingredients",
    "[rot][open_issue][crafting][issue_10757]") {
    const auto state = open_issue_state();
    const auto recipes = restore_on_out_of_scope(
        const_cast<recipe_subset&>(get_avatar().get_learned_recipes()));
    const auto recipe_name =
        GENERATE("potted_meat", "dry_veggy", "dry_meat", "sausage_wasteland_smoke");
    const auto elapsed = GENERATE(0_days, 4_days);
    CAPTURE(recipe_name, to_days<int>(elapsed));
    auto ingredients = std::vector<detached_ptr<item>>();
    const auto add = [&](const char* id) {
        ingredients.push_back(item::spawn(itype_id(id), calendar::turn));
    };
    const auto name = std::string_view(recipe_name);
    if (name == "potted_meat") {
        add("lard");
        add("meat");
        add("salt");
    } else if (name == "dry_veggy") {
        add("veggy");
    } else if (name == "dry_meat") {
        add("meat");
    } else {
        add("sausage_wasteland_raw");
    }

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
    "[rot][open_issue][ingredients][issue_10757]") {
    const auto state = open_issue_state();
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
    "Decayed zombie corpses leave no zombie pheromone", "[rot][open_issue][corpse][issue_5296]") {
    const auto state = open_issue_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    auto&& monsters = MonsterGenerator::generator().get_all_mtypes();
    auto dropping_pheromone = std::vector<mtype_id>();
    for (const auto& type : monsters) {
        const auto& entries = type.harvest.obj();
        const auto drops = std::ranges::any_of(entries, [](const auto& entry) {
            return entry.drop == "pheromone";
        });
        if (drops) { dropping_pheromone.push_back(type.id); }
    }
    REQUIRE_FALSE(dropping_pheromone.empty());

    auto& here = get_map();
    const auto pos = tripoint_bub_ms(60, 60, 0);
    auto& buffer = MAPBUFFER_REGISTRY.get(here.get_bound_dimension());
    const auto sm_pos = project_to<coords::sm>(bub_to_abs(pos));
    auto* sm = buffer.lookup_submap_in_memory(sm_pos);
    REQUIRE(sm != nullptr);
    for (const auto& id : dropping_pheromone) {
        CAPTURE(id.str());
        for (const auto ignored : std::views::iota(0, 40)) {
            static_cast<void>(ignored);
            auto corpse = item::make_corpse(id, calendar::turn);
            corpse->set_relative_rot(11.0);
            here.add_item(pos, std::move(corpse));
        }
        sm->last_touched = calendar::turn - 1_turns;
        buffer.actualize_submap(sm_pos);
        CHECK(std::ranges::none_of(here.i_at(pos), [](const item* it) {
            return it->typeId() == itype_id("pheromone");
        }));
        here.i_clear(pos);
    }
}

TEST_CASE(
    "Perishable items placed by mapgen are fresh when the map is generated",
    "[rot][open_issue][mapgen][issue_9834]") {
    const auto state = open_issue_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto global_spawn_rate = override_option("ITEM_SPAWNRATE", "1.0");
    const auto food_spawn_rate = override_option("SPAWN_RATE_food", "1.0");
    const auto perishable_spawn_rate = override_option("SPAWN_RATE_perishables", "1.0");
    auto& buffer = MAPBUFFER_REGISTRY.get(mapbuffer_registry::primary_dimension_id());
    auto tm = mapgen_constructor(buffer);
    tm.reset_scratch_omt(
        tripoint_abs_omt(11, 13, 0), ter_id("t_floor"), furn_id("f_null"), trap_id("tr_null"));
    const auto group = GENERATE("razorclaw_nest", "razorclaw_roe");
    CAPTURE(group);
    for (const auto ignored : std::views::iota(0, 20)) {
        static_cast<void>(ignored);
        tm.place_items(item_group_id(group), 100, point_omt_ms(2, 2), point_omt_ms(20, 20), true,
                       calendar::turn, 0, 0);
    }
    auto roe = std::vector<item*>();
    for (const auto x : std::views::iota(2, 21)) {
        for (const auto y : std::views::iota(2, 21)) {
            for (auto* const it : tm.i_at(point_omt_ms(x, y))) {
                if (it->typeId() == itype_id("razorclaw_roe")) { roe.push_back(it); }
            }
        }
    }
    REQUIRE_FALSE(roe.empty());
    for (const auto* egg : roe) {
        CHECK_FALSE(egg->rotten());
        CHECK(egg->get_rot() == 0_turns);
    }
}

TEST_CASE(
    "Eating from a container reports no location error and leaves the container usable",
    "[rot][open_issue][container][issue_10683]") {
    const auto state = open_issue_state();
    const auto container_food = GENERATE("can_beans", "can_peach", "jar_pickle_glass");
    CAPTURE(container_food);
    if (!itype_id(container_food).is_valid()) { return; }
    auto& you = get_avatar();
    you.set_stored_kcal(you.max_stored_kcal() / 2);
    auto packed = item::in_its_container(item::spawn(itype_id(container_food), calendar::turn));
    auto& container = you.i_add(std::move(packed));
    const auto messages = capture_debugmsg_during([&]() { you.consume(container); });
    CHECK(messages.empty());
}

TEST_CASE(
    "Using up part of a sealed container's food unseals it",
    "[rot][open_issue][container][issue_3588]") {
    const auto state = open_issue_state();
    const auto sealed = GENERATE("jar_glass_sealed", "can_medium");
    CAPTURE(sealed);
    auto container = item::spawn(itype_id(sealed), calendar::turn);
    REQUIRE(container->is_non_resealable_container());
    auto food = item::spawn("can_beans", calendar::turn, 2);
    container->put_in(std::move(food));
    const auto unsealed = container->type->container->unseals_into;
    REQUIRE(container->typeId() != unsealed);
    auto& you = get_avatar();
    you.set_stored_kcal(you.max_stored_kcal() / 2);
    auto& held = you.i_add(std::move(container));
    you.consume(held);
    CHECK(held.typeId() == unsealed);
}

TEST_CASE(
    "Storage information for perishable food follows the item's location",
    "[rot][open_issue][info][issue_2362]") {
    const auto state = open_issue_state();
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

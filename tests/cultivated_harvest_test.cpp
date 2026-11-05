#include "activity_handlers.h"
#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "crafting.h"
#include "cultivated_harvest.h"
#include "flag.h"
#include "iexamine.h"
#include "item.h"
#include "map/map.h"
#include "player_activity.h"
#include "recipe.h"
#include "requirements.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE("drying_mushrooms_preserves_their_effects", "[farming][crafting][food]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    auto& who = get_avatar();
    who.set_mutation(trait_id("DEBUG_STORAGE"));
    const auto& drying_recipe = recipe_id("dry_mushroom").obj();
    who.learn_recipe(&drying_recipe);

    const auto mushroom_id = GENERATE("mushroom", "mushroom_morel");
    const auto poisonous = GENERATE(false, true);
    const auto hallucinogenic = GENERATE(false, true);
    CAPTURE(mushroom_id, poisonous, hallucinogenic);
    auto mushroom = item::spawn(mushroom_id, calendar::turn, 1);
    if (poisonous) {
        mushroom->set_flag(flag_HIDDEN_POISON);
        mushroom->poison = 5;
    }
    if (hallucinogenic) { mushroom->set_flag(flag_HIDDEN_HALLU); }

    SECTION("dehydrator_or_charcoal_smoker_recipe") {
        auto components = std::vector<detached_ptr<item>>{};
        components.push_back(std::move(mushroom));
        auto craft =
            item::spawn(&drying_recipe, 1, std::move(components), std::vector<item_comp>{});
        complete_craft(who, *craft);
        const auto is_dried = [](const auto& i) { return i.typeId() == itype_id("dry_mushroom"); };
        const auto products = who.items_with(is_dried);
        REQUIRE(products.size() == 1);
        CHECK(products.front()->has_flag(flag_HIDDEN_POISON) == poisonous);
        CHECK(products.front()->has_flag(flag_HIDDEN_HALLU) == hallucinogenic);
        CHECK(products.front()->poison == (poisonous ? 5 : 0));
    }

    SECTION("smoking_rack") {
        auto& here = get_map();
        const auto pos = tripoint_bub_ms(60, 60, 0);
        here.furn_set(pos, furn_str_id("f_smoking_rack_active"));
        here.add_item(pos, std::move(mushroom));
        iexamine::on_smoke_out(pos, calendar::turn);
        REQUIRE(here.i_at(pos).size() == 1);
        const auto& product = here.i_at(pos).only_item();
        CHECK(product.typeId() == itype_id("dry_mushroom"));
        CHECK(product.has_flag(flag_HIDDEN_POISON) == poisonous);
        CHECK(product.has_flag(flag_HIDDEN_HALLU) == hallucinogenic);
        CHECK(product.poison == (poisonous ? 5 : 0));
    }
}

TEST_CASE("harvest_inheritance_is_configured_by_seed_json", "[farming]") {
    const auto seed_id = itype_id(GENERATE(
        "test_harvest_default", "test_harvest_flags", "test_harvest_poison", "test_harvest_both"));
    CAPTURE(seed_id);
    const auto inherit_flags =
        seed_id == itype_id("test_harvest_flags") || seed_id == itype_id("test_harvest_both");
    const auto inherit_poison =
        seed_id == itype_id("test_harvest_poison") || seed_id == itype_id("test_harvest_both");
    auto seed = item::spawn(seed_id);
    seed->set_flag(flag_HIDDEN_ITEM);
    seed->set_flag(flag_HIDDEN_HALLU);
    seed->poison = 7;

    const auto byproducts = GENERATE(false, true);
    const auto harvest = get_cultivated_harvest(*seed, {.byproducts = byproducts});
    REQUIRE(harvest.size() == (byproducts ? 3 : 2));
    for (const auto& product : harvest) {
        const auto is_byproduct = product->typeId() == itype_id("test_rag");
        CHECK(product->has_flag(flag_HIDDEN_ITEM) == (inherit_flags && !is_byproduct));
        CHECK_FALSE(product->has_flag(flag_HIDDEN_HALLU));
        CHECK(product->poison == (inherit_poison && !is_byproduct ? 7 : 0));
    }
}

TEST_CASE("mushroom_cultures_preserve_the_source_mushroom_traits", "[farming][crafting]") {
    const auto variant = GENERATE(0, 1, 2);
    CAPTURE(variant);
    auto mushroom = item::spawn("mushroom");
    if (variant == 1) {
        mushroom->set_flag(flag_HIDDEN_POISON);
        mushroom->poison = 5;
    } else if (variant == 2) {
        mushroom->set_flag(flag_HIDDEN_HALLU);
    }
    auto culture = item::spawn("mushroom_culture");
    culture->inherit_flags(*mushroom, recipe_id("mushroom_culture").obj());
    culture->set_flag(flag_HIDDEN_ITEM);

    const auto byproducts = GENERATE(false, true);
    auto harvest = get_cultivated_harvest(
        *culture, {.plant_count = 1, .seed_count = 1, .byproducts = byproducts});
    REQUIRE(harvest.size() == 2);
    for (const auto& product : harvest) {
        CHECK(product->has_flag(flag_HIDDEN_POISON) == (variant == 1));
        CHECK(product->has_flag(flag_HIDDEN_HALLU) == (variant == 2));
        CHECK_FALSE(product->has_flag(flag_HIDDEN_ITEM));
        if (variant == 1) { CHECK(product->poison == 5); }
        if (product->is_seed()) {
            const auto next_harvest = get_cultivated_harvest(*product, {});
            for (const auto& next_product : next_harvest) {
                CHECK(next_product->has_flag(flag_HIDDEN_POISON) == (variant == 1));
                CHECK(next_product->has_flag(flag_HIDDEN_HALLU) == (variant == 2));
                if (variant == 1) { CHECK(next_product->poison == 5); }
            }
        }
    }
}

TEST_CASE("planted_mushroom_cultures_retain_their_traits_when_harvested", "[farming]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    auto& who = get_avatar();
    auto& here = get_map();
    const auto pos = tripoint_bub_ms(60, 60, 0);
    const auto trait = GENERATE(flag_HIDDEN_POISON, flag_HIDDEN_HALLU);
    auto culture = item::spawn("mushroom_culture");
    culture->set_flag(trait);
    culture->poison = trait == flag_HIDDEN_POISON ? 5 : 0;
    who.i_add(std::move(culture));
    here.furn_set(pos, furn_str_id("f_rack_mushroom"));

    iexamine::plant_seed(who, pos, itype_id("mushroom_culture"));
    activity_handlers::plant_seed_finish(who.activity.get(), &who);
    REQUIRE(here.i_at(pos).size() == 1);
    CHECK(here.i_at(pos).only_item().has_flag(trait));

    here.furn_set(pos, furn_str_id("f_rack_mushroom_harvest"));
    iexamine::harvest_plant(who, pos);
    REQUIRE(here.i_at(pos).size() == 2);
    CHECK(here.furn(pos).id() == furn_str_id("f_rack_mushroom"));
    for (const auto* product : here.i_at(pos)) {
        CHECK(product->has_flag(trait));
        CHECK_FALSE(product->has_flag(flag_HIDDEN_ITEM));
        if (trait == flag_HIDDEN_POISON) { CHECK(product->poison == 5); }
    }
}

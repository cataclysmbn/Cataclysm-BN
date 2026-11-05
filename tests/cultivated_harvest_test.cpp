#include "activity_handlers.h"
#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "cultivated_harvest.h"
#include "flag.h"
#include "iexamine.h"
#include "item.h"
#include "map/map.h"
#include "player_activity.h"
#include "recipe.h"
#include "state_helpers.h"
#include "type_id.h"

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

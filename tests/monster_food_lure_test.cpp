#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "item.h"
#include "map/map.h"
#include "map_helpers.h"
#include "monster.h"
#include "monster_food_lure.h"
#include "monster_plan.h"
#include "mtype.h"
#include "state_helpers.h"
#include "type_id.h"

namespace {

const auto effect_mon_feeding = efftype_id("mon_feeding");
const auto effect_well_fed = efftype_id("well_fed");

const auto trait_lupine = trait_id("THRESH_LUPINE");

/// Far from the avatar so that wild animals do not notice the player.
const auto mon_pos = tripoint_bub_ms(20, 20, 0);
const auto food_pos = mon_pos + tripoint_rel_ms(4, 0, 0);

auto prepare_daylight_map() -> void {
    clear_all_state();
    move_player_out_of_the_way();
    // Also rebuilds the light map; setting calendar::turn alone leaves the map dark.
    set_time(calendar::turn_zero + 12_hours);
}

auto place_food(const tripoint_bub_ms& pos, const std::string& id) -> void {
    get_map().add_item(pos, item::spawn(itype_id(id)));
}

auto has_item_at(const tripoint_bub_ms& pos, const std::string& id) -> bool {
    for (const item* it : get_map().i_at(pos)) {
        if (it->typeId() == itype_id(id)) { return true; }
    }
    return false;
}

/// Runs one monster action through the same plan/decide/execute path the game uses.
auto act(monster& mon) -> void {
    mon.set_moves(100);
    mon.move();
}

} // namespace

TEST_CASE("food_lure_matches_diet_pet_food_and_tamer", "[monster][food_lure]") {
    clear_all_state();
    const auto& weasel = mtype_id("mon_weasel").obj();
    const auto& cow = mtype_id("mon_cow").obj();
    const auto& dog = mtype_id("mon_dog").obj();
    const auto& wolf = mtype_id("mon_wolf").obj();

    const auto meat = item::spawn(itype_id("meat"));
    const auto bone = item::spawn(itype_id("bone"));
    const auto fodder = item::spawn(itype_id("cattlefodder"));
    const auto rock = item::spawn(itype_id("rock"));

    CHECK_FALSE(food_lure::is_domestic(weasel));
    CHECK(food_lure::is_domestic(cow));
    CHECK(food_lure::is_domestic(wolf));

    CHECK(food_lure::matches_diet(weasel, *meat));
    CHECK_FALSE(food_lure::matches_diet(weasel, *fodder));
    CHECK(food_lure::matches_diet(dog, *bone));
    CHECK_FALSE(food_lure::matches_diet(dog, *rock));

    CHECK(food_lure::matches_pet_food(cow, *fodder));
    CHECK_FALSE(food_lure::matches_pet_food(cow, *meat));
    CHECK_FALSE(food_lure::matches_pet_food(weasel, *meat));

    auto& you = get_avatar();
    CHECK_FALSE(food_lure::accepts_tamer(wolf, you));
    CHECK(food_lure::accepts_tamer(cow, you));
    you.set_mutation(trait_lupine);
    CHECK(food_lure::accepts_tamer(wolf, you));
}

TEST_CASE("wild_animal_walks_to_food_it_eats", "[monster][food_lure]") {
    prepare_daylight_map();
    auto& weasel = spawn_test_monster("mon_weasel", mon_pos);

    SECTION("food outside its diet is ignored") {
        place_food(food_pos, "cattlefodder");
        CHECK(weasel.compute_plan().goal != food_pos);
    }
    SECTION("food in its diet becomes the goal") {
        place_food(food_pos, "meat");
        CHECK(weasel.compute_plan().goal == food_pos);
    }
    SECTION("a well fed animal ignores food") {
        place_food(food_pos, "meat");
        weasel.add_effect(effect_well_fed, 1_hours);
        CHECK(weasel.compute_plan().goal != food_pos);
    }
    SECTION("food it can see is noticed up to the lure radius") {
        const auto far_food = mon_pos + tripoint_rel_ms(food_lure::lure_radius, 0, 0);
        place_food(far_food, "meat");
        CHECK(weasel.compute_plan().goal == far_food);
    }
    SECTION("food beyond the lure radius is ignored") {
        const auto too_far = mon_pos + tripoint_rel_ms(food_lure::lure_radius + 1, 0, 0);
        place_food(too_far, "meat");
        CHECK(weasel.compute_plan().goal != too_far);
    }
}

TEST_CASE("flying_animal_comes_down_to_food", "[monster][food_lure]") {
    prepare_daylight_map();
    get_map().build_map_cache(1);
    auto& turkey = spawn_test_monster("mon_turkey", mon_pos + tripoint_rel_ms(0, 0, 1));
    REQUIRE(turkey.flies());
    place_food(food_pos, "wheat");

    CHECK(turkey.compute_plan().goal == food_pos);
}

TEST_CASE("pets_only_eat_pet_food_from_a_feeder", "[monster][food_lure]") {
    prepare_daylight_map();
    auto& dog = spawn_test_monster("mon_dog", mon_pos);
    dog.make_pet();

    SECTION("pet food on the floor is ignored") {
        place_food(food_pos, "dogfood");
        CHECK(dog.compute_plan().goal != food_pos);
    }
    SECTION("diet food on a feeder is ignored") {
        get_map().furn_set(food_pos, furn_str_id("f_trough"));
        place_food(food_pos, "meat");
        CHECK(dog.compute_plan().goal != food_pos);
    }
    SECTION("pet food on a feeder becomes the goal") {
        get_map().furn_set(food_pos, furn_str_id("f_trough"));
        place_food(food_pos, "dogfood");
        CHECK(dog.compute_plan().goal == food_pos);
    }
}

TEST_CASE("eating_on_the_tile_feeds_without_taming", "[monster][food_lure]") {
    prepare_daylight_map();
    auto& weasel = spawn_test_monster("mon_weasel", mon_pos);
    place_food(mon_pos, "meat");

    act(weasel);
    REQUIRE(weasel.has_effect(effect_mon_feeding));
    CHECK(weasel.bub_pos() == mon_pos);

    SECTION("the meal finishes after the feeding time") {
        calendar::turn += food_lure::feeding_time;
        act(weasel);
        CHECK_FALSE(weasel.has_effect(effect_mon_feeding));
        CHECK(weasel.has_effect(effect_well_fed));
        CHECK_FALSE(has_item_at(mon_pos, "meat"));
        CHECK(weasel.friendly == 0);
        CHECK_FALSE(weasel.is_pet());
    }
    SECTION("getting hurt interrupts the meal") {
        weasel.apply_damage(nullptr, bodypart_id("torso"), 1);
        CHECK_FALSE(weasel.has_effect(effect_mon_feeding));
        CHECK_FALSE(weasel.has_effect(effect_well_fed));
        CHECK(has_item_at(mon_pos, "meat"));
    }
    SECTION("losing the food interrupts the meal") {
        get_map().i_clear(mon_pos);
        act(weasel);
        CHECK_FALSE(weasel.has_effect(effect_mon_feeding));
        CHECK_FALSE(weasel.has_effect(effect_well_fed));
    }
}

TEST_CASE("feeding_animals_are_distracted", "[monster][food_lure]") {
    prepare_daylight_map();
    auto& weasel = spawn_test_monster("mon_weasel", mon_pos);
    const auto calm_dodge = weasel.get_dodge();
    const auto calm_sight = weasel.sight_range(default_daylight_level());

    weasel.add_effect(effect_mon_feeding, 2_minutes);
    // Effect stat modifiers are applied when the monster processes its turn.
    weasel.process_turn();
    CHECK(weasel.get_dodge() == Approx(calm_dodge - 3.0f));
    CHECK(weasel.sight_range(default_daylight_level()) == calm_sight / 2);
}

TEST_CASE("domestic_animals_approach_held_pet_food", "[monster][food_lure]") {
    prepare_daylight_map();
    auto& you = get_avatar();
    // Moving the avatar across submaps shifts the map, so place the cow around the avatar instead.
    const auto cow_pos = you.bub_pos() + tripoint_rel_ms(-5, 0, 0);
    auto& cow = spawn_test_monster("mon_cow", cow_pos);

    SECTION("without food the cow is scared of a close player") {
        CHECK(cow.compute_plan().morale < cow.morale);
    }
    SECTION("held pet food draws the cow and calms it") {
        you.wield(item::spawn(itype_id("cattlefodder")));
        REQUIRE(food_lure::is_lured_by_held_food(cow, you));
        const auto plan = cow.compute_plan();
        CHECK(plan.goal == you.bub_pos());
        CHECK(plan.morale == cow.morale);
    }
    SECTION("food the cow does not take as pet food has no pull") {
        you.wield(item::spawn(itype_id("meat")));
        CHECK_FALSE(food_lure::is_lured_by_held_food(cow, you));
    }
    SECTION("wild animals ignore held food") {
        auto& weasel = spawn_test_monster("mon_weasel", cow_pos + tripoint_rel_ms(0, 2, 0));
        you.wield(item::spawn(itype_id("meat")));
        CHECK_FALSE(food_lure::is_lured_by_held_food(weasel, you));
    }
}

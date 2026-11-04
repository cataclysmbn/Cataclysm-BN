#include "avatar.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "mission.h"
#include "skill.h"
#include "state_helpers.h"
#include "type_id.h"

#include <algorithm>
#include <ranges>
#include <vector>

TEST_CASE("a cleared avatar lists its mutations as a fresh one would", "[player_helpers]") {
    namespace ranges = std::ranges;
    using namespace std::views;
    auto& you = get_avatar();
    // Earlier cases grow the mutation table, whose layout orders the mutations screen.
    you.my_mutations.reserve(20);
    clear_all_state();
    auto added = ranges::to<std::vector>(you.my_mutations | keys);
    for (const auto& trait :
         {trait_id("SKIN_LIGHTER"), trait_id("test_preview_trait"),
          trait_id("test_preview_active_trait")}) {
        you.set_mutation(trait);
        added.push_back(trait);
    }
    auto fresh = mutation_collection{};
    for (const auto& trait : added) { fresh.emplace(trait, char_trait_data{}); }
    CHECK(
        ranges::to<std::vector>(you.my_mutations | keys) == ranges::to<std::vector>(fresh | keys));
    clear_all_state();
}

TEST_CASE("a cleared avatar keeps no practice toward its next skill level", "[player_helpers]") {
    const auto athletics = skill_id("athletics");
    auto& you = get_avatar();
    clear_all_state();
    you.get_skill_level_object(athletics).train(25, true);
    REQUIRE(you.get_skill_level_object(athletics).exercise(true) > 0);
    clear_all_state();
    CHECK(you.get_skill_level_object(athletics).exercise(true) == 0);
}

TEST_CASE("a cleared avatar keeps no missions", "[player_helpers]") {
    auto& you = get_avatar();
    clear_all_state();
    const auto& types = mission_type::get_all();
    const auto type = std::ranges::find(types, MGOAL_ASSASSINATE, &mission_type::goal);
    REQUIRE(type != types.end());
    auto* const assigned = mission::reserve_new(type->id, you.getID());
    if (assigned->get_assigned_player_id() == you.getID()) {
        you.on_mission_assignment(*assigned);
    } else {
        assigned->assign(you);
    }
    REQUIRE(you.get_active_missions().size() == 1);
    clear_all_state();
    CHECK(you.get_active_missions().empty());
    CHECK(you.get_active_mission() == nullptr);
}

TEST_CASE("a cleared avatar keeps no vitamin deficiency", "[player_helpers]") {
    auto& you = get_avatar();
    clear_all_state();
    const auto vitamin_a = vitamin_id("vitA");
    const auto fresh = you.vitamin_get(vitamin_a);
    // Starving cases leave the avatar deficient, which blinds it at night.
    you.vitamin_set(vitamin_a, -95);
    clear_all_state();
    CHECK(you.vitamin_get(vitamin_a) == fresh);
}

TEST_CASE("a cleared avatar sees as far as a fresh one", "[player_helpers]") {
    auto& you = get_avatar();
    clear_all_state();
    const auto fresh = you.unimpaired_range();
    // A case that ends with the avatar asleep or blind leaves its sight cut short.
    you.add_effect(efftype_id("blind"), 1_hours);
    you.recalc_sight_limits();
    REQUIRE(you.unimpaired_range() == 0);
    clear_all_state();
    CHECK(you.unimpaired_range() == fresh);
}

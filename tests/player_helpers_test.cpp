#include "avatar.h"
#include "catch/catch.hpp"
#include "skill.h"
#include "state_helpers.h"
#include "type_id.h"

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

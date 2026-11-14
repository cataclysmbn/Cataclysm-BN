#include "avatar.h"
#include "catch/catch.hpp"
#include "player_activity.h"
#include "skill.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE("a cleared avatar keeps no practice toward its next skill level", "[player_helpers]") {
    const auto swimming = skill_id("swimming");
    REQUIRE(swimming.is_valid());
    auto& you = get_avatar();
    clear_all_state();
    you.get_skill_level_object(swimming).train(25, true);
    REQUIRE(you.get_skill_level_object(swimming).exercise(true) > 0);
    clear_all_state();
    CHECK(you.get_skill_level_object(swimming).exercise(true) == 0);
}

TEST_CASE("a cleared avatar keeps no index of its last activity", "[player_helpers]") {
    // `avatar::read( book, true )` copies the current index. The test avatar has ID -1, so index -1
    // turns a later "read once" into "read until you gain a level".
    auto& you = get_avatar();
    clear_all_state();
    you.assign_activity(activity_id("ACT_WAIT"), 100, -1);
    REQUIRE(you.activity->index == -1);
    clear_all_state();
    CHECK(you.activity->index == 0);
}

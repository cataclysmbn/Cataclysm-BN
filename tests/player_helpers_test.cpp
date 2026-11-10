#include "avatar.h"
#include "catch/catch.hpp"
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

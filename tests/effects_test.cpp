#include "avatar.h"
#include "catch/catch.hpp"
#include "effect.h"
#include "enchantments/enchantment.h"
#include "type_id.h"

#include <cstdlib>
#include <map>
#include <sstream>
#include <utility>

static const efftype_id effect_adrenaline("adrenaline");
static const efftype_id effect_adrenaline_comedown("adrenaline_comedown");
static const efftype_id effect_test_juggling_l1("test_juggling_l1");
static const efftype_id effect_test_juggling_l2("test_juggling_l2");
static const efftype_id effect_test_juggling_r1("test_juggling_r1");
static const efftype_id effect_test_juggling_r2("test_juggling_r2");
static const efftype_id effect_test_merged_enchantments("test_merged_enchantments");

static const enchantment_value_id enchantment_value_DEXTERITY("DEXTERITY");
static const enchantment_value_id enchantment_value_INTELLIGENCE("INTELLIGENCE");
static const enchantment_value_id enchantment_value_PERCEPTION("PERCEPTION");
static const enchantment_value_id enchantment_value_STRENGTH("STRENGTH");

TEST_CASE("Adrenaline decays into adrenaline comedown") {
    REQUIRE(effect_adrenaline.is_valid());
    REQUIRE(effect_adrenaline->get_effects_on_remove().size() == 1);
    avatar dummy;
    dummy.add_effect(effect_adrenaline, 0_turns);
    REQUIRE(dummy.has_effect(effect_adrenaline));
    dummy.process_effects();
    CHECK(!dummy.has_effect(effect_adrenaline));
    CHECK(dummy.has_effect(effect_adrenaline_comedown));
    const effect& e = dummy.get_effect(effect_adrenaline_comedown);
    auto on_remove = effect_adrenaline->get_effects_on_remove().front();
    CHECK(to_turns<int>(e.get_duration()) == to_turns<int>(on_remove.duration));
}

TEST_CASE("Removed adrenaline still triggers adrenaline comedown") {
    REQUIRE(effect_adrenaline.is_valid());
    REQUIRE(effect_adrenaline->get_effects_on_remove().size() == 1);
    avatar dummy;
    dummy.add_effect(effect_adrenaline, 100_turns);
    REQUIRE(dummy.has_effect(effect_adrenaline));
    dummy.remove_effect(effect_adrenaline);
    dummy.process_effects();
    CHECK(!dummy.has_effect(effect_adrenaline));
    REQUIRE(dummy.has_effect(effect_adrenaline_comedown));
    const effect& e = dummy.get_effect(effect_adrenaline_comedown);
    auto on_remove = effect_adrenaline->get_effects_on_remove().front();
    CHECK(to_turns<int>(e.get_duration()) == to_turns<int>(on_remove.duration));
}

TEST_CASE("Effect body part switching and inheritance on decay works as expected") {
    REQUIRE(effect_test_juggling_l1.is_valid());
    avatar dummy;
    dummy.add_effect(effect_test_juggling_l1, 0_seconds, body_part_hand_l);
    REQUIRE(dummy.has_effect(effect_test_juggling_l1));

    dummy.process_effects();
    CHECK(!dummy.has_effect(effect_test_juggling_l1));
    CHECK(dummy.has_effect(effect_test_juggling_r1, body_part_hand_r));

    dummy.process_effects();
    CHECK(!dummy.has_effect(effect_test_juggling_r1));
    CHECK(dummy.has_effect(effect_test_juggling_r2, body_part_hand_r));

    dummy.process_effects();
    CHECK(!dummy.has_effect(effect_test_juggling_r2));
    CHECK(dummy.has_effect(effect_test_juggling_l2, body_part_hand_l));

    dummy.process_effects();
    CHECK(!dummy.has_effect(effect_test_juggling_l2));
    CHECK(dummy.has_effect(effect_test_juggling_l1, body_part_hand_l));
}

TEST_CASE("Effect enchantments with matching conditions are merged on load") {
    // Enchantments raising different values still merge when their conditions match.
    REQUIRE(effect_test_merged_enchantments.is_valid());

    avatar dummy;
    dummy.add_effect(effect_test_merged_enchantments, 1_hours);
    auto& e = dummy.get_effect(effect_test_merged_enchantments);

    SECTION("base enchantments are merged into one") {
        const auto enchantments = e.get_enchantments();
        REQUIRE(enchantments.size() == 1);
        CHECK(enchantments.front().get_value_add(enchantment_value_STRENGTH) == 1);
        CHECK(enchantments.front().get_value_add(enchantment_value_DEXTERITY) == 2);
    }

    SECTION("scaling enchantments are merged and loaded") {
        e.set_intensity(3);
        const auto enchantments = e.get_enchantments();
        REQUIRE(enchantments.size() == 2);
        CHECK(enchantments.back().get_value_add(enchantment_value_PERCEPTION) == 2);
        CHECK(enchantments.back().get_value_add(enchantment_value_INTELLIGENCE) == 4);
    }
}

#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "character_description.h"
#include "fstream_utils.h"
#include "npc.h"
#include "npc_class.h"
#include "state_helpers.h"

#include <string>

TEST_CASE("NPC biographies prefer templates over classes", "[npc][description]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    auto who = standard_npc{};
    who.load_npc_template(string_id<npc_template>("test_character_description"));

    const auto template_description = std::string{
        "Before the Cataclysm, they kept the town's radio station running."};
    CHECK(get_character_description(who) == template_description);
    CHECK(who.extended_description().find(template_description) != std::string::npos);

    SECTION("template selection survives a save and reload") {
        auto loaded = standard_npc{};
        deserialize(loaded, serialize(who));
        CHECK(get_character_description(loaded) == template_description);
    }
    SECTION("characters without a template biography use their class") {
        who.load_npc_template(string_id<npc_template>("test_character_description_fallback"));
        CHECK(get_character_description(who) == "They once repaired radios for a living.");
    }
    SECTION("a missing template can fall back to the class") {
        who.idz = "removed_template";
        CHECK(get_character_description(who) == "They once repaired radios for a living.");
    }
    SECTION("custom descriptions can override and restore a template biography") {
        set_character_description(who, "They have begun a new life.");
        CHECK(get_character_description(who) == "They have begun a new life.");
        set_character_description(who, "");
        CHECK(get_character_description(who) == template_description);
    }
    SECTION("ordinary NPCs have no biography by default") {
        who.idz.clear();
        who.myclass = npc_class_id("NC_NONE");
        CHECK(get_character_description(who).empty());
    }
}

TEST_CASE(
    "player biographies survive saving and can be edited or cleared",
    "[avatar][description][save]") {
    clear_all_state();
    auto& you = get_avatar();
    const auto previous_description = get_character_description(you);
    const auto cleanup = on_out_of_scope([&]() {
        set_character_description(you, previous_description);
        clear_all_state();
    });
    set_character_description(you, "");
    const auto description = std::string{
        "Grew up by the coast.\n<color_red>Worked as a medic.</color>\nStill looking for their family."};
    CHECK(get_character_description(you).empty());
    set_character_description(you, description);
    CHECK(you.extended_description().find(description) != std::string::npos);

    auto loaded = avatar{};
    deserialize(loaded, serialize(you));
    CHECK(get_character_description(loaded) == description);

    set_character_description(loaded, "A new chapter in their life.");
    CHECK(get_character_description(loaded) == "A new chapter in their life.");
    set_character_description(loaded, "");
    CHECK(get_character_description(loaded).empty());
    auto cleared = avatar{};
    deserialize(cleared, serialize(loaded));
    CHECK(get_character_description(cleared).empty());
    CHECK(get_character_description(you) == description);
}

TEST_CASE(
    "edited biographies follow characters when swapping control", "[npc][description][control]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    auto player_body = avatar{};
    auto npc_body = standard_npc{};
    auto temporary = standard_npc{};
    set_character_description(player_body, "The original survivor.");
    set_character_description(npc_body, "A former radio technician.");

    player_body.swap_character(npc_body, temporary);
    REQUIRE(get_character_description(player_body) == "A former radio technician.");
    set_character_description(player_body, "A radio technician who now helps other survivors.");
    player_body.swap_character(npc_body, temporary);

    CHECK(get_character_description(player_body) == "The original survivor.");
    CHECK(
        get_character_description(npc_body) == "A radio technician who now helps other survivors.");
    CHECK(npc_body.extended_description().find(get_character_description(npc_body))
          != std::string::npos);
    auto loaded = standard_npc{};
    deserialize(loaded, serialize(npc_body));
    CHECK(get_character_description(loaded) == get_character_description(npc_body));
}

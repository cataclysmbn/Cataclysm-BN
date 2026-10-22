#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "character_martial_arts.h"
#    include "client_screen_support.h"
#    include "debug_menu.h"
#    include "game.h"
#    include "item.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "martialarts.h"
#    include "monster.h"
#    include "monstergenerator.h"
#    include "mtype.h"
#    include "mutation.h"
#    include "player_helpers.h"
#    include "reload/reload_ui.h"
#    include "state_helpers.h"
#    include "ui.h"

#    include <string>

namespace {

using namespace client_screen_test;

auto key_row(const int key) -> std::string { return "action:key:" + std::to_string(key); }

} // namespace

TEST_CASE(
    "the technique reference opens from the technique menu through its F1 key row",
    "[client][interaction][mcp][screens][rawkeys]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    you.martial_arts_data->add_martialart(matype_id("style_brawling"));
    you.martial_arts_data->set_style(matype_id("style_brawling"));
    auto& target = spawn_test_monster("mon_zombie", you.bub_pos() + tripoint_east);
    g->manual_combat_mode = true;
    const auto guard = screen_guard{};
    auto stage = 0;
    auto menus = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        if (snapshot.context == "YESNO") {
            const auto yes = std::ranges::
                find(snapshot.choices, "YES", &game_client::interaction_choice::action_id);
            return choose(snapshot, yes->id);
        }
        if (snapshot.context == "MARTIAL_ARTS") {
            ++stage;
            CHECK(snapshot.title == "Technique Reference");
            CHECK(snapshot.message.find("Active martial art") != std::string::npos);
            return cancel(snapshot);
        }
        // The technique menu itself.
        ++menus;
        if (stage == 0) {
            CHECK(row(snapshot, key_row(KEY_F(1))) != snapshot.choices.end());
            return choose(snapshot, key_row(KEY_F(1)));
        }
        return cancel(snapshot);
    });
    for (auto attempts = 0; attempts < 60 && stage == 0; ++attempts) {
        you.melee_attack(target, true);
        target.set_hp(target.get_hp_max());
    }
    CHECK(stage == 1);
    CHECK(menus >= 1);
    g->manual_combat_mode = false;
}

TEST_CASE(
    "the mutation wish menu toggles a trait and filters by category through its key rows",
    "[client][interaction][mcp][screens][rawkeys]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = screen_guard{};
    auto stage = 0;
    auto trait = trait_id::NULL_ID();
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        if (snapshot.context == "UILIST" && row(snapshot, "entry:0") != snapshot.choices.end()
            && snapshot.choices.size() < 12 && stage == 2) {
            // The category menu: ALL clears the filter.
            ++stage;
            CHECK(row(snapshot, "entry:0")->label == "ALL");
            return choose(snapshot, "entry:0");
        }
        switch (stage++) {
            case 0: {
                // The menu is longer than one page and the key rows are still on the first.
                CHECK(snapshot.choice_total > snapshot.choices.size());
                CHECK(row(snapshot, key_row('t')) != snapshot.choices.end());
                CHECK(row(snapshot, key_row('c')) != snapshot.choices.end());
                const auto cursor = std::ranges::
                    find(snapshot.choices, true, &game_client::interaction_choice::highlighted);
                REQUIRE(cursor != snapshot.choices.end());
                for (const auto& branch : mutation_branch::get_all()) {
                    if (branch.name() == cursor->label) { trait = branch.id; }
                }
                REQUIRE(trait != trait_id::NULL_ID());
                you.toggle_trait(trait);
                REQUIRE(you.has_base_trait(trait));
                return choose(snapshot, key_row('t'));
            }
            case 1:
                return choose(snapshot, key_row('c'));
            default:
                return cancel(snapshot);
        }
    });
    debug_menu::wishmutate(&you);
    CHECK(stage >= 4);
    CHECK_FALSE(you.has_trait(trait));
}

TEST_CASE(
    "the mutation wish menu toggle key promotes a non-base trait without desyncing traits",
    "[client][interaction][mcp][screens][rawkeys]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = screen_guard{};
    auto stage = 0;
    auto trait = trait_id::NULL_ID();
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        switch (stage++) {
            case 0: {
                const auto cursor = std::ranges::
                    find(snapshot.choices, true, &game_client::interaction_choice::highlighted);
                REQUIRE(cursor != snapshot.choices.end());
                for (const auto& branch : mutation_branch::get_all()) {
                    if (branch.name() == cursor->label) { trait = branch.id; }
                }
                REQUIRE(trait != trait_id::NULL_ID());
                you.set_mutation(trait);
                REQUIRE_FALSE(you.has_base_trait(trait));
                return choose(snapshot, key_row('t'));
            }
            default:
                return cancel(snapshot);
        }
    });
    debug_menu::wishmutate(&you);
    CHECK(stage >= 2);
    CHECK(you.has_base_trait(trait));
    CHECK(you.has_trait(trait));
}

TEST_CASE(
    "the monster wish menu spawns a friendly monster after the friendly key row",
    "[client][interaction][mcp][screens][rawkeys]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto pos = you.bub_pos() + tripoint_east * 3;
    const auto guard = screen_guard{};
    auto stage = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        switch (stage++) {
            case 0: {
                CHECK(row(snapshot, key_row('f')) != snapshot.choices.end());
                CHECK(row(snapshot, key_row('h')) != snapshot.choices.end());
                return choose(snapshot, key_row('f'));
            }
            case 1: {
                auto index = 0;
                for (const auto& type : MonsterGenerator::generator().get_all_mtypes()) {
                    if (type.id == mtype_id("mon_zombie")) { break; }
                    ++index;
                }
                return choose(snapshot, "entry:" + std::to_string(index));
            }
            default:
                return cancel(snapshot);
        }
    });
    debug_menu::wishmonster(pos);
    auto* const spawned = g->critter_at<monster>(pos);
    REQUIRE(spawned != nullptr);
    CHECK(spawned->friendly == -1);
}

TEST_CASE(
    "the item wish menu opens its container chooser through the container key row",
    "[client][interaction][mcp][screens][rawkeys]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = screen_guard{};
    auto stage = 0;
    auto menus = std::vector<std::string>{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        menus.push_back(snapshot.title + "|" + std::to_string(snapshot.choices.size()));
        switch (stage++) {
            case 0: {
                CHECK(snapshot.choice_total > snapshot.choices.size());
                CHECK(row(snapshot, key_row('f')) != snapshot.choices.end());
                CHECK(row(snapshot, key_row('F')) != snapshot.choices.end());
                CHECK(row(snapshot, key_row('E')) != snapshot.choices.end());
                return choose(snapshot, key_row('f'));
            }
            case 1:
                return cancel(snapshot);
            default:
                return cancel(snapshot);
        }
    });
    debug_menu::wishitem(&you);
    // The container chooser is a second menu opened over the first.
    CHECK(stage >= 3);
    CHECK(menus.size() >= 3);
    CHECK(menus[1] != menus[0]);
}

TEST_CASE(
    "the reload picker lowers the amount to load through its left key row",
    "[client][interaction][mcp][screens][rawkeys]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    auto& magazine = you.i_add(item::spawn("glockmag", calendar::start_of_cataclysm, 0));
    you.i_add(item::spawn("9mm", calendar::start_of_cataclysm, 10));
    get_map().add_item_or_charges(
        you.bub_pos() + tripoint_east, item::spawn("9mm", calendar::start_of_cataclysm, 5));
    const auto guard = screen_guard{};
    auto stage = 0;
    auto before = std::string{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        switch (stage++) {
            case 0: {
                CHECK(row(snapshot, key_row(KEY_LEFT)) != snapshot.choices.end());
                CHECK(row(snapshot, key_row(KEY_RIGHT)) != snapshot.choices.end());
                before = row(snapshot, "entry:0")->label;
                return choose(snapshot, key_row(KEY_LEFT));
            }
            default:
                CHECK(row(snapshot, "entry:0")->label != before);
                return choose(snapshot, "entry:0");
        }
    });
    const auto selected = reload_ui::select_ammo(you, magazine);
    REQUIRE(selected.ammo != nullptr);
    CHECK(stage == 2);
    CHECK(selected.qty() < selected.ammo->charges);
}

#endif // CATA_MCP

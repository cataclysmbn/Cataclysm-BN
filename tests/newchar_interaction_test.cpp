#if defined(CATA_MCP)

#    include "avatar.h"
#    include "bionics.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "magic/magic.h"
#    include "newcharacter.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "profession.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <functional>
#    include <string>
#    include <vector>

namespace {

struct newchar_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_full_screen_width = FULL_SCREEN_WIDTH;
    int old_full_screen_height = FULL_SCREEN_HEIGHT;

    newchar_guard() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~newchar_guard() {
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

auto event_of(game_client::interaction_command command) -> input_event {
    auto input = game_client::input_command{};
    input.interaction = std::move(command);
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto choose(const game_client::interaction_snapshot& snapshot, const std::string& id)
    -> input_event {
    return event_of({
        .input_id = snapshot.input_id,
        .operation = game_client::interaction_operation::choose,
        .target_id = id,
    });
}

auto find_row(const game_client::interaction_snapshot& snapshot, const std::string& id) {
    return std::ranges::find(snapshot.choices, id, &game_client::interaction_choice::id);
}

auto tab_id(const game_client::interaction_snapshot& snapshot, const std::string& label)
    -> std::string {
    const auto tab = std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
        return choice.id.starts_with("tab:") && choice.label == label;
    });
    REQUIRE(tab != snapshot.choices.end());
    return tab->id;
}

/// Runs the creation with a scripted client. Popups are answered here (a name into the field, yes
/// to a question, a plain message dismissed); the description tab is finished here too, once
/// `script` has returned the tab to go to. `script` sees every other creation screen.
auto create_with(
    avatar& you, std::vector<std::string>& popups,
    const std::function<auto(const game_client::interaction_snapshot&)->input_event>& script)
    -> bool {
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        if (snapshot.field) {
            return event_of({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "Ada",
                .submit = true,
            });
        }
        if (!snapshot.context.starts_with("NEW_CHAR_")) {
            popups.push_back(snapshot.message);
            const auto yes = std::ranges::
                find(snapshot.choices, "YES", &game_client::interaction_choice::description);
            return yes != snapshot.choices.end()
                     ? choose(snapshot, yes->id)
                     : event_of({
                           .input_id = snapshot.input_id,
                           .operation = game_client::interaction_operation::cancel,
                       });
        }
        if (snapshot.context == "NEW_CHAR_DESCRIPTION") {
            const auto name = find_row(snapshot, "field:name");
            REQUIRE(name != snapshot.choices.end());
            return choose(
                snapshot, name->columns.front().value == "Ada" ? "action:NEXT_TAB" : "field:name");
        }
        return script(snapshot);
    });
    return you.create(character_type::CUSTOM);
}

} // namespace

TEST_CASE(
    "a custom character is created through structured tabs",
    "[client][interaction][mcp][newchar]") {
    clear_all_state();
    const auto guard = newchar_guard{};
    auto& you = get_avatar();
    // The creation renames the avatar; the harness checks it is left as it was found.
    const auto original_name = you.name;
    struct restore_name {
        avatar& you;
        std::string name;
        ~restore_name() { you.name = name; }
    } const restore{you, original_name};
    auto contexts = std::vector<std::string>{};
    auto trait_points = std::string{};
    auto picked_trait = std::string{};
    auto skill_level_before = std::string{};
    auto skill_id = std::string{};
    auto traits_before = 0;
    auto traits_after = 0;
    auto stage = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        contexts.push_back(snapshot.context);
        REQUIRE(snapshot.structured);
        // Popups of the creation flow: names are typed into a field, questions are answered yes.
        if (snapshot.field) {
            return event_of({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "Ada",
                .submit = true,
            });
        }
        if (!snapshot.context.starts_with("NEW_CHAR_")) {
            const auto yes = std::ranges::
                find(snapshot.choices, "YES", &game_client::interaction_choice::description);
            REQUIRE(yes != snapshot.choices.end());
            return choose(snapshot, yes->id);
        }
        switch (stage++) {
            case 0: {
                // Points: the tab strip lists every tab and the pool options are rows.
                CHECK(snapshot.context == "NEW_CHAR_POINTS");
                CHECK(snapshot.panes.size() >= 7);
                CHECK(find_row(snapshot, "points:0") != snapshot.choices.end());
                CHECK_FALSE(snapshot.message.empty());
                return choose(snapshot, tab_id(snapshot, "TRAITS"));
            }
            case 1: {
                // Scenario, profession and stats were stepped over; traits shows its rows.
                CHECK(snapshot.context == "NEW_CHAR_TRAITS");
                trait_points = snapshot.message;
                const auto trait = std::ranges::find_if(snapshot.choices, [](const auto& choice) {
                    return choice.id.starts_with("trait:") && choice.enabled && !choice.selected
                        && !choice.columns.empty() && choice.columns.front().value != "0"
                        && choice.columns.front().value.front() != '-';
                });
                REQUIRE(trait != snapshot.choices.end());
                CHECK_FALSE(trait->description.empty());
                picked_trait = trait->id;
                traits_before = static_cast<int>(you.get_mutations().size());
                return choose(snapshot, picked_trait);
            }
            case 2: {
                CHECK(snapshot.context == "NEW_CHAR_TRAITS");
                const auto trait = find_row(snapshot, picked_trait);
                REQUIRE(trait != snapshot.choices.end());
                CHECK(trait->selected);
                CHECK(snapshot.message != trait_points);
                traits_after = static_cast<int>(you.get_mutations().size());
                return choose(snapshot, tab_id(snapshot, "SKILLS"));
            }
            case 3: {
                CHECK(snapshot.context == "NEW_CHAR_SKILLS");
                const auto skill = std::ranges::find_if(snapshot.choices, [](const auto& choice) {
                    return choice.id.starts_with("skill:");
                });
                REQUIRE(skill != snapshot.choices.end());
                skill_id = skill->id;
                skill_level_before = skill->columns.front().value;
                return choose(snapshot, skill_id);
            }
            case 4: {
                const auto skill = find_row(snapshot, skill_id);
                REQUIRE(skill != snapshot.choices.end());
                CHECK(skill->highlighted);
                return choose(snapshot, "action:RIGHT");
            }
            case 5: {
                const auto skill = find_row(snapshot, skill_id);
                REQUIRE(skill != snapshot.choices.end());
                CHECK(skill->columns.front().value != skill_level_before);
                return choose(snapshot, tab_id(snapshot, "OVERVIEW"));
            }
            case 6: {
                CHECK(snapshot.context == "NEW_CHAR_DESCRIPTION");
                const auto name = find_row(snapshot, "field:name");
                REQUIRE(name != snapshot.choices.end());
                return choose(snapshot, "field:name");
            }
            case 7: {
                // The name was entered in the field popup; finish the character.
                const auto name = find_row(snapshot, "field:name");
                REQUIRE(name != snapshot.choices.end());
                CHECK(name->columns.front().value == "Ada");
                return choose(snapshot, "action:NEXT_TAB");
            }
            default:
                FAIL("unexpected extra character creation screen " << snapshot.context);
        }
        return input_event{};
    });

    CHECK(you.create(character_type::CUSTOM));
    CHECK(you.name == "Ada");
    CHECK(traits_after == traits_before + 1);
    CHECK(stage == 8);
    CHECK(std::ranges::find(contexts, "NEW_CHAR_STATS") == contexts.end());
}

TEST_CASE(
    "the bionics tab lists the profession's bionics and refuses to remove a locked one",
    "[client][interaction][mcp][newchar]") {
    clear_all_state();
    auto& you = get_avatar();
    const auto original_name = you.name;
    struct restore_name {
        avatar& you;
        std::string name;
        ~restore_name() { you.name = name; }
    } const restore{you, original_name};
    const auto guard = newchar_guard{};
    auto popups = std::vector<std::string>{};
    auto stage = 0;
    auto prepper_has_metabolics = false;
    auto points_before = std::string{};
    const auto created = create_with(you, popups, [&](const auto& snapshot) {
        switch (stage++) {
            case 0:
                return choose(snapshot, tab_id(snapshot, "PROFESSION"));
            case 1:
                return choose(snapshot, "profession:bionic_prepper");
            case 2:
                return choose(snapshot, tab_id(snapshot, "BIONICS"));
            case 3: {
                const auto bionic = find_row(snapshot, "bionic:bio_metabolics");
                REQUIRE(bionic != snapshot.choices.end());
                prepper_has_metabolics = bionic->selected;
                CHECK_FALSE(bionic->description.empty());
                CHECK(bionic->columns.size() == 2);
                points_before = snapshot.message;
                return choose(snapshot, "bionic:bio_metabolics");
            }
            case 4: {
                // A locked bionic stays installed and the points are untouched.
                const auto bionic = find_row(snapshot, "bionic:bio_metabolics");
                REQUIRE(bionic != snapshot.choices.end());
                CHECK(bionic->selected);
                CHECK(bionic->highlighted);
                CHECK(snapshot.message == points_before);
                return choose(snapshot, tab_id(snapshot, "OVERVIEW"));
            }
            default:
                FAIL("unexpected extra character creation screen " << snapshot.context);
        }
        return input_event{};
    });
    CHECK(created);
    CHECK(prepper_has_metabolics);
    CHECK(you.prof->ident() == profession_id("bionic_prepper"));
    CHECK(you.has_bionic(bionic_id("bio_metabolics")));
    REQUIRE(popups.size() >= 1);
    CHECK(popups.front().find("prevents you from removing") != std::string::npos);
}

TEST_CASE(
    "the magic tab lists the spells a profession allows and learning one costs points",
    "[client][interaction][mcp][newchar]") {
    clear_all_state();
    auto& you = get_avatar();
    const auto original_name = you.name;
    struct restore_name {
        avatar& you;
        std::string name;
        ~restore_name() { you.name = name; }
    } const restore{you, original_name};
    const auto guard = newchar_guard{};
    auto popups = std::vector<std::string>{};
    auto stage = 0;
    auto points_before = std::string{};
    const auto created = create_with(you, popups, [&](const auto& snapshot) {
        switch (stage++) {
            case 0: {
                // No profession chosen yet: the default one allows no spell, so there is no tab.
                CHECK(std::ranges::none_of(snapshot.choices, [](const auto& choice) {
                    return choice.label == "MAGIC";
                }));
                return choose(snapshot, tab_id(snapshot, "PROFESSION"));
            }
            case 1:
                return choose(snapshot, "profession:test_spellcaster");
            case 2:
                return choose(snapshot, tab_id(snapshot, "MAGIC"));
            case 3: {
                const auto spell = find_row(snapshot, "spell:test_spell_apprentice");
                REQUIRE(spell != snapshot.choices.end());
                CHECK_FALSE(spell->selected);
                CHECK_FALSE(spell->description.empty());
                points_before = snapshot.message;
                return choose(snapshot, spell->id);
            }
            case 4: {
                const auto spell = find_row(snapshot, "spell:test_spell_apprentice");
                REQUIRE(spell != snapshot.choices.end());
                CHECK(spell->highlighted);
                CHECK_FALSE(spell->selected);
                return choose(snapshot, "action:RIGHT");
            }
            case 5: {
                const auto spell = find_row(snapshot, "spell:test_spell_apprentice");
                REQUIRE(spell != snapshot.choices.end());
                CHECK(spell->selected);
                CHECK(snapshot.message != points_before);
                return choose(snapshot, tab_id(snapshot, "OVERVIEW"));
            }
            default:
                FAIL("unexpected extra character creation screen " << snapshot.context);
        }
        return input_event{};
    });
    CHECK(created);
    CHECK(you.magic->knows_spell(spell_id("test_spell_apprentice")));
}

#endif // CATA_MCP

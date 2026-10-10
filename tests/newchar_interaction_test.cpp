#if defined(CATA_MCP)

#    include "avatar.h"
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
#    include "newcharacter.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <algorithm>
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

#endif // CATA_MCP

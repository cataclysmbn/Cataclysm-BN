#if defined(CATA_MCP)

#    include "achievement.h"
#    include "auto_note.h"
#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "clzones.h"
#    include "diary.h"
#    include "faction.h"
#    include "game.h"
#    include "kill_tracker.h"
#    include "map_helpers.h"
#    include "player_helpers.h"
#    include "scores_ui.h"
#    include "state_helpers.h"
#    include "stats_tracker.h"

#    include <optional>
#    include <string>

namespace {

using namespace client_screen_test;

/// The native yes/no prompt some of these screens ask on leaving, answered with `answer`.
auto prompt_answer(const snapshot_t& snapshot, const std::string& answer)
    -> std::optional<input_event> {
    if (snapshot.context != "YESNO") { return std::nullopt; }
    const auto choice =
        std::ranges::find(snapshot.choices, answer, &game_client::interaction_choice::description);
    REQUIRE(choice != snapshot.choices.end());
    return choose(snapshot, choice->id);
}

} // namespace

TEST_CASE(
    "the scores screen switches its tabs and shows the text of each",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    auto stats = stats_tracker{};
    auto kills = kill_tracker{};
    auto achievements = achievements_tracker(stats, kills, [](const achievement*) {});
    const auto guard = screen_guard{};
    auto stage = 0;
    auto first_text = std::string{};
    open_screen(
        [&] { show_scores_ui(achievements, stats, kills); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "SCORES");
                    CHECK(rows_with(snapshot, "tab:").size() == 3);
                    CHECK(row(snapshot, "tab:0")->selected);
                    CHECK_FALSE(snapshot.message.empty());
                    first_text = snapshot.message;
                    return choose(snapshot, "tab:1");
                }
                case 1: {
                    CHECK(row(snapshot, "tab:1")->selected);
                    CHECK_FALSE(row(snapshot, "tab:0")->selected);
                    CHECK(snapshot.message != first_text);
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 2);
}

TEST_CASE(
    "the diary lists its pages and adds and deletes one", "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    auto* diary = you.get_avatar_diary();
    REQUIRE(diary != nullptr);
    auto pages_before = std::size_t{0};
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [&] { diary::show_diary_ui(diary); },
        [&](const auto& snapshot) {
            if (const auto answer = prompt_answer(snapshot, "YES")) { return *answer; }
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "DIARY");
                    pages_before = rows_with(snapshot, "page:").size();
                    CHECK(row(snapshot, "action:NEW_PAGE") != snapshot.choices.end());
                    return choose(snapshot, "action:NEW_PAGE");
                }
                case 1: {
                    const auto pages = rows_with(snapshot, "page:");
                    CHECK(pages.size() == pages_before + 1);
                    CHECK(row(snapshot, pages.back())->highlighted);
                    return choose(snapshot, "action:DELETE PAGE");
                }
                case 2: {
                    CHECK(rows_with(snapshot, "page:").size() == pages_before);
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 3);
}

TEST_CASE(
    "the auto notes manager toggles the note of a discovered map extra",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    get_auto_notes_settings().set_discovered(string_id<map_extra>("mx_crater"));
    auto gui = auto_notes::auto_note_manager_gui{};
    const auto guard = screen_guard{};
    auto stage = 0;
    auto enabled_before = false;
    open_screen(
        [&] { gui.show(); },
        [&](const auto& snapshot) {
            if (const auto answer = prompt_answer(snapshot, "NO")) { return *answer; }
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "AUTO_NOTES");
                    const auto extra = row(snapshot, "extra:mx_crater");
                    REQUIRE(extra != snapshot.choices.end());
                    CHECK(extra->highlighted);
                    CHECK(extra->columns.size() == 2);
                    enabled_before = extra->selected;
                    return choose(snapshot, "extra:mx_crater");
                }
                case 1: {
                    // Choosing a map extra is Enter on it: its note is toggled.
                    CHECK(row(snapshot, "extra:mx_crater")->selected == !enabled_before);
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 2);
    CHECK(gui.was_changed());
}

TEST_CASE(
    "the faction manager lists the tabs and the creatures the avatar knows",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    you.set_knows_creature_type(mtype_id("mon_zombie"));
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [] { g->faction_manager_ptr->display(); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "FACTION MANAGER");
                    CHECK(rows_with(snapshot, "tab:").size() == 4);
                    CHECK(rows_with(snapshot, "follower:").empty());
                    CHECK(snapshot.message == "You have no followers");
                    return choose(snapshot, "tab:3");
                }
                case 1: {
                    CHECK(row(snapshot, "tab:3")->selected);
                    const auto creatures = rows_with(snapshot, "creature:");
                    REQUIRE(creatures.size() == 1);
                    const auto zombie = row(snapshot, creatures.front());
                    CHECK(zombie->highlighted);
                    CHECK_FALSE(zombie->description.empty());
                    return choose(snapshot, "tab:2");
                }
                case 2: {
                    CHECK(rows_with(snapshot, "creature:").empty());
                    CHECK(snapshot.message == "You haven't learned anything about the world.");
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 3);
}

TEST_CASE(
    "the zone manager lists the zones and enables and disables the chosen one",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    zone_manager::reset_manager();
    zone_manager::get_manager()
        .add("test pile", zone_type_id("LOOT_UNSORTED"), you.get_faction()->id, false, true,
             you.abs_pos(), you.abs_pos());
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [] { g->zones_manager(); },
        [&](const auto& snapshot) {
            if (const auto answer = prompt_answer(snapshot, "NO")) { return *answer; }
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "ZONES_MANAGER");
                    const auto zone = row(snapshot, "zone:0");
                    REQUIRE(zone != snapshot.choices.end());
                    CHECK(zone->label == "test pile");
                    CHECK(zone->selected);
                    CHECK(zone->highlighted);
                    CHECK(zone->columns.size() == 4);
                    CHECK(row(snapshot, "action:ADD_ZONE") != snapshot.choices.end());
                    return choose(snapshot, "action:DISABLE_ZONE");
                }
                case 1: {
                    CHECK_FALSE(row(snapshot, "zone:0")->selected);
                    return choose(snapshot, "action:ENABLE_ZONE");
                }
                case 2: {
                    CHECK(row(snapshot, "zone:0")->selected);
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 3);
    zone_manager::reset_manager();
}

#endif // CATA_MCP

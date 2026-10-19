#if defined(CATA_MCP)

#    include "achievement.h"
#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "clzones.h"
#    include "engine_client_component.h"
#    include "faction.h"
#    include "game.h"
#    include "kill_tracker.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "npc.h"
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

/// The target the look screen on top shows, as a client reads it.
auto look_target() -> nlohmann::json { return top_json()["root"]["child"]["target"]; }

} // namespace

TEST_CASE(
    "the scores screen switches its tabs and shows the text of each",
    "[client][interaction][mcp][screens][info_screens][scores_screen]") {
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
    open_stack([&] { show_scores_ui(achievements, stats, kills); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   namespace ec = engine_client;
                   REQUIRE(top.root);
                   const auto* const tabs = ec::find_component(*top.root, "scores:tabs");
                   REQUIRE(tabs != nullptr);
                   const auto& entries = std::get<ec::tabs_body>(tabs->body).tabs;
                   REQUIRE(entries.size() == 3);
                   CHECK(entries[0].id == "achievements");
                   CHECK(entries[1].id == "scores");
                   CHECK(entries[2].id == "kills");
                   const auto* const text = ec::find_component(*top.root, "scores:text");
                   REQUIRE(text != nullptr);
                   const auto& markup = std::get<ec::text_body>(text->body).markup;
                   switch (stage++) {
                       case 0: {
                           CHECK(top.context == "SCORES");
                           CHECK(entries[0].selected);
                           CHECK_FALSE(markup.empty());
                           first_text = markup;
                           return invoke_event(top, "scores:tab", {.row = "scores"});
                       }
                       case 1: {
                           CHECK(entries[1].selected);
                           CHECK_FALSE(entries[0].selected);
                           CHECK(markup != first_text);
                           return run_action("QUIT");
                       }
                       default:
                           FAIL("unexpected extra screen " << top.context);
                   }
                   return input_event{};
               });
    CHECK(stage == 2);
}

TEST_CASE(
    "the kills screen shows the kill text of the tracker",
    "[client][interaction][mcp][screens][info_screens][scores_screen]") {
    clear_all_state();
    clear_map();
    clear_character(get_avatar(), false);
    auto kills = kill_tracker{};
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack([&] { show_kills(kills); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   namespace ec = engine_client;
                   REQUIRE(stage++ == 0);
                   CHECK(top.title == "Kills");
                   REQUIRE(top.root);
                   const auto* const text = ec::find_component(*top.root, "kills:text");
                   REQUIRE(text != nullptr);
                   CHECK(std::get<ec::text_body>(text->body).markup == kills.get_kills_text());
                   return run_action("QUIT");
               });
    CHECK(stage == 1);
}

TEST_CASE(
    "the faction manager lists the tabs and the creatures the avatar knows",
    "[client][interaction][mcp][screens][info_screens][factions_screen]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    you.set_knows_creature_type(mtype_id("mon_zombie"));
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [] { g->faction_manager_ptr->display(); },
        [&](const auto& /*snapshot*/, const auto& top) {
            namespace ec = engine_client;
            const auto find = [&](const std::string& id) {
                REQUIRE(top.root);
                return ec::find_component(*top.root, id);
            };
            switch (stage++) {
                case 0: {
                    CHECK(top.context == "FACTION MANAGER");
                    const auto* const tabs = find("factions:tabs");
                    REQUIRE(tabs != nullptr);
                    const auto& entries = std::get<ec::tabs_body>(tabs->body).tabs;
                    REQUIRE(entries.size() == 4);
                    CHECK(entries[0].id == "followers");
                    CHECK(entries[3].id == "creatures");
                    const auto* const none = find("factions:none");
                    REQUIRE(none != nullptr);
                    CHECK(std::get<ec::text_body>(none->body).markup == "You have no followers");
                    const auto swap = std::ranges::find(top.denials, "SWAPTONPC", &ec::denial::id);
                    REQUIRE(swap != top.denials.end());
                    CHECK(swap->reason == "You have no followers");
                    return invoke_event(top, "factions:tab", {.row = "creatures"});
                }
                case 1: {
                    const auto* const list = find("factions:list");
                    REQUIRE(list != nullptr);
                    const auto& rows = std::get<ec::list_body>(list->body).rows;
                    REQUIRE(rows.size() == 1);
                    CHECK(rows[0].id == "creature:mon_zombie");
                    CHECK(top.focus_row == "creature:mon_zombie");
                    const auto* const facts = find("factions:facts");
                    REQUIRE(facts != nullptr);
                    const auto& markup = std::get<ec::text_body>(facts->body).markup;
                    CHECK(markup.find("Difficulty") != std::string::npos);
                    CHECK(markup.find("Origin") != std::string::npos);
                    CHECK(markup.find("Species") != std::string::npos);
                    return invoke_event(top, "factions:tab", {.row = "lore"});
                }
                case 2: {
                    CHECK(find("factions:list") == nullptr);
                    const auto* const none = find("factions:none");
                    REQUIRE(none != nullptr);
                    CHECK(std::get<ec::text_body>(none->body).markup
                          == "You haven't learned anything about the world.");
                    return run_action("QUIT");
                }
                default:
                    FAIL("unexpected extra screen " << top.context);
            }
            return input_event{};
        });
    CHECK(stage == 3);
}

TEST_CASE(
    "the faction manager lets the player talk to and swap to a follower only within reach",
    "[client][interaction][mcp][screens][info_screens][factions_screen]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto within_reach = GENERATE(true, false);
    auto& near = spawn_npc(you.bub_pos() + tripoint_east * (within_reach ? 1 : 30), "test_talker");
    near.set_fac(faction_id("your_followers"));
    near.set_attitude(NPCATT_FOLLOW);
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [] { g->faction_manager_ptr->display(); },
        [&](const auto& /*snapshot*/, const auto& top) {
            namespace ec = engine_client;
            REQUIRE(stage++ == 0);
            REQUIRE(top.root);
            const auto* const list = ec::find_component(*top.root, "factions:list");
            REQUIRE(list != nullptr);
            const auto& rows = std::get<ec::list_body>(list->body).rows;
            REQUIRE(rows.size() == 1);
            CHECK(rows[0].id == "follower:" + std::to_string(near.getID().get_value()));
            const auto* const facts = ec::find_component(*top.root, "factions:facts");
            REQUIRE(facts != nullptr);
            CHECK((std::get<ec::text_body>(facts->body).markup.find("Within interaction range")
                   != std::string::npos)
                  == within_reach);
            CHECK(top.denials.empty() == within_reach);
            if (!within_reach) {
                const auto confirm = std::ranges::find(top.denials, "CONFIRM", &ec::denial::id);
                REQUIRE(confirm != top.denials.end());
                CHECK(confirm->reason == "Both you and follower need a radio");
                CHECK(std::ranges::count(top.denials, "SWAPTONPC", &ec::denial::id) == 1);
            }
            return run_action("QUIT");
        });
    CHECK(stage == 1);
}

TEST_CASE(
    "adding a zone names the selection step of its area in the look target",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    zone_manager::reset_manager();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [] { g->zones_manager(); },
        [&](const auto& snapshot, const auto& top) {
            if (const auto answer = prompt_answer(snapshot, "NO")) { return *answer; }
            switch (stage++) {
                case 0:
                    CHECK(top.kind == "zones");
                    return event_of({.action = "ADD_ZONE"});
                case 1:
                    return choose(snapshot, snapshot.choices.at(0).id);
                case 2:
                    REQUIRE(snapshot.field.has_value());
                    return operate(snapshot, game_client::interaction_operation::fill,
                                   snapshot.field->id, "new zone");
                case 3:
                    CHECK(look_target()["status"] == "zone_first");
                    CHECK(look_target()["status_text"] == "Select first point.");
                    return event_of({.action = "CONFIRM"});
                case 4:
                    CHECK(look_target()["status"] == "zone_second");
                    CHECK(look_target()["status_text"] == "Select second point.");
                    return event_of({.action = "CONFIRM"});
                case 5: {
                    CHECK(top.kind == "zones");
                    REQUIRE(top.root);
                    const auto* const rows = engine_client::find_component(*top.root, "zones:rows");
                    REQUIRE(rows != nullptr);
                    const auto& list = std::get<engine_client::list_body>(rows->body);
                    REQUIRE(list.rows.size() == 1);
                    CHECK(list.rows.front().label == "new zone");
                    return event_of({.action = "QUIT"});
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 6);
    zone_manager::reset_manager();
}

#endif // CATA_MCP

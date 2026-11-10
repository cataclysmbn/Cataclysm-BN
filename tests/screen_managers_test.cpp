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
        std::ranges::find(snapshot.choices, answer, &game_client::interaction_choice::action_id);
    REQUIRE(choice != snapshot.choices.end());
    return choose(snapshot, choice->id);
}

/// The target the look screen on top shows, as a client reads it.
auto look_target() -> nlohmann::json { return top_json()["root"]["child"]["target"]; }

} // namespace

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

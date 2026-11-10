#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "clzones.h"
#    include "engine_client_component.h"
#    include "faction.h"
#    include "game.h"
#    include "map_helpers.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <optional>
#    include <ranges>
#    include <string>
#    include <variant>
#    include <vector>

namespace {
namespace ec = engine_client;
using namespace client_screen_test;

auto part(const ec::screen_view& screen, const std::string& id) -> const ec::component& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}
auto rows_of(const ec::screen_view& screen, const std::string& id) -> const std::vector<ec::row>& {
    const auto& list = part(screen, id);
    REQUIRE(std::holds_alternative<ec::list_body>(list.body));
    return std::get<ec::list_body>(list.body).rows;
}
auto row_of(const ec::screen_view& screen, const std::string& list, const std::string& id)
    -> const ec::row& {
    const auto& rows = rows_of(screen, list);
    const auto found = std::ranges::find(rows, id, &ec::row::id);
    INFO(id);
    REQUIRE(found != rows.end());
    return *found;
}
auto affordance_of(const ec::screen_view& screen, const std::string& id) -> const ec::affordance& {
    const auto found = std::ranges::find(screen.affordances, id, &ec::affordance::id);
    INFO(id);
    REQUIRE(found != screen.affordances.end());
    return *found;
}
/// The native prompt to save changes on leaving, answered no.
auto declined(const snapshot_t& snapshot) -> std::optional<input_event> {
    if (snapshot.context != "YESNO") { return std::nullopt; }
    const auto choice =
        std::ranges::find(snapshot.choices, "NO", &game_client::interaction_choice::action_id);
    REQUIRE(choice != snapshot.choices.end());
    return choose(snapshot, choice->id);
}
auto fresh_zones() -> zone_manager& {
    clear_all_state();
    clear_map();
    clear_character(get_avatar(), false);
    zone_manager::reset_manager();
    return zone_manager::get_manager();
}

} // namespace

TEST_CASE(
    "the zone manager lists its zones with their facts, enabled state and display toggles",
    "[zones_screen][engine_client_component]") {
    auto& zones = fresh_zones();
    auto& you = get_avatar();
    zones.add("test pile", zone_type_id("LOOT_UNSORTED"), you.get_faction()->id, false, true,
              you.abs_pos(), you.abs_pos());
    zones.add("far pile", zone_type_id("LOOT_FOOD"), you.get_faction()->id, false, true,
              you.abs_pos() + tripoint(3, 0, 0), you.abs_pos() + tripoint(3, 0, 0));
    const auto guard = screen_guard{};
    auto stage = 0;
    auto first = std::string{};
    auto second = std::string{};
    open_stack(
        [] { g->zones_manager(); },
        [&](const snapshot_t& snapshot, const ec::screen_view& screen) {
            if (const auto answer = declined(snapshot)) { return *answer; }
            switch (stage++) {
                case 0: {
                    CHECK(screen.kind == "zones");
                    // Natively the window covers the sidebar beside the map it outlines.
                    CHECK(screen.dock == "overlapping");
                    const auto& rows = rows_of(screen, "zones:rows");
                    REQUIRE(rows.size() == 2);
                    // A row is named by its zone, not by its place in the list.
                    CHECK(rows[0].id.starts_with("zone:LOOT_UNSORTED:"));
                    CHECK(rows[1].id.starts_with("zone:LOOT_FOOD:"));
                    first = rows[0].id;
                    second = rows[1].id;
                    CHECK(rows[1].label == "far pile");
                    const auto& pile = rows[0];
                    CHECK(pile.label == "test pile");
                    CHECK(pile.toggled == std::optional{true});
                    CHECK(
                        (pile.columns | std::views::transform(&game_client::interaction_column::key)
                         | std::ranges::to<std::vector>())
                        == std::vector<std::string>{"type", "distance", "direction", "vehicle"});
                    CHECK(screen.focus_row == first);
                    CHECK(affordance_of(screen, "MOVE_ZONE_UP").enabled);
                    CHECK(row_of(screen, "zones:display", "SHOW_ALL_ZONES").toggled
                          == std::optional{false});
                    return invoke_event(screen, "zones:pick", {.row = second});
                }
                case 1:
                    CHECK(screen.focus_row == second);
                    return run_action("DISABLE_ZONE");
                case 2:
                    CHECK(row_of(screen, "zones:rows", second).toggled == std::optional{false});
                    CHECK(row_of(screen, "zones:rows", first).toggled == std::optional{true});
                    return invoke_event(screen, "zones:toggle", {.row = "SHOW_ALL_ZONES"});
                case 3:
                    CHECK(row_of(screen, "zones:display", "SHOW_ALL_ZONES").toggled
                          == std::optional{true});
                    return run_action("QUIT");
                default:
                    FAIL("the zone manager outlived its quit");
                    return run_action("QUIT");
            }
        });
    CHECK(stage == 4);
    zone_manager::reset_manager();
}

TEST_CASE(
    "an empty zone manager says so and refuses the actions that need a zone",
    "[zones_screen][engine_client_component]") {
    fresh_zones();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack([] { g->zones_manager(); },
               [&](const snapshot_t& snapshot, const ec::screen_view& screen) {
                   if (const auto answer = declined(snapshot)) { return *answer; }
                   if (stage++ == 0) {
                       CHECK(screen.kind == "zones");
                       CHECK(rows_of(screen, "zones:rows").empty());
                       const auto& list = part(screen, "zones:list");
                       REQUIRE(std::holds_alternative<ec::panel_body>(list.body));
                       const auto& stats = std::get<ec::panel_body>(list.body).stats;
                       REQUIRE(stats.size() == 1);
                       CHECK(stats.front().label == "No Zones defined.");
                       CHECK(affordance_of(screen, "ADD_ZONE").enabled);
                       CHECK_FALSE(affordance_of(screen, "REMOVE_ZONE").enabled);
                       CHECK_FALSE(affordance_of(screen, "MOVE_ZONE_UP").enabled);
                   }
                   return run_action("QUIT");
               });
    CHECK(stage == 1);
    zone_manager::reset_manager();
}

TEST_CASE(
    "a zone a client activates opens its native edit menu as Enter does",
    "[zones_screen][engine_client_component]") {
    auto& zones = fresh_zones();
    auto& you = get_avatar();
    zones.add("test pile", zone_type_id("LOOT_UNSORTED"), you.get_faction()->id, false, true,
              you.abs_pos(), you.abs_pos());
    const auto guard = screen_guard{};
    auto stage = 0;
    auto edited = false;
    open_stack(
        [] { g->zones_manager(); },
        [&](const snapshot_t& snapshot, const ec::screen_view& screen) {
            if (const auto answer = declined(snapshot)) { return *answer; }
            if (snapshot.message == "What do you want to change:") {
                edited = true;
                return run_action("QUIT");
            }
            if (stage++ == 0) {
                const auto& rows = rows_of(screen, "zones:rows");
                REQUIRE(rows.size() == 1);
                return invoke_event(screen, "bn:activate:zones:pick", {.row = rows.front().id});
            }
            return run_action("QUIT");
        });
    CHECK(edited);
    zone_manager::reset_manager();
}

#endif // CATA_MCP

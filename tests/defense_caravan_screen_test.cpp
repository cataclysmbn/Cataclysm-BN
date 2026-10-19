#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "engine_client_component.h"
#    include "gamemode_defense.h"
#    include "map_helpers.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <ranges>
#    include <set>
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
template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
auto rows_of(const ec::screen_view& screen) -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(screen, "caravan:rows")).rows;
}
auto selected_tab(const ec::screen_view& screen) -> std::string {
    const auto& tabs = body<ec::tabs_body>(part(screen, "caravan:tabs")).tabs;
    const auto found = std::ranges::find(tabs, true, &ec::tab_entry::selected);
    REQUIRE(found != tabs.end());
    return found->id;
}
auto stat_of(const ec::screen_view& screen, const std::string& key) -> std::string {
    const auto& stats = body<ec::panel_body>(part(screen, "caravan:items")).stats;
    const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
    INFO(key);
    REQUIRE(found != stats.end());
    return found->value;
}
auto column_of(const ec::row& row, const std::string& key) -> std::string {
    const auto found = std::ranges::find(row.columns, key, &game_client::interaction_column::key);
    INFO(key);
    REQUIRE(found != row.columns.end());
    return found->value;
}

} // namespace

TEST_CASE(
    "a caravan lists its goods by category and sells what a client puts in the cart",
    "[defense_caravan_screen][engine_client_component]") {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    you.cash = 1000000;
    const auto guard = screen_guard{};
    auto mode = defense_game{};
    auto step = 0;
    auto bought = std::string{};
    open_stack(
        [&] { mode.caravan(); },
        [&](const snapshot_t& /*snapshot*/, const ec::screen_view& screen) {
            switch (step++) {
                case 0: {
                    CHECK(screen.kind == "caravan");
                    CHECK(body<ec::tabs_body>(part(screen, "caravan:tabs")).tabs.size() == 8);
                    CHECK(selected_tab(screen) == "caravan:cart");
                    CHECK(rows_of(screen).empty());
                    CHECK(stat_of(screen, "total") == "$0.00");
                    return invoke_event(screen, "caravan:tab", {.row = "caravan:melee"});
                }
                case 1: {
                    CHECK(selected_tab(screen) == "caravan:melee");
                    const auto& rows = rows_of(screen);
                    REQUIRE(rows.size() > 1);
                    const auto ids =
                        rows | std::views::transform(&ec::row::id) | std::ranges::to<std::set>();
                    CHECK(ids.size() == rows.size());
                    CHECK(column_of(rows.front(), "count") == "0");
                    bought = rows.back().id;
                    return invoke_event(screen, "caravan:pick", {.row = bought});
                }
                case 2:
                    CHECK(screen.focus_component == "caravan:rows");
                    CHECK(screen.focus_row == bought);
                    CHECK_FALSE(
                        body<ec::text_body>(part(screen, "caravan:details")).markup.empty());
                    return run_action("RIGHT");
                case 3: {
                    const auto& rows = rows_of(screen);
                    const auto found = std::ranges::find(rows, bought, &ec::row::id);
                    REQUIRE(found != rows.end());
                    CHECK(column_of(*found, "count") == "1");
                    CHECK(stat_of(screen, "total") != "$0.00");
                    return invoke_event(screen, "caravan:tab", {.row = "caravan:cart"});
                }
                case 4: {
                    const auto& rows = rows_of(screen);
                    REQUIRE(rows.size() == 1);
                    CHECK(rows.front().id == bought);
                    return run_action("CONFIRM");
                }
                case 5:
                    REQUIRE(screen.context == "YESNO");
                    return run_action("YES");
                default:
                    FAIL("the caravan outlived its purchase");
                    return run_action("QUIT");
            }
        });
    CHECK(step == 6);
    CHECK(you.cash < 1000000);
}

TEST_CASE(
    "a caravan names each of its goods by a row of its own in every category",
    "[defense_caravan_screen][engine_client_component]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto mode = defense_game{};
    auto categories = std::vector<std::string>{};
    auto visited = std::size_t{0};
    open_stack(
        [&] { mode.caravan(); },
        [&](const snapshot_t& /*snapshot*/, const ec::screen_view& screen) {
            if (screen.context == "YESNO") { return run_action("YES"); }
            if (categories.empty()) {
                categories =
                    body<ec::tabs_body>(part(screen, "caravan:tabs")).tabs
                    | std::views::transform(&ec::tab_entry::id) | std::ranges::to<std::vector>();
            } else {
                const auto& rows = rows_of(screen);
                const auto ids =
                    rows | std::views::transform(&ec::row::id) | std::ranges::to<std::set>();
                INFO(selected_tab(screen));
                CHECK(ids.size() == rows.size());
                ++visited;
            }
            if (visited == categories.size()) { return run_action("QUIT"); }
            return invoke_event(screen, "caravan:tab", {.row = categories[visited]});
        });
    CHECK(visited == 8);
}

#endif // CATA_MCP

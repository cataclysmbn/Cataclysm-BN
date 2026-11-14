#if defined(CATA_MCP)

#    include "action.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "engine_client_component.h"
#    include "map_helpers.h"
#    include "popup.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <optional>
#    include <string>
#    include <variant>

namespace {

using namespace client_screen_test;
namespace ec = engine_client;

auto part(const ec::screen_view& top, const std::string& id) -> const ec::component& {
    REQUIRE(top.root);
    const auto* const found = ec::find_component(*top.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}

auto has_affordance(const ec::screen_view& top, const std::string& id) -> bool {
    return std::ranges::contains(top.affordances, id, &ec::affordance::id);
}

auto cell_at(const ec::grid_body& grid, const ec::grid_point at) -> const ec::grid_cell& {
    const auto found = std::ranges::find(grid.cells, at, &ec::grid_cell::at);
    INFO(at.x << "," << at.y);
    REQUIRE(found != grid.cells.end());
    return *found;
}

} // namespace

TEST_CASE(
    "a popup asks its question above the choices, each naming the action it runs",
    "[client][interaction][mcp][screens][popup][engine_client_component]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    auto stage = 0;
    auto answer = std::string{};
    open_stack(
        [&] {
            answer =
                query_popup()
                    .context("YESNO")
                    .message("%s", "Continue?")
                    .option("YES")
                    .option("NO")
                    .query()
                    .action;
        },
        [&](const auto& /*snapshot*/, const auto& top) {
            REQUIRE(stage++ == 0);
            CHECK(std::get<ec::text_body>(part(top, "text:message").body).role == "prompt");
            const auto root = top_json()["root"];
            const auto& rows = root["children"][1]["rows"];
            REQUIRE(rows.size() == 2);
            for (const auto& [row, action] :
                 {std::pair{rows[0], "YES"}, std::pair{rows[1], "NO"}}) {
                INFO(row.dump());
                CHECK(row.value("affordance", std::string{}) == action);
                // The action id is no prose to show under the row.
                CHECK(row["description"] == "");
                CHECK(has_affordance(top, action));
            }
            return invoke_event(top, "YES");
        });
    CHECK(stage == 1);
    CHECK(answer == "YES");
}

TEST_CASE(
    "a popup's only row names no action the screen does not offer",
    "[client][interaction][mcp][screens][popup][engine_client_component]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack([&] { query_popup().message("%s", "Done").allow_cancel(true).query(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   REQUIRE(stage++ == 0);
                   const auto rows = top_json()["root"]["children"][1]["rows"];
                   REQUIRE(rows.size() == 1);
                   // The cancel stands for QUIT, so the row runs no affordance of that name.
                   CHECK_FALSE(has_affordance(top, "QUIT"));
                   CHECK_FALSE(rows[0].contains("affordance"));
                   CHECK(rows[0]["description"] == "");
                   return invoke_event(top, "bn:cancel");
               });
    CHECK(stage == 1);
}

#endif // CATA_MCP

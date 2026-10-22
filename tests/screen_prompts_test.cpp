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

TEST_CASE(
    "the direction prompt is a compass of the squares around the avatar",
    "[client][interaction][mcp][screens][direction][engine_client_component]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    for (const auto vertical : {false, true}) {
        INFO("vertical: " << vertical);
        auto stage = 0;
        auto chosen = std::optional<tripoint_rel_ms>{};
        open_stack(
            [&] { chosen = choose_direction("Close where?", vertical); },
            [&](const auto& /*snapshot*/, const auto& top) {
                REQUIRE(stage++ == 0);
                CHECK(top.title == "Choose direction");
                const auto& prompt = std::get<ec::text_body>(part(top, "text:message").body);
                CHECK(prompt.markup == "Close where?");
                CHECK(prompt.role == "prompt");
                const auto& compass = part(top, "direction:compass");
                REQUIRE(std::holds_alternative<ec::grid_body>(compass.body));
                const auto& grid = std::get<ec::grid_body>(compass.body);
                CHECK(grid.cells.size() == (vertical ? 11U : 9U));
                CHECK(grid.cols == (vertical ? 4 : 3));
                CHECK(grid.rows == 3);
                CHECK(cell_at(grid, {1, 0}).label == "North");
                CHECK(cell_at(grid, {1, 0}).glyph == "N");
                CHECK(cell_at(grid, {2, 2}).label == "South East");
                CHECK(cell_at(grid, {1, 1}).label == "Here");
                if (vertical) {
                    CHECK(cell_at(grid, {3, 0}).label == "Above");
                    CHECK(cell_at(grid, {3, 2}).label == "Below");
                }
                for (const auto& cell : grid.cells) {
                    CHECK(cell.enabled);
                    CHECK(has_affordance(top, cell.affordance));
                }
                CHECK(std::ranges::contains(compass.affordances, grid.cells[0].affordance));
                CHECK(has_affordance(top, "bn:cancel"));
                return invoke_event(
                    top, cell_at(grid, {0, 1}).affordance, {.row = cell_at(grid, {0, 1}).id});
            });
        CHECK(stage == 1);
        CHECK(chosen == std::optional{tripoint_rel_ms::west()});
    }
}

#endif // CATA_MCP

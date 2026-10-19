#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "engine_client_component.h"
#    include "gamemode_defense.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <ranges>
#    include <string>
#    include <variant>
#    include <vector>

namespace {
namespace ec = engine_client;
using namespace client_screen_test;

auto rows_of(const ec::screen_view& screen) -> const std::vector<ec::row>& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, "defense:rows");
    REQUIRE(found != nullptr);
    REQUIRE(std::holds_alternative<ec::list_body>(found->body));
    return std::get<ec::list_body>(found->body).rows;
}
auto row_of(const ec::screen_view& screen, const std::string& id) -> const ec::row& {
    const auto& rows = rows_of(screen);
    const auto found = std::ranges::find(rows, id, &ec::row::id);
    INFO(id);
    REQUIRE(found != rows.end());
    return *found;
}

} // namespace

TEST_CASE(
    "the defense setup lists its options and toggles the one a client picks",
    "[defense_setup_screen][engine_client_component]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto mode = defense_game{};
    auto step = 0;
    auto before = false;
    open_stack(
        [&] { mode.setup(); },
        [&](const snapshot_t& /*snapshot*/, const ec::screen_view& screen) {
            switch (step++) {
                case 0: {
                    CHECK(screen.kind == "defense_setup");
                    CHECK(screen.title == "DEFENSE MODE");
                    CHECK(rows_of(screen).size() == 20);
                    CHECK(screen.focus_row == "defense:scenario");
                    CHECK(row_of(screen, "defense:scenario").label == "Scenario:");
                    CHECK_FALSE(row_of(screen, "defense:scenario").columns.empty());
                    CHECK_FALSE(row_of(screen, "defense:scenario").description.empty());
                    CHECK(row_of(screen, "defense:spiders").label == "Spiders");
                    CHECK(row_of(screen, "defense:spiders").toggled.has_value());
                    CHECK_FALSE(row_of(screen, "defense:initial_cash").toggled.has_value());
                    return invoke_event(screen, "defense:pick", {.row = "defense:spiders"});
                }
                case 1: {
                    CHECK(screen.focus_row == "defense:spiders");
                    before = *row_of(screen, "defense:spiders").toggled;
                    return run_action("CONFIRM");
                }
                case 2:
                    CHECK(row_of(screen, "defense:spiders").toggled == std::optional{!before});
                    return run_action("START");
                default:
                    FAIL("the setup outlived its start");
                    return run_action("START");
            }
        });
    CHECK(step == 3);
}

#endif // CATA_MCP

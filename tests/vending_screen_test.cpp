#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "engine_client_component.h"
#    include "iexamine.h"
#    include "item.h"
#    include "item_serial.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <ranges>
#    include <string>
#    include <variant>
#    include <vector>

namespace {
namespace ec = engine_client;
using namespace client_screen_test;

constexpr auto machine_at = tripoint_bub_ms(60, 60, 0);

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
    return body<ec::list_body>(part(screen, "vending:rows")).rows;
}
auto money_of(const ec::screen_view& screen) -> std::string {
    const auto& stats = body<ec::panel_body>(part(screen, "vending:items")).stats;
    const auto found = std::ranges::find(stats, "money", &ec::panel_stat::key);
    REQUIRE(found != stats.end());
    return found->label;
}

} // namespace

TEST_CASE(
    "a vending machine lists its goods and sells the one a client selects",
    "[vending_screen][engine_client_component]") {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    you.setpos(machine_at + tripoint_west);
    auto card = item::spawn("cash_card");
    card->charges = 100000;
    you.i_add(std::move(card));
    auto& here = get_map();
    here.add_item(machine_at, item::spawn("aspirin"));
    here.add_item(machine_at, item::spawn("bandages"));
    const auto stocked =
        here.i_at(machine_at)
        | std::views::transform([](const item* it) { return game_client::item_row_id(*it); })
        | std::ranges::to<std::vector>();
    REQUIRE(stocked.size() == 2);
    const auto guard = screen_guard{};
    auto step = 0;
    auto bought = std::string{};
    open_stack([&] { iexamine::vending(you, machine_at); },
               [&](const snapshot_t& /*snapshot*/, const ec::screen_view& screen) {
                   switch (step++) {
                       case 0: {
                           CHECK(screen.kind == "vending");
                           const auto& rows = rows_of(screen);
                           CHECK((rows | std::views::transform(&ec::row::id)
                                  | std::ranges::to<std::vector>())
                                 == stocked);
                           CHECK(screen.focus_row == stocked.front());
                           CHECK(money_of(screen) == "Money left: $1000.00");
                           CHECK_FALSE(
                               body<ec::text_body>(part(screen, "vending:details")).markup.empty());
                           bought = rows.back().label;
                           return invoke_event(screen, "vending:pick", {.row = stocked.back()});
                       }
                       case 1:
                           CHECK(screen.focus_row == stocked.back());
                           return run_action("CONFIRM");
                       case 2: {
                           const auto& rows = rows_of(screen);
                           REQUIRE(rows.size() == 1);
                           CHECK(rows.front().id == stocked.front());
                           CHECK(money_of(screen) != "Money left: $1000.00");
                           return run_action("QUIT");
                       }
                       default:
                           FAIL("the vending machine outlived its quit");
                           return run_action("QUIT");
                   }
               });
    CHECK(step == 3);
    CHECK(here.i_at(machine_at).size() == 1);
    CHECK(bought.ends_with(item::spawn_temporary("bandages")->tname()));
}

#endif // CATA_MCP

#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "engine_client_component.h"
#    include "perk.h"
#    include "state_helpers.h"

#    include <string>
#    include <variant>

namespace {
namespace ec = engine_client;
using namespace client_screen_test;
} // namespace

TEST_CASE("the perk menu publishes the owned perks", "[perk_screen][engine_client_component]") {
    clear_all_state();
    auto& you = get_avatar();
    const auto owned = perk_id("PROF_DICEMASTER");
    you.add_perk(owned);
    const auto guard = screen_guard{};
    auto step = 0;
    open_stack(
        [&] { draw_perk_menu(you); },
        [&](const snapshot_t& /*snapshot*/, const ec::screen_view& screen) {
            if (step++ == 0) {
                CHECK(screen.kind == "perks");
                REQUIRE(screen.root);
                const auto* const list = ec::find_component(*screen.root, "perks:list");
                REQUIRE(list != nullptr);
                REQUIRE(std::holds_alternative<ec::list_body>(list->body));
                CHECK(std::get<ec::list_body>(list->body).rows.front().label == owned->get_name());
                CHECK(screen.focus_row == "perk:" + owned.str());
                const auto* const info = ec::find_component(*screen.root, "perks:details");
                REQUIRE(info != nullptr);
                CHECK(std::get<ec::text_body>(info->body).markup == owned->get_description());
            }
            return run_action("QUIT");
        });
    CHECK(step == 1);
    you.remove_perk(owned);
}

#endif

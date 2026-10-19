#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "character_id.h"
#    include "client_screen_support.h"
#    include "engine_client_component.h"
#    include "game.h"
#    include "map_helpers.h"
#    include "npc.h"
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

auto rows_of(const ec::screen_view& screen, const std::string& id) -> const std::vector<ec::row>& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    REQUIRE(std::holds_alternative<ec::list_body>(found->body));
    return std::get<ec::list_body>(found->body).rows;
}

} // namespace

TEST_CASE(
    "the debug NPC list names each NPC and monster with where it is",
    "[debug_npcs_screen][engine_client_component]") {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    auto& you = get_avatar();
    you.setpos(tripoint_bub_ms(60, 60, 0));
    const auto& guy = spawn_npc(you.bub_pos() + tripoint_east, "test_talker");
    const auto guy_row = "npc:" + std::to_string(guy.getID().get_value());
    spawn_test_monster("mon_zombie", you.bub_pos() + tripoint(0, 3, 0));
    const auto guard = screen_guard{};
    auto step = 0;
    open_stack([] { g->disp_NPCs(); },
               [&](const snapshot_t& /*snapshot*/, const ec::screen_view& screen) {
                   if (step++ == 0) {
                       CHECK(screen.kind == "debug_npcs");
                       const auto& npcs = rows_of(screen, "debug_npcs:npcs");
                       const auto found = std::ranges::find(npcs, guy_row, &ec::row::id);
                       REQUIRE(found != npcs.end());
                       CHECK(found->label == guy.name);
                       CHECK_FALSE(found->columns.empty());
                       const auto& monsters = rows_of(screen, "debug_npcs:monsters");
                       REQUIRE(monsters.size() == 1);
                       CHECK(monsters.front().label == "zombie");
                       CHECK(monsters.front().columns.front().value == "60, 63, 0");
                   }
                   return run_action("QUIT");
               });
    CHECK(step == 1);
}

#endif // CATA_MCP

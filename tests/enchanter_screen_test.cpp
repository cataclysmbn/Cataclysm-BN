#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "enchantments/enchanter.h"
#    include "engine_client_component.h"
#    include "item.h"
#    include "map/mapdata.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <string>
#    include <variant>

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

} // namespace

TEST_CASE(
    "the enchanter lists its enchantments and what the chosen one needs",
    "[enchanter_screen][engine_client_component]") {
    clear_all_state();
    auto& you = get_avatar();
    auto knife = item::spawn("knife_combat");
    const auto options = furn_str_id("f_cvdmachine")->enchanter;
    REQUIRE(options.size() == 1);
    const auto guard = screen_guard{};
    auto step = 0;
    auto chosen = 0;
    open_stack(
        [&] { chosen = enchanter::enchantment_selector_menu(options, you, *knife); },
        [&](const snapshot_t& /*snapshot*/, const ec::screen_view& screen) {
            if (step++ == 0) {
                CHECK(screen.kind == "enchanter");
                const auto& list = part(screen, "enchanter:options");
                REQUIRE(std::holds_alternative<ec::list_body>(list.body));
                const auto& rows = std::get<ec::list_body>(list.body).rows;
                REQUIRE(rows.size() == 1);
                CHECK(rows.front().id == "enchant:CVD_DIAMOND");
                CHECK(rows.front().label == "Diamond Coating");
                CHECK(screen.focus_row == "enchant:CVD_DIAMOND");
                const auto& info = part(screen, "enchanter:details");
                REQUIRE(std::holds_alternative<ec::text_body>(info.body));
                CHECK(std::get<ec::text_body>(info.body).markup.contains("Time to complete"));
            }
            return run_action("QUIT");
        });
    CHECK(step == 1);
    CHECK(chosen == -1);
}

TEST_CASE(
    "an enchantment a client activates is chosen as Enter chooses it natively",
    "[enchanter_screen][engine_client_component]") {
    clear_all_state();
    auto& you = get_avatar();
    auto knife = item::spawn("knife_combat");
    const auto options = furn_str_id("f_cvdmachine")->enchanter;
    REQUIRE(options.size() == 1);
    const auto guard = screen_guard{};
    auto step = 0;
    auto refused = false;
    open_stack([&] { enchanter::enchantment_selector_menu(options, you, *knife); },
               [&](const snapshot_t& snapshot, const ec::screen_view& screen) {
                   // Without the materials the native choice refuses with a popup.
                   if (snapshot.message.contains("insufficient items")) {
                       refused = true;
                       REQUIRE_FALSE(snapshot.choices.empty());
                       return choose(snapshot, snapshot.choices.front().id);
                   }
                   if (step++ == 0) {
                       return invoke_event(
                           screen, "bn:activate:enchanter:pick", {.row = "enchant:CVD_DIAMOND"});
                   }
                   return run_action("QUIT");
               });
    CHECK(refused);
}

#endif // CATA_MCP

#if defined(CATA_MCP)

#    include "avatar.h"
#    include "bionics.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "client_screen_support.h"
#    include "color.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "engine_client_component.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "item.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "map_item_stack.h"
#    include "mission.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <functional>
#    include <memory>
#    include <string>
#    include <vector>

namespace {

/// Test-only member access reaches the item list, which only the `V` action of the main loop opens.
/// Explicit instantiation permits the member pointer without changing the game header.
struct list_items_tag {
    friend auto list_items_call(list_items_tag, const std::vector<map_item_stack>& stacks) -> void;
};
template <auto Member> struct list_items_access {
    friend auto list_items_call(list_items_tag, const std::vector<map_item_stack>& stacks) -> void {
        static_cast<void>((g.get()->*Member)(stacks));
    }
};
template struct list_items_access<&game::list_items>;

} // namespace

using namespace client_screen_test;

namespace {
auto column(const game_client::interaction_choice& choice, const std::string& key) -> std::string {
    const auto found =
        std::ranges::find(choice.columns, key, &game_client::interaction_column::key);
    REQUIRE(found != choice.columns.end());
    return found->value;
}
} // namespace

TEST_CASE(
    "the mission list switches tabs and makes the chosen mission active",
    "[client][interaction][mcp][screens][info_screens][missions_screen]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    you.reset_all_missions();
    mission* assigned = nullptr;
    for (const mission_type& type : mission_type::get_all()) {
        if (type.goal == MGOAL_ASSASSINATE) {
            assigned = mission::reserve_new(type.id, you.getID());
            break;
        }
    }
    REQUIRE(assigned != nullptr);
    if (assigned->get_assigned_player_id() == you.getID()) {
        you.on_mission_assignment(*assigned);
    } else {
        assigned->assign(you);
    }
    REQUIRE(you.get_active_missions().size() == 1);
    const auto row_id = "mission:" + std::to_string(assigned->get_id());
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [] { g->list_missions(); },
        [&](const auto& /*snapshot*/, const auto& top) {
            namespace ec = engine_client;
            const auto find = [&](const std::string& id) {
                REQUIRE(top.root);
                return ec::find_component(*top.root, id);
            };
            switch (stage++) {
                case 0: {
                    CHECK(top.context == "MISSIONS");
                    const auto* const tabs = find("missions:tabs");
                    REQUIRE(tabs != nullptr);
                    const auto& entries = std::get<ec::tabs_body>(tabs->body).tabs;
                    REQUIRE(entries.size() == 3);
                    CHECK(entries[0].id == "active");
                    CHECK(entries[0].selected);
                    CHECK(entries[1].id == "completed");
                    CHECK(entries[2].id == "failed");
                    const auto* const list = find("missions:list");
                    REQUIRE(list != nullptr);
                    const auto& rows = std::get<ec::list_body>(list->body).rows;
                    REQUIRE(rows.size() == 1);
                    CHECK(rows[0].id == row_id);
                    CHECK(rows[0].label == assigned->name());
                    CHECK(rows[0].color == string_from_color(c_light_green));
                    CHECK(top.focus_row == row_id);
                    const auto* const detail = find("missions:detail");
                    REQUIRE(detail != nullptr);
                    CHECK(std::get<ec::panel_body>(detail->body).title.find(assigned->name()) == 0);
                    return invoke_event(top, "missions:tab", {.row = "completed"});
                }
                case 1: {
                    // Nothing is completed yet: the tab says so and lists no mission.
                    CHECK(find("missions:list") == nullptr);
                    const auto* const none = find("missions:none");
                    REQUIRE(none != nullptr);
                    CHECK(std::get<ec::text_body>(none->body).markup
                          == "You haven't completed any missions!");
                    CHECK(top.focus_row.empty());
                    return invoke_event(top, "missions:tab", {.row = "active"});
                }
                case 2:
                    return invoke_event(top, "missions:select", {.row = row_id});
                default:
                    FAIL("unexpected extra screen " << top.context);
            }
            return input_event{};
        });
    CHECK(stage == 3);
    CHECK(you.get_active_mission() == assigned);
}

TEST_CASE(
    "the item list shows the items around and moves its cursor to a chosen one",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto lighter_pos = you.bub_pos() + tripoint_east;
    const auto knife_pos = you.bub_pos() + tripoint_east * 2;
    get_map().add_item(lighter_pos, item::spawn("lighter", calendar::turn));
    get_map().add_item(knife_pos, item::spawn("knife_combat", calendar::turn));
    const auto& lighter = get_map().i_at(lighter_pos).only_item();
    const auto& knife = get_map().i_at(knife_pos).only_item();
    const auto stacks = std::vector<map_item_stack>{
        map_item_stack(&lighter, tripoint_rel_ms(1, 0, 0)),
        map_item_stack(&knife, tripoint_rel_ms(2, 0, 0)),
    };
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [&] { list_items_call(list_items_tag{}, stacks); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "LIST_ITEMS");
                    const auto items = rows_with(snapshot, "item:");
                    REQUIRE(items.size() == 2);
                    const auto nearest = row(snapshot, items.front());
                    CHECK(nearest->label.find("lighter") != std::string::npos);
                    CHECK(nearest->highlighted);
                    CHECK_FALSE(nearest->description.empty());
                    CHECK(nearest->columns.front().value == "1");
                    CHECK_FALSE(row(snapshot, items.back())->highlighted);
                    return choose(snapshot, items.back());
                }
                case 1: {
                    const auto items = rows_with(snapshot, "item:");
                    const auto farthest = row(snapshot, items.back());
                    CHECK(farthest->label.find("knife") != std::string::npos);
                    CHECK(farthest->highlighted);
                    CHECK_FALSE(row(snapshot, items.front())->highlighted);
                    CHECK(row(snapshot, "action:EXAMINE") != snapshot.choices.end());
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 2);
}

#endif // CATA_MCP

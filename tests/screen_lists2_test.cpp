#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "game.h"
#    include "item.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "mod_manager.h"
#    include "monster.h"
#    include "panels.h"
#    include "player_helpers.h"
#    include "state_helpers.h"
#    include "technique_reference.h"
#    include "vehicle/vehicle.h"
#    include "worldfactory.h"

#    include <string>
#    include <vector>

namespace {

using namespace client_screen_test;

/// Test-only member access reaches the item, monster and vehicle lists, which only the `V` action
/// of the main loop opens. Explicit instantiation permits the member pointer without changing the
/// game header.
struct nearby_lists_tag {
    friend auto nearby_lists_member(nearby_lists_tag);
};
template <auto Member> struct nearby_lists_access {
    friend auto nearby_lists_member(nearby_lists_tag) { return Member; }
};
template struct nearby_lists_access<&game::list_items_monsters>;

auto open_nearby_lists() -> void { (g.get()->*nearby_lists_member(nearby_lists_tag{}))(); }

} // namespace

TEST_CASE(
    "the monster list lists the visible monsters and moves its cursor to a chosen one",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    spawn_test_monster("mon_zombie", you.bub_pos() + tripoint_east);
    spawn_test_monster("mon_zombie", you.bub_pos() + tripoint_east * 3);
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(open_nearby_lists, [&](const auto& snapshot) {
        switch (stage++) {
            case 0: {
                CHECK(snapshot.context == "LIST_MONSTERS");
                const auto monsters = rows_with(snapshot, "monster:");
                REQUIRE(monsters.size() == 2);
                const auto nearest = row(snapshot, monsters.front());
                CHECK(nearest->highlighted);
                CHECK_FALSE(nearest->description.empty());
                CHECK(nearest->columns.size() == 6);
                CHECK_FALSE(row(snapshot, monsters.back())->highlighted);
                CHECK(row(snapshot, "action:SAFEMODE_BLACKLIST_ADD") != snapshot.choices.end());
                return choose(snapshot, monsters.back());
            }
            case 1: {
                const auto monsters = rows_with(snapshot, "monster:");
                CHECK(row(snapshot, monsters.back())->highlighted);
                CHECK_FALSE(row(snapshot, monsters.front())->highlighted);
                CHECK_FALSE(row(snapshot, monsters.back())->description.empty());
                return cancel(snapshot);
            }
            default:
                FAIL("unexpected extra screen " << snapshot.context);
        }
        return input_event{};
    });
    CHECK(stage == 2);
}

TEST_CASE(
    "the vehicle list lists the visible vehicles with their state",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    auto* const cart = get_map().add_vehicle(
        vproto_id("shopping_cart"), you.bub_pos() + tripoint_east, 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(open_nearby_lists, [&](const auto& snapshot) {
        switch (stage++) {
            case 0: {
                CHECK(snapshot.context == "LIST_VEHICLES");
                const auto vehicles = rows_with(snapshot, "vehicle:");
                REQUIRE(vehicles.size() == 1);
                const auto listed = row(snapshot, vehicles.front());
                CHECK(listed->label == cart->name);
                CHECK(listed->highlighted);
                CHECK(listed->description.find("Engine") != std::string::npos);
                CHECK(listed->columns.front().value == "1");
                CHECK(row(snapshot, "action:NEXT_TAB") != snapshot.choices.end());
                return choose(snapshot, vehicles.front());
            }
            case 1: {
                CHECK(row(snapshot, "vehicle:0")->highlighted);
                return cancel(snapshot);
            }
            default:
                FAIL("unexpected extra screen " << snapshot.context);
        }
        return input_event{};
    });
    CHECK(stage == 2);
}

TEST_CASE(
    "the technique reference shows its text and closes on a cancel",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [] { show_technique_reference_popup("crit only: needs a critical hit"); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "MARTIAL_ARTS");
                    CHECK(snapshot.title == "Technique Reference");
                    CHECK(snapshot.message == "crit only: needs a critical hit");
                    CHECK(snapshot.allow_cancel);
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 1);
}

TEST_CASE(
    "the sidebar panel manager toggles a chosen panel and lists the layouts",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    const auto guard = screen_guard{};
    auto stage = 0;
    auto toggled = std::string{};
    auto was_selected = false;
    open_screen(
        [] { panel_manager::get_manager().show_adm(); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "PANEL_MGMT");
                    const auto panels = rows_with(snapshot, "panel:");
                    REQUIRE_FALSE(panels.empty());
                    CHECK_FALSE(rows_with(snapshot, "layout:").empty());
                    CHECK(row(snapshot, "action:MOVE_PANEL") != snapshot.choices.end());
                    toggled = panels.front();
                    was_selected = row(snapshot, toggled)->selected;
                    CHECK(row(snapshot, toggled)->highlighted);
                    return choose(snapshot, toggled);
                }
                case 1: {
                    CHECK(row(snapshot, toggled)->selected == !was_selected);
                    // Toggling it back leaves the saved layout as it was found.
                    return choose(snapshot, toggled);
                }
                case 2: {
                    CHECK(row(snapshot, toggled)->selected == was_selected);
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 3);
}

TEST_CASE(
    "the active world mods list shows the mods and their description for the cursor",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    REQUIRE(world_generator->active_world != nullptr);
    const auto mods = world_generator->active_world->info->active_mod_order;
    REQUIRE_FALSE(mods.empty());
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [&] { world_generator->show_active_world_mods(mods); },
        [&](const auto& snapshot) {
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "DEFAULT");
                    const auto rows = rows_with(snapshot, "mod:");
                    REQUIRE(rows.size() == mods.size());
                    CHECK(row(snapshot, rows.front())->label == mods.front()->name());
                    CHECK(row(snapshot, rows.front())->highlighted);
                    CHECK_FALSE(row(snapshot, rows.front())->description.empty());
                    return choose(snapshot, rows.back());
                }
                case 1: {
                    CHECK(row(snapshot, "mod:" + std::to_string(mods.size() - 1))->highlighted);
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 2);
}

TEST_CASE(
    "the mod manager lists the available and active mods and marks an active one before moving it",
    "[client][interaction][mcp][screens]") {
    clear_all_state();
    clear_map();
    REQUIRE(world_generator->active_world != nullptr);
    const auto active_before = world_generator->active_world->info->active_mod_order.size();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_screen(
        [] { world_generator->edit_active_world_mods(&*world_generator->active_world->info); },
        [&](const auto& snapshot) {
            if (snapshot.context == "YESNO") {
                // Leaving asks whether to save the changes: no.
                const auto no = std::ranges::
                    find(snapshot.choices, "NO", &game_client::interaction_choice::description);
                REQUIRE(no != snapshot.choices.end());
                return choose(snapshot, no->id);
            }
            switch (stage++) {
                case 0: {
                    CHECK(snapshot.context == "MODMANAGER_DIALOG");
                    CHECK_FALSE(rows_with(snapshot, "tab:").empty());
                    CHECK(rows_with(snapshot, "active:").size() == active_before);
                    CHECK(row(snapshot, "action:FILTER") != snapshot.choices.end());
                    CHECK(row(snapshot, "tab:0")->selected);
                    return choose(snapshot, "active:0");
                }
                case 1: {
                    // The first choice only marks the active mod; it stays active.
                    CHECK(rows_with(snapshot, "active:").size() == active_before);
                    CHECK(row(snapshot, "active:0")->highlighted);
                    CHECK_FALSE(row(snapshot, "active:0")->description.empty());
                    return cancel(snapshot);
                }
                default:
                    FAIL("unexpected extra screen " << snapshot.context);
            }
            return input_event{};
        });
    CHECK(stage == 2);
    CHECK(world_generator->active_world->info->active_mod_order.size() == active_before);
}

#endif // CATA_MCP

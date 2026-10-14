#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "game.h"
#    include "item.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "monster.h"
#    include "panels.h"
#    include "player_helpers.h"
#    include "state_helpers.h"
#    include "technique_reference.h"
#    include "vehicle/vehicle.h"
#    include "weather/weather.h"

#    include <string>
#    include <vector>

namespace {

using namespace client_screen_test;

/// Test-only member access reaches the item, monster and vehicle lists, which only the `V` action
/// of the main loop opens. Explicit instantiation permits the member pointer without changing the
/// game header.
struct nearby_lists_tag {
    friend auto nearby_lists_call(nearby_lists_tag) -> void;
};
template <auto Member> struct nearby_lists_access {
    friend auto nearby_lists_call(nearby_lists_tag) -> void { (g.get()->*Member)(); }
};
template struct nearby_lists_access<&game::list_items_monsters>;

auto open_nearby_lists() -> void { nearby_lists_call(nearby_lists_tag{}); }

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
    // Sight needs daylight and the main loop refreshes the lighting and visibility caches before
    // it reads the `V` key; clear_all_state leaves midnight and stale caches.
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    g->m.invalidate_map_cache(0);
    g->m.build_map_cache(0);
    g->refresh_player_visibility_cache_if_needed();
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

#endif // CATA_MCP

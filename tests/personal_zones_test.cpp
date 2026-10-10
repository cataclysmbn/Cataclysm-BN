#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "clzones.h"
#include "item.h"
#include "map/map.h"
#include "map_helpers.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

#include <algorithm>

static const activity_id ACT_MOVE_LOOT("ACT_MOVE_LOOT");

TEST_CASE("personal loot zones follow the avatar until a sort pins them", "[zones][personal]") {
    clear_all_state();

    avatar& you = get_avatar();
    auto& mgr = zone_manager::get_manager();
    const auto cleanup = on_out_of_scope([&mgr]() {
        mgr.clear_sort_filter();
        for (;;) {
            auto listed = mgr.get_zones();
            const auto found = std::ranges::find_if(listed, [](const auto& ref) {
                return ref.get().get_is_personal();
            });
            if (found == listed.end()) { break; }
            mgr.remove(found->get());
        }
    });
    const auto origin = tripoint_bub_ms{60, 60, 0};
    you.setpos(origin);

    const auto start = tripoint_rel_ms{-1, -1, 0};
    const auto end = tripoint_rel_ms{1, 0, 0};
    mgr.add("personal unsorted", zone_type_id("LOOT_UNSORTED"), faction_id("your_followers"), false,
            true, start, end);

    const auto placed_at = you.abs_pos();
    const auto* zone = mgr.get_zone_at(placed_at + start, zone_type_id("LOOT_UNSORTED"));
    REQUIRE(zone != nullptr);
    CHECK(zone->get_is_personal());
    CHECK(zone->get_start_point() == placed_at + start);
    CHECK(zone->get_end_point() == placed_at + end);

    you.setpos(origin + tripoint_rel_ms{4, 0, 0});
    const auto moved_to = you.abs_pos();
    CHECK(mgr.get_zone_at(moved_to + start, zone_type_id("LOOT_UNSORTED")) != nullptr);
    CHECK(mgr.get_zone_at(placed_at + start, zone_type_id("LOOT_UNSORTED")) == nullptr);

    mgr.apply_sort_filter(loot_sort_selection::personal_only, moved_to);
    you.setpos(origin + tripoint_rel_ms{8, 0, 0});
    CHECK(mgr.get_zone_at(moved_to + start, zone_type_id("LOOT_UNSORTED")) != nullptr);
    CHECK(mgr.get_zone_at(you.abs_pos() + start, zone_type_id("LOOT_UNSORTED")) == nullptr);

    const auto player_points = mgr.get_point_set_loot(moved_to, 5, false);
    const auto npc_points = mgr.get_point_set_loot(moved_to, 5, true);
    CHECK_FALSE(player_points.empty());
    CHECK(npc_points.empty());

    mgr.clear_sort_filter();
    CHECK(mgr.get_zone_at(you.abs_pos() + start, zone_type_id("LOOT_UNSORTED")) != nullptr);
    CHECK_FALSE(mgr.personal_zones_are_pinned());

    mgr.apply_sort_filter(loot_sort_selection::regular_only, you.abs_pos());
    CHECK(mgr.get_near(zone_type_id("LOOT_UNSORTED"), you.abs_pos(), 5).empty());
    mgr.clear_sort_filter();
    CHECK_FALSE(mgr.get_near(zone_type_id("LOOT_UNSORTED"), you.abs_pos(), 5).empty());
}

TEST_CASE(
    "personal sort keeps going past the first item and walks the rest of the zone",
    "[zones][personal]") {
    clear_all_state();

    avatar& you = get_avatar();
    you.setpos(tripoint_bub_ms{60, 60, 0});
    auto& mgr = zone_manager::get_manager();
    const auto cleanup = on_out_of_scope([&you, &mgr]() {
        you.clear_destination();
        you.cancel_activity();
        mgr.clear_sort_filter();
        for (;;) {
            auto listed = mgr.get_zones();
            const auto found = std::ranges::find_if(listed, [](const auto& ref) {
                return ref.get().get_is_personal();
            });
            if (found == listed.end()) { break; }
            mgr.remove(found->get());
        }
    });

    const auto unsorted_start = tripoint_rel_ms{1, 0, 0};
    const auto unsorted_end = tripoint_rel_ms{4, 0, 0};
    mgr.add("personal unsorted", zone_type_id("LOOT_UNSORTED"), faction_id("your_followers"), false,
            true, unsorted_start, unsorted_end);
    mgr.add("personal dump", zone_type_id("LOOT_DUMP"), faction_id("your_followers"), false, true,
            tripoint_rel_ms{0, 1, 0}, tripoint_rel_ms{0, 1, 0});

    map& here = get_map();
    const auto adjacent = you.bub_pos() + unsorted_start;
    const auto far = you.bub_pos() + unsorted_end;
    here.add_item_or_charges(adjacent, item::spawn("test_rock"));
    here.add_item_or_charges(adjacent, item::spawn("test_rock"));
    here.add_item_or_charges(far, item::spawn("test_rock"));
    REQUIRE(here.i_at(adjacent).size() >= 1);
    REQUIRE(here.i_at(far).size() == 1);

    const auto pin = you.abs_pos();
    mgr.apply_sort_filter(loot_sort_selection::personal_only, pin);
    auto act = std::make_unique<player_activity>(ACT_MOVE_LOOT, calendar::INDEFINITELY_LONG);
    act->values = {
        0, static_cast<int>(loot_sort_selection::personal_only), pin.x(), pin.y(), pin.z(),
    };
    you.assign_activity(std::move(act), false);
    you.set_moves(10000);
    you.activity->do_turn(you);

    REQUIRE(you.activity);
    CHECK(you.activity->id() == ACT_MOVE_LOOT);
    CHECK(here.i_at(adjacent).empty());
    CHECK(here.i_at(far).size() == 1);
    CHECK(mgr.personal_zones_are_pinned());

    you.set_moves(100);
    you.activity->do_turn(you);
    REQUIRE(you.has_destination());
    CHECK(you.get_destination_activity().id() == ACT_MOVE_LOOT);
    CHECK(mgr.personal_zones_are_pinned());

    REQUIRE(you.destination_point.has_value());
    you.setpos(*you.destination_point);
    REQUIRE(you.has_destination_activity());
    you.start_destination_activity();
    you.set_moves(10000);
    REQUIRE(you.activity);
    CHECK(you.activity->id() == ACT_MOVE_LOOT);
    you.activity->do_turn(you);
    CHECK(here.i_at(far).empty());
    CHECK(mgr.personal_zones_are_pinned());
}

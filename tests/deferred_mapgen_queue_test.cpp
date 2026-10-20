#include "auto_note.h"
#include "calendar.h"
#include "catalua.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "clzones.h"
#include "coordinates.h"
#include "init.h"
#include "map/map.h"
#include "map_helpers.h"
#include "mapgen/map_extras.h"
#include "mapgen/mapgen_async.h"
#include "options_helpers.h"
#include "overmap/overmapbuffer.h"
#include "overmap/overmapbuffer_registry.h"
#include "state_helpers.h"

#include <ranges>
#include <string>
#include <vector>

// Workers queue these as they finish, in an order no run repeats; each queue applies in one order.

TEST_CASE(
    "deferred mapgen zones are added in one order however workers finish", "[mapgen][zones]") {
    const auto start = [](const int x) { return tripoint_abs_ms(x, 0, 0); };
    const auto added_in = [&](const std::vector<int>& finished) {
        zone_manager::reset_manager();
        for (const auto x : finished) {
            defer_zone_add("deferred", zone_type_id("LOOT_UNSORTED"), your_fac, false, true,
                           start(x), start(x) + tripoint_rel_ms(1, 1, 0));
        }
        flush_deferred_zones();
        return zone_manager::get_manager().get_zones()
             | std::views::transform([](const auto& zone) { return zone.get().get_start_point(); })
             | std::ranges::to<std::vector>();
    };
    const auto cleanup = on_out_of_scope([]() { zone_manager::reset_manager(); });
    CHECK(added_in({30, 10, 20}) == added_in({20, 30, 10}));
}

TEST_CASE("deferred map extra notes are placed in one order however workers finish", "[mapgen]") {
    clear_all_state();
    const auto notes = override_option("AUTO_NOTES", "true");
    const auto extras = override_option("AUTO_NOTES_MAP_EXTRAS", "true");
    const auto extra = string_id<map_extra>("mx_burned_ground");
    REQUIRE(extra.is_valid());
    auto& settings = get_auto_notes_settings();
    const auto was_enabled = settings.has_auto_note_enabled(extra);
    const auto restore_setting = on_out_of_scope([&]() {
        settings.set_auto_note_status(extra, was_enabled);
    });
    settings.set_auto_note_status(extra, true);
    const auto dim = get_map().get_bound_dimension();
    auto& overmaps = get_overmapbuffer(dim);
    const auto place = [](const int x) { return tripoint_abs_omt(x, 0, 0); };
    const auto finished_orders = std::vector<std::vector<int>>{{3, 1, 2}, {2, 3, 1}};
    const auto clear_notes = [&]() {
        for (const auto x : finished_orders.front()) { overmaps.delete_note(place(x)); }
    };
    const auto cleanup = on_out_of_scope(clear_notes);
    const auto noted_in = [&](const std::vector<int>& finished) {
        clear_notes();
        for (const auto x : finished) {
            push_deferred_autonote({.dim = dim, .omt_pos = place(x), .extra_id = extra.str()});
        }
        run_deferred_autonotes();
        return overmaps.get_all_notes(0)
             | std::views::transform([](const auto& note) { return note.first; })
             | std::ranges::to<std::vector>();
    };
    const auto first = noted_in(finished_orders.front());
    REQUIRE(first.size() == 3);
    CHECK(first == noted_in(finished_orders.back()));
}

TEST_CASE("deferred mapgen hooks run in one order however workers finish", "[mapgen][lua]") {
    clear_all_state();
    clear_map();
    auto& state = *DynamicDataLoader::get_instance().lua;
    auto hooks = state.lua["game"]["hooks"].get<sol::table>();
    const auto previous = hooks["on_mapgen_postprocess"].get<sol::object>();
    const auto restore = on_out_of_scope([&]() {
        hooks["on_mapgen_postprocess"] = previous;
        refresh_mapgen_postprocess_hook_presence(state);
    });
    auto ran = std::vector<int>{};
    auto recorder = state.lua.create_table();
    recorder[1] = [&](sol::table params) {
        ran.push_back(to_turns<int>(params.get<time_point>("when") - calendar::turn_zero));
    };
    hooks["on_mapgen_postprocess"] = recorder;
    refresh_mapgen_postprocess_hook_presence(state);
    REQUIRE(mapgen_hooks_registered());
    // Generated squares of the loaded map, so the hooks bind resident submaps.
    const auto origin = tripoint_abs_omt(project_to<coords::omt>(get_map().get_abs_sub()), 0);
    const auto dim = get_map().get_bound_dimension();
    const auto ran_in = [&](const std::vector<int>& finished) {
        ran.clear();
        for (const auto x : finished) {
            push_deferred_mapgen_hook(
                {.dim = dim,
                 .omt_pos = origin + tripoint_rel_omt(x, 0, 0),
                 .when = calendar::turn_zero + time_duration::from_turns(x)});
        }
        run_deferred_mapgen_hooks();
        return ran;
    };
    const auto first = ran_in({3, 1, 2});
    REQUIRE(first.size() == 3);
    CHECK(first == ran_in({2, 3, 1}));
}

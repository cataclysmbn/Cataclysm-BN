#include "../src/overmap/overmap.h"
#include "../src/overmap/overmapbuffer.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "debug.h"
#include "enums.h"
#include "fluid_grid.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "map/map.h"
#include "map_helpers.h"
#include "numeric_interval.h"
#include "overmap/omdata.h"
#include "overmap/overmap_special.h"
#include "overmap/overmap_types.h"
#include "regional_settings.h"
#include "rng.h"
#include "state_helpers.h"
#include "type_id.h"

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <vector>

TEST_CASE("city building selection preserves empty bins", "[overmap][city]") {
    auto settings = city_settings{};
    const auto bins =
        {&settings.houses,      &settings.urban_houses, &settings.shops,
         &settings.urban_shops, &settings.parks,        &settings.finales};
    const auto special = overmap_special_id("test_crater");
    REQUIRE(special.is_valid());
    auto expected = overmap_special_id("null");

    SECTION("empty bins") {}
    SECTION("zero weight bins") {
        for (auto* bin : bins) { bin->add(special, 0); }
    }
    SECTION("populated bins") {
        expected = special;
        for (auto* bin : bins) { bin->add(special, 1); }
    }
    for (auto* bin : bins) { bin->finalize(); }

    const auto messages = capture_debugmsg_during([&]() {
        CHECK(settings.pick_house() == expected);
        CHECK(settings.pick_urban_house() == expected);
        CHECK(settings.pick_shop() == expected);
        CHECK(settings.pick_urban_shop() == expected);
        CHECK(settings.pick_park() == expected);
        CHECK(settings.pick_finale() == expected);
    });
    CHECK(messages.empty());
}

TEST_CASE("set_and_get_overmap_scents", "[overmap]") {
    clear_all_state();
    std::unique_ptr<overmap> test_overmap = std::make_unique<overmap>(point_abs_om());

    // By default there are no scents set.
    for (const tripoint_abs_omt& pos :
         std::array<tripoint_abs_omt, 4>{{{0, 0, 0}, {179, 179, 9}, {90, 90, -9}, {75, 85, 0}}}) {
        REQUIRE(test_overmap->scent_at(pos).creation_time == calendar::before_time_starts);
    }

    const time_point tp = calendar::turn_zero + time_duration::from_turns(50);
    scent_trace test_scent(tp, 90);
    test_overmap->set_scent({75, 85, 0}, test_scent);
    REQUIRE(test_overmap->scent_at({75, 85, 0}).creation_time == tp);
    REQUIRE(test_overmap->scent_at({75, 85, 0}).initial_strength == 90);
}

TEST_CASE("fluid grids preserve multiple liquid types", "[overmap][fluid_grid]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([] { clear_all_state(); });
    clear_map();
    auto& here = get_map();
    const auto tank_pos = tripoint_bub_ms{g_half_mapsize_x, g_half_mapsize_y, 0};
    const auto water_tank_pos = tank_pos + point_south;
    const auto autofill_tank_pos = tank_pos + point_east;
    const auto sink_pos = tank_pos + point_west;
    const auto tank_abs_ms = map_local_to_abs(here, tank_pos);
    const auto water_tank_abs_ms = map_local_to_abs(here, water_tank_pos);
    const auto autofill_tank_abs_ms = map_local_to_abs(here, autofill_tank_pos);
    const auto sink_abs_ms = map_local_to_abs(here, sink_pos);
    const auto tank_abs_omt = project_to<coords::omt>(tank_abs_ms);
    const auto grid_node_abs_omt = tank_abs_omt + tripoint_rel_omt{1, 0, 0};
    const auto gasoline = itype_id("gasoline");
    const auto water = itype_id("water");
    const auto diesel = itype_id("diesel");
    const auto& tank_liquids = furn_id("f_standing_tank_plumbed").obj().fluid_grid;
    REQUIRE(tank_liquids.has_value());
    CHECK(tank_liquids->universal_liquids);
    CHECK(tank_liquids->allows_liquid(gasoline));
    CHECK(tank_liquids->autofill);
    const auto& sink_liquids = furn_id("f_sink").obj().fluid_grid;
    REQUIRE(sink_liquids.has_value());
    CHECK(sink_liquids->allows_liquid(water));
    CHECK_FALSE(sink_liquids->allows_liquid(gasoline));
    CHECK_FALSE(sink_liquids->universal_liquids);
    const auto& shower_liquids = furn_id("f_shower").obj().fluid_grid;
    REQUIRE(shower_liquids.has_value());
    CHECK_FALSE(shower_liquids->allows_liquid(gasoline));
    fluid_grid::load(here);
    here.furn_set(tank_pos, furn_id("f_standing_tank_plumbed"));
    here.furn_set(water_tank_pos, furn_id("f_standing_tank_plumbed"));
    here.furn_set(autofill_tank_pos, furn_id("f_standing_tank_plumbed"));
    here.furn_set(sink_pos, furn_id("f_sink"));
    fluid_grid::on_structure_changed(tank_abs_ms);
    fluid_grid::on_structure_changed(water_tank_abs_ms);
    fluid_grid::on_structure_changed(autofill_tank_abs_ms);
    REQUIRE(fluid_grid::add_grid_connection(tank_abs_omt, grid_node_abs_omt));

    REQUIRE(fluid_grid::storage_stats_at(grid_node_abs_omt).capacity > 0_ml);
    REQUIRE(fluid_grid::assign_tank_liquid(tank_abs_ms, gasoline));
    REQUIRE(fluid_grid::assign_tank_liquid(water_tank_abs_ms, water));
    CHECK(fluid_grid::add_liquid_charges(grid_node_abs_omt, diesel, 10) == 10);
    CHECK(fluid_grid::assign_tank_liquid(autofill_tank_abs_ms, diesel));
    CHECK_FALSE(fluid_grid::assign_tank_liquid(autofill_tank_abs_ms, gasoline));
    const auto gasoline_added = fluid_grid::add_liquid_charges(grid_node_abs_omt, gasoline, 500);
    const auto water_added = fluid_grid::add_liquid_charges(grid_node_abs_omt, water, 10);

    CHECK(gasoline_added == 500);
    CHECK(water_added == 10);
    CHECK_FALSE(fluid_grid::assign_tank_liquid(tank_abs_ms, water));
    CHECK(fluid_grid::liquid_charges_at(tank_abs_omt, gasoline) == 500);
    CHECK(fluid_grid::liquid_charges_at(tank_abs_omt, water) == 10);
    CHECK(fluid_grid::liquid_charges_at(tank_abs_omt, diesel) == 10);
    CHECK_FALSE(fluid_grid::set_tank_assigned_liquid(tank_abs_ms, water));
    CHECK_FALSE(fluid_grid::unassign_tank_liquid(tank_abs_ms));
    const auto spare_pos = tank_pos + point_north;
    const auto spare_abs = map_local_to_abs(here, spare_pos);
    here.furn_set(spare_pos, furn_id("f_standing_tank_plumbed"));
    fluid_grid::on_structure_changed(spare_abs);
    REQUIRE(fluid_grid::assign_tank_liquid(spare_abs, gasoline));
    CHECK(fluid_grid::unassign_tank_liquid(tank_abs_ms));
    CHECK(here.furn_vars(tank_pos)->get("fluid_grid_assigned_liquid", "").empty());
    CHECK(fluid_grid::set_tank_assigned_liquid(tank_abs_ms, water));
    CHECK(here.furn_vars(tank_pos)->get("fluid_grid_assigned_liquid", "") == "water");
    CHECK_FALSE(fluid_grid::set_tank_assigned_liquid(tank_abs_ms, water));
    CHECK_FALSE(fluid_grid::set_tank_assigned_liquid(tank_abs_ms, gasoline));
    CHECK(fluid_grid::liquid_charges_at(tank_abs_omt, gasoline) == 500);
    CHECK(fluid_grid::liquid_charges_at(tank_abs_omt, water) == 10);
    CHECK(fluid_grid::set_fixture_assigned_liquid(sink_abs_ms, water));
    CHECK(here.furn_vars(sink_pos)->get("fluid_grid_assigned_liquid", "") == "water");
    CHECK_FALSE(fluid_grid::set_fixture_assigned_liquid(sink_abs_ms, water));
    CHECK_FALSE(fluid_grid::set_fixture_assigned_liquid(sink_abs_ms, gasoline));
    CHECK(fluid_grid::unassign_fixture_liquid(sink_abs_ms));
    CHECK(here.furn_vars(sink_pos)->get("fluid_grid_assigned_liquid", "").empty());
    CHECK_FALSE(fluid_grid::unassign_fixture_liquid(sink_abs_ms));
    CHECK(fluid_grid::set_fixture_assigned_liquid(sink_abs_ms, water));

    auto& owning_overmap =
        *get_overmapbuffer(here.get_bound_dimension()).get_om_global(tank_abs_omt).om;
    const auto owning_omc =
        get_overmapbuffer(here.get_bound_dimension()).get_om_global(tank_abs_omt);
    fluid_grid::storage_for(owning_overmap)[owning_omc.local].capacity = 0_ml;
    auto saved_data = std::ostringstream{};
    owning_overmap.serialize(saved_data);
    fluid_grid::clear();
    auto loaded_data = std::istringstream{saved_data.str()};
    owning_overmap.unserialize(loaded_data, "multifluid grid save test");
    fluid_grid::load(here);

    CHECK(fluid_grid::storage_stats_at(grid_node_abs_omt).capacity > 0_ml);
    CHECK(fluid_grid::liquid_charges_at(grid_node_abs_omt, gasoline) == 500);
    CHECK(fluid_grid::liquid_charges_at(grid_node_abs_omt, water) == 10);
    CHECK(fluid_grid::liquid_charges_at(grid_node_abs_omt, diesel) == 10);
}

TEST_CASE("fluid grid pumps respect liquid capacity while purifying", "[overmap][fluid_grid]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([] { clear_all_state(); });
    clear_map();
    auto& here = get_map();
    const auto tank_pos = tripoint_bub_ms{g_half_mapsize_x, g_half_mapsize_y, 0};
    const auto clean_pos = tank_pos + point_east;
    const auto pump_pos = tank_pos + point_south;
    const auto purifier_pos = clean_pos + point_south;
    const auto tank_abs = map_local_to_abs(here, tank_pos);
    const auto clean_abs = map_local_to_abs(here, clean_pos);
    const auto grid = project_to<coords::omt>(tank_abs);
    const auto water = itype_id("water");
    const auto clean_water = itype_id("water_clean");
    fluid_grid::load(here);
    here.furn_set(tank_pos, furn_id("test_fluid_tank"));
    here.furn_set(clean_pos, furn_id("test_fluid_tank"));
    here.furn_set(pump_pos, furn_id("test_fluid_pump"));
    here.furn_set(purifier_pos, furn_id("test_fluid_purifier"));
    for (const auto& pos : {tank_pos, clean_pos, pump_pos, purifier_pos}) {
        fluid_grid::on_structure_changed(map_local_to_abs(here, pos));
    }
    REQUIRE(fluid_grid::assign_tank_liquid(tank_abs, water));
    REQUIRE(fluid_grid::assign_tank_liquid(clean_abs, clean_water));
    REQUIRE(fluid_grid::storage_stats_at(grid).capacity == 20_liter);

    SECTION("Full water tank blocks pumping but allows purification") {
        REQUIRE(fluid_grid::add_liquid_charges(grid, water, 40) == 40);
        fluid_grid::process_transformers_at(grid, calendar::turn + 10_minutes);
        const auto stats = fluid_grid::storage_stats_at(grid);
        CHECK(stats.stored_for(water) == 9_liter);
        CHECK(stats.stored_for(clean_water) == 1_liter);
    }
    SECTION("Partially full tanks never overflow during combined processing") {
        REQUIRE(fluid_grid::add_liquid_charges(grid, water, 36) == 36);
        REQUIRE(fluid_grid::add_liquid_charges(grid, clean_water, 39) == 39);
        fluid_grid::process_transformers_at(grid, calendar::turn + 10_minutes);
        const auto stats = fluid_grid::storage_stats_at(grid);
        CHECK(stats.stored_for(water) <= 10_liter);
        CHECK(stats.stored_for(clean_water) <= 10_liter);
        CHECK(stats.stored_for(water) + stats.stored_for(clean_water) <= 20_liter);
    }
    SECTION("Full tanks do not bank production for later") {
        REQUIRE(fluid_grid::add_liquid_charges(grid, water, 40) == 40);
        REQUIRE(fluid_grid::add_liquid_charges(grid, clean_water, 40) == 40);
        fluid_grid::process_transformers_at(grid, calendar::turn + 1_hours);
        REQUIRE(fluid_grid::drain_liquid_charges(grid, water, 40) == 40);
        fluid_grid::process_transformers_at(grid, calendar::turn + 70_minutes);
        CHECK(fluid_grid::storage_stats_at(grid).stored_for(water) == 5_liter);
    }
}

TEST_CASE("fluid tank removal respects assigned liquid storage", "[overmap][fluid_grid]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([] { clear_all_state(); });
    clear_map();
    auto& here = get_map();
    const auto tank_pos = tripoint_bub_ms{g_half_mapsize_x, g_half_mapsize_y, 0};
    const auto spare_pos = tank_pos + point_east;
    const auto tank_abs = map_local_to_abs(here, tank_pos);
    const auto spare_abs = map_local_to_abs(here, spare_pos);
    const auto grid = project_to<coords::omt>(tank_abs);
    const auto water = itype_id("water");
    const auto clean = itype_id("water_clean");
    fluid_grid::load(here);
    here.furn_set(tank_pos, furn_id("test_fluid_tank"));
    here.furn_set(spare_pos, furn_id("test_fluid_tank"));
    fluid_grid::on_structure_changed(tank_abs);
    fluid_grid::on_structure_changed(spare_abs);
    REQUIRE(fluid_grid::assign_tank_liquid(tank_abs, water));
    REQUIRE(fluid_grid::add_liquid_charges(grid, water, 40) == 40);

    SECTION("Space assigned to another liquid cannot replace the tank") {
        REQUIRE(fluid_grid::assign_tank_liquid(spare_abs, clean));
        CHECK_FALSE(fluid_grid::unassign_tank_liquid(tank_abs));
        CHECK(here.furn_vars(tank_pos)->get("fluid_grid_assigned_liquid", "") == "water");
        CHECK(fluid_grid::liquid_charges_at(grid, water) == 40);
    }
    SECTION("Autofill storage takes over before unassigning") {
        CHECK(fluid_grid::unassign_tank_liquid(tank_abs));
        CHECK(here.furn_vars(tank_pos)->get("fluid_grid_assigned_liquid", "").empty());
        CHECK(here.furn_vars(spare_pos)->get("fluid_grid_assigned_liquid", "") == "water");
        CHECK(fluid_grid::liquid_charges_at(grid, water) == 40);
    }
    SECTION("Smashing spills only the destroyed tank's liquid") {
        REQUIRE(fluid_grid::assign_tank_liquid(spare_abs, clean));
        REQUIRE(fluid_grid::add_liquid_charges(grid, clean, 4) == 4);
        here.destroy_furn(tank_pos, true);
        CHECK(fluid_grid::liquid_charges_at(grid, water) == 0);
        CHECK(fluid_grid::liquid_charges_at(grid, clean) == 4);
        for (const auto* it : here.i_at(tank_pos)) {
            CHECK(it->typeId() != water);
            CHECK(it->typeId() != clean);
        }
        CHECK(here.get_field_intensity(tank_pos, field_type_id("fd_water")) > 0);
    }
}

TEST_CASE(
    "destroyed standing tank releases at most its 300 liter capacity", "[overmap][fluid_grid]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([] { clear_all_state(); });
    clear_map();
    auto& here = get_map();
    const auto tank_pos = tripoint_bub_ms{g_half_mapsize_x, g_half_mapsize_y, 0};
    const auto spare_pos = tank_pos + point_east;
    const auto tank_abs = map_local_to_abs(here, tank_pos);
    const auto grid = project_to<coords::omt>(tank_abs);
    const auto water = itype_id("water");
    fluid_grid::load(here);
    for (const auto& pos : {tank_pos, spare_pos}) {
        here.furn_set(pos, furn_id("test_fluid_tank_300l"));
        const auto abs = map_local_to_abs(here, pos);
        fluid_grid::on_structure_changed(abs);
        REQUIRE(fluid_grid::assign_tank_liquid(abs, water));
    }
    REQUIRE(fluid_grid::add_liquid_charges(grid, water, 2400) == 2400);
    fluid_grid::on_tank_removed(tank_abs);
    auto spilled = 0_ml;
    for (const auto* it : here.i_at(tank_pos)) {
        if (it->typeId() == water) { spilled += it->volume(); }
    }
    CHECK(spilled == 300_liter);
    CHECK(fluid_grid::storage_stats_at(grid).stored_for(water) == 300_liter);
}

TEST_CASE("default_overmap_generation_always_succeeds", "[overmap][slow]") {
    clear_all_state();
    auto overmaps_to_construct = 3;
    for (const point_abs_om& candidate_addr : closest_points_first(point_abs_om(), 10)) {
        // Skip populated overmaps.
        if (ACTIVE_OVERMAP_BUFFER.has(candidate_addr)) { continue; }
        overmap_special_batch test_specials = overmap_specials::get_default_batch(candidate_addr);
        ACTIVE_OVERMAP_BUFFER.create_custom_overmap(candidate_addr, test_specials);
        for (const auto& special_placement : test_specials) {
            auto special = special_placement.special_details;
            if (special->has_flag("UNIQUE") || special->has_flag("GLOBALLY_UNIQUE")) { continue; }
            INFO(
                "In attempt #" << overmaps_to_construct << " failed to place " << special->id.str());
            int min_occur = special->get_constraints().occurrences.min;
            CHECK(min_occur <= special_placement.instances_placed);
        }
        if (--overmaps_to_construct <= 0) { break; }
    }
}
namespace {

void do_lab_finale_test() {
    const point_abs_om origin = point_abs_om(0, -1);
    static const tripoint_om_omt om_mid{OMAPX / 2, OMAPY / 2, 0};

    ACTIVE_OVERMAP_BUFFER.clear();
    omt_find_params find_params{};
    find_params.types.emplace_back("central_lab_endgame", ot_match_type::exact);
    find_params.search_range = {0, OMAPX / 2};
    find_params.search_layers = omt_find_all_layers;
    const tripoint_abs_omt abs_mid = project_combine(origin, om_mid);
    const tripoint_abs_omt start = ACTIVE_OVERMAP_BUFFER.find_closest(abs_mid, find_params);
    CHECK(start != overmap::invalid_tripoint);
}

} // namespace

TEST_CASE("Exactly one endgame lab finale is generated in 0,0 overmap", "[overmap][slow]") {
    clear_all_state();
    do_lab_finale_test();
}

TEST_CASE("Brute force default batch generation to check for RNG bugs", "[.][overmap][slow]") {
    clear_all_state();
    for (auto i = 0; i < 3; ++i) { do_lab_finale_test(); }
}

TEST_CASE("is_ot_match", "[overmap][terrain]") {
    clear_all_state();
    SECTION("exact match") {
        // Matches the complete string
        CHECK(is_ot_match("forest", oter_id("forest"), ot_match_type::exact));
        CHECK(is_ot_match("central_lab", oter_id("central_lab"), ot_match_type::exact));

        // Does not exactly match if rotation differs
        CHECK_FALSE(is_ot_match("sub_station", oter_id("sub_station_north"), ot_match_type::exact));
        CHECK_FALSE(is_ot_match("sub_station", oter_id("sub_station_south"), ot_match_type::exact));
    }

    SECTION("type match") {
        // Matches regardless of rotation
        CHECK(is_ot_match("sub_station", oter_id("sub_station_north"), ot_match_type::type));
        CHECK(is_ot_match("sub_station", oter_id("sub_station_south"), ot_match_type::type));
        CHECK(is_ot_match("sub_station", oter_id("sub_station_east"), ot_match_type::type));
        CHECK(is_ot_match("sub_station", oter_id("sub_station_west"), ot_match_type::type));

        // Does not match if base type does not match
        CHECK_FALSE(is_ot_match("lab", oter_id("central_lab"), ot_match_type::type));
        CHECK_FALSE(
            is_ot_match("sub_station", oter_id("sewer_sub_station_north"), ot_match_type::type));
    }

    SECTION("prefix match") {
        // Matches the complete string
        CHECK(is_ot_match("forest", oter_id("forest"), ot_match_type::prefix));
        CHECK(is_ot_match("central_lab", oter_id("central_lab"), ot_match_type::prefix));

        // Prefix matches when an underscore separator exists
        CHECK(is_ot_match("central", oter_id("central_lab"), ot_match_type::prefix));
        CHECK(is_ot_match("central", oter_id("central_lab_stairs"), ot_match_type::prefix));

        // Prefix itself may contain underscores
        CHECK(is_ot_match("central_lab", oter_id("central_lab_stairs"), ot_match_type::prefix));
        CHECK(is_ot_match(
            "central_lab_train", oter_id("central_lab_train_depot"), ot_match_type::prefix));

        // Prefix does not match without an underscore separator
        CHECK_FALSE(is_ot_match("fore", oter_id("forest"), ot_match_type::prefix));
        CHECK_FALSE(is_ot_match("fore", oter_id("forest_thick"), ot_match_type::prefix));

        // Prefix does not match the middle or end
        CHECK_FALSE(is_ot_match("lab", oter_id("central_lab"), ot_match_type::prefix));
        CHECK_FALSE(is_ot_match("lab", oter_id("central_lab_stairs"), ot_match_type::prefix));
    }

    SECTION("contains match") {
        // Matches the complete string
        CHECK(is_ot_match("forest", oter_id("forest"), ot_match_type::contains));
        CHECK(is_ot_match("central_lab", oter_id("central_lab"), ot_match_type::contains));

        // Matches the beginning/middle/end of an underscore-delimited id
        CHECK(is_ot_match("central", oter_id("central_lab_stairs"), ot_match_type::contains));
        CHECK(is_ot_match("lab", oter_id("central_lab_stairs"), ot_match_type::contains));
        CHECK(is_ot_match("stairs", oter_id("central_lab_stairs"), ot_match_type::contains));

        // Matches the beginning/middle/end without undercores as well
        CHECK(is_ot_match("cent", oter_id("central_lab_stairs"), ot_match_type::contains));
        CHECK(is_ot_match("ral_lab", oter_id("central_lab_stairs"), ot_match_type::contains));
        CHECK(is_ot_match("_lab_", oter_id("central_lab_stairs"), ot_match_type::contains));
        CHECK(is_ot_match("airs", oter_id("central_lab_stairs"), ot_match_type::contains));

        // Does not match if substring is not contained
        CHECK_FALSE(is_ot_match("forest", oter_id("central_lab"), ot_match_type::contains));
        CHECK_FALSE(is_ot_match("forestry", oter_id("forest"), ot_match_type::contains));
    }
}

TEST_CASE("mutable_overmap_placement", "[overmap][slow]") {
    const overmap_special& special = *overmap_special_id(GENERATE("test_anthill", "test_crater"));
    const city cit;

    constexpr auto num_overmaps = 10;
    constexpr auto num_trials_per_overmap = 25;

    for (auto j = 0; j < num_overmaps; ++j) {
        // overmap objects are really large, so we don't want them on the
        // stack.  Use unique_ptr and put it on the heap
        std::unique_ptr<overmap> om = std::make_unique<overmap>(point_abs_om(point_zero));
        om_direction::type dir = om_direction::type::north;

        int successes = 0;

        for (auto i = 0; i < num_trials_per_overmap; ++i) {
            tripoint_om_omt try_pos(rng(0, OMAPX - 1), rng(0, OMAPY - 1), 0);

            // This test can get very spammy, so abort once an error is
            // observed
            if (debug_has_error_been_observed()) { return; }

            if (om->can_place_special(special, try_pos, dir, false)) {
                std::vector<tripoint_om_omt> placed_points =
                    om->place_special(special, try_pos, dir, cit, false, false);
                CHECK(!placed_points.empty());
                ++successes;
            }
        }

        CHECK(successes > num_trials_per_overmap / 2);
    }
}

TEST_CASE("overmap_special_exact_origin_restricts_placement", "[overmap]") {
    clear_all_state();
    const auto& special = *overmap_special_id("test_exact_origin");
    auto& om = ACTIVE_OVERMAP_BUFFER.get(point_abs_om::zero());

    const auto blocked_a = tripoint_om_omt(7, 20, 0);
    const auto allowed = tripoint_om_omt(10, 20, 0);
    om.ter_set(blocked_a, oter_id("field"));
    om.ter_set(allowed, oter_id("field"));

    CHECK_FALSE(ACTIVE_OVERMAP_BUFFER.place_special(
        special.id, project_combine(point_abs_om::zero(), blocked_a), 0, 0));
    CHECK(ACTIVE_OVERMAP_BUFFER
              .place_special(special.id, project_combine(point_abs_om::zero(), allowed), 0, 0));
    CHECK(om.check_overmap_special_type(special.id, allowed));
    CHECK_FALSE(om.check_overmap_special_type(special.id, blocked_a));
}

TEST_CASE("overmap_special_origin_range_restricts_placement", "[overmap]") {
    clear_all_state();
    const auto& special = *overmap_special_id("test_origin_range");
    auto& om = ACTIVE_OVERMAP_BUFFER.get(point_abs_om::zero());

    const auto blocked = tripoint_om_omt(28, 40, 0);
    const auto allowed = tripoint_om_omt(30, 40, 0);
    om.ter_set(blocked, oter_id("field"));
    om.ter_set(allowed, oter_id("field"));

    CHECK_FALSE(ACTIVE_OVERMAP_BUFFER.place_special(
        special.id, project_combine(point_abs_om::zero(), blocked), 0, 0));
    CHECK(ACTIVE_OVERMAP_BUFFER
              .place_special(special.id, project_combine(point_abs_om::zero(), allowed), 0, 0));
    CHECK(om.check_overmap_special_type(special.id, allowed));
    CHECK_FALSE(om.check_overmap_special_type(special.id, blocked));
}

#if defined(TILES)

// Regression tests for issue #9688: the main view and the (possibly shared) overmap
// tile context each track their own zoom, and switching views must re-assert the
// taking-over view's zoom on the context. The old overmap_ui.cpp save/restore shim
// forced the overmap to the default scale on open and relied on game::set_zoom's
// equality guard on close, desyncing the rendered scale from the stored zoom in
// both directions.

#    include "cached_options.h"
#    include "cata_tiles.h"
#    include "game.h"
#    include "options_helpers.h"
#    include "sdltiles.h"
#    include "tile_helpers.h"

#    include <cmath>
#    include <map>

namespace {

/// Tile widths at each canonical zoom scale, used to read back which zoom a context
/// is actually rendering at. Distinctness is what makes that read-back unambiguous.
auto zoom_scale_widths() -> std::map<float, int> {
    std::map<float, int> widths;
    for (const float scale : {4.f, 8.f, 16.f, 32.f, 64.f}) {
        tilecontext->set_draw_scale(scale);
        widths[scale] = tilecontext->get_tile_width();
    }
    REQUIRE(widths.at(4.f) > 0);
    REQUIRE(widths.at(4.f) < widths.at(8.f));
    REQUIRE(widths.at(8.f) < widths.at(16.f));
    REQUIRE(widths.at(16.f) < widths.at(32.f));
    REQUIRE(widths.at(32.f) < widths.at(64.f));
    return widths;
}

} // namespace

TEST_CASE("zoom_bookkeeping_matches_rendered_scale", "[overmap][zoom][tiles]") {
    clear_all_state();
    tile_context_fixture fixture(/*alias_overmap_context=*/true);
    if (!fixture.valid()) {
        WARN("skipped: this environment cannot set up a headless tile context");
        return;
    }
    const auto widths = zoom_scale_widths();
    // Start every section from a known state: both views at the default scale.
    g->reset_overmap_zoom();
    g->set_zoom(DEFAULT_TILESET_ZOOM);
    g->reapply_zoom();
    REQUIRE(tilecontext->get_tile_width() == widths.at(16.f));

    SECTION("main view steps through the zoom ladder and wraps at both ends") {
        for (const float expected : {32.f, 64.f, 4.f, 8.f}) { // wraps 64 -> 4
            g->zoom_in();
            CHECK(g->get_zoom() == expected);
            CHECK(tilecontext->get_tile_width() == widths.at(expected));
        }
        for (const float expected : {4.f, 64.f}) { // wraps 4 -> 64
            g->zoom_out();
            CHECK(g->get_zoom() == expected);
            CHECK(tilecontext->get_tile_width() == widths.at(expected));
        }
    }

    SECTION("zoom stepping recovers from an off-grid zoom value") {
        g->set_zoom(20.f); // between the 16 and 32 steps
        REQUIRE(g->get_zoom() == 20.f);
        g->zoom_in(); // rounds to the nearest step index, then steps
        CHECK(g->get_zoom() == 32.f);
        g->set_zoom(20.f);
        g->zoom_out();
        CHECK(g->get_zoom() == 8.f);
    }

    SECTION("half steps with ZOOM_STEP_COUNT 2") {
        const override_option zoom_steps("ZOOM_STEP_COUNT", "2");
        g->zoom_in();
        CHECK(g->get_zoom() == Approx(std::pow(2., 4.5)));
        g->zoom_in();
        CHECK(g->get_zoom() == Approx(32.f));
    }

    SECTION("overmap zoom does not leak into the main view on a shared context") {
        // "Open" the overmap and zoom it fully out; the main view's bookkeeping
        // must not move.
        g->reapply_overmap_zoom();
        REQUIRE(overmap_tilecontext->get_tile_width() == widths.at(16.f));
        g->zoom_out_overmap();
        g->zoom_out_overmap();
        REQUIRE(overmap_tilecontext->get_tile_width() == widths.at(4.f));
        CHECK(g->get_zoom() == 16.f);

        // "Close" it: the main view must re-assert its own scale even though
        // tileset_zoom never changed - set_zoom's equality guard would skip this,
        // which was the main-view half of issue #9688.
        g->reapply_zoom();
        CHECK(tilecontext->get_tile_width() == widths.at(16.f));

        // "Reopen": the overmap comes back at its own remembered zoom, not the
        // default the legacy shim forced - the overmap half of issue #9688...
        g->reapply_overmap_zoom();
        CHECK(overmap_tilecontext->get_tile_width() == widths.at(4.f));
        // ...and the zoom keys step from the displayed level.
        g->zoom_in_overmap();
        CHECK(overmap_tilecontext->get_tile_width() == widths.at(8.f));

        g->reapply_zoom();
        CHECK(tilecontext->get_tile_width() == widths.at(16.f));
    }
}

TEST_CASE("separate_overmap_tile_context_keeps_independent_scale", "[overmap][zoom][tiles]") {
    clear_all_state();
    tile_context_fixture fixture(/*alias_overmap_context=*/false);
    if (!fixture.valid()) {
        WARN("skipped: this environment cannot set up a headless tile context");
        return;
    }
    REQUIRE(tilecontext != overmap_tilecontext);
    const auto widths = zoom_scale_widths();
    g->reset_overmap_zoom();
    g->set_zoom(DEFAULT_TILESET_ZOOM);
    g->reapply_zoom();

    // Main-view zooming leaves the overmap context alone...
    g->zoom_in();
    CHECK(tilecontext->get_tile_width() == widths.at(32.f));
    CHECK(overmap_tilecontext->get_tile_width() == widths.at(16.f));
    // ...and overmap zooming leaves the main context alone.
    g->zoom_out_overmap();
    CHECK(overmap_tilecontext->get_tile_width() == widths.at(8.f));
    CHECK(tilecontext->get_tile_width() == widths.at(32.f));
}

#endif // TILES

#include "avatar.h"
#include "cached_options.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "field.h"
#include "field_type.h"
#include "fire_spread_loader.h"
#include "game_constants.h"
#include "map_helpers.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "point.h"
#include "state_helpers.h"
#include "submap.h"
#include "submap_fields.h"
#include "submap_load_manager.h"
#include "type_id.h"
#include "units.h"
#include "vehicle.h"

#include <array>
#include <ranges>
#include <set>

// Dimension ID used only by these tests — never appears in game data.
static const dimension_id TEST_DIM_ID("sim_test_dim");

// Far enough from the test map centre that it is never inside the reality bubble.
static const tripoint_abs_sm FAR_SM_POS{200, 200, 0};

// Create a blank submap at @p pos in @p mb and return the raw pointer.
// Ownership is transferred to @p mb.
static auto make_blank_submap(mapbuffer& mb, const tripoint_abs_sm& pos) -> submap* {
    auto sm = std::make_unique<submap>(pos, mb.get_dimension_id());
    mb.add_submap(pos, sm);
    return mb.lookup_submap_in_memory(pos);
}

static auto add_field_to_submap(
    submap& sm, const point_sm_ms& local, const field_type_id& type, const int intensity,
    const time_duration& age) -> field_entry* {
    if (sm.get_field(local).add_field(type, intensity, age)) {
        ++sm.field_count;
        sm.field_cache.push_back(local);
        sm.is_uniform = false;
    }
    return sm.get_field(local).find_field(type);
}

// Add fd_fire to @p sm at @p local and keep field_count / field_cache / is_uniform consistent.
static auto plant_fire(submap& sm, const point_sm_ms& local, int intensity = 1) -> void {
    if (sm.get_field(local).add_field(fd_fire, intensity, 0_turns)) {
        ++sm.field_count;
        sm.field_cache.push_back(local);
        sm.is_uniform = false;
    }
}

// ── Test 1 ────────────────────────────────────────────────────────────────────
// Verify that process_fields_in_submap() actually processes a fire field that
// lives in a submap outside the player's reality bubble.
//
// The deterministic observable: the universal aging step at the bottom of
// process_fields_in_submap() increments every field's age by exactly 1_turns
// per call.  A newborn field (age == 0_turns) is suppressed from fire-specific
// effects on the first tick but is still aged — so after one call the fire must
// be at 1_turns old.
TEST_CASE("fire_processes_in_loaded_submap_outside_bubble", "[simulation][field]") {
    clear_all_state();
    put_player_underground();

    auto* sm = make_blank_submap(MAPBUFFER, FAR_SM_POS);
    REQUIRE(sm != nullptr);

    const auto fire_pt = point_sm_ms{5, 5};
    plant_fire(*sm, fire_pt);
    REQUIRE(sm->get_field(fire_pt).find_field(fd_fire) != nullptr);

    auto& dummy = get_avatar();
    process_fields_in_submap(dummy.get_dimension(), *sm, FAR_SM_POS, MAPBUFFER);

    const auto* fire = sm->get_field(fire_pt).find_field(fd_fire);
    REQUIRE(fire != nullptr);
    CHECK(fire->get_field_age() == 1_turns);

    MAPBUFFER.unload_omt(project_to<coords::omt>(FAR_SM_POS));
}

TEST_CASE("shock_vent_emits_electricity_from_hidden_field", "[simulation][field][electricity]") {
    clear_all_state();
    put_player_underground();

    const auto center = FAR_SM_POS;
    const auto local = point_sm_ms(SEEX / 2, SEEY / 2);
    for (const auto& offset : closest_points_first(point_abs_sm::zero(), 1)) {
        const auto submap_pos = center + tripoint_rel_sm(offset.x(), offset.y(), 0);
        auto* sm = make_blank_submap(MAPBUFFER, submap_pos);
        REQUIRE(sm != nullptr);
    }

    auto* source_sm = MAPBUFFER.lookup_submap_in_memory(center);
    REQUIRE(source_sm != nullptr);
    auto* vent = add_field_to_submap(*source_sm, local, fd_shock_vent, 1, 1_turns);
    REQUIRE(vent != nullptr);
    CHECK_FALSE(fd_shock_vent->display_field);

    auto& dummy = get_avatar();
    process_fields_in_submap(dummy.get_dimension(), *source_sm, center, MAPBUFFER);

    vent = source_sm->get_field(local).find_field(fd_shock_vent);
    REQUIRE(vent != nullptr);
    CHECK(vent->get_field_intensity() == 3);

    auto electricity_tiles = size_t{0};
    for (const auto& offset : closest_points_first(point_abs_sm::zero(), 1)) {
        const auto submap_pos = center + tripoint_rel_sm(offset.x(), offset.y(), 0);
        const auto* sm = MAPBUFFER.lookup_submap_in_memory(submap_pos);
        REQUIRE(sm != nullptr);
        for (const auto& field_pos : sm->field_cache) {
            if (sm->get_field(field_pos).find_field(fd_electricity) != nullptr) {
                ++electricity_tiles;
            }
        }
    }
    CHECK(electricity_tiles > 0);

    MAPBUFFER.unload_omt(project_to<coords::omt>(center));
}

// ── Test 2 ────────────────────────────────────────────────────────────────────
// Verify that a loaded, no-fire boundary submap stays requested while it is
// adjacent to tracked fire.  This avoids request/load/prune/evict churn at fire
// boundaries during long activities.
TEST_CASE(
    "fire_spread_keeps_no_fire_boundary_submap_while_adjacent_to_fire",
    "[simulation][field][fire_spread]") {
    clear_all_state();
    put_player_underground();

    auto restore_cap = restore_on_out_of_scope<int>(fire_spread_submap_cap);
    fire_spread_submap_cap = 25;

    auto loader = fire_spread_loader{};
    auto& dim = MAPBUFFER_REGISTRY.get(TEST_DIM_ID);
    const auto source_pos = tripoint_abs_sm{400, 400, 0};
    const auto neighbor_pos = tripoint_abs_sm{401, 400, 0};
    const auto request_begin = source_pos.xy();
    const auto request_end = request_begin + point_rel_sm(1, 1);
    const auto proper_handle = submap_loader.request_load(
        load_request_source::reality_bubble, TEST_DIM_ID, request_begin, request_end);
    const auto cleanup = on_out_of_scope([&]() {
        loader.clear(submap_loader);
        submap_loader.release_load(proper_handle);
        MAPBUFFER_REGISTRY.unload_dimension(TEST_DIM_ID);
    });

    auto* source_sm = make_blank_submap(dim, source_pos);
    auto* neighbor_sm = make_blank_submap(dim, neighbor_pos);
    REQUIRE(source_sm != nullptr);
    REQUIRE(neighbor_sm != nullptr);

    const auto fire_pt = point_sm_ms{5, 5};
    plant_fire(*source_sm, fire_pt);

    loader.request_for_fire(TEST_DIM_ID, source_pos);
    loader.request_for_fire(TEST_DIM_ID, neighbor_pos);
    REQUIRE(loader.loaded_count() == 2);

    loader.prune_disconnected(submap_loader);
    CHECK(loader.loaded_count() == 2);

    auto* fire = source_sm->get_field(fire_pt).find_field(fd_fire);
    REQUIRE(fire != nullptr);
    fire->set_field_intensity(0);

    loader.prune_disconnected(submap_loader);
    CHECK(loader.loaded_count() == 0);
}

TEST_CASE(
    "fire_spread_cap_is_global_across_dimensions", "[simulation][field][fire_spread][dimension]") {
    clear_all_state();
    put_player_underground();

    auto restore_cap = restore_on_out_of_scope<int>(fire_spread_submap_cap);
    fire_spread_submap_cap = 1;

    auto loader = fire_spread_loader{};
    const auto first_dim = dimension_id("fire_cap_first_dim");
    const auto second_dim = dimension_id("fire_cap_second_dim");
    const auto first_anchor = tripoint_abs_sm{420, 420, 0};
    const auto second_anchor = tripoint_abs_sm{620, 620, 0};
    const auto first_fire = first_anchor + tripoint_east;
    const auto second_fire = second_anchor + tripoint_east;
    auto first_stable = load_request_handle{};
    auto second_stable = load_request_handle{};
    const auto cleanup = on_out_of_scope([&]() {
        loader.clear(submap_loader);
        submap_loader.release_load(first_stable);
        submap_loader.release_load(second_stable);
        MAPBUFFER_REGISTRY.unload_dimension(first_dim);
        MAPBUFFER_REGISTRY.unload_dimension(second_dim);
    });

    first_stable = submap_loader.request_load(
        load_request_source::reality_bubble, first_dim, first_anchor.xy(),
        first_anchor.xy() + point_rel_sm{1, 1});
    second_stable = submap_loader.request_load(
        load_request_source::reality_bubble, second_dim, second_anchor.xy(),
        second_anchor.xy() + point_rel_sm{1, 1});

    loader.request_for_fire(first_dim, first_fire);
    loader.request_for_fire(second_dim, second_fire);

    CHECK(loader.loaded_count() == 1);
    CHECK(submap_loader.is_requested(first_dim, first_fire)
          != submap_loader.is_requested(second_dim, second_fire));
}

TEST_CASE(
    "stable_load_requests_are_distinguished_from_propagating_requests",
    "[simulation][submap_loading]") {
    clear_all_state();

    static const dimension_id dim_id("stable_request_test_dim");
    const auto stable_pos = tripoint_abs_sm{500, 500, 0};
    const auto unstable_pos = stable_pos + tripoint_east;
    auto stable_handle = load_request_handle{};
    auto unstable_handle = load_request_handle{};
    const auto cleanup = on_out_of_scope([&]() {
        submap_loader.release_load(stable_handle);
        submap_loader.release_load(unstable_handle);
        MAPBUFFER_REGISTRY.unload_dimension(dim_id);
    });

    stable_handle = submap_loader.request_load(
        load_request_source::stable_lua, dim_id, stable_pos.xy(),
        stable_pos.xy() + point_rel_sm{1, 1});
    unstable_handle = submap_loader.request_load(
        load_request_source::fire_spread, dim_id, unstable_pos.xy(),
        unstable_pos.xy() + point_rel_sm{1, 1});

    CHECK(submap_loader.is_stably_requested(dim_id, stable_pos));
    CHECK_FALSE(submap_loader.is_stably_requested(dim_id, unstable_pos));

    submap_loader.release_load(stable_handle);
    submap_loader.release_load(unstable_handle);
}

TEST_CASE("load_request_source_stability_is_explicit", "[simulation][submap_loading]") {
    const auto stable_sources = std::array{
        load_request_source::reality_bubble, load_request_source::power_portal,
        load_request_source::player_claim,   load_request_source::player_priority,
        load_request_source::stable_lua,
    };
    const auto unstable_sources = std::array{
        load_request_source::unstable_lua,      load_request_source::fire_spread,
        load_request_source::vehicle_footprint, load_request_source::lazy_border,
        load_request_source::portal_preload,
    };

    for (const auto source : stable_sources) { CHECK(load_request_source_is_stable(source)); }
    for (const auto source : unstable_sources) {
        CHECK_FALSE(load_request_source_is_stable(source));
    }
}

TEST_CASE(
    "stable_contact_promotes_complete_vehicle_footprint", "[simulation][submap_loading][vehicle]") {
    clear_all_state();
    put_player_underground();

    const auto vehicle_sm = point_abs_sm{200, 200};
    const auto vehicle_pos =
        project_to<coords::ms>(tripoint_abs_sm{vehicle_sm, 0})
        + tripoint_rel_ms{SEEX - 2, SEEY / 2, 0};
    const auto resident_lookup = mapbuffer_lookup_options{
        .mode = mapbuffer_lookup_mode::resident_only,
    };

    const auto submap_begin = point_abs_sm{vehicle_sm.x() - 2, vehicle_sm.y() - 2};
    const auto submap_end = point_abs_sm{vehicle_sm.x() + 4, vehicle_sm.y() + 3};
    for (const auto& pos : point_range<point_abs_sm>(submap_begin, submap_end)) {
        const auto submap_pos = tripoint_abs_sm{pos, 0};
        auto* const submap = make_blank_submap(MAPBUFFER, submap_pos);
        REQUIRE(submap != nullptr);
        submap->set_all_ter(t_grass);
    }

    auto* vehicle = MAPBUFFER.add_vehicle(
        vproto_id("car"), vehicle_pos, 0_degrees, 0, 0, true, std::nullopt, std::nullopt,
        resident_lookup);
    REQUIRE(vehicle != nullptr);

    const auto footprints = MAPBUFFER.get_vehicle_submap_footprints(*vehicle);
    auto footprint_cells = std::set<point_abs_sm>{};
    for (const auto& footprint : footprints) {
        if (!footprint) { continue; }
        for (const auto& pos :
             point_range<point_abs_sm>(footprint->min.xy(), footprint->max.xy())) {
            footprint_cells.insert(pos);
        }
    }
    REQUIRE(footprint_cells.size() > 1);
    REQUIRE(footprint_cells.contains(vehicle_sm));

    const auto non_stable_cell = std::ranges::
        find_if(footprint_cells, [&](const point_abs_sm& pos) { return pos != vehicle_sm; });
    REQUIRE(non_stable_cell != footprint_cells.end());

    auto fire_handle = submap_loader.request_load(
        load_request_source::fire_spread, MAPBUFFER.get_dimension_id(), vehicle_sm,
        vehicle_sm + point_rel_sm{1, 1});
    auto stable_handle = load_request_handle{};
    const auto cleanup = on_out_of_scope([&]() {
        submap_loader.flush_prev_desired();
        if (vehicle != nullptr) { MAPBUFFER.destroy_vehicle(vehicle, resident_lookup); }
        submap_loader.release_load(fire_handle);
        submap_loader.release_load(stable_handle);
        MAPBUFFER.unload_omt(project_to<coords::omt>(tripoint_abs_sm{vehicle_sm, 0}));
    });

    submap_loader.update(true);
    CHECK_FALSE(submap_loader.is_stably_requested(MAPBUFFER.get_dimension_id(), *non_stable_cell));
    CHECK_FALSE(submap_loader.is_requested(MAPBUFFER.get_dimension_id(), *non_stable_cell));

    stable_handle = submap_loader.request_load(
        load_request_source::stable_lua, MAPBUFFER.get_dimension_id(), vehicle_sm,
        vehicle_sm + point_rel_sm{1, 1});

    submap_loader.update(true);

    CHECK(submap_loader.is_stably_requested(MAPBUFFER.get_dimension_id(), vehicle_sm));
    CHECK(submap_loader.is_requested(MAPBUFFER.get_dimension_id(), vehicle_sm));
    for (const auto& pos : footprint_cells) {
        INFO("vehicle footprint submap: " << pos);
        CHECK(submap_loader.is_requested(MAPBUFFER.get_dimension_id(), pos));
    }

    submap_loader.release_load(stable_handle);
    stable_handle = load_request_handle{};
    submap_loader.update(true);
    CHECK(submap_loader.is_requested(MAPBUFFER.get_dimension_id(), vehicle_sm));
    CHECK_FALSE(submap_loader.is_requested(MAPBUFFER.get_dimension_id(), *non_stable_cell));
}

TEST_CASE("vehicle_footprints_do_not_chain", "[simulation][submap_loading][vehicle]") {
    clear_all_state();
    put_player_underground();

    const auto vehicle_sm = point_abs_sm{220, 220};
    const auto vehicle_origin = project_to<coords::ms>(tripoint_abs_sm{vehicle_sm, 0});
    const auto first_vehicle_pos = vehicle_origin + tripoint_rel_ms{SEEX - 2, SEEY / 2, 0};
    const auto second_vehicle_pos = vehicle_origin + tripoint_rel_ms{2 * SEEX - 1, SEEY / 2, 0};
    const auto resident_lookup = mapbuffer_lookup_options{
        .mode = mapbuffer_lookup_mode::resident_only,
    };

    const auto submap_begin = point_abs_sm{vehicle_sm.x() - 2, vehicle_sm.y() - 2};
    const auto submap_end = point_abs_sm{vehicle_sm.x() + 5, vehicle_sm.y() + 3};
    for (const auto& pos : point_range<point_abs_sm>(submap_begin, submap_end)) {
        const auto submap_pos = tripoint_abs_sm{pos, 0};
        auto* const submap = make_blank_submap(MAPBUFFER, submap_pos);
        REQUIRE(submap != nullptr);
        submap->set_all_ter(t_grass);
    }

    auto stable_handle = submap_loader.request_load(
        load_request_source::stable_lua, MAPBUFFER.get_dimension_id(), vehicle_sm,
        vehicle_sm + point_rel_sm{1, 1});
    auto* first_vehicle = static_cast<vehicle*>(nullptr);
    auto* second_vehicle = static_cast<vehicle*>(nullptr);
    const auto discard_vehicle = [&](vehicle*& veh) {
        if (veh == nullptr) { return; }
        if (!MAPBUFFER.has_loaded_vehicle(veh)) {
            veh = nullptr;
            return;
        }
        auto* const owner = MAPBUFFER.lookup_submap_in_memory(veh->abs_sm_pos);
        const auto is_owned =
            owner != nullptr && std::ranges::any_of(owner->vehicles, [&](const auto& candidate) {
                return candidate.get() == veh;
            });
        if (is_owned) {
            MAPBUFFER.destroy_vehicle(veh, resident_lookup);
        } else {
            MAPBUFFER.unregister_vehicle(veh);
        }
        veh = nullptr;
    };
    const auto cleanup = on_out_of_scope([&]() {
        submap_loader.flush_prev_desired();
        discard_vehicle(first_vehicle);
        if (second_vehicle != nullptr && MAPBUFFER.has_loaded_vehicle(second_vehicle)) {
            MAPBUFFER.unregister_vehicle(second_vehicle);
        }
        second_vehicle = nullptr;
        submap_loader.release_load(stable_handle);
        MAPBUFFER.unload_omt(project_to<coords::omt>(tripoint_abs_sm{vehicle_sm, 0}));
    });

    first_vehicle = MAPBUFFER.add_vehicle(
        vproto_id("car"), first_vehicle_pos, 0_degrees, 0, 0, true, std::nullopt, std::nullopt,
        resident_lookup);
    second_vehicle = MAPBUFFER.add_vehicle(
        vproto_id("car"), second_vehicle_pos, 0_degrees, 0, 0, true, std::nullopt, std::nullopt,
        resident_lookup);
    REQUIRE(first_vehicle != nullptr);
    REQUIRE(second_vehicle != nullptr);

    const auto second_footprints = MAPBUFFER.get_vehicle_submap_footprints(*second_vehicle);
    auto second_cells = std::set<point_abs_sm>{};
    for (const auto& footprint : second_footprints) {
        if (!footprint) { continue; }
        for (const auto& pos :
             point_range<point_abs_sm>(footprint->min.xy(), footprint->max.xy())) {
            second_cells.insert(pos);
        }
    }
    REQUIRE(second_cells.size() > 1);

    submap_loader.update(true);

    const auto second_contact_cell =
        std::ranges::find_if(second_cells, [&](const point_abs_sm& pos) {
            return submap_loader.is_requested(MAPBUFFER.get_dimension_id(), pos);
        });
    const auto second_unpromoted_cell =
        std::ranges::find_if(second_cells, [&](const point_abs_sm& pos) {
            return !submap_loader.is_requested(MAPBUFFER.get_dimension_id(), pos);
        });
    REQUIRE(second_contact_cell != second_cells.end());
    REQUIRE(second_unpromoted_cell != second_cells.end());

    MAPBUFFER.destroy_vehicle(first_vehicle, resident_lookup);
    first_vehicle = nullptr;
    submap_loader.update(true);

    CHECK_FALSE(submap_loader.is_requested(MAPBUFFER.get_dimension_id(), *second_contact_cell));
    CHECK_FALSE(submap_loader.is_requested(MAPBUFFER.get_dimension_id(), *second_unpromoted_cell));
}

TEST_CASE("vehicle_footprint_promotion_can_be_disabled", "[simulation][submap_loading][vehicle]") {
    clear_all_state();
    put_player_underground();

    const auto restore_promotion = restore_on_out_of_scope<bool>(
        vehicle_footprint_simulation_enabled);
    vehicle_footprint_simulation_enabled = false;

    const auto vehicle_sm = point_abs_sm{240, 240};
    const auto vehicle_pos =
        project_to<coords::ms>(tripoint_abs_sm{vehicle_sm, 0})
        + tripoint_rel_ms{SEEX - 2, SEEY / 2, 0};
    const auto resident_lookup = mapbuffer_lookup_options{
        .mode = mapbuffer_lookup_mode::resident_only,
    };

    const auto submap_begin = point_abs_sm{vehicle_sm.x() - 2, vehicle_sm.y() - 2};
    const auto submap_end = point_abs_sm{vehicle_sm.x() + 4, vehicle_sm.y() + 3};
    for (const auto& pos : point_range<point_abs_sm>(submap_begin, submap_end)) {
        const auto submap_pos = tripoint_abs_sm{pos, 0};
        auto* const submap = make_blank_submap(MAPBUFFER, submap_pos);
        REQUIRE(submap != nullptr);
        submap->set_all_ter(t_grass);
    }

    auto* vehicle = MAPBUFFER.add_vehicle(
        vproto_id("car"), vehicle_pos, 0_degrees, 0, 0, true, std::nullopt, std::nullopt,
        resident_lookup);
    REQUIRE(vehicle != nullptr);

    const auto footprints = MAPBUFFER.get_vehicle_submap_footprints(*vehicle);
    auto footprint_cells = std::set<point_abs_sm>{};
    for (const auto& footprint : footprints) {
        if (!footprint) { continue; }
        for (const auto& pos :
             point_range<point_abs_sm>(footprint->min.xy(), footprint->max.xy())) {
            footprint_cells.insert(pos);
        }
    }
    REQUIRE(footprint_cells.size() > 1);
    REQUIRE(footprint_cells.contains(vehicle_sm));

    auto stable_handle = submap_loader.request_load(
        load_request_source::stable_lua, MAPBUFFER.get_dimension_id(), vehicle_sm,
        vehicle_sm + point_rel_sm{1, 1});
    const auto cleanup = on_out_of_scope([&]() {
        submap_loader.flush_prev_desired();
        if (vehicle != nullptr) { MAPBUFFER.destroy_vehicle(vehicle, resident_lookup); }
        submap_loader.release_load(stable_handle);
        MAPBUFFER.unload_omt(project_to<coords::omt>(tripoint_abs_sm{vehicle_sm, 0}));
    });

    submap_loader.update(true);

    const auto non_stable_cell = std::ranges::
        find_if(footprint_cells, [&](const point_abs_sm& pos) { return pos != vehicle_sm; });
    REQUIRE(non_stable_cell != footprint_cells.end());
    CHECK(submap_loader.is_requested(MAPBUFFER.get_dimension_id(), vehicle_sm));
    CHECK_FALSE(submap_loader.is_requested(MAPBUFFER.get_dimension_id(), *non_stable_cell));
}

TEST_CASE(
    "vehicle_footprint_promotion_covers_multiple_zlevels",
    "[simulation][submap_loading][vehicle]") {
    clear_all_state();
    put_player_underground();

    const auto vehicle_sm = point_abs_sm{260, 260};
    const auto vehicle_pos =
        project_to<coords::ms>(tripoint_abs_sm{vehicle_sm, 0})
        + tripoint_rel_ms{SEEX / 2, SEEY / 2, 0};
    const auto resident_lookup = mapbuffer_lookup_options{
        .mode = mapbuffer_lookup_mode::resident_only,
    };

    const auto submap_begin = point_abs_sm{vehicle_sm.x() - 2, vehicle_sm.y() - 2};
    const auto submap_end = point_abs_sm{vehicle_sm.x() + 3, vehicle_sm.y() + 3};
    for (const auto z : {0, 1}) {
        for (const auto& pos : point_range<point_abs_sm>(submap_begin, submap_end)) {
            const auto submap_pos = tripoint_abs_sm{pos, z};
            auto* const submap = make_blank_submap(MAPBUFFER, submap_pos);
            REQUIRE(submap != nullptr);
            submap->set_all_ter(t_grass);
        }
    }

    auto* vehicle = MAPBUFFER.add_vehicle(
        vproto_id("car"), vehicle_pos, 0_degrees, 0, 0, true, std::nullopt, std::nullopt,
        resident_lookup);
    REQUIRE(vehicle != nullptr);
    REQUIRE(
        vehicle->install_part(tripoint_mnt_veh{0, 0, 1}, vpart_id("frame_vertical"), true) >= 0);

    const auto footprints = MAPBUFFER.get_vehicle_submap_footprints(*vehicle);
    const auto& upper_footprint = footprints[1 + OVERMAP_DEPTH];
    REQUIRE(upper_footprint);

    const auto stable_handle = submap_loader.request_load(
        load_request_source::stable_lua, MAPBUFFER.get_dimension_id(), vehicle_sm,
        vehicle_sm + point_rel_sm{1, 1});
    const auto cleanup = on_out_of_scope([&]() {
        submap_loader.flush_prev_desired();
        if (vehicle != nullptr) { MAPBUFFER.destroy_vehicle(vehicle, resident_lookup); }
        submap_loader.release_load(stable_handle);
        MAPBUFFER.unload_omt(project_to<coords::omt>(tripoint_abs_sm{vehicle_sm, 0}));
    });

    submap_loader.update(true);

    CHECK(submap_loader.is_requested(MAPBUFFER.get_dimension_id(), upper_footprint->min.xy()));
    CHECK(submap_loader.is_simulated(MAPBUFFER.get_dimension_id(), upper_footprint->min.xy()));
    CHECK(
        MAPBUFFER.is_column_state(upper_footprint->min.xy(), submap_column_load_state::simulated));
    CHECK(MAPBUFFER.lookup_submap_in_memory(tripoint_abs_sm{upper_footprint->min.xy(), 1})
          != nullptr);
}

// ── Test 3 ────────────────────────────────────────────────────────────────────
// Verify that fire in a non-primary dimension does not affect the primary
// dimension when process_fields_in_submap() is called with the secondary
// dimension's mapbuffer.
//
// This tests the fundamental isolation guarantee of the dimension system:
// fire spread uses only the mapbuffer passed in, so a secondary dimension's
// flames can never cross into the primary world.
TEST_CASE("fire_isolated_between_dimensions", "[simulation][field][dimension]") {
    clear_all_state();
    put_player_underground();

    auto& dim = MAPBUFFER_REGISTRY.get(TEST_DIM_ID);
    auto* dim_sm = make_blank_submap(dim, FAR_SM_POS);
    REQUIRE(dim_sm != nullptr);

    const auto fire_pt = point_sm_ms{5, 5};
    plant_fire(*dim_sm, fire_pt);

    // Primary dimension must have no fire at the same absolute position.
    if (const auto* primary_sm = MAPBUFFER.lookup_submap_in_memory(FAR_SM_POS)) {
        REQUIRE(primary_sm->get_field(fire_pt).find_field(fd_fire) == nullptr);
    }

    // Process only the secondary dimension.
    process_fields_in_submap(TEST_DIM_ID, *dim_sm, FAR_SM_POS, dim);

    // Fire in the secondary dimension must have aged (processing occurred).
    const auto* dim_fire = dim_sm->get_field(fire_pt).find_field(fd_fire);
    REQUIRE(dim_fire != nullptr);
    CHECK(dim_fire->get_field_age() == 1_turns);

    // Primary dimension must still be fire-free — no cross-dimension spread.
    if (const auto* primary_sm = MAPBUFFER.lookup_submap_in_memory(FAR_SM_POS)) {
        CHECK(primary_sm->get_field(fire_pt).find_field(fd_fire) == nullptr);
    }

    MAPBUFFER_REGISTRY.unload_dimension(TEST_DIM_ID);
}

#include "catch/catch.hpp"

#include "cached_options.h"
#include "cata_utility.h"
#include "coordinates.h"
#include "field.h"
#include "field_type.h"
#include "fire_spread_loader.h"
#include "map_helpers.h"
#include "mapbuffer.h"
#include "mapbuffer_registry.h"
#include "point.h"
#include "state_helpers.h"
#include "submap.h"
#include "submap_fields.h"
#include "submap_load_manager.h"
#include "units.h"

// Dimension ID used only by these tests — never appears in game data.
static constexpr const char *TEST_DIM_ID = "sim_test_dim";

// Far enough from the test map centre that it is never inside the reality bubble.
static const tripoint_abs_sm FAR_SM_POS{ 200, 200, 0 };

// Create a blank submap at @p pos in @p mb and return the raw pointer.
// Ownership is transferred to @p mb.
static auto make_blank_submap( mapbuffer &mb, const tripoint_abs_sm &pos ) -> submap *
{
    auto sm = std::make_unique<submap>( pos );
    mb.add_submap( pos, sm );
    return mb.lookup_submap_in_memory( pos );
}

// Add fd_fire to @p sm at @p local and keep field_count / field_cache / is_uniform consistent.
static auto plant_fire( submap &sm, const point_sm_ms &local, int intensity = 1 ) -> void
{
    if( sm.get_field( local ).add_field( fd_fire, intensity, 0_turns ) ) {
        ++sm.field_count;
        sm.field_cache.push_back( local );
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
TEST_CASE( "fire_processes_in_loaded_submap_outside_bubble", "[simulation][field]" )
{
    clear_all_state();
    put_player_underground();

    auto *sm = make_blank_submap( MAPBUFFER, FAR_SM_POS );
    REQUIRE( sm != nullptr );

    const auto fire_pt = point_sm_ms{ 5, 5 };
    plant_fire( *sm, fire_pt );
    REQUIRE( sm->get_field( fire_pt ).find_field( fd_fire ) != nullptr );

    process_fields_in_submap( *sm, FAR_SM_POS, MAPBUFFER );

    const auto *fire = sm->get_field( fire_pt ).find_field( fd_fire );
    REQUIRE( fire != nullptr );
    CHECK( fire->get_field_age() == 1_turns );

    MAPBUFFER.unload_omt( project_to<coords::omt>( FAR_SM_POS ), false );
}

// ── Test 2 ────────────────────────────────────────────────────────────────────
// Verify that a loaded, no-fire boundary submap stays requested while it is
// adjacent to tracked fire.  This avoids request/load/prune/evict churn at fire
// boundaries during long activities.
TEST_CASE( "fire_spread_keeps_no_fire_boundary_submap_while_adjacent_to_fire",
           "[simulation][field][fire_spread]" )
{
    clear_all_state();
    put_player_underground();

    auto restore_cap = restore_on_out_of_scope<int>( fire_spread_submap_cap );
    fire_spread_submap_cap = 25;

    auto loader = fire_spread_loader{};
    auto &dim = MAPBUFFER_REGISTRY.get( TEST_DIM_ID );
    const auto source_pos = tripoint_abs_sm{ 400, 400, 0 };
    const auto neighbor_pos = tripoint_abs_sm{ 401, 400, 0 };
    const auto proper_handle = submap_loader.request_load( load_request_source::reality_bubble,
                               TEST_DIM_ID, source_pos, 0 );
    const auto cleanup = on_out_of_scope( [&]() {
        loader.clear( submap_loader );
        submap_loader.release_load( proper_handle );
        MAPBUFFER_REGISTRY.unload_dimension( TEST_DIM_ID );
    } );

    auto *source_sm = make_blank_submap( dim, source_pos );
    auto *neighbor_sm = make_blank_submap( dim, neighbor_pos );
    REQUIRE( source_sm != nullptr );
    REQUIRE( neighbor_sm != nullptr );

    const auto fire_pt = point_sm_ms{ 5, 5 };
    plant_fire( *source_sm, fire_pt );

    loader.request_for_fire( TEST_DIM_ID, source_pos );
    loader.request_for_fire( TEST_DIM_ID, neighbor_pos );
    REQUIRE( loader.loaded_count() == 2 );

    loader.prune_disconnected( submap_loader );
    CHECK( loader.loaded_count() == 2 );

    auto *fire = source_sm->get_field( fire_pt ).find_field( fd_fire );
    REQUIRE( fire != nullptr );
    fire->set_field_intensity( 0 );

    loader.prune_disconnected( submap_loader );
    CHECK( loader.loaded_count() == 0 );
}

// ── Test 3 ────────────────────────────────────────────────────────────────────
// Verify that fire in a non-primary dimension does not affect the primary
// dimension when process_fields_in_submap() is called with the secondary
// dimension's mapbuffer.
//
// This tests the fundamental isolation guarantee of the dimension system:
// fire spread uses only the mapbuffer passed in, so a secondary dimension's
// flames can never cross into the primary world.
TEST_CASE( "fire_isolated_between_dimensions", "[simulation][field][dimension]" )
{
    clear_all_state();
    put_player_underground();

    auto &dim = MAPBUFFER_REGISTRY.get( TEST_DIM_ID );
    auto *dim_sm = make_blank_submap( dim, FAR_SM_POS );
    REQUIRE( dim_sm != nullptr );

    const auto fire_pt = point_sm_ms{ 5, 5 };
    plant_fire( *dim_sm, fire_pt );

    // Primary dimension must have no fire at the same absolute position.
    if( const auto *primary_sm = MAPBUFFER.lookup_submap_in_memory( FAR_SM_POS ) ) {
        REQUIRE( primary_sm->get_field( fire_pt ).find_field( fd_fire ) == nullptr );
    }

    // Process only the secondary dimension.
    process_fields_in_submap( *dim_sm, FAR_SM_POS, dim );

    // Fire in the secondary dimension must have aged (processing occurred).
    const auto *dim_fire = dim_sm->get_field( fire_pt ).find_field( fd_fire );
    REQUIRE( dim_fire != nullptr );
    CHECK( dim_fire->get_field_age() == 1_turns );

    // Primary dimension must still be fire-free — no cross-dimension spread.
    if( const auto *primary_sm = MAPBUFFER.lookup_submap_in_memory( FAR_SM_POS ) ) {
        CHECK( primary_sm->get_field( fire_pt ).find_field( fd_fire ) == nullptr );
    }

    MAPBUFFER_REGISTRY.unload_dimension( TEST_DIM_ID );
}

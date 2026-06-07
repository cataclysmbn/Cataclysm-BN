#include "catch/catch.hpp"
#include "map_helpers.h"

#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "coordinates.h"
#include "game.h"
#include "map.h"
#include "options_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE( "solar_cache_uses_date_sensitive_hour", "[vision][zlevel][sun]" )
{
    clear_all_state();

    override_option fov3d( "FOV_3D", "true" );
    override_option fov3d_occlusion( "FOV_3D_OCCLUSION", "true" );
    const auto restore_fov_3d = restore_on_out_of_scope<bool>( fov_3d );
    const auto restore_fov_3d_occlusion = restore_on_out_of_scope<bool>( fov_3d_occlusion );
    fov_3d = true;
    fov_3d_occlusion = true;

    auto &here = get_map();
    const auto one_season = calendar::season_length();
    const auto summer_after_sunrise = calendar::turn_zero + one_season + 5_hours + 30_minutes;
    const auto winter_before_sunrise = calendar::turn_zero + one_season * 3 + 5_hours + 30_minutes;
    const auto sample = tripoint_bub_ms( 60, 60, OVERMAP_HEIGHT );

    REQUIRE( time_past_midnight( summer_after_sunrise ) ==
             time_past_midnight( winter_before_sunrise ) );
    REQUIRE( summer_after_sunrise > sunrise( summer_after_sunrise ) );
    REQUIRE( summer_after_sunrise < sunset( summer_after_sunrise ) );
    REQUIRE( winter_before_sunrise < sunrise( winter_before_sunrise ) );

    calendar::turn = summer_after_sunrise;
    g->reset_light_level();
    here.invalidate_map_cache( sample.z() );
    here.build_map_cache( sample.z() );

    const auto &summer_cache = here.access_cache( sample.z() );
    const auto sample_idx = static_cast<size_t>( summer_cache.idx( sample.x(), sample.y() ) );
    REQUIRE( summer_cache.angled_sunlight_cache[sample_idx] );

    calendar::turn = winter_before_sunrise;
    g->reset_light_level();
    here.invalidate_lightmap_caches();
    here.build_map_cache( sample.z() );

    const auto &winter_cache = here.access_cache( sample.z() );
    CHECK_FALSE( winter_cache.angled_sunlight_cache[sample_idx] );
}

TEST_CASE( "opening_floor_invalidates_below_seen_cache", "[vision][zlevel]" )
{
    clear_all_state();

    map &here = get_map();

    const ter_id t_floor( "t_floor" );
    const ter_id t_open_air( "t_open_air" );

    // Place the player on z=1 so we have a meaningful "below".
    g->place_player( tripoint_bub_ms( 60, 60, 1 ) );

    const auto hole_pos = g->u.bub_pos() + point_east;

    // Ensure deterministic starting terrain.
    here.ter_set( hole_pos, t_floor );
    here.ter_set( hole_pos + tripoint_below, t_floor );

    // Simulate the pre-breach state where the below-z tile wasn't previously seen.
    level_cache &below_cache = here.access_cache( hole_pos.z() - 1 );
    below_cache.seen_cache_dirty = false;
    below_cache.seen_cache[below_cache.idx( hole_pos.x(), hole_pos.y() )] = 0.0f;
    below_cache.camera_cache[below_cache.idx( hole_pos.x(), hole_pos.y() )] = 0.0f;

    REQUIRE_FALSE( here.access_cache( hole_pos.z() - 1 ).seen_cache_dirty );

    // Breach the floor (what explosions often do). This must invalidate the below-z seen cache.
    here.ter_set( hole_pos, t_open_air );

    CHECK( here.access_cache( hole_pos.z() - 1 ).seen_cache_dirty );
}

TEST_CASE( "opening_floor_rebuilds_below_light", "[vision][zlevel]" )
{
    clear_all_state();

    override_option fov3d( "FOV_3D", "false" );

    map &here = get_map();

    const ter_id t_floor( "t_floor" );
    const ter_id t_open_air( "t_open_air" );

    g->place_player( tripoint_bub_ms( 60, 60, 1 ) );

    calendar::turn = calendar::turn_zero + 12_hours;

    const auto hole_pos = g->u.bub_pos() + point_east;

    here.ter_set( hole_pos, t_open_air );
    here.ter_set( hole_pos + tripoint_below, t_floor );

    here.build_map_cache( g->u.bub_pos().z() );
    here.update_visibility_cache( g->u.bub_pos().z() );

    const level_cache &below_cache = here.access_cache( hole_pos.z() - 1 );

    CHECK( below_cache.seen_cache[below_cache.idx( hole_pos.x(), hole_pos.y() )] > 0.0f );
    CHECK( below_cache.visibility_cache[below_cache.idx( hole_pos.x(),
                                                         hole_pos.y() )] != lit_level::BLANK );
}

#include "catch/catch.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include "calendar.h"
#include "enums.h"
#include "game_constants.h"
#include "numeric_interval.h"
#include "omdata.h"
#include "overmap.h"
#include "overmap_special.h"
#include "overmap_types.h"
#include "overmapbuffer.h"
#include "point.h"
#include "rng.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE( "set_and_get_overmap_scents", "[overmap]" )
{
    clear_all_state();
    std::unique_ptr<overmap> test_overmap = std::make_unique<overmap>( point_abs_om() );

    // By default there are no scents set.
    for( int x = 0; x < 180; ++x ) {
        for( int y = 0; y < 180; ++y ) {
            for( int z = -10; z < 10; ++z ) {
                REQUIRE( test_overmap->scent_at( { x, y, z } ).creation_time == calendar::before_time_starts );
            }
        }
    }

    const time_point tp = calendar::turn_zero + time_duration::from_turns( 50 );
    scent_trace test_scent( tp, 90 );
    test_overmap->set_scent( { 75, 85, 0 }, test_scent );
    REQUIRE( test_overmap->scent_at( { 75, 85, 0} ).creation_time == tp );
    REQUIRE( test_overmap->scent_at( { 75, 85, 0} ).initial_strength == 90 );
}

TEST_CASE( "default_overmap_generation_always_succeeds", "[overmap][slow]" )
{
    clear_all_state();
    int overmaps_to_construct = 10;
    for( const point_abs_om &candidate_addr : closest_points_first( point_abs_om(), 10 ) ) {
        // Skip populated overmaps.
        if( overmap_buffer.has( candidate_addr ) ) {
            continue;
        }
        overmap_special_batch test_specials = overmap_specials::get_default_batch( candidate_addr );
        overmap_buffer.create_custom_overmap( candidate_addr, test_specials );
        for( const auto &special_placement : test_specials ) {
            auto special = special_placement.special_details;
            if( special->has_flag( "UNIQUE" ) || special->has_flag( "GLOBALLY_UNIQUE" ) ) {
                continue;
            }
            INFO( "In attempt #" << overmaps_to_construct
                  << " failed to place " << special->id.str() );
            int min_occur = special->get_constraints().occurrences.min;
            CHECK( min_occur <= special_placement.instances_placed );
        }
        if( --overmaps_to_construct <= 0 ) {
            break;
        }
    }
}
namespace
{

void do_lab_finale_test()
{
    const oter_id labt_endgame( "central_lab_endgame" );
    const point_abs_om origin;
    auto batch = overmap_specials::get_default_batch( origin );
    overmap_buffer.create_custom_overmap( origin, batch );
    overmap *test_overmap = overmap_buffer.get_existing( origin );
    int endgame_count = 0;
    for( int z = -OVERMAP_DEPTH; z < 0; ++z ) {
        for( int x = 0; x < OMAPX; ++x ) {
            for( int y = 0; y < OMAPY; ++y ) {
                const oter_id t = test_overmap->ter( { x, y, z } );
                if( t == labt_endgame ) {
                    endgame_count++;
                }
            }
        }
    }
    CHECK( endgame_count == 1 );
}

} //namespace

TEST_CASE( "Exactly one endgame lab finale is generated in 0,0 overmap", "[overmap][slow]" )
{
    clear_all_state();
    do_lab_finale_test();
}

TEST_CASE( "Brute force default batch generation to check for RNG bugs", "[.][overmap][slow]" )
{
    clear_all_state();
    for( size_t i = 0; i < 100; i++ ) {
        do_lab_finale_test();
    }
}

TEST_CASE( "is_ot_match", "[overmap][terrain]" )
{
    clear_all_state();
    SECTION( "exact match" ) {
        // Matches the complete string
        CHECK( is_ot_match( "forest", oter_id( "forest" ), ot_match_type::exact ) );
        CHECK( is_ot_match( "central_lab", oter_id( "central_lab" ), ot_match_type::exact ) );

        // Does not exactly match if rotation differs
        CHECK_FALSE( is_ot_match( "sub_station", oter_id( "sub_station_north" ), ot_match_type::exact ) );
        CHECK_FALSE( is_ot_match( "sub_station", oter_id( "sub_station_south" ), ot_match_type::exact ) );
    }

    SECTION( "type match" ) {
        // Matches regardless of rotation
        CHECK( is_ot_match( "sub_station", oter_id( "sub_station_north" ), ot_match_type::type ) );
        CHECK( is_ot_match( "sub_station", oter_id( "sub_station_south" ), ot_match_type::type ) );
        CHECK( is_ot_match( "sub_station", oter_id( "sub_station_east" ), ot_match_type::type ) );
        CHECK( is_ot_match( "sub_station", oter_id( "sub_station_west" ), ot_match_type::type ) );

        // Does not match if base type does not match
        CHECK_FALSE( is_ot_match( "lab", oter_id( "central_lab" ), ot_match_type::type ) );
        CHECK_FALSE( is_ot_match( "sub_station", oter_id( "sewer_sub_station_north" ),
                                  ot_match_type::type ) );
    }

    SECTION( "prefix match" ) {
        // Matches the complete string
        CHECK( is_ot_match( "forest", oter_id( "forest" ), ot_match_type::prefix ) );
        CHECK( is_ot_match( "central_lab", oter_id( "central_lab" ), ot_match_type::prefix ) );

        // Prefix matches when an underscore separator exists
        CHECK( is_ot_match( "central", oter_id( "central_lab" ), ot_match_type::prefix ) );
        CHECK( is_ot_match( "central", oter_id( "central_lab_stairs" ), ot_match_type::prefix ) );

        // Prefix itself may contain underscores
        CHECK( is_ot_match( "central_lab", oter_id( "central_lab_stairs" ), ot_match_type::prefix ) );
        CHECK( is_ot_match( "central_lab_train", oter_id( "central_lab_train_depot" ),
                            ot_match_type::prefix ) );

        // Prefix does not match without an underscore separator
        CHECK_FALSE( is_ot_match( "fore", oter_id( "forest" ), ot_match_type::prefix ) );
        CHECK_FALSE( is_ot_match( "fore", oter_id( "forest_thick" ), ot_match_type::prefix ) );

        // Prefix does not match the middle or end
        CHECK_FALSE( is_ot_match( "lab", oter_id( "central_lab" ), ot_match_type::prefix ) );
        CHECK_FALSE( is_ot_match( "lab", oter_id( "central_lab_stairs" ), ot_match_type::prefix ) );
    }

    SECTION( "contains match" ) {
        // Matches the complete string
        CHECK( is_ot_match( "forest", oter_id( "forest" ), ot_match_type::contains ) );
        CHECK( is_ot_match( "central_lab", oter_id( "central_lab" ), ot_match_type::contains ) );

        // Matches the beginning/middle/end of an underscore-delimited id
        CHECK( is_ot_match( "central", oter_id( "central_lab_stairs" ), ot_match_type::contains ) );
        CHECK( is_ot_match( "lab", oter_id( "central_lab_stairs" ), ot_match_type::contains ) );
        CHECK( is_ot_match( "stairs", oter_id( "central_lab_stairs" ), ot_match_type::contains ) );

        // Matches the beginning/middle/end without undercores as well
        CHECK( is_ot_match( "cent", oter_id( "central_lab_stairs" ), ot_match_type::contains ) );
        CHECK( is_ot_match( "ral_lab", oter_id( "central_lab_stairs" ), ot_match_type::contains ) );
        CHECK( is_ot_match( "_lab_", oter_id( "central_lab_stairs" ), ot_match_type::contains ) );
        CHECK( is_ot_match( "airs", oter_id( "central_lab_stairs" ), ot_match_type::contains ) );

        // Does not match if substring is not contained
        CHECK_FALSE( is_ot_match( "forest", oter_id( "central_lab" ), ot_match_type::contains ) );
        CHECK_FALSE( is_ot_match( "forestry", oter_id( "forest" ), ot_match_type::contains ) );
    }
}

TEST_CASE( "mutable_overmap_placement", "[overmap][slow]" )
{
    const overmap_special &special =
        *overmap_special_id( GENERATE( "test_anthill", "test_crater" ) );
    const city cit;

    constexpr int num_overmaps = 100;
    constexpr int num_trials_per_overmap = 100;

    for( int j = 0; j < num_overmaps; ++j ) {
        // overmap objects are really large, so we don't want them on the
        // stack.  Use unique_ptr and put it on the heap
        std::unique_ptr<overmap> om = std::make_unique<overmap>( point_abs_om( point_zero ) );
        om_direction::type dir = om_direction::type::north;

        int successes = 0;

        for( int i = 0; i < num_trials_per_overmap; ++i ) {
            tripoint_om_omt try_pos( rng( 0, OMAPX - 1 ), rng( 0, OMAPY - 1 ), 0 );

            // This test can get very spammy, so abort once an error is
            // observed
            if( debug_has_error_been_observed() ) {
                return;
            }

            if( om->can_place_special( special, try_pos, dir, false ) ) {
                std::vector<tripoint_om_omt> placed_points =
                    om->place_special( special, try_pos, dir, cit, false, false );
                CHECK( !placed_points.empty() );
                ++successes;
            }
        }

        CHECK( successes > num_trials_per_overmap / 2 );
    }
}

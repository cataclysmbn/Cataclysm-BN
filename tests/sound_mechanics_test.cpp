#include "catch/catch.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_set>

#include "debug.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "sounds.h"
#include "state_helpers.h"

namespace
{
struct sound_direction_case {
    tripoint_bub_ms listener;
    uint8_t expected;
    const char *label;
};
} // namespace

TEST_CASE( "sound_direction_index_matches_compass_directions", "[sound]" )
{
    const auto source = tripoint_bub_ms( 60, 60, 0 );
    const auto cases = std::array<sound_direction_case, 12> { {
            { tripoint_bub_ms( 50, 50, 0 ), SDI_NW, "northwest" },
            { tripoint_bub_ms( 60, 50, 0 ), SDI_N, "north" },
            { tripoint_bub_ms( 70, 50, 0 ), SDI_NE, "northeast" },
            { tripoint_bub_ms( 70, 60, 0 ), SDI_E, "east" },
            { tripoint_bub_ms( 70, 70, 0 ), SDI_SE, "southeast" },
            { tripoint_bub_ms( 60, 70, 0 ), SDI_S, "south" },
            { tripoint_bub_ms( 50, 70, 0 ), SDI_SW, "southwest" },
            { tripoint_bub_ms( 50, 60, 0 ), SDI_W, "west" },
            { tripoint_bub_ms( 70, 59, 0 ), SDI_E, "slightly north of east" },
            { tripoint_bub_ms( 70, 61, 0 ), SDI_E, "slightly south of east" },
            { tripoint_bub_ms( 50, 59, 0 ), SDI_W, "slightly north of west" },
            { tripoint_bub_ms( 50, 61, 0 ), SDI_W, "slightly south of west" },
        }
    };

    for( const auto &test_case : cases ) {
        CAPTURE( test_case.label );
        CHECK( sounds::direction_index_to_sound_source( source,
                test_case.listener ) == test_case.expected );
    }

    CHECK( sounds::direction_index_to_sound_source( source,
            tripoint_bub_ms( 60, 60, -1 ) ) == SDI_DOWN );
    CHECK( sounds::direction_index_to_sound_source( source,
            tripoint_bub_ms( 60, 60, 1 ) ) == SDI_UP );
}

TEST_CASE( "sound_filter_key_distinguishes_noise_fear", "[sound]" )
{
    auto ignores_noise = sound_filter_key();
    auto fears_noise = ignores_noise;
    fears_noise.noise_fear = true;

    CHECK_FALSE( ignores_noise == fears_noise );

    auto filter_keys = std::unordered_set<sound_filter_key>();
    filter_keys.insert( ignores_noise );
    filter_keys.insert( fears_noise );

    CHECK( filter_keys.size() == 2 );
}

TEST_CASE( "flood_fill_sound_skips_out_of_cache_origins", "[sound][cache]" )
{
    clear_all_state();
    auto &here = get_map();
    const auto &level_cache = here.get_cache_ref( 0 );
    const auto invalid_origin = tripoint_bub_ms( -1, 60, 0 );
    auto out_of_cache_sound = sound_event{
        .volume = 80,
        .origin = invalid_origin,
        .category = sounds::sound_t::alarm,
        .description = "out of cache regression sound"
    };

    REQUIRE_FALSE( level_cache.inbounds( invalid_origin.xy() ) );

    const auto debug_msg = capture_debugmsg_during( [&]() {
        here.flood_fill_sound( out_of_cache_sound, invalid_origin.z() );
    } );

    CHECK( debug_msg.empty() );
    CHECK( here.m_sound_cache.sound_instances.empty() );
}

TEST_CASE( "process_sounds_skips_out_of_cache_monsters", "[sound][cache]" )
{
    clear_all_state();
    auto &here = get_map();
    const auto &level_cache = here.get_cache_ref( 0 );
    auto source_sound = sound_event{
        .volume = 80,
        .origin = tripoint_bub_ms( 60, 60, 0 ),
        .category = sounds::sound_t::alarm,
        .description = "monster cache regression sound"
    };
    auto cache = sound_instance_cache( source_sound, get_flood_dist_enum( source_sound.volume ),
                                       get_flood_radius_by_enum( get_flood_dist_enum( source_sound.volume ) ) );
    cache.terrain_sound_absorbtion_at_source = level_cache.absorption_cache[level_cache.idx(
                source_sound.origin.x(), source_sound.origin.y() )];
    here.m_sound_cache.sound_instances.push_back( cache );

    auto &critter = spawn_test_monster( "mon_zombie", tripoint_bub_ms( 60, 61, 0 ) );
    const auto invalid_critter_pos = tripoint_bub_ms( level_cache.cache_x, 60, 0 );
    critter.setpos( invalid_critter_pos );

    REQUIRE_FALSE( level_cache.inbounds( invalid_critter_pos.xy() ) );

    const auto debug_msg = capture_debugmsg_during( [&]() {
        sounds::process_sounds();
    } );

    CHECK( debug_msg.empty() );
    CHECK( here.m_sound_cache.sound_instances.front().heard_by_monsters );
}

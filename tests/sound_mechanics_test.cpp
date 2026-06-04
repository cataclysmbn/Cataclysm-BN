#include "catch/catch.hpp"

#include <array>
#include <cstdint>
#include <unordered_set>

#include "map.h"
#include "sounds.h"

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

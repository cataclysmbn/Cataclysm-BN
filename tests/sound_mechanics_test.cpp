#include "catch/catch.hpp"
#include "map.h"
#include "map_helpers.h"
#include "sounds.h"

#include <array>
#include <cstdint>
#include <unordered_set>

namespace {
struct sound_direction_case {
    tripoint_bub_ms listener;
    uint8_t expected;
    const char* label;
};
} // namespace

TEST_CASE("sound_direction_index_matches_compass_directions", "[sound]") {
    const auto source = bub_test_origin();
    const auto cases = std::array<sound_direction_case, 12>{{
        {source + point_rel_ms(-10, -10), SDI_NW, "northwest"},
        {source + point_rel_ms(0, -10), SDI_N, "north"},
        {source + point_rel_ms(10, -10), SDI_NE, "northeast"},
        {source + point_rel_ms(10, 0), SDI_E, "east"},
        {source + point_rel_ms(10, 10), SDI_SE, "southeast"},
        {source + point_rel_ms(0, 10), SDI_S, "south"},
        {source + point_rel_ms(-10, 10), SDI_SW, "southwest"},
        {source + point_rel_ms(-10, 0), SDI_W, "west"},
        {source + point_rel_ms(10, -1), SDI_E, "slightly north of east"},
        {source + point_rel_ms(10, 1), SDI_E, "slightly south of east"},
        {source + point_rel_ms(-10, -1), SDI_W, "slightly north of west"},
        {source + point_rel_ms(-10, 1), SDI_W, "slightly south of west"},
    }};

    for (const auto& test_case : cases) {
        CAPTURE(test_case.label);
        CHECK(sounds::direction_index_to_sound_source(source, test_case.listener)
              == test_case.expected);
    }

    CHECK(sounds::direction_index_to_sound_source(source, source + tripoint_rel_ms::below())
          == SDI_DOWN);
    CHECK(sounds::direction_index_to_sound_source(source, source + tripoint_rel_ms::above())
          == SDI_UP);
}

TEST_CASE("sound_filter_key_distinguishes_noise_fear", "[sound]") {
    auto ignores_noise = sound_filter_key();
    auto fears_noise = ignores_noise;
    fears_noise.noise_fear = true;

    CHECK_FALSE(ignores_noise == fears_noise);

    auto filter_keys = std::unordered_set<sound_filter_key>();
    filter_keys.insert(ignores_noise);
    filter_keys.insert(fears_noise);

    CHECK(filter_keys.size() == 2);
}

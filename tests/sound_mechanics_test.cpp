#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "debug.h"
#include "line.h"
#include "map.h"
#include "map_helpers.h"
#include "sounds.h"
#include "state_helpers.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_set>

namespace {
struct sound_direction_case {
    tripoint_bub_ms listener;
    uint8_t expected;
    const char* label;
};
} // namespace

TEST_CASE("sound_direction_index_matches_compass_directions", "[sound]") {
    const auto source = tripoint_bub_ms(60, 60, 0);
    const auto cases = std::array<sound_direction_case, 12>{{
        {tripoint_bub_ms(50, 50, 0), SDI_NW, "northwest"},
        {tripoint_bub_ms(60, 50, 0), SDI_N, "north"},
        {tripoint_bub_ms(70, 50, 0), SDI_NE, "northeast"},
        {tripoint_bub_ms(70, 60, 0), SDI_E, "east"},
        {tripoint_bub_ms(70, 70, 0), SDI_SE, "southeast"},
        {tripoint_bub_ms(60, 70, 0), SDI_S, "south"},
        {tripoint_bub_ms(50, 70, 0), SDI_SW, "southwest"},
        {tripoint_bub_ms(50, 60, 0), SDI_W, "west"},
        {tripoint_bub_ms(70, 59, 0), SDI_E, "slightly north of east"},
        {tripoint_bub_ms(70, 61, 0), SDI_E, "slightly south of east"},
        {tripoint_bub_ms(50, 59, 0), SDI_W, "slightly north of west"},
        {tripoint_bub_ms(50, 61, 0), SDI_W, "slightly south of west"},
    }};

    for (const auto& test_case : cases) {
        CAPTURE(test_case.label);
        CHECK(sounds::direction_index_to_sound_source(source, test_case.listener)
              == test_case.expected);
    }

    CHECK(sounds::direction_index_to_sound_source(source, tripoint_bub_ms(60, 60, -1)) == SDI_DOWN);
    CHECK(sounds::direction_index_to_sound_source(source, tripoint_bub_ms(60, 60, 1)) == SDI_UP);
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

TEST_CASE("sound_distance_loss_never_amplifies", "[sound]") {
    CHECK(get_cumulative_vol_dist_loss(0, 70, 0) == 0);
    CHECK(get_cumulative_vol_dist_loss(4, 0, 0) == 0);
    CHECK(get_cumulative_vol_dist_loss(4, 4, 0) == 0);
    CHECK(get_cumulative_vol_dist_loss(12, 5, 0) == 0);

    const short open_field_loss = get_cumulative_vol_dist_loss(4, 105, 0);
    CHECK(open_field_loss >= 0);
    CHECK(open_field_loss <= MAXIMUM_VOLUME_ATMOSPHERE);
    CHECK(get_cumulative_vol_dist_loss(4, 105, -254) == open_field_loss);

    constexpr short origin_mdB = 4000;
    CHECK(origin_mdB - open_field_loss <= origin_mdB);
    CHECK(origin_mdB - open_field_loss != 26825);
}

TEST_CASE("sound_flood_envelope_matches_volume_vector", "[sound]") {
    sound_event se;
    se.origin = tripoint_bub_ms(40, 40, 0);
    se.volume = 40;
    se.description = "growling";
    sound_instance_cache cache(se, sound_vol_for_flood_dist::QUIET, flood_radius_QUIET);

    REQUIRE(cache.envelope_side() == (2 * flood_radius_QUIET) + 1);
    REQUIRE(
        cache.volume.size() == static_cast<size_t>(cache.envelope_side() * cache.envelope_side()));
    CHECK(cache.in_envelope(se.origin));
    CHECK(cache.vol_at_tri(se.origin) == 0);
    CHECK_FALSE(cache.in_envelope(se.origin + tripoint_rel_ms(10, 0, 0)));

    cache.dist_enum = sound_vol_for_flood_dist::DEAFENING;
    CHECK_FALSE(cache.in_envelope(se.origin + tripoint_rel_ms(10, 0, 0)));
    CHECK(cache.vol_at_tri(se.origin + tripoint_rel_ms(10, 0, 0)) == 0);
}

TEST_CASE("quiet_monster_growl_is_not_impossibly_loud_at_range", "[sound]") {
    const auto cleanup = on_out_of_scope([]() {
        sounds::reset_sounds();
        clear_all_state();
    });
    clear_all_state();
    clear_map();

    auto& here = get_map();
    auto& player_character = get_avatar();
    const int half = here.getmapsize() * SEEX / 2;
    const auto listener = tripoint_bub_ms(half, half, 0);
    auto source = listener + tripoint_rel_ms(34, 71, 0);
    if (!here.inbounds(source)) { source = listener + tripoint_rel_ms(20, 20, 0); }
    REQUIRE(here.inbounds(listener));
    REQUIRE(here.inbounds(source));
    REQUIRE(rl_dist(listener, source) > flood_radius_QUIET);

    player_character.setpos(listener);
    sounds::reset_sounds();

    sound_event se;
    se.origin = source;
    se.volume = 40;
    se.category = sounds::sound_t::movement;
    se.movement_noise = true;
    se.description = "growling";
    se.from_monster = true;
    sounds::sound(se);
    here.batch_flood_fill_sounds();

    const auto debug_msg = capture_debugmsg_during([&]() {
        sounds::process_sound_markers(&player_character);
    });
    CHECK(debug_msg.find("impossibly loud") == std::string::npos);
    CHECK(debug_msg.find("impossible escape volume") == std::string::npos);
    CHECK(debug_msg.find("louder volume than the origin") == std::string::npos);
}

#include "../src/map/map.h"
#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "game.h"
#include "options_helpers.h"
#include "sounds.h"
#include "state_helpers.h"
#include "tile_helpers.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_set>
#include <vector>

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

TEST_CASE("queued_sounds_outside_resized_map_are_discarded", "[sound][resize]") {
    auto& here = get_map();
    const auto old_size = here.getmapsize();
    const auto cleanup = on_out_of_scope([&]() {
        sounds::reset_sounds();
        sounds::clear_floodfill_que(true);
        here.resize(old_size);
    });
    sounds::reset_sounds();
    here.resize(13);
    const auto sources = std::array{
        tripoint_bub_ms(108, 0, 5), tripoint_bub_ms(0, 108, 5), tripoint_bub_ms(146, 40, 5),
        tripoint_bub_ms(107, 107, 5), tripoint_bub_ms(0, 0, 5)};
    for (const auto& source : sources) {
        REQUIRE(here.inbounds(source));
        sounds::sound(
            {.volume = 50,
             .origin = source,
             .category = sounds::sound_t::movement,
             .description = "gasping",
             .movement_noise = true,
             .from_monster = true});
    }
    here.resize(9);
    here.batch_flood_fill_sounds();

    const auto& instances = here.m_sound_cache.sound_instances;
    REQUIRE(instances.size() == 2);
    CHECK(instances[0].origin == sources[3]);
    CHECK(instances[1].origin == sources[4]);
    here.batch_flood_fill_sounds();
    CHECK(instances.size() == 2);
}

TEST_CASE("sounds_keep_absolute_positions_when_reality_bubble_resizes", "[sound][resize]") {
#if defined(TILES)
    const auto tiles = tile_context_fixture(true);
    REQUIRE(tiles.valid());
#endif
    clear_all_state();
    auto& here = get_map();
    auto& you = get_avatar();
    const auto original_position = you.abs_pos();
    const auto cleanup = on_out_of_scope([&]() {
        sounds::reset_sounds();
        sounds::clear_floodfill_que(true);
        g->on_options_changed();
        you.setpos(original_position);
        clear_all_state();
    });
    const auto larger_bubble = override_option("REALITY_BUBBLE_SIZE", "5");
    g->on_options_changed();
    you.setpos(map_local_to_abs(here, tripoint_bub_ms(72, 72, 0)));
    here.build_map_cache(0);
    sounds::reset_sounds();

    const auto cached_source = tripoint_bub_ms(80, 80, 0);
    const auto cached_absolute = map_local_to_abs(here, cached_source);
    sounds::sound(
        {.volume = 50,
         .origin = cached_source,
         .category = sounds::sound_t::movement,
         .description = "footsteps",
         .from_player = true});
    auto& instances = here.m_sound_cache.sound_instances;
    REQUIRE(instances.size() == 1);
    const auto cached_volume = instances.front().vol_at_tri(cached_source);
    REQUIRE(cached_volume > 0);
    here.m_sound_cache.sound_list_filtered[sound_filter_key()] = {0};

    const auto retained_sources = std::array{
        tripoint_bub_ms(24, 24, 0), tripoint_bub_ms(131, 24, 0), tripoint_bub_ms(24, 131, 0),
        tripoint_bub_ms(131, 131, 0)};
    const auto discarded_sources = std::array{
        tripoint_bub_ms(23, 80, 0), tripoint_bub_ms(132, 80, 0), tripoint_bub_ms(80, 23, 0),
        tripoint_bub_ms(80, 132, 0)};
    auto retained_absolute = std::vector<tripoint_abs_ms>{cached_absolute};
    for (const auto& source : retained_sources) {
        retained_absolute.push_back(map_local_to_abs(here, source));
    }
    for (const auto& sources : {retained_sources, discarded_sources}) {
        for (const auto& source : sources) {
            sounds::sound(
                {.volume = 50,
                 .origin = source,
                 .category = sounds::sound_t::movement,
                 .description = "gasping",
                 .from_monster = true});
        }
    }

    {
        const auto smaller_bubble = override_option("REALITY_BUBBLE_SIZE", "3");
        g->on_options_changed();
        REQUIRE(here.getmapsize() == 9);
        REQUIRE(instances.size() == 1);
        REQUIRE(map_local_to_abs(here, instances.front().origin) == cached_absolute);
        CHECK(here.m_sound_cache.sound_list_filtered.empty());
        CHECK(instances.front().vol_at_tri(instances.front().origin) == cached_volume);

        here.batch_flood_fill_sounds();
        REQUIRE(instances.size() == retained_absolute.size());
        for (const auto& sound : instances) {
            CHECK(here.inbounds(sound.origin));
            CHECK(sound.origin == sound.sound.origin);
            CHECK(std::ranges::contains(retained_absolute, map_local_to_abs(here, sound.origin)));
        }

        sounds::sound(
            {.volume = 50,
             .origin = instances.front().origin,
             .category = sounds::sound_t::movement,
             .description = "gasping",
             .from_monster = true});
    }

    g->on_options_changed();
    REQUIRE(here.getmapsize() == 13);
    here.batch_flood_fill_sounds();
    REQUIRE(instances.size() == retained_absolute.size() + 1);
    for (const auto& sound : instances) {
        CHECK(here.inbounds(sound.origin));
        CHECK(sound.origin == sound.sound.origin);
        CHECK(std::ranges::contains(retained_absolute, map_local_to_abs(here, sound.origin)));
    }
    CHECK(instances.front().origin == cached_source);
    CHECK(instances.front().vol_at_tri(cached_source) == cached_volume);
    CHECK(instances.back().origin == cached_source);
}

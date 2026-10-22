#include "avatar.h"
#include "avatar_action.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "engine_client_presentation.h"
#include "game.h"
#include "item.h"
#include "map/map.h"
#include "map_helpers.h"
#include "map_perception.h"
#include "options_helpers.h"
#include "sdlsound.h"
#include "sounds.h"
#include "state_helpers.h"
#include "weather/weather.h"

#include <ranges>
#include <string>
#include <vector>

using namespace engine_client;

namespace {

constexpr auto you_at = tripoint_bub_ms(62, 62, 0);

/// A daylit room with sound on and a listener; every channel and the music start silent.
struct sound_scene {
    sound_scene(): live(test_mode), enabled(sounds::sound_enabled) {
        build_test_map(ter_id("t_floor"));
        calendar::turn = calendar::turn_zero + 12_hours;
        get_weather().weather_id = weather_type_id("clear");
        g->reset_light_level();
        get_avatar().setpos(you_at);
        test_mode = true;
        sounds::sound_enabled = true;
        auto& here = get_map();
        here.invalidate_visibility_caches();
        here.build_map_cache(0);
        map_perception::acquire();
        sfx::fade_audio_channel(sfx::channel::any, 0);
        stop_music();
        presentation::collect(true);
        presentation::take();
    }
    ~sound_scene() {
        sfx::fade_audio_channel(sfx::channel::any, 0);
        stop_music();
        presentation::collect(false);
    }
    sound_scene(const sound_scene&) = delete;
    auto operator=(const sound_scene&) -> sound_scene& = delete;

    bool cleared = (clear_all_state(), true);
    restore_on_out_of_scope<bool> live;
    restore_on_out_of_scope<bool> enabled;
    override_option effects{"SOUND_EFFECT_VOLUME", "100"};
    override_option ambient{"AMBIENT_SOUND_VOLUME", "64"};
    override_option music{"MUSIC_VOLUME", "50"};
};

auto cues(const std::string& type) -> std::vector<sound_cue> {
    return presentation::take()
         | std::views::filter([&](const auto& fact) { return fact.type == type; })
         | std::views::transform([](const auto& fact) { return fact.sound.value(); })
         | std::ranges::to<std::vector>();
}

} // namespace

TEST_CASE("opening a door publishes the native door sound", "[engine_client_sound_cue]") {
    // Without the native sound player there are no native sound calls to tell.
    if (!sound_supported()) { return; }
    const auto room = sound_scene{};
    const auto door = you_at + tripoint_east * 2;
    auto& here = get_map();
    here.ter_set(door, ter_id("t_door_c"));
    REQUIRE(here.open_door(static_cast<Character*>(&get_avatar()), door, true));
    here.batch_flood_fill_sounds();
    sounds::process_sound_markers(&get_avatar());
    const auto played = cues("sound.played");
    REQUIRE(played.size() == 1);
    CHECK(played[0].id == "open_door");
    CHECK(played[0].variant == "t_door_c");
    CHECK(played[0].volume == sfx::get_heard_volume(door, 50));
    CHECK_FALSE(played[0].angle.has_value());
}

TEST_CASE("walking into a locked door publishes the error sound", "[engine_client_sound_cue]") {
    if (!sound_supported()) { return; }
    const auto room = sound_scene{};
    auto& here = get_map();
    here.ter_set(you_at + tripoint_east, ter_id("t_door_locked"));
    REQUIRE_FALSE(avatar_action::move(get_avatar(), here, tripoint_rel_ms::east()));
    const auto played = cues("sound.played");
    REQUIRE(played.size() == 1);
    CHECK(played[0].id == "menu_error");
    // A step into an open floor makes no such sound.
    here.ter_set(you_at + tripoint_east, ter_id("t_floor"));
    CHECK(avatar_action::move(get_avatar(), here, tripoint_rel_ms::east()));
    CHECK(cues("sound.played").empty());
}

TEST_CASE("a gunshot publishes the native gun sound", "[engine_client_sound_cue]") {
    if (!sound_supported()) { return; }
    const auto room = sound_scene{};
    const auto gun = item::spawn("glock_19");
    // The native gun sound is chosen only outside test mode.
    test_mode = false;
    sfx::generate_gun_sound(you_at, *gun, 150);
    test_mode = true;
    const auto played = cues("sound.played");
    REQUIRE(played.size() == 1);
    CHECK(played[0].id == "fire_gun");
    CHECK(played[0].variant == "glock_19");
    CHECK(played[0].angle == 0);
    CHECK(played[0].stacks);
    CHECK(played[0].wait_ms == 80);
}

TEST_CASE(
    "every step publishes its footstep however fast the steps come", "[engine_client_sound_cue]") {
    if (!sound_supported()) { return; }
    const auto room = sound_scene{};
    test_mode = false;
    sfx::do_footstep();
    sfx::do_footstep();
    test_mode = true;
    const auto played = cues("sound.played");
    REQUIRE(played.size() == 2);
    CHECK(played[1].id == "plmove");
    CHECK(played[1].variant == "walk_barefoot");
    CHECK(played[1].wait_ms == 400);
}

TEST_CASE(
    "sound effect volume follows the native option and silence", "[engine_client_sound_cue]") {
    if (!sound_supported()) { return; }
    const auto room = sound_scene{};
    {
        const auto half = override_option{"SOUND_EFFECT_VOLUME", "50"};
        sfx::play_variant_sound("menu_move", "default", 100);
        const auto played = cues("sound.played");
        REQUIRE(played.size() == 1);
        CHECK(played[0].volume == 50);
    }
    // Nothing is played at volume 0 or with sound off, as the native player does.
    sfx::play_variant_sound("menu_move", "default", 0);
    sounds::sound_enabled = false;
    sfx::play_variant_sound("menu_move", "default", 100);
    sfx::play_ambient_variant_sound(
        "environment", "daytime", 100, sfx::channel::daytime_outdoors_env, 1000);
    CHECK(presentation::take().empty());
    CHECK_FALSE(presentation::channel_playing(sfx::channel::daytime_outdoors_env));
}

TEST_CASE(
    "a channel publishes what it starts, changes and stops once", "[engine_client_sound_cue]") {
    if (!sound_supported()) { return; }
    const auto room = sound_scene{};
    constexpr auto day = sfx::channel::daytime_outdoors_env;
    sfx::play_ambient_variant_sound("environment", "daytime", 100, day, 1000);
    // A sound that repeats until it stops plays on: the native player ignores a second start.
    sfx::play_ambient_variant_sound("environment", "daytime", 100, day, 1000);
    auto started = cues("sound.channel");
    REQUIRE(started.size() == 1);
    CHECK(started[0].channel == "daytime_outdoors_env");
    CHECK(started[0].id == "environment");
    CHECK(started[0].variant == "daytime");
    CHECK(started[0].volume == 64);
    CHECK(started[0].fade_ms == 1000);
    CHECK(started[0].loops == -1);
    CHECK(presentation::channel_playing(day));

    sfx::set_channel_volume(day, 30);
    sfx::set_channel_volume(day, 30);
    sfx::set_channel_3d_position(day, 90_degrees);
    const auto changed = cues("sound.channel");
    REQUIRE(changed.size() == 2);
    // The change sets the gain itself, as the native one does, past the volume option and the pack.
    CHECK(changed[0].gain == 30);
    CHECK(changed[0].volume == 64);
    CHECK(changed[0].id == "environment");
    CHECK(changed[1].angle == 90);
    CHECK(changed[1].fade_ms == 0);

    // A group fades the channels of it that play, once.
    sfx::fade_audio_group(sfx::group::time_of_day, 500);
    sfx::fade_audio_group(sfx::group::time_of_day, 500);
    const auto stopped = cues("sound.channel");
    REQUIRE(stopped.size() == 1);
    CHECK(stopped[0].channel == "daytime_outdoors_env");
    CHECK(stopped[0].id.empty());
    CHECK(stopped[0].fade_ms == 500);
    CHECK_FALSE(presentation::channel_playing(day));
    // A channel that plays nothing has nothing to change.
    sfx::set_channel_volume(day, 10);
    CHECK(cues("sound.channel").empty());
}

TEST_CASE(
    "a channel sound that ends by itself is published at every start",
    "[engine_client_sound_cue]") {
    if (!sound_supported()) { return; }
    const auto room = sound_scene{};
    constexpr auto radio = sfx::channel::radio;
    sfx::play_ambient_variant_sound("radio", "static", 100, radio, 300, -1, 0);
    sfx::play_ambient_variant_sound("radio", "static", 100, radio, 300, -1, 0);
    const auto started = cues("sound.channel");
    REQUIRE(started.size() == 2);
    CHECK(started[1].loops == 0);
    CHECK_FALSE(presentation::channel_playing(radio));
    // Stopping every channel stops it and the sound effects too, in one cue.
    sfx::fade_audio_channel(sfx::channel::any, 300);
    const auto stopped = cues("sound.channel");
    REQUIRE(stopped.size() == 1);
    CHECK(stopped[0].channel == "any");
    CHECK(stopped[0].fade_ms == 300);
}

TEST_CASE(
    "a new listener hears the channels and the music that already play",
    "[engine_client_sound_cue]") {
    if (!sound_supported()) { return; }
    const auto room = sound_scene{};
    constexpr auto day = sfx::channel::daytime_outdoors_env;
    // The title music starts before the first client listens.
    presentation::collect(false);
    play_music("title");
    sfx::play_ambient_variant_sound("environment", "daytime", 100, day, 1000);
    CHECK(presentation::channel_playing(day));
    presentation::collect(true);
    const auto told = presentation::take();
    REQUIRE(told.size() == 2);
    CHECK(told[0].type == "sound.channel");
    CHECK(told[0].sound->id == "environment");
    CHECK(told[1].type == "music.changed");
    CHECK(told[1].sound->playlist == "title");
    // A new stream of the same listener goes on from the facts it was told.
    presentation::collect(true);
    CHECK(presentation::take().empty());
    // A listener that leaves forgets what it was told.
    presentation::collect(false);
    CHECK_FALSE(presentation::channel_playing(day));
    presentation::collect(true);
    CHECK(presentation::take().empty());
}

TEST_CASE("the music publishes its playlist when it changes", "[engine_client_sound_cue]") {
    if (!sound_supported()) { return; }
    const auto room = sound_scene{};
    play_music("title");
    play_music("title");
    auto changed = cues("music.changed");
    REQUIRE(changed.size() == 1);
    CHECK(changed[0].playlist == "title");
    CHECK(changed[0].volume == 50);
    stop_music();
    stop_music();
    changed = cues("music.changed");
    REQUIRE(changed.size() == 1);
    CHECK(changed[0].playlist.empty());
}

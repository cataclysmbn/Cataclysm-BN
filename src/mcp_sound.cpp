#include "sdlsound.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cached_options.h"
#include "debug.h"
#include "engine_client_presentation.h"
#include "init.h"
#include "json.h"
#include "options.h"
#include "path_info.h"
#include "sounds.h"

/// The sound API of the protocol engine: nothing plays here, a client plays what the facts tell it.
namespace
{

/// The sound pack's effects, for the variants the game picks.
auto effects = std::set<std::pair<std::string, std::string>> {};
auto current_soundpack_path = std::string {};

/// The reserved channels a group fades together.
auto group_channels( const sfx::group group ) -> std::vector<sfx::channel>
{
    using ch = sfx::channel;
    switch( group ) {
        case sfx::group::time_of_day:
            return { ch::daytime_outdoors_env, ch::nighttime_outdoors_env };
        case sfx::group::weather:
            return { ch::underground_env, ch::indoors_env, ch::indoors_rain_env, ch::outdoors_snow_env,
                     ch::outdoors_flurry_env, ch::outdoors_thunderstorm_env, ch::outdoors_rain_env,
                     ch::outdoors_drizzle_env, ch::outdoor_blizzard };
        case sfx::group::context_themes:
            return { ch::danger_extreme_theme, ch::danger_high_theme, ch::danger_medium_theme, ch::danger_low_theme };
        case sfx::group::fatigue:
            return { ch::stamina_75, ch::stamina_50, ch::stamina_35 };
    }
    return {};
}

} // namespace

bool sounds::sound_enabled = false;

auto sound_supported() -> bool { return true; }
auto init_sound() -> bool { return false; }
auto shutdown_sound() -> void {}
auto update_volumes() -> void {}
auto play_music( const std::string &playlist ) -> void { engine_client::presentation::record_music( playlist ); }
auto stop_music() -> void { engine_client::presentation::record_music( {} ); }
auto soundpack_path() -> std::string { return current_soundpack_path; }

auto load_soundset() -> void
{
    auto pack = get_option<std::string>( "SOUNDPACKS" );
    current_soundpack_path = pack.empty() ? std::string{} :
                             SOUNDPACKS[pack];
    if( current_soundpack_path.empty() ) { current_soundpack_path = PATH_INFO::defaultsounddir(); }
    try {
        init::load_soundpack_files( current_soundpack_path );
    } catch( const std::exception &err ) {
        DebugLog( DL::Error, DC::SDL ) << "failed to load sounds: " << err.what();
    }
}

auto sfx::load_sound_effects( const JsonObject &jsobj ) -> void
{
    jsobj.allow_omitted_members();
    if( !jsobj.get_array( "files" ).empty() ) {
        effects.emplace( jsobj.get_string( "id" ), jsobj.get_string( "variant", "default" ) );
    }
}

auto sfx::load_sound_effect_preload( const JsonObject &jsobj ) -> void { jsobj.allow_omitted_members(); }
auto sfx::load_playlist( const JsonObject &jsobj ) -> void { jsobj.allow_omitted_members(); }

auto sfx::has_variant_sound( const std::string &id, const std::string &variant ) -> bool
{
    return effects.contains( { id, variant } ) || effects.contains( { id, "default" } );
}

auto sfx::play_variant_sound( const std::string &id, const std::string &variant, const int volume,
                              const bool stacks ) -> void
{
    engine_client::presentation::record_sound_effect( { .id = id, .variant = variant, .volume = volume, .stacks = stacks } );
}

auto sfx::play_variant_sound( const std::string &id, const std::string &variant, const int volume,
                              const units::angle angle, double /*pitch_min*/, double /*pitch_max*/,
                              const bool stacks ) -> void
{
    engine_client::presentation::record_sound_effect( { .id = id, .variant = variant, .volume = volume, .angle = angle, .stacks = stacks } );
}

auto sfx::play_paced_sound( const paced_sound_options &options ) -> void
{
    engine_client::presentation::record_sound_effect( {
        .id = options.id, .variant = options.variant, .volume = options.volume,
        .angle = options.angle, .stacks = options.stacks, .wait_ms = options.wait_ms
    } );
}

auto sfx::play_ambient_variant_sound( const std::string &id, const std::string &variant,
                                      const int volume,
                                      const channel channel, const int fade_in_duration, double /*pitch*/,
                                      const int loops ) -> void
{
    engine_client::presentation::record_channel_sound( {
        .channel = channel, .id = id, .variant = variant, .volume = volume,
        .fade_in_ms = fade_in_duration, .loops = loops
    } );
}

auto sfx::fade_audio_group( const group group, const int duration ) -> void
{
    for( const auto channel : group_channels( group ) ) {
        engine_client::presentation::record_channel_stop( channel, duration );
    }
}

auto sfx::fade_audio_channel( const channel channel, const int duration ) -> void
{
    engine_client::presentation::record_channel_stop( channel, duration );
}

auto sfx::is_channel_playing( const channel channel ) -> bool
{
    // The channels play what the published facts told them to.
    return !test_mode && engine_client::presentation::channel_playing( channel );
}

auto sfx::stop_sound_effect_fade( const channel channel, const int duration ) -> void
{
    engine_client::presentation::record_channel_stop( channel, duration );
}

auto sfx::stop_sound_effect_timed( const channel channel, const int time ) -> void
{
    if( channel != channel::any ) { engine_client::presentation::record_channel_stop( channel, time ); }
}

auto sfx::set_channel_volume( const channel channel, const int volume ) -> int
{
    engine_client::presentation::record_channel_change( { .channel = channel, .volume = volume } );
    return test_mode ? 0 : -1;
}

auto sfx::set_channel_3d_position( const channel channel, const units::angle angle,
                                   int /*distance*/ ) -> void
{
    engine_client::presentation::record_channel_change( { .channel = channel, .angle = angle } );
}

auto sfx::get_channel_volume( const channel /*channel*/ ) -> int { return 0; }

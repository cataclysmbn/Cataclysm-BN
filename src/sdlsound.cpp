#if defined(SDL_SOUND)

#include "sdlsound.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL3_mixer/SDL_mixer.h>

#include "cached_options.h"
#include "debug.h"
#include "init.h"
#include "json.h"
#include "messages.h"
#include "options.h"
#include "path_info.h"
#include "rng.h"
#include "sdl_wrappers.h"
#include "sounds.h"
#include "units_angle.h"

#define dbg(x) DebugLogFL((x),DC::SDL)

// ── Types ──────────────────────────────────────────────────────────────────────

using id_and_variant = std::pair<std::string, std::string>;

struct sound_effect_resource {
    std::string path;
    struct deleter {
        auto operator()( MIX_Audio *a ) const -> void {
            MIX_DestroyAudio( a );
        }
    };
    std::unique_ptr<MIX_Audio, deleter> audio;
};

struct sound_effect {
    int volume;
    int resource_id;
};

struct sfx_resources_t {
    std::vector<sound_effect_resource> resource;
    std::map<id_and_variant, std::vector<sound_effect>> sound_effects;
};

struct music_playlist {
    struct entry {
        std::string file;
        int volume;
    };
    std::vector<entry> entries;
    bool shuffle;
    music_playlist() : shuffle( false ) {}
};

// ── Volume helpers ─────────────────────────────────────────────────────────────
// Game audio volumes use integers in [0, 128]; SDL3_mixer uses float gain: 0.0–1.0.

static constexpr auto volume_to_gain( const int volume ) -> float
{
    return static_cast<float>( volume ) / 128.0f;
}

// MIX_StopTrack takes a fade duration in sample frames (44100 Hz).
// MIX_StopTag/MIX_StopAllTracks take milliseconds.
static constexpr auto ms_to_frames( const int ms ) -> Sint64
{
    return static_cast<Sint64>( ms ) * 44100 / 1000;
}

// ── String tags ───────────────────────────────────────────────────────────────
// SDL3_mixer uses string-based tags for grouping tracks.

static constexpr const char *TAG_ALL_SFX    = "cbn.all_sfx";
static constexpr const char *TAG_TIME_OF_DAY = "cbn.time_of_day";
static constexpr const char *TAG_WEATHER    = "cbn.weather";
static constexpr const char *TAG_CONTEXT    = "cbn.context_themes";
static constexpr const char *TAG_FATIGUE    = "cbn.fatigue";

static auto sfx_group_to_tag( const sfx::group group ) -> const char *
{
    switch( group ) {
        case sfx::group::time_of_day:    return TAG_TIME_OF_DAY;
        case sfx::group::weather:        return TAG_WEATHER;
        case sfx::group::context_themes: return TAG_CONTEXT;
        case sfx::group::fatigue:        return TAG_FATIGUE;
        default:                         return nullptr;
    }
}

// ── Global state ──────────────────────────────────────────────────────────────

static MIX_Mixer *g_mixer = nullptr;

// Dedicated persistent track for music playback
static MIX_Track *music_track           = nullptr;
static MIX_Audio *current_music_audio   = nullptr;
static int current_music_track_volume   = 0;

static std::string current_playlist;
static size_t current_playlist_at       = 0;
static size_t absolute_playlist_at      = 0;
static std::vector<std::size_t> playlist_indexes;
static bool sound_init_success          = false;
static std::map<std::string, music_playlist> playlists;
static std::string current_soundpack_path;

// Persistent tracks for reserved/named channels (sfx::channel enum)
static constexpr int MAX_CHANNEL_COUNT = static_cast<int>( sfx::channel::MAX_CHANNEL );
static std::array<MIX_Track *, MAX_CHANNEL_COUNT> channel_tracks{};

// Pre-allocated pool for fire-and-forget one-shot effects (channel::any).
// Tracks are reused once they finish playing.
static constexpr int TEMP_TRACK_POOL_SIZE = 64;
static std::array<MIX_Track *, TEMP_TRACK_POOL_SIZE> temp_tracks{};

struct ambient_parameters {
    std::string id;
    std::string variant;
    int volume;
    sfx::channel channel;
    int fade_in_duration;
    double pitch;
    int loops;
};
static ambient_parameters current_ambient;

static std::unordered_map<std::string, int> unique_paths;
static sfx_resources_t sfx_resources;
static std::vector<id_and_variant> sfx_preload;

bool sounds::sound_enabled = false;

// ── Internal helpers ───────────────────────────────────────────────────────────

static inline auto check_sound( const int volume = 1 ) -> bool
{
    return sound_init_success && sounds::sound_enabled && volume > 0;
}

// Returns a free (idle) track from the temp pool, or nullptr if all are busy.
static auto get_free_temp_track() -> MIX_Track *
{
    for( auto *t : temp_tracks ) {
        if( t && !MIX_TrackPlaying( t ) ) {
            return t;
        }
    }
    return nullptr;
}

// ── Initialization / Shutdown ─────────────────────────────────────────────────

auto init_sound() -> bool
{
    if( sound_init_success ) {
        return true;
    }

    if( !MIX_Init() ) {
        dbg( DL::Error ) << "MIX_Init failed: " << SDL_GetError();
        return false;
    }

    const char *driver = SDL_GetCurrentAudioDriver();
    DebugLog( DL::Info, DC::Main ) << "Active audio driver: " << ( driver ? driver : "(none)" );

    SDL_AudioSpec spec = {};
    spec.format   = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq     = 44100;

    g_mixer = MIX_CreateMixerDevice( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec );
    if( !g_mixer ) {
        dbg( DL::Error ) << "MIX_CreateMixerDevice failed: " << SDL_GetError();
        MIX_Quit();
        return false;
    }

    // Persistent tracks for each named/reserved channel
    for( int i = 0; i < MAX_CHANNEL_COUNT; ++i ) {
        channel_tracks[i] = MIX_CreateTrack( g_mixer );
    }

    // Fire-and-forget pool
    for( int i = 0; i < TEMP_TRACK_POOL_SIZE; ++i ) {
        temp_tracks[i] = MIX_CreateTrack( g_mixer );
    }

    music_track = MIX_CreateTrack( g_mixer );

    // ── String-tag grouping ──────────────────────────────────────────────────

    using ch = sfx::channel;

    // All SFX (everything except music) — used by fade_audio_channel(any)
    for( int i = 0; i < MAX_CHANNEL_COUNT; ++i ) {
        MIX_TagTrack( channel_tracks[i], TAG_ALL_SFX );
    }
    for( auto *t : temp_tracks ) {
        MIX_TagTrack( t, TAG_ALL_SFX );
    }

    // Mirrors original: Mix_GroupChannels(daytime_outdoors_env, nighttime_outdoors_env, time_of_day)
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::daytime_outdoors_env )],   TAG_TIME_OF_DAY );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::nighttime_outdoors_env )], TAG_TIME_OF_DAY );

    // Mirrors original: Mix_GroupChannels(underground_env, outdoor_blizzard, weather)
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::underground_env )],           TAG_WEATHER );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::indoors_env )],               TAG_WEATHER );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::indoors_rain_env )],          TAG_WEATHER );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::outdoors_snow_env )],         TAG_WEATHER );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::outdoors_flurry_env )],       TAG_WEATHER );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::outdoors_thunderstorm_env )], TAG_WEATHER );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::outdoors_rain_env )],         TAG_WEATHER );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::outdoors_drizzle_env )],      TAG_WEATHER );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::outdoor_blizzard )],          TAG_WEATHER );

    // Mirrors original: Mix_GroupChannels(danger_extreme_theme, danger_low_theme, context_themes)
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::danger_extreme_theme )], TAG_CONTEXT );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::danger_high_theme )],    TAG_CONTEXT );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::danger_medium_theme )],  TAG_CONTEXT );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::danger_low_theme )],     TAG_CONTEXT );

    // Mirrors original: Mix_GroupChannels(stamina_75, stamina_35, fatigue)
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::stamina_75 )], TAG_FATIGUE );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::stamina_50 )], TAG_FATIGUE );
    MIX_TagTrack( channel_tracks[static_cast<int>( ch::stamina_35 )], TAG_FATIGUE );

    DebugLog( DL::Info, DC::Main ) << "SDL3_mixer initialized successfully";
    sound_init_success = true;
    return true;
}

auto shutdown_sound() -> void
{
    sfx_resources.resource.clear();
    sfx_resources.sound_effects.clear();
    playlists.clear();

    if( !sound_init_success ) {
        return;
    }
    sound_init_success = false;

    if( g_mixer ) {
        MIX_StopAllTracks( g_mixer, 0 );

        if( current_music_audio ) {
            MIX_DestroyAudio( current_music_audio );
            current_music_audio = nullptr;
        }

        // Destroy tracks before the mixer
        if( music_track ) {
            MIX_DestroyTrack( music_track );
            music_track = nullptr;
        }
        for( auto *t : channel_tracks ) {
            if( t ) {
                MIX_DestroyTrack( t );
            }
        }
        channel_tracks.fill( nullptr );
        for( auto *t : temp_tracks ) {
            if( t ) {
                MIX_DestroyTrack( t );
            }
        }
        temp_tracks.fill( nullptr );

        MIX_DestroyMixer( g_mixer );
        g_mixer = nullptr;
    }

    MIX_Quit();
}

// ── Music ─────────────────────────────────────────────────────────────────────

void musicFinished();

// MIX_TrackStoppedCallback signature: userdata first, then track.
static auto music_stopped_cb( void * /*userdata*/, MIX_Track * /*track*/ ) -> void
{
    if( current_music_audio ) {
        MIX_DestroyAudio( current_music_audio );
        current_music_audio = nullptr;
    }
    musicFinished();
}

static auto play_music_file( const std::string &filename, const int volume ) -> void
{
    if( test_mode ) {
        return;
    }
    current_music_track_volume = 0;

    if( !check_sound( volume ) ) {
        return;
    }

    const std::string path = current_soundpack_path + "/" + filename;
    // Music is streamed (predecode=false) to avoid loading large files into RAM.
    auto *audio = MIX_LoadAudio( g_mixer, path.c_str(), false );
    if( !audio ) {
        dbg( DL::Error ) << "Failed to load music file " << path << ": " << SDL_GetError();
        return;
    }

    MIX_SetTrackGain( music_track,
                      volume_to_gain( volume * get_option<int>( "MUSIC_VOLUME" ) / 100 ) );
    MIX_SetTrackStoppedCallback( music_track, music_stopped_cb, nullptr );
    MIX_SetTrackAudio( music_track, audio );
    MIX_SetTrackLoops( music_track, 0 );

    if( !MIX_PlayTrack( music_track, 0 ) ) {
        dbg( DL::Error ) << "Starting playlist " << path << " failed: " << SDL_GetError();
        MIX_DestroyAudio( audio );
        return;
    }

    current_music_audio        = audio;
    current_music_track_volume = volume;
}

void musicFinished()
{
    if( test_mode ) {
        return;
    }

    const auto iter = playlists.find( current_playlist );
    if( iter == playlists.end() ) {
        return;
    }
    const music_playlist &list = iter->second;
    if( list.entries.empty() ) {
        return;
    }

    absolute_playlist_at++;
    if( absolute_playlist_at >= list.entries.size() ) {
        absolute_playlist_at = 0;
    }

    current_playlist_at = playlist_indexes.at( absolute_playlist_at );
    const music_playlist::entry &next = list.entries[current_playlist_at];
    play_music_file( next.file, next.volume );
}

auto play_music( const std::string &playlist ) -> void
{
    const auto iter = playlists.find( playlist );
    if( iter == playlists.end() ) {
        return;
    }
    const music_playlist &list = iter->second;
    if( list.entries.empty() ) {
        return;
    }
    if( playlist == current_playlist ) {
        return;
    }

    for( size_t i = 0; i < list.entries.size(); i++ ) {
        playlist_indexes.push_back( i );
    }
    if( list.shuffle ) {
        // NOLINTNEXTLINE(cata-determinism)
        static auto eng = cata_default_random_engine(
                              std::chrono::system_clock::now().time_since_epoch().count() );
        std::shuffle( playlist_indexes.begin(), playlist_indexes.end(), eng );
    }

    current_playlist    = playlist;
    current_playlist_at = playlist_indexes.at( absolute_playlist_at );
    const music_playlist::entry &next = list.entries[current_playlist_at];
    play_music_file( next.file, next.volume );
}

auto stop_music() -> void
{
    if( test_mode ) {
        return;
    }
    if( music_track ) {
        // Clear callback before stopping so musicFinished() does not queue the next song
        MIX_SetTrackStoppedCallback( music_track, nullptr, nullptr );
        MIX_StopTrack( music_track, 0 );
    }
    if( current_music_audio ) {
        MIX_DestroyAudio( current_music_audio );
        current_music_audio = nullptr;
    }
    current_playlist.clear();
    current_playlist_at  = 0;
    absolute_playlist_at = 0;
}

auto update_volumes() -> void
{
    if( test_mode ) {
        return;
    }
    if( !sounds::sound_enabled ) {
        stop_music();
        return;
    }

    if( music_track && MIX_TrackPlaying( music_track ) ) {
        MIX_SetTrackGain( music_track,
                          volume_to_gain(
                              current_music_track_volume * get_option<int>( "MUSIC_VOLUME" ) / 100 ) );
    }

    play_music( "title" );

    if( !current_ambient.id.empty() ) {
        for( int i = 0; i < MAX_CHANNEL_COUNT; ++i ) {
            if( channel_tracks[i] && MIX_TrackPlaying( channel_tracks[i] ) ) {
                MIX_StopTrack( channel_tracks[i], 0 );
            }
        }
        sfx::play_ambient_variant_sound( current_ambient.id, current_ambient.variant,
                                         current_ambient.volume, current_ambient.channel,
                                         current_ambient.fade_in_duration,
                                         current_ambient.pitch, current_ambient.loops );
    }
}

// ── SFX resource management ───────────────────────────────────────────────────

static auto get_sfx_resource( const int resource_id ) -> MIX_Audio *
{
    sound_effect_resource &resource = sfx_resources.resource[resource_id];
    if( !resource.audio ) {
        const std::string path = current_soundpack_path + "/" + resource.path;
        // SFX are predecoded for low-latency playback.
        resource.audio.reset( MIX_LoadAudio( g_mixer, path.c_str(), true ) );
        if( !resource.audio ) {
            debugmsg( "Failed to load sfx audio file %s: %s", path.c_str(), SDL_GetError() );
        }
    }
    return resource.audio.get();
}

static auto add_sfx_path( const std::string &path ) -> int
{
    const auto find_result = unique_paths.find( path );
    if( find_result != unique_paths.end() ) {
        return find_result->second;
    }
    const int result = sfx_resources.resource.size();
    sound_effect_resource new_resource;
    new_resource.path = path;
    sfx_resources.resource.push_back( std::move( new_resource ) );
    unique_paths[path] = result;
    return result;
}

// ── sfx:: JSON loading ────────────────────────────────────────────────────────

auto sfx::load_sound_effects( const JsonObject &jsobj ) -> void
{
    if( !sound_init_success ) {
        return;
    }
    const id_and_variant key( jsobj.get_string( "id" ), jsobj.get_string( "variant", "default" ) );
    const int volume = jsobj.get_int( "volume", 100 );
    auto &effects = sfx_resources.sound_effects[key];

    for( const std::string file : jsobj.get_array( "files" ) ) {
        sound_effect new_sound_effect;
        new_sound_effect.volume      = volume;
        new_sound_effect.resource_id = add_sfx_path( file );
        effects.push_back( new_sound_effect );
    }
}

auto sfx::load_sound_effect_preload( const JsonObject &jsobj ) -> void
{
    if( !sound_init_success ) {
        return;
    }
    for( JsonObject aobj : jsobj.get_array( "preload" ) ) {
        const id_and_variant preload_key( aobj.get_string( "id" ),
                                          aobj.get_string( "variant", "default" ) );
        sfx_preload.push_back( preload_key );
    }
}

auto sfx::load_playlist( const JsonObject &jsobj ) -> void
{
    if( !sound_init_success ) {
        return;
    }
    for( JsonObject playlist : jsobj.get_array( "playlists" ) ) {
        const std::string playlist_id = playlist.get_string( "id" );
        music_playlist playlist_to_load;
        playlist_to_load.shuffle = playlist.get_bool( "shuffle", false );
        for( JsonObject entry : playlist.get_array( "files" ) ) {
            const music_playlist::entry e{ entry.get_string( "file" ), entry.get_int( "volume" ) };
            playlist_to_load.entries.push_back( e );
        }
        playlists[playlist_id] = std::move( playlist_to_load );
    }
}

// ── Effect lookup ─────────────────────────────────────────────────────────────

static auto find_random_effect( const id_and_variant &id_variants_pair ) -> const sound_effect *
{
    const auto iter = sfx_resources.sound_effects.find( id_variants_pair );
    if( iter == sfx_resources.sound_effects.end() ) {
        return nullptr;
    }
    return &random_entry_ref( iter->second );
}

static auto find_random_effect( const std::string &id,
                                 const std::string &variant ) -> const sound_effect *
{
    const sound_effect *eff = find_random_effect( id_and_variant( id, variant ) );
    if( eff != nullptr ) {
        return eff;
    }
    return find_random_effect( id_and_variant( id, "default" ) );
}

auto sfx::has_variant_sound( const std::string &id, const std::string &variant ) -> bool
{
    return find_random_effect( id, variant ) != nullptr;
}

// ── Playback ──────────────────────────────────────────────────────────────────

// NOTE: Pitch shift is not supported. MIX_Audio is opaque — raw PCM buffer access
// is unavailable. Sounds play at their natural pitch. Future work could use
// SDL_AudioStream sample-rate conversion if pitch variation is required.

auto sfx::play_variant_sound( const std::string &id, const std::string &variant,
                               const int volume ) -> void
{
    if( test_mode ) {
        return;
    }
    add_msg( m_debug, "sound id: %s, variant: %s, volume: %d ", id, variant, volume );
    if( !check_sound( volume ) ) {
        return;
    }

    const sound_effect *eff = find_random_effect( id, variant );
    if( eff == nullptr ) {
        eff = find_random_effect( id, "default" );
        if( eff == nullptr ) {
            return;
        }
    }

    auto *audio = get_sfx_resource( eff->resource_id );
    if( !audio ) {
        return;
    }

    auto *track = get_free_temp_track();
    if( !track ) {
        dbg( DL::Warn ) << "No free track for sound effect: " << id;
        return;
    }

    const int vol = eff->volume * get_option<int>( "SOUND_EFFECT_VOLUME" ) * volume / ( 100 * 100 );
    MIX_SetTrackGain( track, volume_to_gain( vol ) );
    MIX_SetTrackAudio( track, audio );
    MIX_SetTrackLoops( track, 0 );
    if( !MIX_PlayTrack( track, 0 ) ) {
        dbg( DL::Warn ) << "Failed to play sound effect: " << SDL_GetError();
    }
}

auto sfx::play_variant_sound( const std::string &id, const std::string &variant, const int volume,
                               const units::angle angle,
                               double /*pitch_min*/, double /*pitch_max*/ ) -> void
{
    if( test_mode ) {
        return;
    }
    add_msg( m_debug, "sound id: %s, variant: %s, volume: %d ", id, variant, volume );
    if( !check_sound( volume ) ) {
        return;
    }

    const sound_effect *eff = find_random_effect( id, variant );
    if( eff == nullptr ) {
        return;
    }

    auto *audio = get_sfx_resource( eff->resource_id );
    if( !audio ) {
        return;
    }

    auto *track = get_free_temp_track();
    if( !track ) {
        dbg( DL::Warn ) << "No free track for sound effect: " << id;
        return;
    }

    const int vol = eff->volume * get_option<int>( "SOUND_EFFECT_VOLUME" ) * volume / ( 100 * 100 );
    MIX_SetTrackGain( track, volume_to_gain( vol ) );

    const float angle_rad = static_cast<float>( to_degrees( angle ) ) * SDL_PI_F / 180.0f;
    const MIX_Point3D pos = { SDL_sinf( angle_rad ), 0.0f, SDL_cosf( angle_rad ) };
    MIX_SetTrack3DPosition( track, &pos );

    MIX_SetTrackAudio( track, audio );
    MIX_SetTrackLoops( track, 0 );
    if( !MIX_PlayTrack( track, 0 ) ) {
        dbg( DL::Debug ) << "Failed to play sound effect { id: " << id
                         << ", variant: " << variant << " }: " << SDL_GetError();
    }
}

auto sfx::play_ambient_variant_sound( const std::string &id, const std::string &variant,
                                      const int volume, const channel channel,
                                      const int fade_in_duration,
                                      double /*pitch*/, const int loops ) -> void
{
    if( test_mode ) {
        return;
    }
    if( !check_sound( volume ) ) {
        return;
    }

    const int ch = static_cast<int>( channel );
    if( ch < 0 || ch >= MAX_CHANNEL_COUNT ) {
        return;
    }
    if( is_channel_playing( channel ) ) {
        return;
    }

    const sound_effect *eff = find_random_effect( id, variant );
    if( eff == nullptr ) {
        return;
    }

    auto *audio = get_sfx_resource( eff->resource_id );
    if( !audio ) {
        return;
    }

    auto *track = channel_tracks[ch];
    const int vol = eff->volume * get_option<int>( "AMBIENT_SOUND_VOLUME" ) * volume / ( 100 * 100 );
    MIX_SetTrackGain( track, volume_to_gain( vol ) );
    MIX_SetTrackAudio( track, audio );
    MIX_SetTrackLoops( track, loops );

    // Apply fade-in via play properties if requested
    SDL_PropertiesID props = 0;
    if( fade_in_duration > 0 ) {
        props = SDL_CreateProperties();
        SDL_SetNumberProperty( props, MIX_PROP_PLAY_FADE_IN_FRAMES_NUMBER,
                               ms_to_frames( fade_in_duration ) );
    }

    if( !MIX_PlayTrack( track, props ) ) {
        dbg( DL::Debug ) << "Failed to play ambient sound { id: " << id
                         << ", variant: " << variant << " }: " << SDL_GetError();
        if( props ) {
            SDL_DestroyProperties( props );
        }
        return;
    }

    if( props ) {
        SDL_DestroyProperties( props );
    }

    current_ambient.id               = id;
    current_ambient.variant          = variant;
    current_ambient.volume           = volume;
    current_ambient.channel          = channel;
    current_ambient.fade_in_duration = fade_in_duration;
    current_ambient.pitch            = -1.0; // pitch shift not supported with SDL3_mixer 3.x
    current_ambient.loops            = loops;
}

// ── sfx:: channel management (moved from sounds.cpp to access channel_tracks) ──

auto sfx::fade_audio_group( const group group, const int duration ) -> void
{
    if( test_mode ) {
        return;
    }
    const char *tag = sfx_group_to_tag( group );
    if( tag ) {
        MIX_StopTag( g_mixer, tag, static_cast<Sint64>( duration ) );
    }
}

auto sfx::fade_audio_channel( const channel channel, const int duration ) -> void
{
    if( test_mode ) {
        return;
    }
    if( channel == channel::any ) {
        MIX_StopTag( g_mixer, TAG_ALL_SFX, static_cast<Sint64>( duration ) );
        return;
    }
    const int idx = static_cast<int>( channel );
    if( channel_tracks[idx] ) {
        MIX_StopTrack( channel_tracks[idx], ms_to_frames( duration ) );
    }
}

auto sfx::is_channel_playing( const channel channel ) -> bool
{
    if( test_mode ) {
        return false;
    }
    if( channel == channel::any ) {
        return false;
    }
    const int idx = static_cast<int>( channel );
    return channel_tracks[idx] && MIX_TrackPlaying( channel_tracks[idx] );
}

auto sfx::stop_sound_effect_fade( const channel channel, const int duration ) -> void
{
    if( test_mode ) {
        return;
    }
    if( channel == channel::any ) {
        MIX_StopTag( g_mixer, TAG_ALL_SFX, static_cast<Sint64>( duration ) );
        return;
    }
    const int idx = static_cast<int>( channel );
    if( channel_tracks[idx] ) {
        MIX_StopTrack( channel_tracks[idx], ms_to_frames( duration ) );
    }
}

auto sfx::stop_sound_effect_timed( const channel channel, const int time ) -> void
{
    if( test_mode ) {
        return;
    }
    if( channel == channel::any ) {
        return;
    }
    const int idx = static_cast<int>( channel );
    if( channel_tracks[idx] ) {
        // SDL3_mixer has no expire-after-N-ms equivalent; treat time as fade-out duration
        MIX_StopTrack( channel_tracks[idx], ms_to_frames( time ) );
    }
}

auto sfx::set_channel_volume( const channel channel, const int volume ) -> int
{
    if( test_mode ) {
        return 0;
    }
    if( channel == channel::any ) {
        return -1;
    }
    const int idx   = static_cast<int>( channel );
    auto *track     = channel_tracks[idx];
    if( !track || !MIX_TrackPlaying( track ) ) {
        return -1;
    }
    if( MIX_GetTrackFadeFrames( track ) != 0 ) {
        return -1;
    }
    const int old_vol = static_cast<int>( MIX_GetTrackGain( track ) * 128.0f );
    MIX_SetTrackGain( track, volume_to_gain( volume ) );
    return old_vol;
}

auto sfx::set_channel_3d_position( const channel channel, const units::angle angle,
                                   int /*distance*/ ) -> void
{
    if( !sound_init_success || channel == channel::any ) {
        return;
    }
    const int idx = static_cast<int>( channel );
    auto *track   = channel_tracks[idx];
    if( !track ) {
        return;
    }
    const float angle_rad = static_cast<float>( to_degrees( angle ) ) * SDL_PI_F / 180.0f;
    const MIX_Point3D pos = { SDL_sinf( angle_rad ), 0.0f, SDL_cosf( angle_rad ) };
    MIX_SetTrack3DPosition( track, &pos );
}

auto sfx::get_channel_volume( const channel channel ) -> int
{
    if( !sound_init_success || channel == channel::any ) {
        return 0;
    }
    const int idx = static_cast<int>( channel );
    auto *track   = channel_tracks[idx];
    if( !track ) {
        return 0;
    }
    return static_cast<int>( MIX_GetTrackGain( track ) * 128.0f );
}

// ── Soundset loading ──────────────────────────────────────────────────────────

auto load_soundset() -> void
{
    const std::string default_path     = PATH_INFO::defaultsounddir();
    const std::string default_soundpack = "basic";
    std::string current_soundpack       = get_option<std::string>( "SOUNDPACKS" );
    std::string soundpack_path;

    if( current_soundpack.empty() ) {
        dbg( DL::Error ) << "Soundpack not set in options or empty.";
        soundpack_path    = default_path;
        current_soundpack = default_soundpack;
    } else {
        dbg( DL::Info ) << "Current soundpack is: " << current_soundpack;
        soundpack_path = SOUNDPACKS[current_soundpack];
    }

    if( soundpack_path.empty() ) {
        dbg( DL::Error ) << "Soundpack with name " << current_soundpack
                         << " can't be found or empty string";
        soundpack_path    = default_path;
        current_soundpack = default_soundpack;
    } else {
        dbg( DL::Info ) << '"' << current_soundpack << '"'
                        << " soundpack: found path: " << soundpack_path;
    }

    current_soundpack_path = soundpack_path;
    try {
        init::load_soundpack_files( soundpack_path );
    } catch( const std::exception &err ) {
        dbg( DL::Error ) << "failed to load sounds: " << err.what();
    }

    for( const id_and_variant &preload : sfx_preload ) {
        const auto &[id, variant] = preload;

        if( variant == "all" ) {
            for( const auto &[key, sfxs] : sfx_resources.sound_effects ) {
                if( key.first == id ) {
                    for( const auto &sfx : sfxs ) {
                        get_sfx_resource( sfx.resource_id );
                    }
                }
            }
        } else {
            const auto find_result = sfx_resources.sound_effects.find( preload );
            if( find_result != sfx_resources.sound_effects.end() ) {
                for( const auto &sfx : find_result->second ) {
                    get_sfx_resource( sfx.resource_id );
                }
            }
        }
    }

    {
        unique_paths.clear();
        std::unordered_map<std::string, int> t_swap;
        unique_paths.swap( t_swap );
    }
    {
        sfx_preload.clear();
        std::vector<id_and_variant> t_swap;
        sfx_preload.swap( t_swap );
    }
}

#endif

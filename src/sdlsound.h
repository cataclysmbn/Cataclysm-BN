#pragma once

#include <string>
#if defined(CATA_MCP)
#include "sounds.h"

namespace sfx
{
struct paced_sound_options {
    const std::string &id;
    const std::string &variant;
    int volume = 0;
    units::angle angle = units::from_degrees( 0 );
    bool stacks = false;
    /// How long after the last paced sound this one may play.
    int wait_ms = 0;
};
/// A footstep, obstacle or gunshot sound: published every time, whatever its wait.
auto play_paced_sound( const paced_sound_options &options ) -> void;
} // namespace sfx

auto sound_supported() -> bool;
auto soundpack_path() -> std::string;
#endif

#if defined(SDL_SOUND) || defined(CATA_MCP)

/**
 * Attempt to initialize an audio device.  Returns false if initialization fails.
 */
bool init_sound();
void shutdown_sound();
void play_music( const std::string &playlist );
void stop_music();
void update_volumes();
void load_soundset();

#else

inline bool init_sound()
{
    return false;
}
inline void shutdown_sound() { }
inline void play_music( const std::string &/*playlist*/ )
{
}
inline void update_volumes() { }
inline void load_soundset() { }

#endif



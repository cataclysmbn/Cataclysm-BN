#pragma once

#include <cstddef>
#include <string>

namespace sfx
{
/// Select an audio presentation variant without advancing the simulation RNG.
auto presentation_random_effect_index( std::size_t count ) -> std::size_t;
} // namespace sfx

/**
 * Attempt to initialize an audio device.  Returns false if initialization fails.
 */
auto sound_supported() -> bool;
bool init_sound();
void shutdown_sound();
void play_music( const std::string &playlist );
void stop_music();
void update_volumes();
void load_soundset();



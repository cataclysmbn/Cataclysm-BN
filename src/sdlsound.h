#pragma once

#include <cstddef>
#include <string>

#include "units_angle.h"

namespace sfx
{
/// Select an audio presentation variant without advancing the simulation RNG.
auto presentation_random_effect_index( std::size_t count ) -> std::size_t;

struct paced_sound_options {
    const std::string &id;
    const std::string &variant;
    int volume = 0;
    units::angle angle = units::from_degrees( 0 );
    bool stacks = false;
    /// How long after the last paced sound this one may play.
    int wait_ms = 0;
};
/// A footstep, obstacle or gunshot sound: published every time, played only once its wait has passed since the
/// last paced sound played, and then starting the wait again.
auto play_paced_sound( const paced_sound_options &options ) -> void;
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
/// The folder `load_soundset` loaded the sound pack from; empty before.
auto soundpack_path() -> std::string;



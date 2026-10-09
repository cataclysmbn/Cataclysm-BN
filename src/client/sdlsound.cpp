#include "sdlsound.h"

#include <chrono>
#include <random>

namespace {

// Audio variation is presentation-only and must never advance the simulation RNG.
auto audio_random_engine() -> std::minstd_rand0& // *NOPAD*
{
    // NOLINTNEXTLINE(cata-determinism)
    static auto engine = std::minstd_rand0(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return engine;
}

} // namespace

namespace sfx {

auto presentation_random_effect_index(const std::size_t count) -> std::size_t {
    auto distribution = std::uniform_int_distribution<std::size_t>(0, count - 1);
    return distribution(audio_random_engine());
}

} // namespace sfx

#if !defined(SDL_SOUND)

auto sound_supported() -> bool { return false; }

auto init_sound() -> bool { return false; }

auto shutdown_sound() -> void {}

auto play_music(const std::string& /*playlist*/) -> void {}

auto stop_music() -> void {}

auto update_volumes() -> void {}

auto load_soundset() -> void {}

#endif // !SDL_SOUND

#include "platform/sdl_video.h"

#if defined(CATA_SDL)

#include <SDL3/SDL.h>
#include <cstdlib>

auto use_offscreen_video_driver_for_headless_sdl() -> offscreen_sdl_hint_result {
#if defined(__linux__) && !defined(__ANDROID__)
    if (std::getenv("SDL_VIDEO_DRIVER") != nullptr || std::getenv("SDL_VIDEODRIVER") != nullptr
        || std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr) {
        return offscreen_sdl_hint_result::skipped;
    }

    return SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen")
             ? offscreen_sdl_hint_result::applied
             : offscreen_sdl_hint_result::failed;
#else
    return offscreen_sdl_hint_result::skipped;
#endif
}

#endif

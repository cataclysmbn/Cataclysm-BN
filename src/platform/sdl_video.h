#pragma once

#if defined(CATA_SDL)

enum class offscreen_sdl_hint_result {
    skipped,
    applied,
    failed,
};

auto use_offscreen_video_driver_for_headless_sdl() -> offscreen_sdl_hint_result;

#endif

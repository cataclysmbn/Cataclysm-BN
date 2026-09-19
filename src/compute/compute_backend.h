#pragma once

#include "preload_config.h"

#include <string_view>

#if defined(CATA_SDL)
#    include "compute/gpu_platform.h"
#endif

namespace cata_compute {

enum class backend : int {
    sdl_gpu_hardware,
    sdl_gpu_software,
    cpu_compute,
};

inline auto selected_backend() -> backend {
    using preload_config::compute_accel;

    switch (preload_config::get_compute_accel()) {
        case compute_accel::cpu:
            return backend::cpu_compute;
        case compute_accel::gpu_software:
            return backend::sdl_gpu_software;
        case compute_accel::gpu:
            return backend::sdl_gpu_hardware;
        case compute_accel::auto_select:
        default:
#if defined(CATA_SDL)
            if (cata_gpu::get_device() != nullptr) { return backend::sdl_gpu_hardware; }
#endif
            return backend::cpu_compute;
    }
}

inline auto selected_backend_name() -> std::string_view {
    switch (selected_backend()) {
        case backend::sdl_gpu_hardware:
            return "sdl_gpu_hardware";
        case backend::sdl_gpu_software:
            return "sdl_gpu_software";
        case backend::cpu_compute:
        default:
            return "cpu_compute";
    }
}

inline auto uses_cpu_compute() -> bool { return selected_backend() == backend::cpu_compute; }

inline auto uses_sdl_gpu_compute() -> bool { return !uses_cpu_compute(); }

} // namespace cata_compute

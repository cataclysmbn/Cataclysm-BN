#include "slang_cpu_kernels.h"

#include <array>
#include <cstdint>

#if defined(CATA_SLANG_CPU_GENERATED)
#define cpu_main cata_slang_lm_clear_seen_view_cpu_main
#define _cpu_main cata_slang_lm_clear_seen_view_cpu_main_entry
#define cpu_main_Group cata_slang_lm_clear_seen_view_cpu_main_group
#define cpu_main_Thread cata_slang_lm_clear_seen_view_cpu_main_thread
#include "lm_clear_seen_view_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

#include "slang_cpu_dispatch.h"

namespace cata_compute::slang_cpu::kernels {

auto clear_seen_view(clear_seen_view_params const& params) -> bool {
#if defined(CATA_SLANG_CPU_GENERATED)
    if (params.seen_raw == nullptr || params.seen == nullptr || params.cache_x <= 0
        || params.cache_y <= 0 || params.cache_xy <= 0 || params.z_count <= 0
        || params.dispatch_z_count <= 0) {
        return false;
    }

    auto constants = ClearSeenViewConstants_0{};
    constants.player_x_0 = params.player_x;
    constants.player_y_0 = params.player_y;
    constants.cache_x_0 = params.cache_x;
    constants.cache_y_0 = params.cache_y;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_count_0 = params.z_count;
    constants.view_radius_0 = params.view_radius;
    constants.z_start_idx_0 = params.z_start_idx;
    constants.dispatch_z_count_0 = params.dispatch_z_count;

    const auto output_count = static_cast<uint32_t>(params.cache_xy * params.z_count);
    auto globals = GlobalParams_0{};
    globals.seen_raw_all_0 = writable_buffer(params.seen_raw, output_count);
    globals.seen_all_0 = writable_buffer(params.seen, output_count);
    globals.constants_0 = &constants;

    const auto group_count = static_cast<uint32_t>(params.view_radius * 2 + 1 + 7) / 8U;
    dispatch_independent_kernel(
        {
            .group_x = group_count,
            .group_y = group_count,
            .group_z = static_cast<uint32_t>(params.dispatch_z_count),
        },
        globals, cata_slang_lm_clear_seen_view_cpu_main);
    return true;
#else
    (void)params;
    return false;
#endif
}

auto run_clear_seen_view_probe() -> bool {
#if defined(CATA_SLANG_CPU_GENERATED)
    auto seen_raw = std::array<float, 9>{};
    seen_raw.fill(1.0f);
    auto seen = seen_raw;
    if (!clear_seen_view({
            .seen_raw = seen_raw.data(),
            .seen = seen.data(),
            .player_x = 1,
            .player_y = 1,
            .cache_x = 3,
            .cache_y = 3,
            .cache_xy = 9,
            .z_count = 1,
            .view_radius = 1,
            .z_start_idx = 0,
            .dispatch_z_count = 1,
        })) {
        return false;
    }

    return seen_raw == std::array<float, 9>{} && seen == std::array<float, 9>{};
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

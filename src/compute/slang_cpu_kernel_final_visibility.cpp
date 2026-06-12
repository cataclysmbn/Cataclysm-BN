#include "slang_cpu_kernels.h"

#include <array>
#include <bit>
#include <cstdint>

#if defined(CATA_SLANG_CPU_GENERATED)
#define cpu_main cata_slang_lm_visibility_cpu_main
#define _cpu_main cata_slang_lm_visibility_cpu_main_entry
#define cpu_main_Group cata_slang_lm_visibility_cpu_main_group
#define cpu_main_Thread cata_slang_lm_visibility_cpu_main_thread
#include "lm_visibility_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

#include "slang_cpu_dispatch.h"

namespace cata_compute::slang_cpu::kernels {

namespace {

auto float_bits(const float value) -> uint32_t { return std::bit_cast<uint32_t>(value); }

} // namespace

auto final_visibility(final_visibility_params const& params) -> bool {
#if defined(CATA_SLANG_CPU_GENERATED)
    if (params.transparency == nullptr || params.lightmap == nullptr || params.seen == nullptr
        || params.camera == nullptr || params.source_map == nullptr || params.visibility == nullptr
        || params.cache_x <= 0 || params.cache_y <= 0 || params.cache_xy <= 0 || params.z_count <= 0
        || params.dispatch_z_count <= 0) {
        return false;
    }

    auto constants = VisibilityConstants_0{};
    constants.player_x_0 = params.player_x;
    constants.player_y_0 = params.player_y;
    constants.player_z_idx_0 = params.player_z_idx;
    constants.cache_x_0 = params.cache_x;
    constants.cache_y_0 = params.cache_y;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_count_0 = params.z_count;
    constants.trigdist_0 = params.trigdist;
    constants.u_clairvoyance_0 = params.u_clairvoyance;
    constants.u_unimpaired_range_0 = params.u_unimpaired_range;
    constants.g_light_level_0 = params.g_light_level;
    constants.vision_threshold_0 = params.vision_threshold;
    constants.visibility_scale_factor_0 = params.visibility_scale_factor;
    constants.visible_threshold_0 = params.visible_threshold;
    constants.detail_range_0 = params.detail_range;
    constants.z_start_idx_0 = params.z_start_idx;
    constants.dispatch_z_count_0 = params.dispatch_z_count;

    const auto total_tiles = static_cast<uint32_t>(params.cache_xy * params.z_count);
    auto globals = GlobalParams_0{};
    globals.transparency_all_0 = readonly_buffer(params.transparency, total_tiles);
    globals.lm_all_0 = readonly_buffer(params.lightmap, total_tiles);
    globals.seen_all_0 = readonly_buffer(params.seen, total_tiles);
    globals.camera_all_0 = readonly_buffer(params.camera, total_tiles);
    globals.source_map_all_0 = readonly_buffer(params.source_map, total_tiles);
    globals.visibility_all_0 = writable_buffer(params.visibility, total_tiles);
    globals.constants_0 = &constants;

    const auto dispatch_tiles = static_cast<uint32_t>(params.dispatch_z_count * params.cache_xy);
    dispatch_independent_kernel(
        {
            .group_x = tile_groups(dispatch_tiles),
        },
        globals, cata_slang_lm_visibility_cpu_main);
    return true;
#else
    (void)params;
    return false;
#endif
}

auto run_final_visibility_probe() -> bool {
#if defined(CATA_SLANG_CPU_GENERATED)
    auto transparency = std::array<float, 9>{};
    transparency.fill(0.038376418216f);
    auto lightmap = std::array<uint32_t, 9>{};
    lightmap.fill(float_bits(5.0f));
    auto seen = std::array<float, 9>{};
    seen.fill(1.0f);
    auto camera = std::array<uint32_t, 9>{};
    camera.fill(0U);
    auto source_map = std::array<float, 9>{};
    source_map.fill(0.0f);
    auto visibility = std::array<uint32_t, 9>{};
    visibility.fill(6U);

    if (!final_visibility({
            .transparency = transparency.data(),
            .lightmap = lightmap.data(),
            .seen = seen.data(),
            .camera = camera.data(),
            .source_map = source_map.data(),
            .visibility = visibility.data(),
            .player_x = 1,
            .player_y = 1,
            .player_z_idx = 0,
            .cache_x = 3,
            .cache_y = 3,
            .cache_xy = 9,
            .z_count = 1,
            .trigdist = 0U,
            .u_clairvoyance = -1,
            .u_unimpaired_range = 10,
            .g_light_level = 100,
            .vision_threshold = 3.0f,
            .visibility_scale_factor = 1.0f,
            .visible_threshold = 0.000398107193f,
            .detail_range = 10.0f,
            .z_start_idx = 0,
            .dispatch_z_count = 1,
        })) {
        return false;
    }

    return visibility[4] == 1U;
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

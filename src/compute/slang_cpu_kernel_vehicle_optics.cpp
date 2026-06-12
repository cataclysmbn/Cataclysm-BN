#include "slang_cpu_kernels.h"

#if defined(CATA_SDL)

#include <array>
#include <bit>
#include <cstdint>
#include <vector>

#if defined(CATA_SLANG_CPU_GENERATED)
#define cpu_main cata_slang_lm_vehicle_optics_cpu_main
#define _cpu_main cata_slang_lm_vehicle_optics_cpu_main_entry
#define cpu_main_Group cata_slang_lm_vehicle_optics_cpu_main_group
#define cpu_main_Thread cata_slang_lm_vehicle_optics_cpu_main_thread
#include "lm_vehicle_optics_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

#include "slang_cpu_dispatch.h"

namespace cata_compute::slang_cpu::kernels {

namespace {

auto bits_float(const uint32_t value) -> float { return std::bit_cast<float>(value); }

} // namespace

auto vehicle_optics(vehicle_optics_params const& params) -> bool {
#if defined(CATA_SLANG_CPU_GENERATED)
    if (params.transparency == nullptr || params.seen == nullptr || params.optics.empty()
        || params.camera == nullptr || params.cache_x <= 0 || params.cache_y <= 0
        || params.cache_xy <= 0 || params.z_count <= 0 || params.max_radius <= 0) {
        return false;
    }

    auto shader_optics = std::vector<GpuVehicleOptic_0>{};
    shader_optics.reserve(params.optics.size());
    for (const auto& optic : params.optics) {
        shader_optics.push_back(copy_vehicle_optic<GpuVehicleOptic_0>(optic));
    }

    auto constants = VehicleOpticsConstants_0{};
    constants.cache_x_0 = params.cache_x;
    constants.cache_y_0 = params.cache_y;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_count_0 = params.z_count;
    constants.num_optics_0 = static_cast<uint32_t>(params.optics.size());
    constants.max_radius_0 = params.max_radius;
    constants.trigdist_0 = params.trigdist;
    constants.visible_threshold_0 = params.visible_threshold;
    constants.max_view_distance_0 = params.max_view_distance;

    const auto total_tiles = static_cast<uint32_t>(params.cache_xy * params.z_count);
    auto globals = GlobalParams_0{};
    globals.transparency_all_0 = readonly_buffer(params.transparency, total_tiles);
    globals.seen_all_0 = readonly_buffer(params.seen, total_tiles);
    globals.vehicle_optics_0 =
        readonly_buffer(shader_optics.data(), static_cast<uint32_t>(shader_optics.size()));
    globals.camera_all_0 = writable_buffer(params.camera, total_tiles);
    globals.constants_0 = &constants;

    const auto optic_count = static_cast<uint32_t>(params.optics.size());
    return dispatch_accumulating_uint(
        {
            .group_x = optic_count,
            .group_y = ray_group_side(params.max_radius),
            .group_z = ray_group_side(params.max_radius),
            .output_values = total_tiles,
        },
        globals,
        [](auto& chunk_globals, uint32_t* data, const uint32_t count) {
            chunk_globals.camera_all_0 = writable_buffer(data, count);
        },
        [](auto& chunk_globals, cpu_dispatch_range const& range) {
            dispatch_generated_kernel(cata_slang_lm_vehicle_optics_cpu_main, chunk_globals, range);
        },
        [&](uint32_t* scratch, const uint32_t count) {
            return max_uint({
                .target = params.camera,
                .source = scratch,
                .count = count,
            });
        });
#else
    (void)params;
    return false;
#endif
}

auto run_vehicle_optics_probe() -> bool {
#if defined(CATA_SLANG_CPU_GENERATED)
    auto transparency = std::array<float, 9>{};
    transparency.fill(0.038376418216f);
    auto seen = std::array<float, 9>{};
    seen.fill(1.0f);
    auto optics = std::vector<cata_gpu::GpuVehicleOptic>{
        {
            .x = 1,
            .y = 1,
            .z_idx = 0,
            .kind = cata_gpu::vehicle_optic_camera,
            .range = 1,
            .offset_distance = 0,
            ._pad = {},
        },
    };
    auto camera = std::array<uint32_t, 9>{};
    camera.fill(0U);

    if (!vehicle_optics({
            .transparency = transparency.data(),
            .seen = seen.data(),
            .optics = optics,
            .camera = camera.data(),
            .cache_x = 3,
            .cache_y = 3,
            .cache_xy = 9,
            .z_count = 1,
            .trigdist = 0U,
            .visible_threshold = 0.000398107193f,
            .max_view_distance = 60,
            .max_radius = 1,
        })) {
        return false;
    }

    return bits_float(camera[4]) == 1.0f && bits_float(camera[5]) > 0.9f;
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

#endif // defined( CATA_SDL )

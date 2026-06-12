#pragma once

#include <cstdint>
#include <span>
#include <vector>

#if defined(CATA_SDL)
#include "gpu_lm.h"
#include "gpu_transparency.h"
#endif

namespace cata_compute::slang_cpu::kernels {

struct fill_uint_params {
    uint32_t* target = nullptr;
    uint32_t count = 0;
    uint32_t value = 0;
};

struct fill_float_params {
    float* target = nullptr;
    uint32_t total_tiles = 0;
    int32_t cache_xy = 0;
    int32_t z_start_idx = 0;
    float value = 0.0f;
};

struct max_uint_params {
    uint32_t* target = nullptr;
    uint32_t* source = nullptr;
    uint32_t count = 0;
};

struct shift_uint_params {
    uint32_t* target = nullptr;
    uint32_t* source = nullptr;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
    int32_t shift_x_tiles = 0;
    int32_t shift_y_tiles = 0;
    uint32_t fill_value = 0;
};

struct shift_float_params {
    float* target = nullptr;
    float* source = nullptr;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
    int32_t shift_x_tiles = 0;
    int32_t shift_y_tiles = 0;
    float fill_value = 0.0f;
};

struct clear_seen_params {
    float* seen_raw = nullptr;
    float* seen = nullptr;
    uint32_t total_tiles = 0;
    int32_t cache_xy = 0;
    int32_t z_start_idx = 0;
};

struct clear_seen_view_params {
    float* seen_raw = nullptr;
    float* seen = nullptr;
    int32_t player_x = 0;
    int32_t player_y = 0;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
    int32_t view_radius = 0;
    int32_t z_start_idx = 0;
    int32_t dispatch_z_count = 0;
};

struct final_visibility_params {
    float const* transparency = nullptr;
    uint32_t const* lightmap = nullptr;
    float const* seen = nullptr;
    uint32_t const* camera = nullptr;
    float const* source_map = nullptr;
    uint32_t* visibility = nullptr;
    int32_t player_x = 0;
    int32_t player_y = 0;
    int32_t player_z_idx = 0;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
    uint32_t trigdist = 0;
    int32_t u_clairvoyance = 0;
    int32_t u_unimpaired_range = 0;
    int32_t g_light_level = 0;
    float vision_threshold = 0.0f;
    float visibility_scale_factor = 1.0f;
    float visible_threshold = 0.0f;
    float detail_range = 0.0f;
    int32_t z_start_idx = 0;
    int32_t dispatch_z_count = 0;
};

struct seen_params {
    float const* transparency = nullptr;
    uint32_t const* floor = nullptr;
    uint32_t const* vehicle_floor = nullptr;
    uint32_t const* vehicle_obscured = nullptr;
    float* seen = nullptr;
    int32_t player_x = 0;
    int32_t player_y = 0;
    int32_t player_z_idx = 0;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
    int32_t view_radius = 0;
    float z_scale = 1.0f;
    int32_t z_start_idx = 0;
    int32_t dispatch_z_count = 0;
    uint32_t trigdist = 0;
    uint32_t vision_block_mask = 0;
    uint32_t vehicle_obscured_z_mask = 0;
};

struct seen_walls_params {
    float const* transparency = nullptr;
    float const* seen_src = nullptr;
    uint32_t const* vehicle_floor = nullptr;
    uint32_t const* vehicle_obscured = nullptr;
    float* seen_dst = nullptr;
    int32_t player_x = 0;
    int32_t player_y = 0;
    int32_t player_z_idx = 0;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
    int32_t view_radius = 0;
    float z_scale = 1.0f;
    int32_t z_start_idx = 0;
    int32_t dispatch_z_count = 0;
    uint32_t trigdist = 0;
    uint32_t vehicle_obscured_z_mask = 0;
};

struct daylight_diffuse_params {
    uint32_t const* daylight_seed = nullptr;
    uint32_t const* daylight_src = nullptr;
    float const* transparency = nullptr;
    uint32_t* daylight_dst = nullptr;
    uint32_t* lightmap = nullptr;
    uint32_t total_tiles = 0;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
    float diffuse_decay = 1.0f;
    float min_light = 0.0f;
};

struct ambient_params {
    uint32_t const* floor = nullptr;
    float const* transparency = nullptr;
    float const* source_map = nullptr;
    uint32_t const* vehicle_floor = nullptr;
    uint32_t* lightmap = nullptr;
    uint32_t* daylight_seed = nullptr;
    float inside_light = 0.0f;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
    int32_t overmap_depth = 0;
    uint32_t angled_sunlight_shadows = 0;
    uint32_t direct_sunlight = 0;
    float sun_dx_per_z = 0.0f;
    float sun_dy_per_z = 0.0f;
    float solar_shadow_light = 0.0f;
    float natural_light[6][4] = {};
};

#if defined(CATA_SDL)
struct transparency_params {
    cata_gpu::transparency_luts const* luts = nullptr;
    std::span<cata_gpu::transparency_submap_in const> submaps = {};
    cata_gpu::transparency_push_constants const* push = nullptr;
    std::vector<float>* compact_output = nullptr;
    std::vector<float>* full_output = nullptr;
};

struct sight_pairs_params {
    float const* transparency = nullptr;
    uint32_t const* floor = nullptr;
    std::span<cata_gpu::GpuSightPair const> pairs = {};
    std::vector<uint32_t>* results = nullptr;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
};

struct raytrace_params {
    float const* transparency = nullptr;
    uint32_t const* floor = nullptr;
    uint32_t const* vehicle_floor = nullptr;
    std::span<cata_gpu::GpuLightSource const> sources = {};
    uint32_t* lightmap = nullptr;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
    float z_scale = 1.0f;
    uint32_t source_offset = 0;
    uint32_t num_sources = 0;
    int32_t max_radius = 0;
};

struct color_raytrace_params {
    float const* transparency = nullptr;
    uint32_t const* floor = nullptr;
    uint32_t const* vehicle_floor = nullptr;
    std::span<cata_gpu::GpuColoredLightSource const> sources = {};
    uint32_t* color = nullptr;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
    float z_scale = 1.0f;
    uint32_t source_offset = 0;
    uint32_t num_sources = 0;
    int32_t max_radius = 0;
};

struct vehicle_optics_params {
    float const* transparency = nullptr;
    float const* seen = nullptr;
    std::span<cata_gpu::GpuVehicleOptic const> optics = {};
    uint32_t* camera = nullptr;
    int32_t cache_x = 0;
    int32_t cache_y = 0;
    int32_t cache_xy = 0;
    int32_t z_count = 0;
    uint32_t trigdist = 0;
    float visible_threshold = 0.0f;
    int32_t max_view_distance = 0;
    int32_t max_radius = 0;
};
#endif

auto generated_kernels_available() -> bool;
auto run_startup_probes() -> bool;
auto run_test_compute_probe() -> bool;
auto run_fill_uint_probe() -> bool;
auto run_fill_float_probe() -> bool;
auto run_max_uint_probe() -> bool;
auto run_shift_uint_probe() -> bool;
auto run_shift_float_probe() -> bool;
auto run_clear_seen_probe() -> bool;
auto run_clear_seen_view_probe() -> bool;
auto run_final_visibility_probe() -> bool;
auto run_seen_probe() -> bool;
auto run_seen_walls_probe() -> bool;
auto run_daylight_diffuse_probe() -> bool;
auto run_ambient_probe() -> bool;
#if defined(CATA_SDL)
auto run_transparency_probe() -> bool;
auto run_sight_pairs_probe() -> bool;
auto run_raytrace_probe() -> bool;
auto run_color_raytrace_probe() -> bool;
auto run_vehicle_optics_probe() -> bool;
#endif
auto fill_uint(fill_uint_params const& params) -> bool;
auto fill_float(fill_float_params const& params) -> bool;
auto max_uint(max_uint_params const& params) -> bool;
auto shift_uint(shift_uint_params const& params) -> bool;
auto shift_float(shift_float_params const& params) -> bool;
auto clear_seen(clear_seen_params const& params) -> bool;
auto clear_seen_view(clear_seen_view_params const& params) -> bool;
auto final_visibility(final_visibility_params const& params) -> bool;
auto seen(seen_params const& params) -> bool;
auto seen_walls(seen_walls_params const& params) -> bool;
auto daylight_diffuse(daylight_diffuse_params const& params) -> bool;
auto ambient(ambient_params const& params) -> bool;
#if defined(CATA_SDL)
auto dispatch_transparency(transparency_params const& params) -> bool;
auto sight_pairs(sight_pairs_params const& params) -> bool;
auto raytrace(raytrace_params const& params) -> bool;
auto color_raytrace(color_raytrace_params const& params) -> bool;
auto vehicle_optics(vehicle_optics_params const& params) -> bool;
#endif

} // namespace cata_compute::slang_cpu::kernels

#pragma once

#include <cstdint>
#include <string_view>
#include <span>
#include <vector>

#if defined( CATA_SDL )
#include "gpu_lm.h"
#include "gpu_transparency.h"
#endif

namespace cata_compute
{

enum class backend_kind : int {
    unavailable,
    sdl_gpu,
    slang_cpu,
};

struct backend_status {
    backend_kind kind = backend_kind::unavailable;
    bool available = false;
    bool supports_lighting = false;
    bool supports_visibility = false;
    bool supports_transparency = false;
    bool supports_sight_pairs = false;
    std::string_view name = "unavailable";
    std::string_view detail = "no compute backend";
};

auto init() -> void;
auto shutdown() -> void;
auto active_backend() -> backend_status;
auto backend_available() -> bool;
auto active_backend_name() -> std::string_view;

#if defined( CATA_SDL )

using sight_pair = cata_gpu::GpuSightPair;
using transparency_luts = cata_gpu::transparency_luts;
using transparency_submap_in = cata_gpu::transparency_submap_in;
using transparency_submap_ref = cata_gpu::transparency_submap_ref;
using transparency_push_constants = cata_gpu::transparency_push_constants;

struct lighting_params {
    map const *m = nullptr;
    std::vector<int> const *dirty_levels = nullptr;
    std::vector<int> const *seen_dirty_levels = nullptr;
    int player_x = 0;
    int player_y = 0;
    int player_zlev = 0;
    bool transparency_dirty = false;
    std::vector<int> const *transparency_dirty_levels = nullptr;
    bool floor_dirty = false;
    std::vector<int> const *floor_dirty_levels = nullptr;
    bool vehicle_floor_dirty = false;
    std::vector<int> const *vehicle_floor_dirty_levels = nullptr;
    bool vehicle_obscured_dirty = false;
    std::vector<int> const *vehicle_obscured_dirty_levels = nullptr;
    bool rebuild_seen_cache = false;
    bool download_seen_cache = false;
    bool download_lightmap = true;
    uint32_t vision_block_mask = 0;
    bool angled_sunlight_shadows = false;
    bool direct_sunlight = false;
    float sun_dx_per_z = 0.0f;
    float sun_dy_per_z = 0.0f;
};

struct visibility_params {
    map const *m = nullptr;
    std::vector<int> const *download_levels = nullptr;
    int zlev = 0;
    int player_x = 0;
    int player_y = 0;
    int player_zlev = 0;
    int g_light_level = 0;
    int u_clairvoyance = 0;
    int u_unimpaired_range = 0;
    float vision_threshold = 0.0f;
    float visibility_scale_factor = 1.0f;
    float detail_range = 0.0f;
    uint32_t vision_block_mask = 0;
    bool rebuild_seen_cache = false;
};

struct begin_sight_pairs_params {
    map const *m = nullptr;
    std::vector<sight_pair> const *pairs = nullptr;
    int zlev = 0;
};

struct run_sight_pairs_params {
    map const *m = nullptr;
    std::vector<sight_pair> const *pairs = nullptr;
    std::vector<uint32_t> *results = nullptr;
    int zlev = 0;
};

struct lighting_work {
    backend_kind backend = backend_kind::unavailable;
    uint64_t id = 0;
    cata_gpu::gpu_lighting_work sdl = {};
};

struct visibility_work {
    backend_kind backend = backend_kind::unavailable;
    uint64_t id = 0;
    cata_gpu::gpu_visibility_work sdl = {};
};

struct sight_pairs_work {
    backend_kind backend = backend_kind::unavailable;
    uint64_t id = 0;
    cata_gpu::gpu_sight_pairs_work sdl = {};
};

struct resident_transparency_output {
    backend_kind backend = backend_kind::unavailable;
    uint64_t id = 0;
    uint32_t output_offset = 0;
    cata_gpu::resident_transparency_output sdl = {};
};

struct resident_lighting_ready_params {
    int cache_x = 0;
    int cache_y = 0;
    int z_count = 0;
};

struct prepare_lighting_transparency_output_params {
    int cache_x = 0;
    int cache_y = 0;
    int z_count = 0;
    int zlev = 0;
};

struct shift_lighting_residency_params {
    int cache_x = 0;
    int cache_y = 0;
    int z_count = 0;
    int shift_x_submaps = 0;
    int shift_y_submaps = 0;
};

struct dispatch_transparency_params {
    transparency_luts const *luts = nullptr;
    std::vector<transparency_submap_in> const *submaps = nullptr;
    transparency_push_constants push = {};
    int cache_size = 0;
    std::vector<float> *out_buffer = nullptr;
    resident_transparency_output output = {};
    uint32_t output_offset = 0;
};

struct resident_sight_pair_inputs_params {
    map const *m = nullptr;
    std::vector<sight_pair> const *pairs = nullptr;
    int zlev = 0;
};

auto resident_lighting_ready_for_visibility( resident_lighting_ready_params const &p ) -> bool;
auto resident_lighting_ready_for_sight_pairs( resident_sight_pair_inputs_params const &p )
-> bool;

auto prepare_lighting_transparency_output( prepare_lighting_transparency_output_params const &p )
-> resident_transparency_output;
auto mark_lighting_transparency_level_updated( int zlev ) -> void;
auto lighting_transparency_level_is_valid( int zlev ) -> bool;
auto invalidate_lighting_transparency_levels( std::vector<int> const &levels ) -> void;
auto shift_lighting_resident_inputs( shift_lighting_residency_params const &p ) -> bool;

auto rebuild_transparency_luts( transparency_luts &luts ) -> void;
auto prepare_transparency_inputs(
    std::span<transparency_submap_ref const> refs, std::vector<transparency_submap_in> &out )
-> void;
auto dispatch_transparency( dispatch_transparency_params const &p ) -> bool;

#if defined( CATA_GPU_VERIFY )
auto verify_transparency_against_cpu( map const &m, int zlev, float sight_penalty ) -> void;
#endif

auto begin_lighting( lighting_params const &p ) -> lighting_work;
auto finish_lighting( lighting_work const &work ) -> bool;
auto run_lighting( lighting_params const &p ) -> bool;

auto begin_visibility( visibility_params const &p ) -> visibility_work;
auto finish_visibility( visibility_work const &work ) -> bool;
auto run_visibility( visibility_params const &p ) -> bool;

auto begin_sight_pairs( begin_sight_pairs_params const &p ) -> sight_pairs_work;
auto finish_sight_pairs( sight_pairs_work const &work, std::vector<uint32_t> &results ) -> bool;
auto run_sight_pairs( run_sight_pairs_params const &p ) -> bool;

#endif

} // namespace cata_compute

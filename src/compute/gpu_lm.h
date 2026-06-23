#pragma once
#if defined(CATA_SDL)

#include "coordinates.h"
#include "game_constants.h"
#include "map.h"

#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <vector>

namespace cata_gpu {

// ---------------------------------------------------------------------------
// GpuLightSource
// One light source record.  Uploaded every frame as a single storage buffer.
// One shader workgroup processes one source.
// 48 bytes; C++ layout matches HLSL struct exactly.
// ---------------------------------------------------------------------------
inline constexpr auto light_source_directional = uint32_t{1u << 0};
inline constexpr auto light_source_external_vehicle = uint32_t{1u << 1};

struct GpuLightSource {
    int32_t x;       // flat map tile x coordinate
    int32_t y;       // flat map tile y coordinate
    int32_t z_idx;   // z-level index: zlev + OVERMAP_DEPTH  (0..OVERMAP_LAYERS-1)
    uint32_t flags;  // light_source_* bitfield
    float luminance; // emitted intensity
    float radius;    // effective range in tiles (pre-computed from luminance)
    float dir_x;     // normalized horizontal direction for cone lights
    float dir_y;
    float cone_cos;   // cosine of half-angle for cone lights
    float z_frac;     // fractional source height inside z_idx
    uint32_t _pad[2]; // pad to 48 bytes
};
static_assert(sizeof(GpuLightSource) == 48);

struct GpuColoredLightSource {
    int32_t x;
    int32_t y;
    int32_t z_idx;
    uint32_t flags;
    float luminance;
    float radius;
    float dir_x;
    float dir_y;
    float cone_cos;
    float z_frac;
    uint32_t color_rgb;
    uint32_t _pad;
};
static_assert(sizeof(GpuColoredLightSource) == 48);

// ---------------------------------------------------------------------------
// lm_ambient_push_constants
// Uniform data for the ambient initialisation pass.
// Total = 144 bytes (multiple of 16; fits in the minimum Vulkan push constant
// window and in SDL_GPU's uniform buffer slot).
// ---------------------------------------------------------------------------
// natural_light is stored as 24 floats (= six 16-byte rows) so that the GPU-side
// cbuffer layout remains stable without requiring vector element indexing in
// the shader.  Only the first OVERMAP_LAYERS (21) values are meaningful; the
// remaining 3 slots are unused padding.
struct lm_ambient_push_constants {
    float inside_light;               //  4 bytes, offset  0
    int32_t cache_x;                  //  4 bytes, offset  4
    int32_t cache_y;                  //  4 bytes, offset  8
    int32_t cache_xy;                 //  4 bytes, offset 12 = 16
    int32_t z_count;                  //  4 bytes, offset 16
    int32_t overmap_depth;            //  4 bytes, offset 20
    uint32_t angled_sunlight_shadows; //  4 bytes, offset 24
    uint32_t direct_sunlight;         //  4 bytes, offset 28 = 32
    float sun_dx_per_z;               //  4 bytes, offset 32
    float sun_dy_per_z;               //  4 bytes, offset 36
    float solar_shadow_light;         //  4 bytes, offset 40
    uint32_t _pad0;                   //  4 bytes, offset 44 = 48
    float natural_light[24];          // 96 bytes, offset 48 = 144 total
};
static_assert(sizeof(lm_ambient_push_constants) == 144);

// ---------------------------------------------------------------------------
// lm_raytrace_push_constants
// Uniform data for the per-source ray-casting pass.
// 32 bytes.
// ---------------------------------------------------------------------------
struct lm_raytrace_push_constants {
    int32_t cache_x;        //  4 bytes
    int32_t cache_y;        //  4 bytes
    int32_t cache_xy;       //  4 bytes
    int32_t z_count;        //  4 bytes = 16
    float z_scale;          //  4 bytes
    uint32_t num_sources;   //  4 bytes
    int32_t max_radius;     //  4 bytes
    uint32_t source_offset; //  4 bytes = 32
};
static_assert(sizeof(lm_raytrace_push_constants) == 32);

// ---------------------------------------------------------------------------
// lm_seen_push_constants
// Uniform data for the seen_cache ray-casting pass.
// 64 bytes.
// ---------------------------------------------------------------------------
struct lm_seen_push_constants {
    int32_t player_x;           //  4 bytes
    int32_t player_y;           //  4 bytes
    int32_t player_z_idx;       //  4 bytes
    int32_t cache_x;            //  4 bytes = 16
    int32_t cache_y;            //  4 bytes
    int32_t cache_xy;           //  4 bytes
    int32_t z_count;            //  4 bytes
    int32_t view_radius;        //  4 bytes = 32
    float z_scale;              //  4 bytes
    int32_t z_start_idx;        //  4 bytes
    int32_t dispatch_z_count;   // 4 bytes
    uint32_t trigdist;          //  4 bytes = 48
    uint32_t vision_block_mask; // 4 bytes
    uint32_t _pad[3];           // 12 bytes = 64
};
static_assert(sizeof(lm_seen_push_constants) == 64);

// ---------------------------------------------------------------------------
// lm_visibility_push_constants
// Uniform data for final apparent-light classification.
// 80 bytes.
// ---------------------------------------------------------------------------
struct lm_visibility_push_constants {
    int32_t player_x;              //  4 bytes
    int32_t player_y;              //  4 bytes
    int32_t player_z_idx;          //  4 bytes
    int32_t cache_x;               //  4 bytes = 16
    int32_t cache_y;               //  4 bytes
    int32_t cache_xy;              //  4 bytes
    int32_t z_count;               //  4 bytes
    uint32_t trigdist;             //  4 bytes = 32
    int32_t u_clairvoyance;        //  4 bytes
    int32_t u_unimpaired_range;    //  4 bytes
    int32_t g_light_level;         //  4 bytes
    float vision_threshold;        //  4 bytes = 48
    float visibility_scale_factor; //  4 bytes
    float visible_threshold;       //  4 bytes
    float detail_range;            //  4 bytes
    int32_t z_start_idx;           //  4 bytes = 64
    int32_t dispatch_z_count;      //  4 bytes
    uint32_t _pad[3];              // 12 bytes = 80
};
static_assert(sizeof(lm_visibility_push_constants) == 80);

// ---------------------------------------------------------------------------
// Compute a conservative dispatch radius from source luminance.
// Matches lm_raytrace_compute.hlsl: exponential attenuation divided by linear
// distance, capped to the runtime view radius.
// ---------------------------------------------------------------------------
auto compute_light_radius(float luminance) -> float;

// ---------------------------------------------------------------------------
// run_gpu_lighting_params
// Input to run_gpu_lighting.  Uses an options struct because there are more
// than three parameters.
// ---------------------------------------------------------------------------
struct run_gpu_lighting_params {
    map const* m;                                        // source of all level caches
    std::vector<int> const* dirty_levels;                // z-levels whose lm needs rebuild
    std::vector<int> const* seen_dirty_levels = nullptr; // CPU seen slices to refresh when seen
                                                         // rebuilds
    int player_x;                                        // player tile coordinates (flat map space)
    int player_y;
    int player_zlev; // actual game z-level (not z_idx)
    bool transparency_dirty;
    std::vector<int> const* transparency_dirty_levels = nullptr;
    bool floor_dirty;
    std::vector<int> const* floor_dirty_levels = nullptr;
    bool vehicle_floor_dirty;
    std::vector<int> const* vehicle_floor_dirty_levels = nullptr;
    bool vehicle_obscured_dirty;
    std::vector<int> const* vehicle_obscured_dirty_levels = nullptr;
    bool rebuild_seen_cache;
    bool download_seen_cache = false;
    bool download_lightmap = true; // transitional CPU readback request
    uint32_t vision_block_mask = 0;
    bool angled_sunlight_shadows;
    bool direct_sunlight;
    float sun_dx_per_z;
    float sun_dy_per_z;
};

struct gpu_lighting_work {
    uint64_t id = 0;
};

struct resident_transparency_output {
    SDL_GPUBuffer* buffer = nullptr;
    uint32_t output_offset = 0; // float elements from the start of the resident volume
};

struct prepare_lighting_transparency_output_params {
    SDL_GPUDevice* device = nullptr;
    int cache_x = 0;
    int cache_y = 0;
    int z_count = 0;
    int zlev = 0;
};

// Ensure the lighting input transparency buffer exists and return the resident
// output binding for one z-level.  Used by transparency_compute so a successful
// transparency dispatch can seed the lighting input buffer directly.
auto prepare_lighting_transparency_output(prepare_lighting_transparency_output_params const& p)
    -> resident_transparency_output;

// Mark levels whose resident transparency slice was written by the transparency
// shader this cache cycle.  Later CPU-side transparency mutations, such as
// vehicle opacity stamps, must invalidate these markers before lighting runs.
auto mark_lighting_transparency_level_updated(int zlev) -> void;
auto lighting_transparency_level_is_valid(int zlev) -> bool;
auto invalidate_lighting_transparency_levels(std::vector<int> const& levels) -> void;

struct resident_lighting_visibility_params {
    SDL_GPUDevice* device = nullptr;
    int cache_x = 0;
    int cache_y = 0;
    int z_count = 0;
};

auto resident_lighting_ready_for_visibility(resident_lighting_visibility_params const& p) -> bool;

struct shift_lighting_residency_params {
    SDL_GPUDevice* device = nullptr;
    int cache_x = 0;
    int cache_y = 0;
    int z_count = 0;
    int shift_x_submaps = 0;
    int shift_y_submaps = 0;
};

// Shift resident GPU lighting input buffers in flat bubble coordinates after
// map::shift().  This preserves valid resident transparency so dirty submap
// transparency dispatches can patch only the newly loaded edge bands.
auto shift_lighting_resident_inputs(shift_lighting_residency_params const& p) -> bool;

// Begin the full GPU lighting pass for the dirty z-levels.
//   1. Pack inputs from CPU level caches.
//   2. Collect typed GPU light sources.
//   3. Ambient init pass  -> initialises lm_all and daylight_seed.
//   4. Daylight diffusion -> gather-propagates seed daylight through transparent openings.
//   5. Raytrace pass      -> InterlockedMax per-source contributions into lm_all.
//   6. Seen-cache pass    -> ray cast from player into raw seen_all when requested
//                           or when resident GPU seen data is invalid.
//   7. Surface pass       -> make glancing surfaces inherit adjacent visibility.
//   8. Schedule lm/seen downloads when CPU readback was requested.
// device must be non-null (caller responsibility).
// Returns an id-bearing handle if the GPU pass was submitted.
// A failed SDL_GPU lighting pass is an error; it must not silently rebuild with
// legacy CPU lighting.
auto begin_gpu_lighting(SDL_GPUDevice* device, run_gpu_lighting_params const& p)
    -> gpu_lighting_work;

// Finish a submitted lighting pass, wait for any CPU-facing download, unpack
// requested CPU cache data, and commit resident input/output validity.
auto finish_gpu_lighting(SDL_GPUDevice* device, gpu_lighting_work const& work) -> bool;

// Synchronous compatibility wrapper for callers that still need completed CPU
// data before returning.
auto run_gpu_lighting(SDL_GPUDevice* device, run_gpu_lighting_params const& p) -> bool;

// ---------------------------------------------------------------------------
// run_gpu_visibility_params
// Input to the final visibility classification compute pass.
// ---------------------------------------------------------------------------
struct run_gpu_visibility_params {
    map const* m;
    std::vector<int> const* download_levels = nullptr;
    int zlev;
    int player_x;
    int player_y;
    int player_zlev;
    int g_light_level;
    int u_clairvoyance;
    int u_unimpaired_range;
    float vision_threshold;
    float visibility_scale_factor;
    float detail_range;
    uint32_t vision_block_mask = 0;
    bool rebuild_seen_cache = false;
};

struct gpu_visibility_work {
    uint64_t id = 0;
};

// Begin the final apparent-light classification pass and schedule the
// requested lit_level download.
auto begin_gpu_visibility(SDL_GPUDevice* device, run_gpu_visibility_params const& p)
    -> gpu_visibility_work;

// Finish a submitted visibility pass and unpack downloaded lit_level values
// into level_cache::visibility_cache for requested z-levels.
auto finish_gpu_visibility(SDL_GPUDevice* device, gpu_visibility_work const& work) -> bool;

// Synchronous compatibility wrapper for current callers.
auto run_gpu_visibility(SDL_GPUDevice* device, run_gpu_visibility_params const& p) -> bool;

struct GpuSightPair {
    int32_t from_x;
    int32_t from_y;
    int32_t from_z_idx;
    int32_t to_x;
    int32_t to_y;
    int32_t to_z_idx;
    int32_t range;
    uint32_t _pad;
};
static_assert(sizeof(GpuSightPair) == 32);

struct run_gpu_sight_pairs_params {
    map const* m = nullptr;
    std::vector<GpuSightPair> const* pairs = nullptr;
    std::vector<uint32_t>* results = nullptr;
    int zlev = 0;
};

struct begin_gpu_sight_pairs_params {
    map const* m = nullptr;
    std::vector<GpuSightPair> const* pairs = nullptr;
    int zlev = 0;
};

struct resident_sight_pair_inputs_params {
    SDL_GPUDevice* device = nullptr;
    map const* m = nullptr;
    std::vector<GpuSightPair> const* pairs = nullptr;
    int zlev = 0;
};

struct gpu_sight_pairs_work {
    uint64_t id = 0;
};

auto resident_lighting_ready_for_sight_pairs(resident_sight_pair_inputs_params const& p) -> bool;
auto begin_gpu_sight_pairs(SDL_GPUDevice* device, begin_gpu_sight_pairs_params const& p)
    -> gpu_sight_pairs_work;
auto finish_gpu_sight_pairs(
    SDL_GPUDevice* device, gpu_sight_pairs_work const& work, std::vector<uint32_t>& results)
    -> bool;
auto run_gpu_sight_pairs(SDL_GPUDevice* device, run_gpu_sight_pairs_params const& p) -> bool;

// Release all GPU pipeline objects owned by the lm module.
// Called from cata_gpu::shutdown() before the device is destroyed.
auto shutdown_lm() -> void;

} // namespace cata_gpu
#endif // defined( CATA_SDL )

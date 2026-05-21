#pragma once
#if defined( CATA_SDL )

#include <cstdint>
#include <SDL3/SDL_gpu.h>
#include <vector>

#include "coordinates.h"
#include "game_constants.h"
#include "map.h"

namespace cata_gpu
{

// ---------------------------------------------------------------------------
// GpuLightSource
// One omnidirectional point light source record.  Uploaded every frame as a
// single storage buffer.  One shader workgroup processes one source.
// 32 bytes; C++ layout matches HLSL struct exactly.
// ---------------------------------------------------------------------------
struct GpuLightSource {
    int32_t  x;           // flat map tile x coordinate
    int32_t  y;           // flat map tile y coordinate
    int32_t  z_idx;       // z-level index: zlev + OVERMAP_DEPTH  (0..OVERMAP_LAYERS-1)
    float    luminance;   // emitted intensity
    float    radius;      // effective range in tiles (pre-computed from luminance)
    uint32_t _pad[3];     // pad to 32 bytes
};
static_assert( sizeof( GpuLightSource ) == 32 );

// ---------------------------------------------------------------------------
// lm_ambient_push_constants
// Uniform data for the ambient initialisation pass.
// Total = 144 bytes (multiple of 16; fits in the minimum Vulkan push constant
// window and in SDL_GPU's uniform buffer slot).
// ---------------------------------------------------------------------------
// natural_light is stored as float[6][4] (= six float4s = 96 bytes) so that the
// GPU-side cbuffer element stride is 16 bytes in both std140 (Vulkan uniform
// buffer default) and HLSL scalar layout.  The HLSL shader declares the
// corresponding field as float4 natural_light[6] and accesses element zi as
// natural_light[zi / 4][zi % 4].  Only the first OVERMAP_LAYERS (21) values
// are meaningful; the remaining 3 slots (indices 21-23) are unused.
struct lm_ambient_push_constants {
    float    inside_light;             //  4 bytes, offset  0
    int32_t  cache_x;                  //  4 bytes, offset  4
    int32_t  cache_y;                  //  4 bytes, offset  8
    int32_t  cache_xy;                 //  4 bytes, offset 12 = 16
    int32_t  z_count;                  //  4 bytes, offset 16
    int32_t  overmap_depth;            //  4 bytes, offset 20
    uint32_t angled_sunlight_shadows;  //  4 bytes, offset 24
    uint32_t direct_sunlight;          //  4 bytes, offset 28 = 32
    float    sun_dx_per_z;             //  4 bytes, offset 32
    float    sun_dy_per_z;             //  4 bytes, offset 36
    uint32_t _pad0[2];                 //  8 bytes, offset 40 = 48
    float    natural_light[6][4];      // 96 bytes, offset 48 = 144 total
};
static_assert( sizeof( lm_ambient_push_constants ) == 144 );

// ---------------------------------------------------------------------------
// lm_raytrace_push_constants
// Uniform data for the per-source ray-casting pass.
// 32 bytes.
// ---------------------------------------------------------------------------
struct lm_raytrace_push_constants {
    int32_t  cache_x;       //  4 bytes
    int32_t  cache_y;       //  4 bytes
    int32_t  cache_xy;      //  4 bytes
    int32_t  z_count;       //  4 bytes = 16
    float    z_scale;       //  4 bytes
    uint32_t num_sources;   //  4 bytes
    int32_t  max_radius;    //  4 bytes
    uint32_t _pad;          //  4 bytes = 32
};
static_assert( sizeof( lm_raytrace_push_constants ) == 32 );

// ---------------------------------------------------------------------------
// lm_seen_push_constants
// Uniform data for the seen_cache ray-casting pass.
// 48 bytes.
// ---------------------------------------------------------------------------
struct lm_seen_push_constants {
    int32_t  player_x;      //  4 bytes
    int32_t  player_y;      //  4 bytes
    int32_t  player_z_idx;  //  4 bytes
    int32_t  cache_x;       //  4 bytes = 16
    int32_t  cache_y;       //  4 bytes
    int32_t  cache_xy;      //  4 bytes
    int32_t  z_count;       //  4 bytes
    int32_t  view_radius;   //  4 bytes = 32
    float    z_scale;       //  4 bytes
    uint32_t _pad[3];       // 12 bytes = 48
};
static_assert( sizeof( lm_seen_push_constants ) == 48 );

// ---------------------------------------------------------------------------
// Compute the effective illumination radius from source luminance.
// Uses the same formula as LIGHT_RANGE() in lightmap.h, capped at
// MAX_VIEW_DISTANCE to bound dispatch dimensions.
// ---------------------------------------------------------------------------
auto compute_light_radius( float luminance ) -> float;

// ---------------------------------------------------------------------------
// run_gpu_lighting_params
// Input to run_gpu_lighting.  Uses an options struct because there are more
// than three parameters.
// ---------------------------------------------------------------------------
struct run_gpu_lighting_params {
    map const          *m;             // source of all level caches
    std::vector<int> const *dirty_levels; // z-levels whose lm needs rebuild
    int                 player_x;      // player tile coordinates (flat map space)
    int                 player_y;
    int                 player_zlev;   // actual game z-level (not z_idx)
    bool                angled_sunlight_shadows;
    bool                direct_sunlight;
    float               sun_dx_per_z;
    float               sun_dy_per_z;
};

// Run the full GPU lighting pass for the dirty z-levels.
//   1. Pack inputs from CPU level caches.
//   2. Collect light sources (light_source_buffer + character/monster lights).
//   3. Ambient init pass  → initialises lm_all.
//   4. Raytrace pass      → InterlockedMax per-source contributions into lm_all.
//   5. Seen-cache pass    → ray cast from player into raw seen_all.
//   6. Surface pass       → make glancing surfaces inherit adjacent visibility.
//   7. Download lm and seen_cache back to map_cache for each dirty level.
// device must be non-null (caller responsibility).
// Returns false if the GPU pass could not run (e.g. shaders not compiled).
// A failed SDL_GPU lighting pass is an error; it must not silently rebuild with
// legacy CPU lighting.
auto run_gpu_lighting( SDL_GPUDevice *device,
                       run_gpu_lighting_params const &p ) -> bool;

// Release all GPU pipeline objects owned by the lm module.
// Called from cata_gpu::shutdown() before the device is destroyed.
auto shutdown_lm() -> void;

} // namespace cata_gpu
#endif // defined( CATA_SDL )

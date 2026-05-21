#pragma once
#if defined( CATA_SDL )

#include <cstdint>
#include <span>
#include <vector>

#include "game_constants.h"

class map;
struct SDL_GPUDevice;
struct submap;

namespace cata_gpu
{

// ---------------------------------------------------------------------------
// transparency_submap_in
//
// Per-submap GPU upload record for the transparency compute pass.
// One record per submap dispatched; SV_GroupID.x indexes into the array.
//
// Tile arrays use submap-local flat indexing: idx = sx * SEEY + sy.
// C-array [SEEX][SEEY] is contiguous with the same layout so memcpy is valid.
//
// All integer fields are uint32_t to match HLSL's uint without byte-packing.
// The HLSL struct must match this layout exactly.
// ---------------------------------------------------------------------------
struct transparency_submap_in {
    uint32_t ter_ids[SEEX * SEEY];       // terrain int_id values
    uint32_t furn_ids[SEEX * SEEY];      // furniture int_id values
    float    field_opacity[SEEX * SEEY]; // product of non-transparent field translucencies;
                                              // 1.0 = no opaque fields present at this tile
    uint32_t outside_flags[SEEX * SEEY]; // 1 = tile exposed to sky (weather penalty applies)
    int32_t  cache_offset_x; // tile x of this submap's origin in the flat level cache
    int32_t  cache_offset_y; // tile y of this submap's origin in the flat level cache
};
static_assert( sizeof( transparency_submap_in ) == 2312 );

// ---------------------------------------------------------------------------
// transparency_luts
//
// Lookup tables indexed by int_id::to_i().
// Rebuilt at world load, after all terrain and furniture types are registered
// from JSON.  Must be rebuilt again if the active mod set changes.
// Size matches the registered type count at the time of the call.
// ---------------------------------------------------------------------------
struct transparency_luts {
    std::vector<uint32_t> ter_transparent;  // 1 = terrain type passes light
    std::vector<uint32_t> furn_transparent; // 1 = furniture type passes light
};

// ---------------------------------------------------------------------------
// transparency_push_constants
//
// Uniform data broadcast to every thread in a transparency dispatch.
// std140 layout requires the struct to be padded to a multiple of 16 bytes.
// ---------------------------------------------------------------------------
struct transparency_push_constants {
    float    sight_penalty; // weather sight_penalty (1.0 = clear, <1.0 = rain/fog/etc.)
    int32_t  cache_y;       // flat level-cache y-stride (= SEEY * mapsize)
    uint32_t num_submaps;   // number of entries in the per-submap storage buffer
    uint32_t _pad = 0;      // std140: struct size rounded up to next 16 bytes
};
static_assert( sizeof( transparency_push_constants ) == 16 );

// ---------------------------------------------------------------------------
// transparency_submap_ref
//
// Thin descriptor passed by the map layer to prepare_transparency_inputs.
// Keeps the compute module free of a direct map.h dependency at the call site.
// Pre-condition: sm->outside_cache must be current (rebuild_outside_cache runs
// before transparency in build_map_cache, so this is satisfied in normal flow).
// ---------------------------------------------------------------------------
struct transparency_submap_ref {
    submap const *sm;        // source submap data
    int           offset_x;  // sm_offset.x() — tile x of submap origin in flat level cache
    int           offset_y;  // sm_offset.y()
};

// Rebuild ter_transparent / furn_transparent from the currently loaded type
// registries.  Call once after world load and again when the active mod set
// changes.
auto rebuild_transparency_luts( transparency_luts &luts ) -> void;

// Collect transparency_submap_ref records for all submaps at zlev.
// Uses only public map accessors (get_submap_at_grid, get_cache_ref).
auto gather_transparency_refs( map const &m, int zlev )
-> std::vector<transparency_submap_ref>;

// Build GPU upload records for a set of submaps.
// Reads ter/furn/field/outside_cache data from each ref.sm and writes one
// transparency_submap_in per ref into out (clearing it first).
auto prepare_transparency_inputs( std::span<transparency_submap_ref const> refs,
                                  std::vector<transparency_submap_in>      &out ) -> void;

// Upload the submap records, dispatch the transparency compute shader, and
// synchronously download the result into out_buffer (resized to cache_size).
// out_buffer is always populated; callers decide whether to use or compare it.
auto dispatch_transparency( SDL_GPUDevice                              *device,
                             transparency_luts const                    &luts,
                             std::vector<transparency_submap_in> const  &submaps,
                             transparency_push_constants const           &push,
                             int                                          cache_size,
                             std::vector<float>                          &out_buffer ) -> void;

#if defined( CATA_GPU_VERIFY )
// Orchestrates the full GPU verify cycle for one z-level:
// gather → prepare → dispatch → compare against the CPU result already in
// m.get_cache_ref(zlev).transparency_cache.
// sight_penalty comes from get_weather().weather_id->sight_penalty — the
// caller (lightmap.cpp) already has that value in scope.
auto verify_transparency_against_cpu( map const &m, int zlev,
                                      float sight_penalty ) -> void;

// Kept for API symmetry; superseded by verify_transparency_against_cpu for
// Phase 6.  Will be wired for standalone readback in a later phase.
auto verify_transparency_readback( SDL_GPUDevice *device,
                                   float const   *cpu_reference,
                                   int            cache_size ) -> void;
#endif

} // namespace cata_gpu
#endif // defined( CATA_SDL )

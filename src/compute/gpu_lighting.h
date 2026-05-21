#pragma once
#if defined( CATA_SDL )

#include <cstdint>

class map;
struct SDL_GPUBuffer;

namespace cata_gpu
{

// GPU-resident floor and vehicle_floor cache buffers.
//
// floor_cache:         char per tile; non-zero = solid floor, blocks vertical ray propagation.
// vehicle_floor_cache: char per tile; non-zero = vehicle roof present above this tile.
//
// Uploaded unconditionally each frame after the corresponding build phase in build_map_cache().
// The lm and seen_cache shaders consume these as read-only compute storage buffers.

// GPU buffer accessors for lm / seen_cache shader bindings.
// Return nullptr before the first upload_floor_caches / upload_vehicle_floor_caches call.
auto get_floor_cache_buffer( int zlev ) -> SDL_GPUBuffer *;
auto get_vehicle_floor_cache_buffer( int zlev ) -> SDL_GPUBuffer *;

// Upload floor_cache for all z-levels in [minz, maxz] in one GPU command buffer.
// Call after Phase1_floor completes in build_map_cache().
auto upload_floor_caches( map const &m, int minz, int maxz ) -> void;

// Upload vehicle_floor_cache for all z-levels in [minz, maxz] in one GPU command buffer.
// Call after Phase3_vehicles completes in build_map_cache().
auto upload_vehicle_floor_caches( map const &m, int minz, int maxz ) -> void;

// Release all GPU buffers owned by the lighting module.
// Must be called before SDL_DestroyGPUDevice — invoked from cata_gpu::shutdown().
auto shutdown_lighting() -> void;

} // namespace cata_gpu
#endif // defined( CATA_SDL )

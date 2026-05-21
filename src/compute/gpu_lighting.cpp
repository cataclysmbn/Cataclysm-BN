#if defined( CATA_SDL )
#include "gpu_lighting.h"
#include "gpu_platform.h"

#include "game_constants.h"
#include "map.h"

#include <SDL3/SDL_gpu.h>

#include <array>
#include <cstring>
#include <ranges>
#include <span>

namespace cata_gpu
{

namespace
{

struct ZBuf {
    SDL_GPUBuffer *buf  = nullptr;
    Uint32         size = 0;
};

static std::array<ZBuf, OVERMAP_LAYERS> s_floor_bufs;
static std::array<ZBuf, OVERMAP_LAYERS> s_vehicle_floor_bufs;

// Ensure *slot holds a compute-storage-read GPU buffer of exactly `size` bytes.
// Releases and reallocates only when the size changes.
auto ensure_buf( SDL_GPUDevice *const device, ZBuf &slot, Uint32 const size ) -> void
{
    if( slot.size == size ) {
        return;
    }
    if( slot.buf != nullptr ) {
        SDL_ReleaseGPUBuffer( device, slot.buf );
        slot.buf  = nullptr;
        slot.size = 0;
    }
    SDL_GPUBufferCreateInfo const ci{
        .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, .size = size, .props = 0
    };
    slot.buf  = SDL_CreateGPUBuffer( device, &ci );
    slot.size = size;
}

// Common batch-upload implementation for any per-z-level vector<char> cache.
// Iterates [minz, maxz], ensures GPU buffers, stages into one transfer buffer,
// uploads in a single copy pass, and synchronously waits for completion.
auto upload_char_cache_batch( SDL_GPUDevice *const device,
                               std::array<ZBuf, OVERMAP_LAYERS> &slots,
                               map const &m, int const minz, int const maxz,
                               auto cache_accessor ) -> void
{
    // Ensure GPU buffers and compute total upload size.
    auto total_size = Uint32{ 0 };
    for( auto const z : std::views::iota( minz, maxz + 1 ) ) {
        auto const &vec  = cache_accessor( m.get_cache_ref( z ) );
        auto const  size = static_cast<Uint32>( vec.size() );
        if( size == 0 ) {
            continue;
        }
        ensure_buf( device, slots[z + OVERMAP_DEPTH], size );
        total_size += size;
    }
    if( total_size == 0 ) {
        return;
    }

    // Stage all z-levels into a single transfer buffer.
    SDL_GPUTransferBufferCreateInfo const tbuf_ci{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = total_size, .props = 0
    };
    auto *const tbuf   = SDL_CreateGPUTransferBuffer( device, &tbuf_ci );
    auto *const mapped = static_cast<std::byte *>( SDL_MapGPUTransferBuffer( device, tbuf, false ) );

    auto offset = Uint32{ 0 };
    for( auto const z : std::views::iota( minz, maxz + 1 ) ) {
        auto const &vec  = cache_accessor( m.get_cache_ref( z ) );
        auto const  size = static_cast<Uint32>( vec.size() );
        if( size == 0 ) {
            continue;
        }
        std::memcpy( mapped + offset, vec.data(), size );
        offset += size;
    }
    SDL_UnmapGPUTransferBuffer( device, tbuf );

    // Upload all z-levels in a single copy pass.
    auto *const cmd = SDL_AcquireGPUCommandBuffer( device );
    auto *const cp  = SDL_BeginGPUCopyPass( cmd );

    offset = 0;
    for( auto const z : std::views::iota( minz, maxz + 1 ) ) {
        auto const &vec  = cache_accessor( m.get_cache_ref( z ) );
        auto const  size = static_cast<Uint32>( vec.size() );
        if( size == 0 ) {
            continue;
        }
        auto const &slot = slots[z + OVERMAP_DEPTH];
        SDL_GPUTransferBufferLocation const src{ .transfer_buffer = tbuf, .offset = offset };
        SDL_GPUBufferRegion const           dst{ .buffer = slot.buf, .offset = 0, .size = size };
        SDL_UploadToGPUBuffer( cp, &src, &dst, false );
        offset += size;
    }

    SDL_EndGPUCopyPass( cp );
    auto *const fence = SDL_SubmitGPUCommandBufferAndAcquireFence( cmd );
    SDL_WaitForGPUFences( device, true, &fence, 1 );
    SDL_ReleaseGPUFence( device, fence );
    SDL_ReleaseGPUTransferBuffer( device, tbuf );
}

} // namespace

auto get_floor_cache_buffer( int const zlev ) -> SDL_GPUBuffer *
{
    return s_floor_bufs[zlev + OVERMAP_DEPTH].buf;
}

auto get_vehicle_floor_cache_buffer( int const zlev ) -> SDL_GPUBuffer *
{
    return s_vehicle_floor_bufs[zlev + OVERMAP_DEPTH].buf;
}

auto upload_floor_caches( map const &m, int const minz, int const maxz ) -> void
{
    auto *const device = get_device();
    if( device == nullptr ) {
        return;
    }
    upload_char_cache_batch( device, s_floor_bufs, m, minz, maxz,
    []( level_cache const &lc ) -> std::vector<char> const & {
        return lc.floor_cache;
    } );
}

auto upload_vehicle_floor_caches( map const &m, int const minz, int const maxz ) -> void
{
    auto *const device = get_device();
    if( device == nullptr ) {
        return;
    }
    upload_char_cache_batch( device, s_vehicle_floor_bufs, m, minz, maxz,
    []( level_cache const &lc ) -> std::vector<char> const & {
        return lc.vehicle_floor_cache;
    } );
}

auto shutdown_lighting() -> void
{
    auto *const device = get_device();
    for( auto &slot : s_floor_bufs ) {
        if( slot.buf != nullptr ) {
            SDL_ReleaseGPUBuffer( device, slot.buf );
            slot.buf  = nullptr;
            slot.size = 0;
        }
    }
    for( auto &slot : s_vehicle_floor_bufs ) {
        if( slot.buf != nullptr ) {
            SDL_ReleaseGPUBuffer( device, slot.buf );
            slot.buf  = nullptr;
            slot.size = 0;
        }
    }
}

} // namespace cata_gpu
#endif // defined( CATA_SDL )

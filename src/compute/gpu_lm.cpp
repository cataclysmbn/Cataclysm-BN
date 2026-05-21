#if defined( CATA_SDL )
#include "gpu_lm.h"
#include "gpu_platform.h"

#include "cached_options.h"
#include "character.h"
#include "debug.h"
#include "effect.h"
#include "game.h"
#include "game_constants.h"
#include "lightmap.h"
#include "map.h"
#include "monster.h"
#include "npc.h"
#include "path_info.h"
#include "shadowcasting.h"

#include <SDL3/SDL_gpu.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cata_gpu
{

namespace
{

// ---------------------------------------------------------------------------
// Pipeline management
// ---------------------------------------------------------------------------

auto read_blob( std::string const &path ) -> std::vector<std::byte>
{
    auto ifs = std::ifstream( path, std::ios::binary | std::ios::ate );
    if( !ifs ) {
        return {};
    }
    auto const size = static_cast<std::size_t>( ifs.tellg() );
    ifs.seekg( 0 );
    std::vector<std::byte> buf( size );
    ifs.read( reinterpret_cast<char *>( buf.data() ), static_cast<std::streamsize>( size ) );
    return buf;
}

auto preferred_ext( SDL_GPUShaderFormat const fmts ) -> std::string_view
{
    if( fmts & SDL_GPU_SHADERFORMAT_DXIL  ) {
        return ".dxil";
    }
    if( fmts & SDL_GPU_SHADERFORMAT_SPIRV ) {
        return ".spv";
    }
    if( fmts & SDL_GPU_SHADERFORMAT_MSL   ) {
        return ".msl";
    }
    return {};
}

auto preferred_fmt( SDL_GPUShaderFormat const fmts ) -> SDL_GPUShaderFormat
{
    if( fmts & SDL_GPU_SHADERFORMAT_DXIL  ) {
        return SDL_GPU_SHADERFORMAT_DXIL;
    }
    if( fmts & SDL_GPU_SHADERFORMAT_SPIRV ) {
        return SDL_GPU_SHADERFORMAT_SPIRV;
    }
    if( fmts & SDL_GPU_SHADERFORMAT_MSL   ) {
        return SDL_GPU_SHADERFORMAT_MSL;
    }
    return SDL_GPU_SHADERFORMAT_INVALID;
}

SDL_GPUComputePipeline *s_ambient_pipeline  = nullptr;
SDL_GPUComputePipeline *s_raytrace_pipeline = nullptr;
SDL_GPUComputePipeline *s_seen_pipeline     = nullptr;
SDL_GPUComputePipeline *s_seen_walls_pipeline = nullptr;

auto load_pipeline( SDL_GPUDevice *const device,
                    std::string_view const name,
                    int const ro_bufs,
                    int const rw_bufs,
                    int const threadcount_x,
                    int const threadcount_y ) -> SDL_GPUComputePipeline *
{
    auto const fmts = SDL_GetGPUShaderFormats( device );
    auto const fmt  = preferred_fmt( fmts );
    auto const ext  = preferred_ext( fmts );

    if( fmt == SDL_GPU_SHADERFORMAT_INVALID || ext.empty() ) {
        DebugLog( DL::Error, DC::Main )
                << "SDL_GPU: lm: no supported shader format for " << name;
        return nullptr;
    }

    auto const path = PATH_INFO::shaders() + std::string{ name } + std::string{ ext };
    auto const blob = read_blob( path );
    if( blob.empty() ) {
        DebugLog( DL::Error, DC::Main )
                << "SDL_GPU: lm: shader blob not found: " << path
                << " (run a build with shadercross to compile shaders)";
        return nullptr;
    }

    SDL_GPUComputePipelineCreateInfo const info{
        .code_size                    = blob.size(),
        .code                         = reinterpret_cast<Uint8 const *>( blob.data() ),
        .entrypoint                   = "main",
        .format                       = fmt,
        .num_samplers                 = 0,
        .num_readonly_storage_textures  = 0,
        .num_readonly_storage_buffers   = static_cast<Uint32>( ro_bufs ),
        .num_readwrite_storage_textures = 0,
        .num_readwrite_storage_buffers  = static_cast<Uint32>( rw_bufs ),
        .num_uniform_buffers            = 1,
        .threadcount_x                  = static_cast<Uint32>( threadcount_x ),
        .threadcount_y                  = static_cast<Uint32>( threadcount_y ),
        .threadcount_z                  = 1,
        .props                          = 0,
    };

    auto *const pipeline = SDL_CreateGPUComputePipeline( device, &info );
    if( pipeline == nullptr ) {
        DebugLog( DL::Error, DC::Main )
                << "SDL_GPU: lm: pipeline creation failed for " << name
                << ": " << SDL_GetError();
    }
    return pipeline;
}

auto ensure_pipelines( SDL_GPUDevice *const device ) -> bool
{
    if( s_ambient_pipeline == nullptr ) {
        s_ambient_pipeline = load_pipeline( device, "lm_ambient_compute",
                                            /*ro=*/3, /*rw=*/1, 64, 1 );
    }
    if( s_raytrace_pipeline == nullptr ) {
        s_raytrace_pipeline = load_pipeline( device, "lm_raytrace_compute",
                                             /*ro=*/4, /*rw=*/1, 8, 8 );
    }
    if( s_seen_pipeline == nullptr ) {
        s_seen_pipeline = load_pipeline( device, "lm_seen_compute",
                                         /*ro=*/3, /*rw=*/1, 8, 8 );
    }
    if( s_seen_walls_pipeline == nullptr ) {
        s_seen_walls_pipeline = load_pipeline( device, "lm_seen_walls_compute",
                                               /*ro=*/2, /*rw=*/1, 8, 8 );
    }
    return s_ambient_pipeline != nullptr
           && s_raytrace_pipeline != nullptr
           && s_seen_pipeline     != nullptr
           && s_seen_walls_pipeline != nullptr;
}

// ---------------------------------------------------------------------------
// Transient GPU buffer helpers
// ---------------------------------------------------------------------------

auto alloc_ro( SDL_GPUDevice *const device, Uint32 const bytes ) -> SDL_GPUBuffer *
{
    SDL_GPUBufferCreateInfo const ci{
        .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, .size = bytes, .props = 0
    };
    return SDL_CreateGPUBuffer( device, &ci );
}

auto alloc_rw( SDL_GPUDevice *const device, Uint32 const bytes ) -> SDL_GPUBuffer *
{
    SDL_GPUBufferCreateInfo const ci{
        .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
        .size  = bytes,
        .props = 0,
    };
    return SDL_CreateGPUBuffer( device, &ci );
}

auto alloc_upload( SDL_GPUDevice *const device, Uint32 const bytes ) -> SDL_GPUTransferBuffer *
{
    SDL_GPUTransferBufferCreateInfo const ci{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = bytes, .props = 0
    };
    return SDL_CreateGPUTransferBuffer( device, &ci );
}

auto alloc_download( SDL_GPUDevice *const device, Uint32 const bytes ) -> SDL_GPUTransferBuffer *
{
    SDL_GPUTransferBufferCreateInfo const ci{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = bytes, .props = 0
    };
    return SDL_CreateGPUTransferBuffer( device, &ci );
}

// ---------------------------------------------------------------------------
// Light source collection
// ---------------------------------------------------------------------------

auto make_source( int const x, int const y, int const zlev,
                  float const luminance ) -> GpuLightSource
{
    return GpuLightSource{
        .x = x,
        .y = y,
        .z_idx = zlev + OVERMAP_DEPTH,
        .luminance = luminance,
        .radius = compute_light_radius( luminance ),
        ._pad = {},
    };
}

auto collect_sources( map const &m,
                      std::vector<int> const &dirty_levels ) -> std::vector<GpuLightSource>
{
    auto sources = std::vector<GpuLightSource>{};

    // Collect omnidirectional point sources from light_source_buffer.
    // These were populated by generate_lightmap_worker (collect-only mode)
    // and cover terrain, furniture, item, and vehicle circular lights.
    for( int const z : dirty_levels ) {
        auto const &lc          = m.get_cache_ref( z );
        auto const &lsb         = lc.light_source_buffer;
        auto const  cache_y_sz  = lc.cache_y;

        for( int x = 0; x < lc.cache_x; ++x ) {
            for( int y = 0; y < cache_y_sz; ++y ) {
                float const lum = lsb[lc.idx( x, y )];
                if( lum > LIGHT_AMBIENT_LOW ) {
                    sources.push_back( make_source( x, y, z, lum ) );
                }
            }
        }
    }

    // Collect character lights (player + NPCs).
    // effect_haslight is a CPU-path artifact that marks characters lit by the
    // CPU lightmap; it must not be used here because the GPU derives lm itself.
    // The character's actual emitted light is captured by active_light() and
    // by light_source_buffer (populated during generate_lightmap_worker).
    static const efftype_id effect_onfire( "onfire" );

    auto add_char = [&]( Character const &ch ) {
        auto const &pos = ch.bub_pos();
        if( !m.inbounds( pos ) ) {
            return;
        }
        if( ch.has_effect( effect_onfire ) ) {
            sources.push_back( make_source( pos.x(), pos.y(), pos.z(), 8.0f ) );
        }
        float const held = ch.active_light();
        if( held > LIGHT_AMBIENT_LOW ) {
            sources.push_back( make_source( pos.x(), pos.y(), pos.z(), held ) );
        }
    };

    add_char( get_player_character() );
    for( npc const &guy : g->all_npcs() ) {
        add_char( guy );
    }

    // Collect monster lights.
    for( monster const &critter : g->all_monsters() ) {
        if( critter.is_hallucination() ) {
            continue;
        }
        auto const &mp = critter.bub_pos();
        if( !m.inbounds( mp ) ) {
            continue;
        }
        if( critter.has_effect( effect_onfire ) ) {
            sources.push_back( make_source( mp.x(), mp.y(), mp.z(), 8.0f ) );
        }
        if( critter.type->luminance > 0 ) {
            sources.push_back(
                make_source( mp.x(), mp.y(), mp.z(),
                             static_cast<float>( critter.type->luminance ) ) );
        }
    }

    return sources;
}

// ---------------------------------------------------------------------------
// Input buffer packing (CPU → flat 3D arrays for GPU upload)
// ---------------------------------------------------------------------------

// Pack a per-z-level std::vector<float> cache into a combined 3D buffer.
// Accumulates into `out`, advancing `offset` by each z-level's contribution.
auto pack_float_cache( map const &m,
                       std::vector<int> const &levels,
                       int const z_count,
                       int const cache_xy,
                       std::vector<float> &out ) -> void
{
    out.assign( static_cast<std::size_t>( z_count * cache_xy ), 0.0f );
    for( int const z : levels ) {
        auto const  &lc  = m.get_cache_ref( z );
        auto const   zi  = z + OVERMAP_DEPTH;
        auto const  &src = lc.transparency_cache;
        auto        *dst = out.data() + zi * cache_xy;
        std::ranges::copy( src, dst );
    }
}

// Pack a per-z-level std::vector<char> cache into a combined 3D uint buffer.
// Non-zero char → uint 1; zero → uint 0.
auto pack_char_cache_uint( map const &m,
                           std::vector<int> const &levels,
                           int const z_count,
                           int const cache_xy,
                           auto const cache_accessor,
                           std::vector<uint32_t> &out ) -> void
{
    out.assign( static_cast<std::size_t>( z_count * cache_xy ), 0u );
    for( int const z : levels ) {
        auto const &lc  = m.get_cache_ref( z );
        auto const  zi  = z + OVERMAP_DEPTH;
        auto const &src = cache_accessor( lc );
        auto       *dst = out.data() + zi * cache_xy;
        std::ranges::transform( src, dst,
                                []( char const c ) -> uint32_t { return c != 0 ? 1u : 0u; } );
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

auto compute_light_radius( float const luminance ) -> float
{
    if( luminance <= LIGHT_AMBIENT_LOW ) {
        return 0.0f;
    }
    auto const raw = -std::log( LIGHT_AMBIENT_LOW / luminance )
                     * ( 1.0f / LIGHT_TRANSPARENCY_OPEN_AIR );
    return std::min( raw, static_cast<float>( MAX_VIEW_DISTANCE ) );
}

auto run_gpu_lighting( SDL_GPUDevice *const device,
                       run_gpu_lighting_params const &p ) -> bool
{
    if( p.dirty_levels == nullptr || p.dirty_levels->empty() ) {
        return false;
    }
    if( !ensure_pipelines( device ) ) {
        return false;
    }

    auto const &lc0     = p.m->get_cache_ref( ( *p.dirty_levels )[0] );
    auto const  cache_x = lc0.cache_x;
    auto const  cache_y = lc0.cache_y;
    auto const  cache_xy = cache_x * cache_y;
    auto const  z_count  = OVERMAP_LAYERS;

    // ── Collect light sources ────────────────────────────────────────────────
    auto const sources    = collect_sources( *p.m, *p.dirty_levels );
    auto const num_src    = static_cast<Uint32>( sources.size() );

    // Compute max dispatch radius (clamp to 0 when no sources).
    auto max_radius = 0;
    for( auto const &src : sources ) {
        max_radius = std::max( max_radius, static_cast<int>( std::ceil( src.radius ) ) );
    }
    auto const groups_xy = static_cast<Uint32>( ( 2 * max_radius + 1 + 7 ) / 8 );

    // ── Pack CPU input buffers into flat 3D arrays ───────────────────────────
    // Transparency, floor, and vehicle_floor are packed for ALL z-levels so
    // the seen and ambient shaders have correct data for cross-z-level floor
    // blocking and ceiling checks even when only a subset of levels is dirty.
    auto all_levels = std::vector<int>( z_count );
    std::iota( all_levels.begin(), all_levels.end(), -OVERMAP_DEPTH );

    auto transparency_cpu  = std::vector<float>{};
    auto floor_cpu         = std::vector<uint32_t>{};
    auto vehicle_floor_cpu = std::vector<uint32_t>{};

    pack_float_cache( *p.m, all_levels, z_count, cache_xy, transparency_cpu );
    pack_char_cache_uint( *p.m, all_levels, z_count, cache_xy,
                          []( level_cache const &lc ) -> std::vector<char> const & {
        return lc.floor_cache;
    }, floor_cpu );
    pack_char_cache_uint( *p.m, all_levels, z_count, cache_xy,
                          []( level_cache const &lc ) -> std::vector<char> const & {
        return lc.vehicle_floor_cache;
    }, vehicle_floor_cpu );

    // ── Compute ambient constants per z-level ────────────────────────────────
    auto ambient_push = lm_ambient_push_constants{};
    {
        auto const outside_light = g->natural_light_level( 0 );
        ambient_push.inside_light   = ( outside_light > LIGHT_SOURCE_BRIGHT )
                                      ? LIGHT_AMBIENT_DIM * 0.8f
                                      : LIGHT_AMBIENT_LOW;
        ambient_push.cache_x        = cache_x;
        ambient_push.cache_y        = cache_y;
        ambient_push.cache_xy       = cache_xy;
        ambient_push.z_count        = z_count;
        ambient_push.overmap_depth  = OVERMAP_DEPTH;
        ambient_push.angled_sunlight_shadows = p.angled_sunlight_shadows ? 1u : 0u;
        ambient_push.direct_sunlight = p.direct_sunlight ? 1u : 0u;
        ambient_push.sun_dx_per_z    = p.sun_dx_per_z;
        ambient_push.sun_dy_per_z    = p.sun_dy_per_z;
        for( int zi = 0; zi < OVERMAP_LAYERS; ++zi ) {
            ambient_push.natural_light[zi / 4][zi % 4] = g->natural_light_level( zi - OVERMAP_DEPTH );
        }
    }

    // ── Allocate GPU buffers ─────────────────────────────────────────────────
    auto const t_bytes   = static_cast<Uint32>( transparency_cpu.size()  * sizeof( float    ) );
    auto const f_bytes   = static_cast<Uint32>( floor_cpu.size()         * sizeof( uint32_t ) );
    auto const vf_bytes  = static_cast<Uint32>( vehicle_floor_cpu.size() * sizeof( uint32_t ) );
    // lm and seen buffers cover all z_count * cache_xy tiles.
    auto const out_bytes = static_cast<Uint32>( z_count * cache_xy * sizeof( uint32_t ) );
    auto const src_bytes = num_src > 0
                           ? static_cast<Uint32>( num_src * sizeof( GpuLightSource ) )
                           : static_cast<Uint32>( sizeof( GpuLightSource ) ); // dummy slot

    // Read-only input buffers.
    auto *t_buf   = alloc_ro( device, t_bytes  );
    auto *f_buf   = alloc_ro( device, f_bytes  );
    auto *vf_buf  = alloc_ro( device, vf_bytes );
    auto *src_buf = alloc_ro( device, src_bytes );

    // Read-write lm output (uint, for InterlockedMax).
    auto *lm_buf  = alloc_rw( device, out_bytes );
    // Raw seen output from center-to-center ray casting.
    auto *seen_raw_buf = alloc_rw( device, t_bytes );
    // Final seen output after surface/edge visibility expansion.
    auto *seen_buf = alloc_rw( device, t_bytes );

    // ── Upload: single transfer buffer covering all inputs ───────────────────
    auto const seen_zero_bytes = t_bytes;
    auto const upload_total = t_bytes + f_bytes + vf_bytes + src_bytes
                              + seen_zero_bytes + seen_zero_bytes;
    auto *upload_tbuf = alloc_upload( device, upload_total );
    {
        auto *const mapped = static_cast<std::byte *>(
                                 SDL_MapGPUTransferBuffer( device, upload_tbuf, false ) );
        auto off = Uint32{ 0 };
        std::memcpy( mapped + off, transparency_cpu.data(),   t_bytes  ); off += t_bytes;
        std::memcpy( mapped + off, floor_cpu.data(),          f_bytes  ); off += f_bytes;
        std::memcpy( mapped + off, vehicle_floor_cpu.data(),  vf_bytes ); off += vf_bytes;
        if( num_src > 0 ) {
            std::memcpy( mapped + off, sources.data(), src_bytes );
        }
        off += src_bytes;
        std::memset( mapped + off, 0, seen_zero_bytes + seen_zero_bytes );
        SDL_UnmapGPUTransferBuffer( device, upload_tbuf );
    }

    // Download buffers: lm_all and seen_all.
    auto *lm_dl_tbuf   = alloc_download( device, out_bytes );
    auto *seen_dl_tbuf = alloc_download( device, t_bytes   );

    // ── Build command buffer ─────────────────────────────────────────────────
    auto *const cmd = SDL_AcquireGPUCommandBuffer( device );

    // [Pass 1] Copy: upload all input buffers.
    {
        auto *const cp = SDL_BeginGPUCopyPass( cmd );
        auto off = Uint32{ 0 };

        auto upload = [&]( SDL_GPUBuffer *dst, Uint32 const bytes ) {
            SDL_GPUTransferBufferLocation const src_loc{ .transfer_buffer = upload_tbuf, .offset = off };
            SDL_GPUBufferRegion const dst_reg{ .buffer = dst, .offset = 0, .size = bytes };
            SDL_UploadToGPUBuffer( cp, &src_loc, &dst_reg, false );
            off += bytes;
        };
        upload( t_buf,   t_bytes  );
        upload( f_buf,   f_bytes  );
        upload( vf_buf,  vf_bytes );
        upload( src_buf, src_bytes );
        upload( seen_raw_buf, seen_zero_bytes );
        upload( seen_buf, seen_zero_bytes );

        SDL_EndGPUCopyPass( cp );
    }

    // [Pass 2] Compute: ambient initialisation.
    // Writes lm_all[tile] = asuint(ambient_value) for every tile.
    // Uses floor_all and vehicle_floor_all to determine sky access physically.
    {
        SDL_GPUStorageBufferReadWriteBinding const rw_lm{
            .buffer = lm_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0
        };
        auto *const cp = SDL_BeginGPUComputePass( cmd, nullptr, 0, &rw_lm, 1 );
        SDL_BindGPUComputePipeline( cp, s_ambient_pipeline );

        SDL_GPUBuffer *const ro_bufs[3] = { f_buf, vf_buf, t_buf };
        SDL_BindGPUComputeStorageBuffers( cp, 0, ro_bufs, 3 );

        SDL_PushGPUComputeUniformData( cmd, 0, &ambient_push, sizeof( ambient_push ) );

        auto const total_tiles = static_cast<Uint32>( z_count * cache_xy );
        SDL_DispatchGPUCompute( cp, ( total_tiles + 63 ) / 64, 1, 1 );
        SDL_EndGPUComputePass( cp );
    }

    // [Pass 3] Compute: per-source ray casting.
    // InterlockedMax(lm_all[tile], asuint(intensity)) for each source.
    if( num_src > 0 ) {
        SDL_GPUStorageBufferReadWriteBinding const rw_lm{
            .buffer = lm_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0
        };
        auto *const cp = SDL_BeginGPUComputePass( cmd, nullptr, 0, &rw_lm, 1 );
        SDL_BindGPUComputePipeline( cp, s_raytrace_pipeline );

        SDL_GPUBuffer *const ro_bufs[4] = { t_buf, f_buf, vf_buf, src_buf };
        SDL_BindGPUComputeStorageBuffers( cp, 0, ro_bufs, 4 );

        lm_raytrace_push_constants const raytrace_push{
            .cache_x     = cache_x,
            .cache_y     = cache_y,
            .cache_xy    = cache_xy,
            .z_count     = z_count,
            .z_scale     = Z_LEVEL_SCALE,
            .num_sources = num_src,
            .max_radius  = max_radius,
            ._pad        = 0u,
        };
        SDL_PushGPUComputeUniformData( cmd, 0, &raytrace_push, sizeof( raytrace_push ) );
        SDL_DispatchGPUCompute( cp, num_src, groups_xy, groups_xy );
        SDL_EndGPUComputePass( cp );
    }

    lm_seen_push_constants const seen_push{
        .player_x     = p.player_x,
        .player_y     = p.player_y,
        .player_z_idx = p.player_zlev + OVERMAP_DEPTH,
        .cache_x      = cache_x,
        .cache_y      = cache_y,
        .cache_xy     = cache_xy,
        .z_count      = z_count,
        .view_radius  = MAX_VIEW_DISTANCE,
        .z_scale      = Z_LEVEL_SCALE,
        ._pad         = {},
    };
    auto const diam   = static_cast<Uint32>( 2 * MAX_VIEW_DISTANCE + 1 );
    auto const g_seen = ( diam + 7 ) / 8;

    // [Pass 4] Compute: raw seen_cache ray casting from player.
    {
        SDL_GPUStorageBufferReadWriteBinding const rw_seen{
            .buffer = seen_raw_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0
        };
        auto *const cp = SDL_BeginGPUComputePass( cmd, nullptr, 0, &rw_seen, 1 );
        SDL_BindGPUComputePipeline( cp, s_seen_pipeline );

        SDL_GPUBuffer *const ro_bufs[3] = { t_buf, f_buf, vf_buf };
        SDL_BindGPUComputeStorageBuffers( cp, 0, ro_bufs, 3 );

        SDL_PushGPUComputeUniformData( cmd, 0, &seen_push, sizeof( seen_push ) );
        SDL_DispatchGPUCompute( cp, g_seen, g_seen, static_cast<Uint32>( z_count ) );
        SDL_EndGPUComputePass( cp );
    }

    // [Pass 4b] Compute: surface/edge visibility expansion.
    {
        SDL_GPUStorageBufferReadWriteBinding const rw_seen{
            .buffer = seen_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0
        };
        auto *const cp = SDL_BeginGPUComputePass( cmd, nullptr, 0, &rw_seen, 1 );
        SDL_BindGPUComputePipeline( cp, s_seen_walls_pipeline );

        SDL_GPUBuffer *const ro_bufs[2] = { t_buf, seen_raw_buf };
        SDL_BindGPUComputeStorageBuffers( cp, 0, ro_bufs, 2 );

        SDL_PushGPUComputeUniformData( cmd, 0, &seen_push, sizeof( seen_push ) );
        SDL_DispatchGPUCompute( cp, g_seen, g_seen, static_cast<Uint32>( z_count ) );
        SDL_EndGPUComputePass( cp );
    }

    // [Pass 5] Copy: download lm and seen_cache results.
    {
        auto *const cp = SDL_BeginGPUCopyPass( cmd );

        {
            SDL_GPUBufferRegion const src_lm{ .buffer = lm_buf, .offset = 0, .size = out_bytes };
            SDL_GPUTransferBufferLocation const dst_lm{ .transfer_buffer = lm_dl_tbuf, .offset = 0 };
            SDL_DownloadFromGPUBuffer( cp, &src_lm, &dst_lm );
        }
        {
            SDL_GPUBufferRegion const src_seen{ .buffer = seen_buf, .offset = 0, .size = t_bytes };
            SDL_GPUTransferBufferLocation const dst_seen{ .transfer_buffer = seen_dl_tbuf, .offset = 0 };
            SDL_DownloadFromGPUBuffer( cp, &src_seen, &dst_seen );
        }

        SDL_EndGPUCopyPass( cp );
    }

    // Submit and wait for all GPU work to complete.
    auto *const fence = SDL_SubmitGPUCommandBufferAndAcquireFence( cmd );
    SDL_WaitForGPUFences( device, true, &fence, 1 );
    SDL_ReleaseGPUFence( device, fence );

    // ── Download results to CPU level_cache ──────────────────────────────────
    {
        // lm_all stores uint (bit-reinterpretation of positive floats).
        // Copying uint bytes directly into float storage is valid since the
        // bit pattern is preserved.
        auto const *lm_mapped =
            static_cast<uint32_t const *>( SDL_MapGPUTransferBuffer( device, lm_dl_tbuf, false ) );
        auto const *seen_mapped =
            static_cast<float const *>( SDL_MapGPUTransferBuffer( device, seen_dl_tbuf, false ) );

        for( int const z : *p.dirty_levels ) {
            // get_cache_ref is the public accessor; const_cast is safe because the
            // underlying level_cache is non-const — the const qualifier is only on
            // the return type of the accessor.
            auto &lc     = const_cast<level_cache &>( p.m->get_cache_ref( z ) );
            auto const zi = z + OVERMAP_DEPTH;
            auto const sz = static_cast<std::size_t>( cache_xy );

            auto const *lm_src   = lm_mapped   + zi * cache_xy;
            // Reinterpret uint bits as float values for lm.
            std::memcpy( lc.lm.data(), lm_src, sz * sizeof( float ) );
        }

        std::ranges::for_each( std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ),
        [&]( int const z ) {
            auto &lc      = const_cast<level_cache &>( p.m->get_cache_ref( z ) );
            auto const zi = z + OVERMAP_DEPTH;
            auto const sz = static_cast<std::size_t>( cache_xy );
            auto const *seen_src = seen_mapped + zi * cache_xy;
            std::ranges::copy( std::span{ seen_src, sz }, lc.seen_cache.begin() );
            lc.seen_cache_dirty = false;
        } );

        SDL_UnmapGPUTransferBuffer( device, lm_dl_tbuf   );
        SDL_UnmapGPUTransferBuffer( device, seen_dl_tbuf );
    }

    // ── Release all transient GPU resources ──────────────────────────────────
    SDL_ReleaseGPUTransferBuffer( device, upload_tbuf  );
    SDL_ReleaseGPUTransferBuffer( device, lm_dl_tbuf   );
    SDL_ReleaseGPUTransferBuffer( device, seen_dl_tbuf );
    SDL_ReleaseGPUBuffer( device, t_buf   );
    SDL_ReleaseGPUBuffer( device, f_buf   );
    SDL_ReleaseGPUBuffer( device, vf_buf  );
    SDL_ReleaseGPUBuffer( device, src_buf );
    SDL_ReleaseGPUBuffer( device, lm_buf  );
    SDL_ReleaseGPUBuffer( device, seen_raw_buf );
    SDL_ReleaseGPUBuffer( device, seen_buf );
    return true;
}

auto shutdown_lm() -> void
{
    auto *const device = get_device();
    if( device == nullptr ) {
        return;
    }
    if( s_ambient_pipeline != nullptr ) {
        SDL_ReleaseGPUComputePipeline( device, s_ambient_pipeline );
        s_ambient_pipeline = nullptr;
    }
    if( s_raytrace_pipeline != nullptr ) {
        SDL_ReleaseGPUComputePipeline( device, s_raytrace_pipeline );
        s_raytrace_pipeline = nullptr;
    }
    if( s_seen_pipeline != nullptr ) {
        SDL_ReleaseGPUComputePipeline( device, s_seen_pipeline );
        s_seen_pipeline = nullptr;
    }
    if( s_seen_walls_pipeline != nullptr ) {
        SDL_ReleaseGPUComputePipeline( device, s_seen_walls_pipeline );
        s_seen_walls_pipeline = nullptr;
    }
}

} // namespace cata_gpu
#endif // defined( CATA_SDL )

#if defined( CATA_SDL )
#include "gpu_transparency.h"
#include "gpu_platform.h"

#include "coordinates.h"
#include "debug.h"
#include "field.h"
#include "map.h"
#include "mapdata.h"
#include "path_info.h"
#include "submap.h"

#include <SDL3/SDL_gpu.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace cata_gpu
{

namespace
{

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

auto preferred_shader_format( SDL_GPUShaderFormat const fmts ) -> SDL_GPUShaderFormat
{
    if( fmts & SDL_GPU_SHADERFORMAT_DXIL  ) { return SDL_GPU_SHADERFORMAT_DXIL;  }
    if( fmts & SDL_GPU_SHADERFORMAT_SPIRV ) { return SDL_GPU_SHADERFORMAT_SPIRV; }
    if( fmts & SDL_GPU_SHADERFORMAT_MSL   ) { return SDL_GPU_SHADERFORMAT_MSL;   }
    return SDL_GPU_SHADERFORMAT_INVALID;
}

auto preferred_shader_ext( SDL_GPUShaderFormat const fmts ) -> std::string_view
{
    if( fmts & SDL_GPU_SHADERFORMAT_DXIL  ) { return ".dxil"; }
    if( fmts & SDL_GPU_SHADERFORMAT_SPIRV ) { return ".spv";  }
    if( fmts & SDL_GPU_SHADERFORMAT_MSL   ) { return ".msl";  }
    return {};
}

SDL_GPUComputePipeline *s_pipeline = nullptr;

auto ensure_pipeline( SDL_GPUDevice *const device ) -> SDL_GPUComputePipeline *
{
    if( s_pipeline != nullptr ) {
        return s_pipeline;
    }

    auto const fmts = SDL_GetGPUShaderFormats( device );
    auto const fmt  = preferred_shader_format( fmts );
    auto const ext  = preferred_shader_ext( fmts );

    if( fmt == SDL_GPU_SHADERFORMAT_INVALID || ext.empty() ) {
        DebugLog( DL::Error, DC::Main ) << "SDL_GPU: transparency: no supported shader format";
        return nullptr;
    }

    auto const path = PATH_INFO::shaders() + "transparency_compute" + std::string{ ext };
    auto const blob = read_blob( path );
    if( blob.empty() ) {
        DebugLog( DL::Error, DC::Main )
                << "SDL_GPU: transparency shader blob not found: " << path
                << " (build with shadercross to compile shaders)";
        return nullptr;
    }

    SDL_GPUComputePipelineCreateInfo const info{
        .code_size                    = blob.size(),
        .code                         = reinterpret_cast<Uint8 const *>( blob.data() ),
        .entrypoint                   = "main",
        .format                       = fmt,
        .num_samplers                 = 0,
        .num_readonly_storage_textures  = 0,
        .num_readonly_storage_buffers   = 3,  // submap_in, ter_lut, furn_lut
        .num_readwrite_storage_textures = 0,
        .num_readwrite_storage_buffers  = 1,  // transparency_out
        .num_uniform_buffers            = 1,  // push constants (slot 0)
        .threadcount_x                = 12,
        .threadcount_y                = 12,
        .threadcount_z                = 1,
        .props                        = 0,
    };

    s_pipeline = SDL_CreateGPUComputePipeline( device, &info );
    if( s_pipeline == nullptr ) {
        DebugLog( DL::Error, DC::Main )
                << "SDL_GPU: transparency pipeline creation failed: " << SDL_GetError();
    }
    return s_pipeline;
}

} // namespace

// ---------------------------------------------------------------------------

auto rebuild_transparency_luts( transparency_luts &luts ) -> void
{
    auto const &all_ter = ter_t::get_all();
    luts.ter_transparent.resize( all_ter.size() );
    std::ranges::transform( all_ter, luts.ter_transparent.begin(),
                            []( ter_t const &t ) -> uint32_t { return t.transparent ? 1u : 0u; } );

    auto const &all_furn = furn_t::get_all();
    luts.furn_transparent.resize( all_furn.size() );
    std::ranges::transform( all_furn, luts.furn_transparent.begin(),
                            []( furn_t const &f ) -> uint32_t { return f.transparent ? 1u : 0u; } );
}

auto gather_transparency_refs( map const &m, int const zlev )
-> std::vector<transparency_submap_ref>
{
    auto const &lc      = m.get_cache_ref( zlev );
    auto const  mapsize = lc.cache_mapsize;

    auto refs = std::vector<transparency_submap_ref>{};
    refs.reserve( static_cast<std::size_t>( mapsize * mapsize ) );

    for( int smx = 0; smx < mapsize; ++smx ) {
        for( int smy = 0; smy < mapsize; ++smy ) {
            auto const sm_pos = tripoint_bub_sm{ smx, smy, zlev };
            auto *const sm    = m.get_submap_at_grid( sm_pos );
            if( sm == nullptr ) {
                continue;
            }
            auto const sm_offset = project_to<coords::ms>( sm_pos );
            refs.push_back( { sm, sm_offset.x(), sm_offset.y() } );
        }
    }
    return refs;
}

auto prepare_transparency_inputs( std::span<transparency_submap_ref const> refs,
                                  std::vector<transparency_submap_in>      &out ) -> void
{
    out.clear();
    out.reserve( refs.size() );

    for( auto const &ref : refs ) {
        auto const *const sm  = ref.sm;
        auto             &rec = out.emplace_back();

        rec.cache_offset_x = ref.offset_x;
        rec.cache_offset_y = ref.offset_y;

        for( const auto p : submap_tiles() ) {
            auto const tile = static_cast<std::size_t>( p.x() * SEEY + p.y() );

            rec.ter_ids[tile]       = static_cast<uint32_t>( sm->get_ter( p ).to_i() );
            rec.furn_ids[tile]      = static_cast<uint32_t>( sm->get_furn( p ).to_i() );
            rec.outside_flags[tile] = sm->outside_cache[p.x()][p.y()] ? 1u : 0u;

            auto opacity = 1.0f;
            for( auto const &[ftype, fentry] : sm->get_field( p ) ) {
                if( !ftype.is_valid() ) {
                    break;
                }
                if( !fentry.is_transparent() ) {
                    opacity *= fentry.translucency();
                }
            }
            rec.field_opacity[tile] = opacity;
        }
    }
}

auto dispatch_transparency( SDL_GPUDevice                              *const device,
                             transparency_luts const                    &luts,
                             std::vector<transparency_submap_in> const  &submaps,
                             transparency_push_constants const           &push,
                             int const                                    cache_size,
                             std::vector<float>                          &out_buffer ) -> void
{
    if( submaps.empty() || cache_size <= 0 ) {
        return;
    }

    auto *const pipeline = ensure_pipeline( device );
    if( pipeline == nullptr ) {
        return;
    }

    auto const submap_bytes  = static_cast<Uint32>( submaps.size() * sizeof( transparency_submap_in ) );
    auto const ter_lut_bytes = static_cast<Uint32>( luts.ter_transparent.size() * sizeof( uint32_t ) );
    auto const fur_lut_bytes = static_cast<Uint32>( luts.furn_transparent.size() * sizeof( uint32_t ) );
    auto const output_bytes  = static_cast<Uint32>( cache_size * sizeof( float ) );

    if( ter_lut_bytes == 0 || fur_lut_bytes == 0 ) {
        DebugLog( DL::Error, DC::Main ) << "SDL_GPU: transparency LUTs are empty — call rebuild_transparency_luts first";
        return;
    }

    // --- Allocate GPU buffers ---
    SDL_GPUBufferCreateInfo const submap_buf_ci{
        .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, .size = submap_bytes, .props = 0
    };
    auto *const submap_buf  = SDL_CreateGPUBuffer( device, &submap_buf_ci );

    SDL_GPUBufferCreateInfo const ter_lut_buf_ci{
        .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, .size = ter_lut_bytes, .props = 0
    };
    auto *const ter_lut_buf = SDL_CreateGPUBuffer( device, &ter_lut_buf_ci );

    SDL_GPUBufferCreateInfo const fur_lut_buf_ci{
        .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ, .size = fur_lut_bytes, .props = 0
    };
    auto *const fur_lut_buf = SDL_CreateGPUBuffer( device, &fur_lut_buf_ci );

    SDL_GPUBufferCreateInfo const output_buf_ci{
        .usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE, .size = output_bytes, .props = 0
    };
    auto *const output_buf  = SDL_CreateGPUBuffer( device, &output_buf_ci );

    // Single upload transfer buffer covering all input data.
    auto const upload_bytes = submap_bytes + ter_lut_bytes + fur_lut_bytes;
    SDL_GPUTransferBufferCreateInfo const upload_tbuf_ci{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = upload_bytes, .props = 0
    };
    auto *const upload_tbuf   = SDL_CreateGPUTransferBuffer( device, &upload_tbuf_ci );

    SDL_GPUTransferBufferCreateInfo const download_tbuf_ci{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, .size = output_bytes, .props = 0
    };
    auto *const download_tbuf = SDL_CreateGPUTransferBuffer( device, &download_tbuf_ci );

    // --- Map upload buffer and copy input data ---
    {
        auto *const mapped = static_cast<std::byte *>( SDL_MapGPUTransferBuffer( device, upload_tbuf, false ) );
        auto offset = Uint32{ 0 };
        std::memcpy( mapped + offset, submaps.data(), submap_bytes );
        offset += submap_bytes;
        std::memcpy( mapped + offset, luts.ter_transparent.data(), ter_lut_bytes );
        offset += ter_lut_bytes;
        std::memcpy( mapped + offset, luts.furn_transparent.data(), fur_lut_bytes );
        SDL_UnmapGPUTransferBuffer( device, upload_tbuf );
    }

    auto *const cmd = SDL_AcquireGPUCommandBuffer( device );

    // --- Copy pass: upload all inputs ---
    {
        auto *const cp = SDL_BeginGPUCopyPass( cmd );

        SDL_GPUTransferBufferLocation const submap_src{ .transfer_buffer = upload_tbuf, .offset = 0 };
        SDL_GPUBufferRegion const submap_dst{ .buffer = submap_buf, .offset = 0, .size = submap_bytes };
        SDL_UploadToGPUBuffer( cp, &submap_src, &submap_dst, false );

        SDL_GPUTransferBufferLocation const ter_src{ .transfer_buffer = upload_tbuf, .offset = submap_bytes };
        SDL_GPUBufferRegion const ter_dst{ .buffer = ter_lut_buf, .offset = 0, .size = ter_lut_bytes };
        SDL_UploadToGPUBuffer( cp, &ter_src, &ter_dst, false );

        SDL_GPUTransferBufferLocation const fur_src{
            .transfer_buffer = upload_tbuf, .offset = submap_bytes + ter_lut_bytes
        };
        SDL_GPUBufferRegion const fur_dst{ .buffer = fur_lut_buf, .offset = 0, .size = fur_lut_bytes };
        SDL_UploadToGPUBuffer( cp, &fur_src, &fur_dst, false );

        SDL_EndGPUCopyPass( cp );
    }

    // --- Compute pass ---
    {
        SDL_GPUStorageBufferReadWriteBinding const rw_binding{
            .buffer = output_buf, .cycle = false, .padding1 = 0, .padding2 = 0, .padding3 = 0
        };
        auto *const cp = SDL_BeginGPUComputePass( cmd, nullptr, 0, &rw_binding, 1 );
        SDL_BindGPUComputePipeline( cp, pipeline );

        SDL_GPUBuffer *const ro_bufs[3] = { submap_buf, ter_lut_buf, fur_lut_buf };
        SDL_BindGPUComputeStorageBuffers( cp, 0, ro_bufs, 3 );

        // Uniform slot 0 → cbuffer Constants (register b0, space2) in HLSL.
        SDL_PushGPUComputeUniformData( cmd, 0, &push, sizeof( push ) );

        SDL_DispatchGPUCompute( cp, static_cast<Uint32>( submaps.size() ), 1, 1 );
        SDL_EndGPUComputePass( cp );
    }

    // --- Copy pass: download output ---
    {
        auto *const cp = SDL_BeginGPUCopyPass( cmd );
        SDL_GPUBufferRegion const output_src{ .buffer = output_buf, .offset = 0, .size = output_bytes };
        SDL_GPUTransferBufferLocation const output_dst{ .transfer_buffer = download_tbuf, .offset = 0 };
        SDL_DownloadFromGPUBuffer( cp, &output_src, &output_dst );
        SDL_EndGPUCopyPass( cp );
    }

    // --- Submit and wait ---
    auto *const fence = SDL_SubmitGPUCommandBufferAndAcquireFence( cmd );
    SDL_WaitForGPUFences( device, true, &fence, 1 );
    SDL_ReleaseGPUFence( device, fence );

    // --- Map download buffer and copy results out ---
    out_buffer.resize( static_cast<std::size_t>( cache_size ) );
    {
        auto const *const mapped = static_cast<float const *>(
                                       SDL_MapGPUTransferBuffer( device, download_tbuf, false ) );
        std::ranges::copy( std::span{ mapped, static_cast<std::size_t>( cache_size ) },
                           out_buffer.begin() );
        SDL_UnmapGPUTransferBuffer( device, download_tbuf );
    }

    // --- Release all temporary GPU resources ---
    SDL_ReleaseGPUTransferBuffer( device, upload_tbuf );
    SDL_ReleaseGPUTransferBuffer( device, download_tbuf );
    SDL_ReleaseGPUBuffer( device, submap_buf );
    SDL_ReleaseGPUBuffer( device, ter_lut_buf );
    SDL_ReleaseGPUBuffer( device, fur_lut_buf );
    SDL_ReleaseGPUBuffer( device, output_buf );
}

#if defined( CATA_GPU_VERIFY )

auto verify_transparency_against_cpu( map const &m, int const zlev,
                                      float const sight_penalty ) -> void
{
    auto *const device = get_device();
    if( device == nullptr ) {
        DebugLog( DL::Warn, DC::Main ) << "SDL_GPU: transparency verify skipped — no device";
        return;
    }

    auto const &lc         = m.get_cache_ref( zlev );
    auto const  cache_size = lc.cache_x * lc.cache_y;

    // LUTs are stable for the lifetime of a loaded world; rebuild on first call.
    static auto s_luts       = transparency_luts{};
    static auto s_luts_valid = false;
    if( !s_luts_valid ) {
        rebuild_transparency_luts( s_luts );
        s_luts_valid = true;
    }

    static auto s_inputs = std::vector<transparency_submap_in>{};
    auto const  refs     = gather_transparency_refs( m, zlev );
    prepare_transparency_inputs( refs, s_inputs );

    auto const push = transparency_push_constants{
        .sight_penalty = sight_penalty,
        .cache_y       = lc.cache_y,
        .num_submaps   = static_cast<uint32_t>( s_inputs.size() ),
        ._pad          = 0,
    };

    static auto s_gpu_result = std::vector<float>{};
    dispatch_transparency( device, s_luts, s_inputs, push, cache_size, s_gpu_result );

    if( s_gpu_result.empty() ) {
        DebugLog( DL::Error, DC::Main ) << "SDL_GPU: transparency verify: dispatch returned no data";
        return;
    }

    // Compare GPU output against each submap's local transparency_cache[][],
    // which is the direct output of rebuild_transparency_cache.  The flat
    // level cache (lc.transparency_cache) is NOT used here because it carries
    // stale values for non-dirty submaps when rebuild_all=false — those skipped
    // submaps haven't been rewritten to the flat cache, but the per-submap
    // arrays always reflect the last actual computation for that submap's terrain.
    static constexpr float threshold = 1e-5f;
    auto diff_count = 0;
    for( auto const &ref : refs ) {
        if( diff_count >= 20 ) {
            break;
        }
        for( const auto p : submap_tiles() ) {
            if( diff_count >= 20 ) {
                break;
            }
            auto const cx       = ref.offset_x + p.x();
            auto const cy       = ref.offset_y + p.y();
            auto const flat_idx = static_cast<std::size_t>( cx * push.cache_y + cy );
            auto const gpu_val  = s_gpu_result[flat_idx];
            auto const cpu_val  = ref.sm->transparency_cache[p.x()][p.y()];
            if( std::fabs( gpu_val - cpu_val ) > threshold ) {
                ++diff_count;
                DebugLog( DL::Warn, DC::Main )
                        << "SDL_GPU: transparency mismatch flat_idx=" << flat_idx
                        << " gpu=" << gpu_val << " cpu=" << cpu_val;
            }
        }
    }

    if( diff_count == 0 ) {
        DebugLog( DL::Info, DC::Main )
                << "SDL_GPU: transparency verify OK (zlev=" << zlev << ")";
    } else {
        DebugLog( DL::Warn, DC::Main )
                << "SDL_GPU: transparency verify FAILED zlev=" << zlev
                << " mismatches=" << diff_count;
    }
}

auto verify_transparency_readback( SDL_GPUDevice * /*device*/,
                                   float const   * /*cpu_reference*/,
                                   int             /*cache_size*/ ) -> void
{
    // Superseded by verify_transparency_against_cpu for Phase 6.
}

#endif // defined( CATA_GPU_VERIFY )

} // namespace cata_gpu
#endif // defined( CATA_SDL )

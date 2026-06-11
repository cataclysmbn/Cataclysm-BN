#include "slang_cpu_kernels.h"

#if defined( CATA_SDL )

#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_raytrace_cpu_main
#define _cpu_main cata_slang_lm_raytrace_cpu_main_entry
#define cpu_main_Group cata_slang_lm_raytrace_cpu_main_group
#define cpu_main_Thread cata_slang_lm_raytrace_cpu_main_thread
#include "lm_raytrace_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

#include "slang_cpu_dispatch.h"

namespace cata_compute::slang_cpu::kernels
{

namespace
{

#if defined( CATA_SLANG_CPU_GENERATED )
auto copy_light_source( cata_gpu::GpuLightSource const &source ) -> GpuLightSource_0
{
    auto result = GpuLightSource_0 {};
    result.x_0 = source.x;
    result.y_0 = source.y;
    result.z_idx_0 = source.z_idx;
    result.flags_0 = source.flags;
    result.luminance_0 = source.luminance;
    result.radius_0 = source.radius;
    result.dir_x_0 = source.dir_x;
    result.dir_y_0 = source.dir_y;
    result.cone_cos_0 = source.cone_cos;
    result.z_frac_0 = source.z_frac;
    return result;
}
#endif

auto bits_float( const uint32_t value ) -> float
{
    return std::bit_cast<float>( value );
}

} // namespace

auto raytrace( raytrace_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.transparency == nullptr || params.floor == nullptr ||
        params.vehicle_floor == nullptr || params.sources.empty() || params.lightmap == nullptr ||
        params.cache_x <= 0 || params.cache_y <= 0 || params.cache_xy <= 0 ||
        params.z_count <= 0 || params.num_sources == 0 ) {
        return false;
    }
    const auto source_offset = static_cast<std::size_t>( params.source_offset );
    const auto requested_sources = static_cast<std::size_t>( params.num_sources );
    if( source_offset > params.sources.size() ||
        requested_sources > params.sources.size() - source_offset ) {
        return false;
    }

    auto shader_sources = std::vector<GpuLightSource_0> {};
    shader_sources.reserve( params.sources.size() );
    for( const auto &source : params.sources ) {
        shader_sources.push_back( copy_light_source( source ) );
    }

    auto constants = RaytraceConstants_0 {};
    constants.cache_x_0 = params.cache_x;
    constants.cache_y_0 = params.cache_y;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_count_0 = params.z_count;
    constants.z_scale_0 = params.z_scale;
    constants.num_sources_0 = params.num_sources;
    constants.max_radius_0 = params.max_radius;
    constants.source_offset_0 = params.source_offset;

    const auto total_tiles = static_cast<uint32_t>( params.cache_xy * params.z_count );
    auto globals = GlobalParams_0 {};
    globals.transparency_all_0 = StructuredBuffer<float> {
        .data = const_cast<float *>( params.transparency ),
        .count = total_tiles,
    };
    globals.floor_all_0 = StructuredBuffer<uint32_t> {
        .data = const_cast<uint32_t *>( params.floor ),
        .count = total_tiles,
    };
    globals.vehicle_floor_all_0 = StructuredBuffer<uint32_t> {
        .data = const_cast<uint32_t *>( params.vehicle_floor ),
        .count = total_tiles,
    };
    globals.light_sources_0 = StructuredBuffer<GpuLightSource_0> {
        .data = shader_sources.data(),
        .count = static_cast<uint32_t>( shader_sources.size() ),
    };
    globals.lm_all_0 = RWStructuredBuffer<uint32_t> {
        .data = params.lightmap,
        .count = total_tiles,
    };
    globals.constants_0 = &constants;

    const auto group_side = static_cast<uint32_t>( params.max_radius * 2 + 1 + 7 ) / 8U;
    const auto ranges = make_accumulating_ranges( {
        .group_x = params.num_sources,
        .group_y = group_side,
        .group_z = group_side,
        .output_values = total_tiles,
    } );

    if( ranges.size() == 1 ) {
        dispatch_accumulating_chunks( ranges, [&]( const std::size_t, cpu_dispatch_range const &range ) {
            auto varying = make_varying( range );
            cata_slang_lm_raytrace_cpu_main( &varying, nullptr, &globals );
        } );
        return true;
    }

    auto scratch_buffers = make_zero_uint_buffers( ranges.size(), total_tiles );
    dispatch_accumulating_chunks( ranges, [&]( const std::size_t chunk_index,
                                    cpu_dispatch_range const &range ) {
        auto chunk_globals = globals;
        chunk_globals.lm_all_0 = RWStructuredBuffer<uint32_t> {
            .data = scratch_buffers[chunk_index].data(),
            .count = total_tiles,
        };
        auto varying = make_varying( range );
        cata_slang_lm_raytrace_cpu_main( &varying, nullptr, &chunk_globals );
    } );

    for( auto &scratch : scratch_buffers ) {
        if( !max_uint( {
            .target = params.lightmap,
            .source = scratch.data(),
            .count = total_tiles,
        } ) ) {
            return false;
        }
    }
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_raytrace_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto transparency = std::vector<float>( 9, 0.038376418216f );
    auto floor = std::vector<uint32_t>( 9, 0U );
    auto vehicle_floor = floor;
    auto sources = std::vector<cata_gpu::GpuLightSource> {
        {
            .x = 1,
            .y = 1,
            .z_idx = 0,
            .flags = 0U,
            .luminance = 100.0f,
            .radius = 2.0f,
            .dir_x = 0.0f,
            .dir_y = 1.0f,
            .cone_cos = 0.0f,
            .z_frac = 0.5f,
            ._pad = {},
        },
    };
    auto lm = std::vector<uint32_t>( 9, 0U );

    if( !raytrace( {
        .transparency = transparency.data(),
        .floor = floor.data(),
        .vehicle_floor = vehicle_floor.data(),
        .sources = sources,
        .lightmap = lm.data(),
        .cache_x = 3,
        .cache_y = 3,
        .cache_xy = 9,
        .z_count = 1,
        .z_scale = 3.0f,
        .source_offset = 0U,
        .num_sources = static_cast<uint32_t>( sources.size() ),
        .max_radius = 1,
    } ) ) {
        return false;
    }

    return bits_float( lm[4] ) > 99.0f && bits_float( lm[5] ) > 90.0f;
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

#endif // defined( CATA_SDL )

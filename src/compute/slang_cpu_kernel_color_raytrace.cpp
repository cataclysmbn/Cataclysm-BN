#include "slang_cpu_kernels.h"

#if defined( CATA_SDL )

#include <cstdint>
#include <vector>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_color_raytrace_cpu_main
#define _cpu_main cata_slang_lm_color_raytrace_cpu_main_entry
#define cpu_main_Group cata_slang_lm_color_raytrace_cpu_main_group
#define cpu_main_Thread cata_slang_lm_color_raytrace_cpu_main_thread
#include "lm_color_raytrace_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

#include "slang_cpu_dispatch.h"

namespace cata_compute::slang_cpu::kernels
{

auto color_raytrace( color_raytrace_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.transparency == nullptr || params.floor == nullptr ||
        params.vehicle_floor == nullptr || params.sources.empty() || params.color == nullptr ||
        params.cache_x <= 0 || params.cache_y <= 0 || params.cache_xy <= 0 ||
        params.z_count <= 0 || params.num_sources == 0 ) {
        return false;
    }
    if( !source_window_is_valid( params.sources, params.source_offset, params.num_sources ) ) {
        return false;
    }

    auto shader_sources = std::vector<GpuColoredLightSource_0> {};
    shader_sources.reserve( params.sources.size() );
    for( const auto &source : params.sources ) {
        shader_sources.push_back( copy_colored_light_source<GpuColoredLightSource_0>( source ) );
    }

    auto constants = ColorRaytraceConstants_0 {};
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
    globals.transparency_all_0 = readonly_buffer( params.transparency, total_tiles );
    globals.floor_all_0 = readonly_buffer( params.floor, total_tiles );
    globals.vehicle_floor_all_0 = readonly_buffer( params.vehicle_floor, total_tiles );
    globals.light_sources_0 = readonly_buffer( shader_sources.data(),
                               static_cast<uint32_t>( shader_sources.size() ) );
    globals.color_all_0 = writable_buffer( params.color, total_tiles );
    globals.constants_0 = &constants;

    return dispatch_accumulating_uint( {
        .group_x = params.num_sources,
        .group_y = ray_group_side( params.max_radius ),
        .group_z = ray_group_side( params.max_radius ),
        .output_values = total_tiles,
    }, globals, []( auto &chunk_globals, uint32_t *data, const uint32_t count ) {
        chunk_globals.color_all_0 = writable_buffer( data, count );
    }, []( auto &chunk_globals, cpu_dispatch_range const &range ) {
        dispatch_generated_kernel( cata_slang_lm_color_raytrace_cpu_main, chunk_globals, range );
    }, [&]( uint32_t *scratch, const uint32_t count ) {
        return max_uint( {
            .target = params.color,
            .source = scratch,
            .count = count,
        } );
    } );
#else
    ( void )params;
    return false;
#endif
}

auto run_color_raytrace_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto transparency = std::vector<float>( 9, 0.038376418216f );
    auto floor = std::vector<uint32_t>( 9, 0U );
    auto vehicle_floor = floor;
    auto sources = std::vector<cata_gpu::GpuColoredLightSource> {
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
            .color_rgb = 0x00ff0000U,
            ._pad = 0U,
        },
    };
    auto color = std::vector<uint32_t>( 9, 0U );

    if( !color_raytrace( {
        .transparency = transparency.data(),
        .floor = floor.data(),
        .vehicle_floor = vehicle_floor.data(),
        .sources = sources,
        .color = color.data(),
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

    return ( color[4] & 0x00ffffffU ) == 0x00ff0000U && ( color[4] >> 24 ) > 100U;
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

#endif // defined( CATA_SDL )

#include "slang_cpu_kernels.h"

#if defined( CATA_SDL )

#include <array>
#include <bit>
#include <cstdint>
#include <vector>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_vehicle_optics_cpu_main
#define _cpu_main cata_slang_lm_vehicle_optics_cpu_main_entry
#define cpu_main_Group cata_slang_lm_vehicle_optics_cpu_main_group
#define cpu_main_Thread cata_slang_lm_vehicle_optics_cpu_main_thread
#include "lm_vehicle_optics_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

namespace cata_compute::slang_cpu::kernels
{

namespace
{

#if defined( CATA_SLANG_CPU_GENERATED )
auto copy_vehicle_optic( cata_gpu::GpuVehicleOptic const &source ) -> GpuVehicleOptic_0
{
    auto result = GpuVehicleOptic_0 {};
    result.x_0 = source.x;
    result.y_0 = source.y;
    result.z_idx_0 = source.z_idx;
    result.kind_0 = source.kind;
    result.range_0 = source.range;
    result.offset_distance_0 = source.offset_distance;
    return result;
}
#endif

auto bits_float( const uint32_t value ) -> float
{
    return std::bit_cast<float>( value );
}

} // namespace

auto vehicle_optics( vehicle_optics_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.transparency == nullptr || params.seen == nullptr || params.optics.empty() ||
        params.camera == nullptr || params.cache_x <= 0 || params.cache_y <= 0 ||
        params.cache_xy <= 0 || params.z_count <= 0 || params.max_radius <= 0 ) {
        return false;
    }

    auto shader_optics = std::vector<GpuVehicleOptic_0> {};
    shader_optics.reserve( params.optics.size() );
    for( const auto &optic : params.optics ) {
        shader_optics.push_back( copy_vehicle_optic( optic ) );
    }

    auto constants = VehicleOpticsConstants_0 {};
    constants.cache_x_0 = params.cache_x;
    constants.cache_y_0 = params.cache_y;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_count_0 = params.z_count;
    constants.num_optics_0 = static_cast<uint32_t>( params.optics.size() );
    constants.max_radius_0 = params.max_radius;
    constants.trigdist_0 = params.trigdist;
    constants.visible_threshold_0 = params.visible_threshold;
    constants.max_view_distance_0 = params.max_view_distance;

    const auto total_tiles = static_cast<uint32_t>( params.cache_xy * params.z_count );
    auto globals = GlobalParams_0 {};
    globals.transparency_all_0 = StructuredBuffer<float> {
        .data = const_cast<float *>( params.transparency ),
        .count = total_tiles,
    };
    globals.seen_all_0 = StructuredBuffer<float> {
        .data = const_cast<float *>( params.seen ),
        .count = total_tiles,
    };
    globals.vehicle_optics_0 = StructuredBuffer<GpuVehicleOptic_0> {
        .data = shader_optics.data(),
        .count = static_cast<uint32_t>( shader_optics.size() ),
    };
    globals.camera_all_0 = RWStructuredBuffer<uint32_t> {
        .data = params.camera,
        .count = total_tiles,
    };
    globals.constants_0 = &constants;

    const auto group_side = static_cast<uint32_t>( params.max_radius * 2 + 1 + 7 ) / 8U;
    auto varying = ComputeVaryingInput {
        .startGroupID = uint3( 0U, 0U, 0U ),
        .endGroupID = uint3( static_cast<uint32_t>( params.optics.size() ), group_side, group_side ),
    };

    cata_slang_lm_vehicle_optics_cpu_main( &varying, nullptr, &globals );
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_vehicle_optics_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto transparency = std::array<float, 9> {};
    transparency.fill( 0.038376418216f );
    auto seen = std::array<float, 9> {};
    seen.fill( 1.0f );
    auto optics = std::vector<cata_gpu::GpuVehicleOptic> {
        {
            .x = 1,
            .y = 1,
            .z_idx = 0,
            .kind = cata_gpu::vehicle_optic_camera,
            .range = 1,
            .offset_distance = 0,
            ._pad = {},
        },
    };
    auto camera = std::array<uint32_t, 9> {};
    camera.fill( 0U );

    if( !vehicle_optics( {
        .transparency = transparency.data(),
        .seen = seen.data(),
        .optics = optics,
        .camera = camera.data(),
        .cache_x = 3,
        .cache_y = 3,
        .cache_xy = 9,
        .z_count = 1,
        .trigdist = 0U,
        .visible_threshold = 0.000398107193f,
        .max_view_distance = 60,
        .max_radius = 1,
    } ) ) {
        return false;
    }

    return bits_float( camera[4] ) == 1.0f && bits_float( camera[5] ) > 0.9f;
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

#endif // defined( CATA_SDL )

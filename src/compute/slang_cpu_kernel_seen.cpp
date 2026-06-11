#include "slang_cpu_kernels.h"

#include <array>
#include <cmath>
#include <cstdint>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_seen_cpu_main
#define _cpu_main cata_slang_lm_seen_cpu_main_entry
#define cpu_main_Group cata_slang_lm_seen_cpu_main_group
#define cpu_main_Thread cata_slang_lm_seen_cpu_main_thread
#include "lm_seen_compute.cpp"
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

auto close_enough( const float lhs, const float rhs ) -> bool
{
    return std::fabs( lhs - rhs ) < 0.0001f;
}

} // namespace

auto seen( seen_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.transparency == nullptr || params.floor == nullptr ||
        params.vehicle_floor == nullptr || params.vehicle_obscured == nullptr ||
        params.seen == nullptr || params.cache_x <= 0 || params.cache_y <= 0 ||
        params.cache_xy <= 0 || params.z_count <= 0 || params.dispatch_z_count <= 0 ) {
        return false;
    }

    auto constants = SeenConstants_0 {};
    constants.player_x_0 = params.player_x;
    constants.player_y_0 = params.player_y;
    constants.player_z_idx_0 = params.player_z_idx;
    constants.cache_x_0 = params.cache_x;
    constants.cache_y_0 = params.cache_y;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_count_0 = params.z_count;
    constants.view_radius_0 = params.view_radius;
    constants.z_scale_0 = params.z_scale;
    constants.z_start_idx_0 = params.z_start_idx;
    constants.dispatch_z_count_0 = params.dispatch_z_count;
    constants.trigdist_0 = params.trigdist;
    constants.vision_block_mask_0 = params.vision_block_mask;

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
    globals.vehicle_obscured_all_0 = StructuredBuffer<uint32_t> {
        .data = const_cast<uint32_t *>( params.vehicle_obscured ),
        .count = total_tiles,
    };
    globals.seen_all_0 = RWStructuredBuffer<float> {
        .data = params.seen,
        .count = total_tiles,
    };
    globals.constants_0 = &constants;

    const auto group_count = static_cast<uint32_t>( params.view_radius * 2 + 1 + 7 ) / 8U;
    dispatch_independent( {
        .group_x = group_count,
        .group_y = group_count,
        .group_z = static_cast<uint32_t>( params.dispatch_z_count ),
    }, [&]( cpu_dispatch_range const &range ) {
        auto varying = make_varying( range );
        cata_slang_lm_seen_cpu_main( &varying, nullptr, &globals );
    } );
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_seen_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto transparency = std::array<float, 9> {};
    transparency.fill( 0.038376418216f );
    auto floor = std::array<uint32_t, 9> {};
    floor.fill( 0U );
    auto vehicle_floor = floor;
    auto vehicle_obscured = floor;
    auto seen_values = std::array<float, 9> {};
    seen_values.fill( 0.0f );

    if( !seen( {
        .transparency = transparency.data(),
        .floor = floor.data(),
        .vehicle_floor = vehicle_floor.data(),
        .vehicle_obscured = vehicle_obscured.data(),
        .seen = seen_values.data(),
        .player_x = 1,
        .player_y = 1,
        .player_z_idx = 0,
        .cache_x = 3,
        .cache_y = 3,
        .cache_xy = 9,
        .z_count = 1,
        .view_radius = 1,
        .z_scale = 3.0f,
        .z_start_idx = 0,
        .dispatch_z_count = 1,
        .trigdist = 0U,
        .vision_block_mask = 0U,
    } ) ) {
        return false;
    }

    return close_enough( seen_values[4], 1.0f ) && seen_values[5] > 0.9f;
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

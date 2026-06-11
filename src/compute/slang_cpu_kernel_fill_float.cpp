#include "slang_cpu_kernels.h"

#include <array>
#include <cstdint>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_fill_float_cpu_main
#define _cpu_main cata_slang_lm_fill_float_cpu_main_entry
#define cpu_main_Group cata_slang_lm_fill_float_cpu_main_group
#define cpu_main_Thread cata_slang_lm_fill_float_cpu_main_thread
#include "lm_fill_float_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

#include "slang_cpu_dispatch.h"

namespace cata_compute::slang_cpu::kernels
{

auto fill_float( fill_float_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.target == nullptr || params.cache_xy <= 0 || params.total_tiles == 0 ) {
        return false;
    }

    auto constants = FillFloatConstants_0 {};
    constants.total_tiles_0 = params.total_tiles;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_start_idx_0 = params.z_start_idx;
    constants.value_0 = params.value;

    auto globals = GlobalParams_0 {};
    globals.target_values_0 = writable_buffer( params.target,
                               static_cast<uint32_t>( params.z_start_idx * params.cache_xy ) +
                               params.total_tiles );
    globals.constants_0 = &constants;

    dispatch_independent_kernel( {
        .group_x = tile_groups( params.total_tiles ),
    }, globals, cata_slang_lm_fill_float_cpu_main );
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_fill_float_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto values = std::array<float, 6> {};
    if( !fill_float( {
        .target = values.data(),
        .total_tiles = 2U,
        .cache_xy = 3,
        .z_start_idx = 1,
        .value = 8.0f,
    } ) ) {
        return false;
    }

    return values == std::array<float, 6> { 0.0f, 0.0f, 0.0f, 8.0f, 8.0f, 0.0f };
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

#include "slang_cpu_kernels.h"

#include <array>
#include <cstdint>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_shift_uint_cpu_main
#define _cpu_main cata_slang_lm_shift_uint_cpu_main_entry
#define cpu_main_Group cata_slang_lm_shift_uint_cpu_main_group
#define cpu_main_Thread cata_slang_lm_shift_uint_cpu_main_thread
#include "lm_shift_uint_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

#include "slang_cpu_dispatch.h"

namespace cata_compute::slang_cpu::kernels
{

auto shift_uint( shift_uint_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.target == nullptr || params.source == nullptr || params.cache_x <= 0 ||
        params.cache_y <= 0 || params.cache_xy <= 0 || params.z_count <= 0 ) {
        return false;
    }

    auto constants = ShiftUintConstants_0 {};
    constants.cache_x_0 = params.cache_x;
    constants.cache_y_0 = params.cache_y;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_count_0 = params.z_count;
    constants.shift_x_tiles_0 = params.shift_x_tiles;
    constants.shift_y_tiles_0 = params.shift_y_tiles;
    constants.fill_value_0 = params.fill_value;

    const auto total_tiles = static_cast<uint32_t>( params.z_count * params.cache_xy );
    auto globals = GlobalParams_0 {};
    globals.src_all_0 = readonly_buffer( params.source, total_tiles );
    globals.dst_all_0 = writable_buffer( params.target, total_tiles );
    globals.constants_0 = &constants;

    dispatch_independent_kernel( {
        .group_x = tile_groups( total_tiles ),
    }, globals, cata_slang_lm_shift_uint_cpu_main );
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_shift_uint_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto source = std::array<uint32_t, 6> { 1U, 2U, 3U, 4U, 5U, 6U };
    auto target = std::array<uint32_t, 6> {};
    if( !shift_uint( {
        .target = target.data(),
        .source = source.data(),
        .cache_x = 3,
        .cache_y = 2,
        .cache_xy = 6,
        .z_count = 1,
        .shift_x_tiles = 1,
        .shift_y_tiles = 0,
        .fill_value = 99U,
    } ) ) {
        return false;
    }

    return target == std::array<uint32_t, 6> { 3U, 4U, 5U, 6U, 99U, 99U };
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

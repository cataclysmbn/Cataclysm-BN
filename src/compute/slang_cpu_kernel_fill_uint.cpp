#include "slang_cpu_kernels.h"

#include <algorithm>
#include <array>
#include <cstdint>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_fill_uint_cpu_main
#define _cpu_main cata_slang_lm_fill_uint_cpu_main_entry
#define cpu_main_Group cata_slang_lm_fill_uint_cpu_main_group
#define cpu_main_Thread cata_slang_lm_fill_uint_cpu_main_thread
#include "lm_fill_uint_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

#include "slang_cpu_dispatch.h"

namespace cata_compute::slang_cpu::kernels
{

auto fill_uint( fill_uint_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.target == nullptr ) {
        return false;
    }

    auto constants = FillUintConstants_0 {};
    constants.total_tiles_0 = params.count;
    constants.value_0 = params.value;

    auto globals = GlobalParams_0 {};
    globals.target_values_0 = writable_buffer( params.target, params.count );
    globals.constants_0 = &constants;

    dispatch_independent_kernel( {
        .group_x = tile_groups( params.count ),
    }, globals, cata_slang_lm_fill_uint_cpu_main );
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_fill_uint_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    static constexpr auto probe_value = uint32_t{ 37U };
    auto values = std::array<uint32_t, 9> {};
    if( !fill_uint( {
        .target = values.data(),
        .count = static_cast<uint32_t>( values.size() ),
        .value = probe_value,
    } ) ) {
        return false;
    }
    return std::ranges::all_of( values, []( const uint32_t value ) {
        return value == probe_value;
    } );
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

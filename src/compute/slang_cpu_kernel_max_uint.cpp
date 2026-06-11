#include "slang_cpu_kernels.h"

#include <array>
#include <cstdint>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_max_uint_cpu_main
#define _cpu_main cata_slang_lm_max_uint_cpu_main_entry
#define cpu_main_Group cata_slang_lm_max_uint_cpu_main_group
#define cpu_main_Thread cata_slang_lm_max_uint_cpu_main_thread
#include "lm_max_uint_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

#include "slang_cpu_dispatch.h"

namespace cata_compute::slang_cpu::kernels
{

auto max_uint( max_uint_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.target == nullptr || params.source == nullptr || params.count == 0 ) {
        return false;
    }

    auto constants = MaxUintConstants_0 {};
    constants.total_tiles_0 = params.count;

    auto globals = GlobalParams_0 {};
    globals.source_values_0 = readonly_buffer( params.source, params.count );
    globals.target_values_0 = writable_buffer( params.target, params.count );
    globals.constants_0 = &constants;

    dispatch_independent_kernel( {
        .group_x = tile_groups( params.count ),
    }, globals, cata_slang_lm_max_uint_cpu_main );
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_max_uint_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto source = std::array<uint32_t, 4> { 1U, 9U, 3U, 8U };
    auto target = std::array<uint32_t, 4> { 2U, 4U, 7U, 1U };
    if( !max_uint( {
        .target = target.data(),
        .source = source.data(),
        .count = static_cast<uint32_t>( target.size() ),
    } ) ) {
        return false;
    }

    return target == std::array<uint32_t, 4> { 2U, 9U, 7U, 8U };
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

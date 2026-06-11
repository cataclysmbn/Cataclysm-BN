#include "slang_cpu_kernels.h"

#if defined( CATA_SDL )

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <vector>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_sight_pairs_cpu_main
#define _cpu_main cata_slang_lm_sight_pairs_cpu_main_entry
#define cpu_main_Group cata_slang_lm_sight_pairs_cpu_main_group
#define cpu_main_Thread cata_slang_lm_sight_pairs_cpu_main_thread
#include "lm_sight_pairs_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

#include "slang_cpu_dispatch.h"

namespace cata_compute::slang_cpu::kernels
{

auto sight_pairs( sight_pairs_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.transparency == nullptr || params.floor == nullptr || params.pairs.empty() ||
        params.results == nullptr || params.cache_x <= 0 || params.cache_y <= 0 ||
        params.cache_xy <= 0 || params.z_count <= 0 ) {
        return false;
    }

    auto shader_pairs = std::vector<GpuSightPair_0> {};
    shader_pairs.reserve( params.pairs.size() );
    std::ranges::transform( params.pairs, std::back_inserter( shader_pairs ),
                             []( cata_gpu::GpuSightPair const &pair ) {
        return copy_sight_pair<GpuSightPair_0>( pair );
    } );

    params.results->assign( params.pairs.size(), 0U );

    auto constants = SightPairsConstants_0 {};
    constants.cache_x_0 = params.cache_x;
    constants.cache_y_0 = params.cache_y;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_count_0 = params.z_count;
    constants.pair_count_0 = static_cast<uint32_t>( params.pairs.size() );

    const auto total_tiles = static_cast<uint32_t>( params.cache_xy * params.z_count );
    auto globals = GlobalParams_0 {};
    globals.transparency_all_0 = readonly_buffer( params.transparency, total_tiles );
    globals.floor_all_0 = readonly_buffer( params.floor, total_tiles );
    globals.pairs_0 = readonly_buffer( shader_pairs.data(),
                                       static_cast<uint32_t>( shader_pairs.size() ) );
    globals.results_0 = writable_buffer( params.results->data(),
                                         static_cast<uint32_t>( params.results->size() ) );
    globals.constants_0 = &constants;

    dispatch_independent_kernel( {
        .group_x = tile_groups( static_cast<uint32_t>( params.pairs.size() ) ),
    }, globals, cata_slang_lm_sight_pairs_cpu_main );
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_sight_pairs_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto transparency = std::vector<float>( 9, 0.038376418216f );
    auto floor = std::vector<uint32_t>( 9, 0U );
    auto pairs = std::vector<cata_gpu::GpuSightPair> {
        {
            .from_x = 0,
            .from_y = 0,
            .from_z_idx = 0,
            .to_x = 2,
            .to_y = 0,
            .to_z_idx = 0,
            .range = 10,
            ._pad = 0U,
        },
        {
            .from_x = 0,
            .from_y = 0,
            .from_z_idx = 0,
            .to_x = 2,
            .to_y = 2,
            .to_z_idx = 0,
            .range = 10,
            ._pad = 0U,
        },
    };
    transparency[4] = 0.0f;

    auto results = std::vector<uint32_t> {};
    if( !sight_pairs( {
        .transparency = transparency.data(),
        .floor = floor.data(),
        .pairs = pairs,
        .results = &results,
        .cache_x = 3,
        .cache_y = 3,
        .cache_xy = 9,
        .z_count = 1,
    } ) ) {
        return false;
    }

    return results.size() == 2 && results[0] == 1U && results[1] == 0U;
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

#endif // defined( CATA_SDL )

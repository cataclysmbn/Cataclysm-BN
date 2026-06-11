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

namespace
{

#if defined( CATA_SLANG_CPU_GENERATED )
auto copy_sight_pair( cata_gpu::GpuSightPair const &source ) -> GpuSightPair_0
{
    auto result = GpuSightPair_0 {};
    result.from_x_0 = source.from_x;
    result.from_y_0 = source.from_y;
    result.from_z_idx_0 = source.from_z_idx;
    result.to_x_0 = source.to_x;
    result.to_y_0 = source.to_y;
    result.to_z_idx_0 = source.to_z_idx;
    result.range_0 = source.range;
    return result;
}
#endif

} // namespace

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
    std::ranges::transform( params.pairs, std::back_inserter( shader_pairs ), copy_sight_pair );

    params.results->assign( params.pairs.size(), 0U );

    auto constants = SightPairsConstants_0 {};
    constants.cache_x_0 = params.cache_x;
    constants.cache_y_0 = params.cache_y;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_count_0 = params.z_count;
    constants.pair_count_0 = static_cast<uint32_t>( params.pairs.size() );

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
    globals.pairs_0 = StructuredBuffer<GpuSightPair_0> {
        .data = shader_pairs.data(),
        .count = static_cast<uint32_t>( shader_pairs.size() ),
    };
    globals.results_0 = RWStructuredBuffer<uint32_t> {
        .data = params.results->data(),
        .count = static_cast<uint32_t>( params.results->size() ),
    };
    globals.constants_0 = &constants;

    dispatch_independent( {
        .group_x = ( static_cast<uint32_t>( params.pairs.size() ) + 63U ) / 64U,
    }, [&]( cpu_dispatch_range const &range ) {
        auto varying = make_varying( range );
        cata_slang_lm_sight_pairs_cpu_main( &varying, nullptr, &globals );
    } );
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

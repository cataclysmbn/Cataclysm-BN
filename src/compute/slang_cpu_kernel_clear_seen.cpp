#include "slang_cpu_kernels.h"

#include <array>
#include <cstdint>
#include <ranges>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_clear_seen_cpu_main
#define _cpu_main cata_slang_lm_clear_seen_cpu_main_entry
#define cpu_main_Group cata_slang_lm_clear_seen_cpu_main_group
#define cpu_main_Thread cata_slang_lm_clear_seen_cpu_main_thread
#include "lm_clear_seen_compute.cpp"
#undef cpu_main_Thread
#undef cpu_main_Group
#undef _cpu_main
#undef cpu_main
#endif

namespace cata_compute::slang_cpu::kernels
{

auto clear_seen( clear_seen_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.seen_raw == nullptr || params.seen == nullptr || params.total_tiles == 0 ||
        params.cache_xy <= 0 ) {
        return false;
    }

    auto constants = ClearSeenConstants_0 {};
    constants.total_tiles_0 = params.total_tiles;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_start_idx_0 = params.z_start_idx;

    const auto output_count =
        static_cast<uint32_t>( params.z_start_idx * params.cache_xy ) + params.total_tiles;
    auto globals = GlobalParams_0 {};
    globals.seen_raw_all_0 = RWStructuredBuffer<float> {
        .data = params.seen_raw,
        .count = output_count,
    };
    globals.seen_all_0 = RWStructuredBuffer<float> {
        .data = params.seen,
        .count = output_count,
    };
    globals.constants_0 = &constants;

    auto varying = ComputeVaryingInput {
        .startGroupID = uint3( 0U, 0U, 0U ),
        .endGroupID = uint3( ( params.total_tiles + 63U ) / 64U, 1U, 1U ),
    };

    cata_slang_lm_clear_seen_cpu_main( &varying, nullptr, &globals );
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_clear_seen_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto seen_raw = std::array<float, 6> { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    auto seen = seen_raw;
    if( !clear_seen( {
        .seen_raw = seen_raw.data(),
        .seen = seen.data(),
        .total_tiles = 2U,
        .cache_xy = 3,
        .z_start_idx = 1,
    } ) ) {
        return false;
    }

    return seen_raw == std::array<float, 6> { 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f } &&
           seen == std::array<float, 6> { 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f };
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

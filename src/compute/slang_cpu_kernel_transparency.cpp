#include "slang_cpu_kernels.h"

#if defined( CATA_SDL )

#include "gpu_transparency.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <vector>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_transparency_cpu_main
#define _cpu_main cata_slang_transparency_cpu_main_entry
#define cpu_main_Group cata_slang_transparency_cpu_main_group
#define cpu_main_Thread cata_slang_transparency_cpu_main_thread
#include "transparency_compute.cpp"
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

static constexpr auto transparency_tile_count = SEEX * SEEY;
static constexpr auto light_transparency_open_air = 0.038376418216f;

#if defined( CATA_SLANG_CPU_GENERATED )
auto copy_transparency_submap( cata_gpu::transparency_submap_in const &source )
-> TransparencySubmapIn_0
{
    auto result = TransparencySubmapIn_0 {};
    for( const auto tile : std::views::iota( 0, transparency_tile_count ) ) {
        result.ter_ids_0[tile] = source.ter_ids[tile];
        result.furn_ids_0[tile] = source.furn_ids[tile];
        result.field_opacity_0[tile] = source.field_opacity[tile];
        result.outside_flags_0[tile] = source.outside_flags[tile];
    }
    result.cache_offset_x_0 = source.cache_offset_x;
    result.cache_offset_y_0 = source.cache_offset_y;
    result.submap_output_offset_0 = source.output_offset;
    return result;
}

auto full_output_required_count( transparency_params const &params ) -> std::size_t
{
    auto required_count = std::size_t { 0 };
    for( const auto &submap : params.submaps ) {
        const auto max_x = submap.cache_offset_x + SEEX - 1;
        const auto max_y = submap.cache_offset_y + SEEY - 1;
        if( max_x < 0 || max_y < 0 || params.push->cache_y <= 0 ) {
            continue;
        }
        const auto last_index = params.push->output_offset + submap.output_offset +
                                static_cast<uint32_t>( max_x * params.push->cache_y + max_y );
        required_count = std::max( required_count, static_cast<std::size_t>( last_index + 1U ) );
    }
    return required_count;
}
#endif

auto approximately_equal( const float lhs, const float rhs ) -> bool
{
    return std::fabs( lhs - rhs ) < 0.00001f;
}

} // namespace

auto dispatch_transparency( transparency_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.luts == nullptr || params.submaps.empty() || params.push == nullptr ||
        params.compact_output == nullptr || params.full_output == nullptr ||
        params.luts->ter_transparent.empty() || params.luts->furn_transparent.empty() ) {
        return false;
    }

    auto shader_submaps = std::vector<TransparencySubmapIn_0> {};
    shader_submaps.reserve( params.submaps.size() );
    std::ranges::transform( params.submaps, std::back_inserter( shader_submaps ),
                            copy_transparency_submap );

    params.compact_output->assign( params.submaps.size() * transparency_tile_count, 0.0f );
    params.full_output->resize( std::max( params.full_output->size(),
                                          full_output_required_count( params ) ) );

    auto constants = TransparencyConstants_0 {};
    constants.sight_penalty_0 = params.push->sight_penalty;
    constants.cache_y_0 = params.push->cache_y;
    constants.num_submaps_0 = static_cast<uint32_t>( params.submaps.size() );
    constants.output_offset_0 = params.push->output_offset;

    auto globals = GlobalParams_0 {};
    globals.submap_in_0 = StructuredBuffer<TransparencySubmapIn_0> {
        .data = shader_submaps.data(),
        .count = static_cast<uint32_t>( shader_submaps.size() ),
    };
    globals.ter_lut_0 = StructuredBuffer<uint32_t> {
        .data = const_cast<uint32_t *>( params.luts->ter_transparent.data() ),
        .count = static_cast<uint32_t>( params.luts->ter_transparent.size() ),
    };
    globals.furn_lut_0 = StructuredBuffer<uint32_t> {
        .data = const_cast<uint32_t *>( params.luts->furn_transparent.data() ),
        .count = static_cast<uint32_t>( params.luts->furn_transparent.size() ),
    };
    globals.compact_transparency_out_0 = RWStructuredBuffer<float> {
        .data = params.compact_output->data(),
        .count = static_cast<uint32_t>( params.compact_output->size() ),
    };
    globals.full_transparency_out_0 = RWStructuredBuffer<float> {
        .data = params.full_output->data(),
        .count = static_cast<uint32_t>( params.full_output->size() ),
    };
    globals.constants_0 = &constants;

    dispatch_independent( {
        .group_x = static_cast<uint32_t>( params.submaps.size() ),
    }, [&]( cpu_dispatch_range const &range ) {
        auto varying = make_varying( range );
        cata_slang_transparency_cpu_main( &varying, nullptr, &globals );
    } );
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_transparency_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto luts = cata_gpu::transparency_luts {
        .ter_transparent = { 1U, 0U },
        .furn_transparent = { 1U, 0U },
    };

    auto submaps = std::vector<cata_gpu::transparency_submap_in>( 1 );
    auto &submap = submaps.front();
    for( const auto tile : std::views::iota( 0, transparency_tile_count ) ) {
        submap.ter_ids[tile] = 0U;
        submap.furn_ids[tile] = 0U;
        submap.field_opacity[tile] = 1.0f;
        submap.outside_flags[tile] = 0U;
    }
    submap.ter_ids[1] = 1U;
    submap.field_opacity[2] = 0.25f;
    submap.outside_flags[0] = 1U;

    auto push = cata_gpu::transparency_push_constants {
        .sight_penalty = 0.5f,
        .cache_y = SEEY,
        .num_submaps = static_cast<uint32_t>( submaps.size() ),
        .output_offset = 0U,
    };
    auto compact_output = std::vector<float> {};
    auto full_output = std::vector<float> {};

    if( !dispatch_transparency( {
        .luts = &luts,
        .submaps = submaps,
        .push = &push,
        .compact_output = &compact_output,
        .full_output = &full_output,
    } ) ) {
        return false;
    }

    return compact_output.size() == transparency_tile_count &&
           full_output.size() == transparency_tile_count &&
           approximately_equal( compact_output[0], light_transparency_open_air * 0.5f ) &&
           approximately_equal( compact_output[1], 0.0f ) &&
           approximately_equal( compact_output[2], light_transparency_open_air * 0.25f ) &&
           approximately_equal( full_output[0], compact_output[0] ) &&
           approximately_equal( full_output[1], compact_output[1] ) &&
           approximately_equal( full_output[2], compact_output[2] );
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

#endif // defined( CATA_SDL )

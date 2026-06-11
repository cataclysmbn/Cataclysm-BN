#include "slang_cpu_kernels.h"

#include <array>
#include <bit>
#include <cstdint>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_daylight_diffuse_cpu_main
#define _cpu_main cata_slang_lm_daylight_diffuse_cpu_main_entry
#define cpu_main_Group cata_slang_lm_daylight_diffuse_cpu_main_group
#define cpu_main_Thread cata_slang_lm_daylight_diffuse_cpu_main_thread
#include "lm_daylight_diffuse_compute.cpp"
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

auto float_bits( const float value ) -> uint32_t
{
    return std::bit_cast<uint32_t>( value );
}

auto bits_float( const uint32_t value ) -> float
{
    return std::bit_cast<float>( value );
}

} // namespace

auto daylight_diffuse( daylight_diffuse_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.daylight_seed == nullptr || params.daylight_src == nullptr ||
        params.transparency == nullptr || params.daylight_dst == nullptr ||
        params.lightmap == nullptr || params.total_tiles == 0 || params.cache_x <= 0 ||
        params.cache_y <= 0 || params.cache_xy <= 0 || params.z_count <= 0 ) {
        return false;
    }

    auto constants = DaylightDiffuseConstants_0 {};
    constants.total_tiles_0 = params.total_tiles;
    constants.cache_x_0 = params.cache_x;
    constants.cache_y_0 = params.cache_y;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_count_0 = params.z_count;
    constants.diffuse_decay_0 = params.diffuse_decay;
    constants.min_light_0 = params.min_light;

    auto globals = GlobalParams_0 {};
    globals.daylight_seed_all_0 = StructuredBuffer<uint32_t> {
        .data = const_cast<uint32_t *>( params.daylight_seed ),
        .count = params.total_tiles,
    };
    globals.daylight_src_all_0 = StructuredBuffer<uint32_t> {
        .data = const_cast<uint32_t *>( params.daylight_src ),
        .count = params.total_tiles,
    };
    globals.transparency_all_0 = StructuredBuffer<float> {
        .data = const_cast<float *>( params.transparency ),
        .count = params.total_tiles,
    };
    globals.daylight_dst_all_0 = RWStructuredBuffer<uint32_t> {
        .data = params.daylight_dst,
        .count = params.total_tiles,
    };
    globals.lm_all_0 = RWStructuredBuffer<uint32_t> {
        .data = params.lightmap,
        .count = params.total_tiles,
    };
    globals.constants_0 = &constants;

    dispatch_independent( {
        .group_x = ( params.total_tiles + 63U ) / 64U,
    }, [&]( cpu_dispatch_range const &range ) {
        auto varying = make_varying( range );
        cata_slang_lm_daylight_diffuse_cpu_main( &varying, nullptr, &globals );
    } );
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_daylight_diffuse_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto seed = std::array<uint32_t, 9> {};
    seed.fill( 0U );
    auto source = std::array<uint32_t, 9> {};
    source.fill( 0U );
    source[4] = float_bits( 10.0f );
    auto transparency = std::array<float, 9> {};
    transparency.fill( 0.038376418216f );
    auto dst = std::array<uint32_t, 9> {};
    dst.fill( 0U );
    auto lm = std::array<uint32_t, 9> {};
    lm.fill( 0U );

    if( !daylight_diffuse( {
        .daylight_seed = seed.data(),
        .daylight_src = source.data(),
        .transparency = transparency.data(),
        .daylight_dst = dst.data(),
        .lightmap = lm.data(),
        .total_tiles = static_cast<uint32_t>( dst.size() ),
        .cache_x = 3,
        .cache_y = 3,
        .cache_xy = 9,
        .z_count = 1,
        .diffuse_decay = 0.9f,
        .min_light = 3.5f,
    } ) ) {
        return false;
    }

    return bits_float( dst[5] ) > 8.9f && bits_float( lm[5] ) > 8.9f;
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

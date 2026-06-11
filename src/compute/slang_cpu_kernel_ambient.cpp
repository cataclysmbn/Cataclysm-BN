#include "slang_cpu_kernels.h"

#include <array>
#include <bit>
#include <cstdint>
#include <ranges>

#if defined( CATA_SLANG_CPU_GENERATED )
#define cpu_main cata_slang_lm_ambient_cpu_main
#define _cpu_main cata_slang_lm_ambient_cpu_main_entry
#define cpu_main_Group cata_slang_lm_ambient_cpu_main_group
#define cpu_main_Thread cata_slang_lm_ambient_cpu_main_thread
#include "lm_ambient_compute.cpp"
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

auto ambient( ambient_params const &params ) -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    if( params.floor == nullptr || params.transparency == nullptr || params.source_map == nullptr ||
        params.vehicle_floor == nullptr || params.lightmap == nullptr ||
        params.daylight_seed == nullptr || params.cache_x <= 0 || params.cache_y <= 0 ||
        params.cache_xy <= 0 || params.z_count <= 0 ) {
        return false;
    }

    auto constants = AmbientConstants_0 {};
    constants.inside_light_0 = params.inside_light;
    constants.cache_x_0 = params.cache_x;
    constants.cache_y_0 = params.cache_y;
    constants.cache_xy_0 = params.cache_xy;
    constants.z_count_0 = params.z_count;
    constants.overmap_depth_0 = params.overmap_depth;
    constants.angled_sunlight_shadows_0 = params.angled_sunlight_shadows;
    constants.direct_sunlight_0 = params.direct_sunlight;
    constants.sun_dx_per_z_0 = params.sun_dx_per_z;
    constants.sun_dy_per_z_0 = params.sun_dy_per_z;
    constants.solar_shadow_light_0 = params.solar_shadow_light;
    for( const auto row : std::views::iota( std::size_t{ 0 }, std::size_t{ 6 } ) ) {
        for( const auto column : std::views::iota( std::size_t{ 0 }, std::size_t{ 4 } ) ) {
            constants.natural_light_0[row][column] = params.natural_light[row][column];
        }
    }

    const auto total_tiles = static_cast<uint32_t>( params.cache_xy * params.z_count );
    auto globals = GlobalParams_0 {};
    globals.floor_all_0 = readonly_buffer( params.floor, total_tiles );
    globals.transparency_all_0 = readonly_buffer( params.transparency, total_tiles );
    globals.source_map_all_0 = readonly_buffer( params.source_map, total_tiles );
    globals.vehicle_floor_all_0 = readonly_buffer( params.vehicle_floor, total_tiles );
    globals.lm_all_0 = writable_buffer( params.lightmap, total_tiles );
    globals.daylight_seed_all_0 = writable_buffer( params.daylight_seed, total_tiles );
    globals.constants_0 = &constants;

    dispatch_independent_kernel( {
        .group_x = tile_groups( total_tiles ),
    }, globals, cata_slang_lm_ambient_cpu_main );
    return true;
#else
    ( void )params;
    return false;
#endif
}

auto run_ambient_probe() -> bool
{
#if defined( CATA_SLANG_CPU_GENERATED )
    auto floor = std::array<uint32_t, 9> {};
    floor.fill( 0U );
    auto transparency = std::array<float, 9> {};
    transparency.fill( 0.038376418216f );
    auto source_map = std::array<float, 9> {};
    source_map.fill( 0.0f );
    auto vehicle_floor = std::array<uint32_t, 9> {};
    vehicle_floor.fill( 0U );
    auto lightmap = std::array<uint32_t, 9> {};
    lightmap.fill( 0U );
    auto daylight_seed = std::array<uint32_t, 9> {};
    daylight_seed.fill( 0U );
    auto params = ambient_params {
        .floor = floor.data(),
        .transparency = transparency.data(),
        .source_map = source_map.data(),
        .vehicle_floor = vehicle_floor.data(),
        .lightmap = lightmap.data(),
        .daylight_seed = daylight_seed.data(),
        .inside_light = 3.5f,
        .cache_x = 3,
        .cache_y = 3,
        .cache_xy = 9,
        .z_count = 1,
        .overmap_depth = 0,
        .angled_sunlight_shadows = 0U,
        .direct_sunlight = 1U,
        .sun_dx_per_z = 0.0f,
        .sun_dy_per_z = 0.0f,
        .solar_shadow_light = 5.0f,
    };
    params.natural_light[0][0] = 42.0f;

    if( !ambient( params ) ) {
        return false;
    }

    return bits_float( lightmap[4] ) == 42.0f && daylight_seed[4] == float_bits( 42.0f );
#else
    return false;
#endif
}

} // namespace cata_compute::slang_cpu::kernels

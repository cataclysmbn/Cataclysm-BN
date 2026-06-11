#include "slang_cpu_backend.h"

#include "cached_options.h"
#include "game.h"
#include "game_constants.h"
#include "lightmap.h"
#include "map.h"
#include "profile.h"
#include "shadowcasting.h"
#include "slang_cpu_kernels.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace cata_compute::slang_cpu
{

namespace
{

auto s_initialized = false;
auto s_probe_ok = false;

#if defined( CATA_SDL )
static constexpr auto solar_shadow_scatter = 0.09f;
static constexpr auto daylight_diffusion_passes = 16;

struct lighting_residency {
    int cache_x = 0;
    int cache_y = 0;
    int z_count = 0;
    uint64_t id = 0;
    std::vector<float> transparency = {};
    std::vector<uint32_t> floor = {};
    std::vector<uint32_t> vehicle_floor = {};
    std::vector<uint32_t> vehicle_obscured = {};
    std::vector<float> source_map = {};
    std::vector<uint32_t> lightmap = {};
    std::vector<uint32_t> daylight_seed = {};
    std::vector<uint32_t> daylight_diffuse_a = {};
    std::vector<uint32_t> daylight_diffuse_b = {};
    std::vector<float> seen_raw = {};
    std::vector<float> seen = {};
    std::vector<uint32_t> camera = {};
    std::vector<uint32_t> visibility = {};
    std::vector<uint32_t> colored_light = {};
    std::vector<char> transparency_valid_levels = {};
    bool floor_valid = false;
    bool vehicle_floor_valid = false;
    bool vehicle_obscured_valid = false;
    bool source_map_valid = false;
    bool lighting_outputs_valid = false;
    std::vector<char> seen_valid_levels = {};
    bool seen_origin_valid = false;
    int seen_origin_x = 0;
    int seen_origin_y = 0;
};

auto s_lighting = lighting_residency {};
auto s_next_residency_id = uint64_t { 1 };

struct pending_visibility_work {
    bool active = false;
    uint64_t id = 0;
};

struct pending_lighting_work {
    bool active = false;
    uint64_t id = 0;
};

auto s_pending_lighting = pending_lighting_work {};

struct pending_sight_pairs_work {
    bool active = false;
    uint64_t id = 0;
    map const *m = nullptr;
    std::vector<sight_pair> pairs = {};
    int zlev = 0;
};

auto s_pending_visibility = pending_visibility_work {};
auto s_pending_sight_pairs = pending_sight_pairs_work {};
auto s_next_work_id = uint64_t { 1 };

auto next_work_id() -> uint64_t
{
    auto result = s_next_work_id++;
    if( s_next_work_id == 0 ) {
        s_next_work_id = 1;
    }
    if( result == 0 ) {
        result = s_next_work_id++;
    }
    return result;
}

auto z_to_resident_index( const int zlev ) -> int
{
    return zlev + OVERMAP_DEPTH;
}

auto resident_index_is_valid( const int idx ) -> bool
{
    return idx >= 0 && idx < s_lighting.z_count;
}

auto volume_tiles( const int cache_x, const int cache_y, const int z_count ) -> std::size_t
{
    return static_cast<std::size_t>( cache_x ) * static_cast<std::size_t>( cache_y ) *
           static_cast<std::size_t>( z_count );
}

auto reset_seen_residency() -> void
{
    s_lighting.seen_valid_levels.assign( static_cast<std::size_t>( s_lighting.z_count ), '\0' );
    s_lighting.seen_origin_valid = false;
}

auto reset_residency_for_shape( const int cache_x, const int cache_y, const int z_count ) -> void
{
    if( s_lighting.cache_x == cache_x && s_lighting.cache_y == cache_y &&
        s_lighting.z_count == z_count && !s_lighting.transparency.empty() ) {
        return;
    }

    s_lighting = {};
    s_lighting.cache_x = cache_x;
    s_lighting.cache_y = cache_y;
    s_lighting.z_count = z_count;
    ++s_next_residency_id;
    if( s_next_residency_id == 0 ) {
        s_next_residency_id = 1;
    }
    s_lighting.id = s_next_residency_id;

    const auto tiles = volume_tiles( cache_x, cache_y, z_count );
    s_lighting.transparency.assign( tiles, LIGHT_TRANSPARENCY_OPEN_AIR );
    s_lighting.floor.assign( tiles, 0U );
    s_lighting.vehicle_floor.assign( tiles, 0U );
    s_lighting.vehicle_obscured.assign( tiles, 0U );
    s_lighting.source_map.assign( tiles, 0.0f );
    s_lighting.lightmap.assign( tiles, 0U );
    s_lighting.daylight_seed.assign( tiles, 0U );
    s_lighting.daylight_diffuse_a.assign( tiles, 0U );
    s_lighting.daylight_diffuse_b.assign( tiles, 0U );
    s_lighting.seen_raw.assign( tiles, 0.0f );
    s_lighting.seen.assign( tiles, 0.0f );
    s_lighting.camera.assign( tiles, 0U );
    s_lighting.visibility.assign( tiles, static_cast<uint32_t>( lit_level::BLANK ) );
    s_lighting.colored_light.assign( tiles, 0U );
    s_lighting.transparency_valid_levels.assign( static_cast<std::size_t>( z_count ), '\0' );
    reset_seen_residency();
}

auto refresh_transparency_valid() -> bool
{
    return !s_lighting.transparency_valid_levels.empty() &&
           std::ranges::all_of( s_lighting.transparency_valid_levels, []( const char value ) {
        return value != '\0';
    } );
}

auto transparency_level_is_valid( const int zlev ) -> bool
{
    const auto z_idx = z_to_resident_index( zlev );
    return resident_index_is_valid( z_idx ) &&
           s_lighting.transparency_valid_levels[static_cast<std::size_t>( z_idx )] != '\0';
}

auto seen_levels_valid( std::vector<int> const &levels, const int player_x, const int player_y )
-> bool
{
    if( !s_lighting.seen_origin_valid || s_lighting.seen_origin_x != player_x ||
        s_lighting.seen_origin_y != player_y ) {
        return false;
    }
    return std::ranges::all_of( levels, []( const int zlev ) {
        const auto z_idx = z_to_resident_index( zlev );
        return resident_index_is_valid( z_idx ) &&
               s_lighting.seen_valid_levels[static_cast<std::size_t>( z_idx )] != '\0';
    } );
}

auto mark_seen_levels_valid( std::vector<int> const &levels, const int player_x, const int player_y,
                             const bool force_invalidate ) -> void
{
    if( force_invalidate || !s_lighting.seen_origin_valid ||
        s_lighting.seen_origin_x != player_x || s_lighting.seen_origin_y != player_y ) {
        reset_seen_residency();
    }
    for( const auto zlev : levels ) {
        const auto z_idx = z_to_resident_index( zlev );
        if( resident_index_is_valid( z_idx ) ) {
            s_lighting.seen_valid_levels[static_cast<std::size_t>( z_idx )] = '\1';
        }
    }
    s_lighting.seen_origin_valid = true;
    s_lighting.seen_origin_x = player_x;
    s_lighting.seen_origin_y = player_y;
}

auto all_levels() -> std::vector<int>
{
    auto result = std::vector<int> {};
    result.reserve( static_cast<std::size_t>( OVERMAP_LAYERS ) );
    std::ranges::copy( std::views::iota( -OVERMAP_DEPTH, OVERMAP_HEIGHT + 1 ),
                       std::back_inserter( result ) );
    return result;
}

auto sorted_unique( std::vector<int> levels ) -> std::vector<int>
{
    std::ranges::sort( levels );
    levels.erase( std::ranges::unique( levels ).begin(), levels.end() );
    return levels;
}

struct dispatch_range {
    int z_start_idx = 0;
    int z_count = 0;
};

auto make_dispatch_range( std::vector<int> const &levels ) -> dispatch_range
{
    if( levels.empty() ) {
        return {
            .z_start_idx = 0,
            .z_count = s_lighting.z_count,
        };
    }
    auto contiguous = true;
    auto previous = levels.front();
    for( const auto zlev : levels | std::views::drop( 1 ) ) {
        if( zlev != previous + 1 ) {
            contiguous = false;
            break;
        }
        previous = zlev;
    }
    if( !contiguous ) {
        return {
            .z_start_idx = 0,
            .z_count = s_lighting.z_count,
        };
    }
    return {
        .z_start_idx = levels.front() + OVERMAP_DEPTH,
        .z_count = static_cast<int>( levels.size() ),
    };
}

auto copy_visibility_to_level_cache( map const &m, std::vector<int> const &levels ) -> void
{
    for( const auto zlev : levels ) {
        const auto z_idx = z_to_resident_index( zlev );
        if( !resident_index_is_valid( z_idx ) ) {
            continue;
        }
        auto &lc = const_cast<level_cache &>( m.get_cache_ref( zlev ) );
        const auto src = std::span {
            s_lighting.visibility.data() + static_cast<std::size_t>( z_idx ) *
            static_cast<std::size_t>( s_lighting.cache_x * s_lighting.cache_y ),
            static_cast<std::size_t>( s_lighting.cache_x * s_lighting.cache_y )
        };
        std::ranges::transform( src, lc.visibility_cache.begin(), []( const uint32_t value ) {
            return static_cast<lit_level>( value );
        } );
        lc.visibility_cache_dirty = false;
    }
}

auto sight_pair_levels( std::vector<sight_pair> const &pairs ) -> std::vector<int>
{
    auto result = std::vector<int> {};
    result.reserve( pairs.size() );
    for( const auto &pair : pairs ) {
        const auto min_z_idx = std::min( pair.from_z_idx, pair.to_z_idx );
        const auto max_z_idx = std::max( pair.from_z_idx, pair.to_z_idx );
        std::ranges::copy(
            std::views::iota( min_z_idx, max_z_idx + 1 ) |
            std::views::filter( []( const int z_idx ) {
            return z_idx >= 0 && z_idx < OVERMAP_LAYERS;
        } ) |
        std::views::transform( []( const int z_idx ) {
            return z_idx - OVERMAP_DEPTH;
        } ),
        std::back_inserter( result ) );
    }
    return sorted_unique( std::move( result ) );
}

auto level_offset( const int zlev ) -> std::size_t
{
    return static_cast<std::size_t>( z_to_resident_index( zlev ) ) *
           static_cast<std::size_t>( s_lighting.cache_x * s_lighting.cache_y );
}

auto float_bits( const float value ) -> uint32_t
{
    return std::bit_cast<uint32_t>( value );
}

auto daylight_diffusion_decay_for_pass( const int pass ) -> float
{
    const auto air_decay = std::exp( -LIGHT_TRANSPARENCY_OPEN_AIR );
    if( pass == 0 ) {
        return air_decay;
    }
    const auto distance = static_cast<float>( pass + 1 );
    return air_decay * ( distance - 1.0f ) / distance;
}

struct choose_resident_level_update_params {
    bool dirty = false;
    bool resident_valid = false;
    std::vector<int> const *dirty_levels = nullptr;
    std::vector<int> const *all_levels = nullptr;
};

auto choose_resident_level_update( choose_resident_level_update_params const &p ) -> std::vector<int>
{
    if( p.all_levels == nullptr ) {
        return {};
    }
    if( !p.dirty && p.resident_valid ) {
        return {};
    }
    if( !p.resident_valid || p.dirty_levels == nullptr || p.dirty_levels->empty() ) {
        return *p.all_levels;
    }
    return sorted_unique( *p.dirty_levels );
}

auto choose_transparency_update_levels(
    const bool dirty, std::vector<int> const *dirty_levels,
    std::vector<int> const &all_levels ) -> std::vector<int>
{
    auto result = std::vector<int> {};
    for( const auto zlev : all_levels ) {
        if( !transparency_level_is_valid( zlev ) ) {
            result.push_back( zlev );
        }
    }
    if( dirty ) {
        const auto &source = dirty_levels == nullptr || dirty_levels->empty() ? all_levels : *dirty_levels;
        std::ranges::copy( source, std::back_inserter( result ) );
    }
    return sorted_unique( std::move( result ) );
}

auto copy_transparency_levels( map const &m, std::vector<int> const &levels,
                               std::vector<float> &target ) -> void
{
    for( const auto zlev : levels ) {
        const auto &lc = m.get_cache_ref( zlev );
        const auto offset = level_offset( zlev );
        std::ranges::copy( lc.transparency_cache,
                           target.begin() + static_cast<std::ptrdiff_t>( offset ) );
        const auto z_idx = z_to_resident_index( zlev );
        if( resident_index_is_valid( z_idx ) ) {
            s_lighting.transparency_valid_levels[static_cast<std::size_t>( z_idx )] = '\1';
        }
    }
}

auto copy_source_map_levels( map const &m, std::vector<int> const &levels,
                             std::vector<float> &target ) -> void
{
    for( const auto zlev : levels ) {
        const auto &lc = m.get_cache_ref( zlev );
        const auto offset = level_offset( zlev );
        std::ranges::copy( lc.sm, target.begin() + static_cast<std::ptrdiff_t>( offset ) );
    }
}

auto copy_floor_levels( map const &m, std::vector<int> const &levels,
                        std::vector<uint32_t> &target ) -> void
{
    for( const auto zlev : levels ) {
        const auto &lc = m.get_cache_ref( zlev );
        const auto offset = level_offset( zlev );
        std::ranges::transform( lc.floor_cache,
                                target.begin() + static_cast<std::ptrdiff_t>( offset ),
        []( const char value ) -> uint32_t {
            return value != '\0' ? 1U : 0U;
        } );
    }
}

auto copy_vehicle_floor_levels( map const &m, std::vector<int> const &levels,
                                std::vector<uint32_t> &target ) -> void
{
    for( const auto zlev : levels ) {
        const auto &lc = m.get_cache_ref( zlev );
        const auto offset = level_offset( zlev );
        std::ranges::transform( lc.vehicle_floor_cache,
                                target.begin() + static_cast<std::ptrdiff_t>( offset ),
        []( const char value ) -> uint32_t {
            return value != '\0' ? 1U : 0U;
        } );
    }
}

auto copy_vehicle_obscured_levels( map const &m, std::vector<int> const &levels,
                                   std::vector<uint32_t> &target ) -> void
{
    for( const auto zlev : levels ) {
        const auto &lc = m.get_cache_ref( zlev );
        const auto offset = level_offset( zlev );
        std::ranges::transform( lc.vehicle_obscured_cache,
                                target.begin() + static_cast<std::ptrdiff_t>( offset ),
        []( const diagonal_blocks &value ) -> uint32_t {
            return ( value.nw ? 1U : 0U ) | ( value.ne ? 2U : 0U );
        } );
    }
}

auto copy_lightmap_to_level_caches( map const &m, std::vector<int> const &levels ) -> void
{
    for( const auto zlev : levels ) {
        auto &lc = const_cast<level_cache &>( m.get_cache_ref( zlev ) );
        const auto offset = level_offset( zlev );
        const auto count = static_cast<std::size_t>( s_lighting.cache_x * s_lighting.cache_y );
        std::memcpy( lc.lm.data(), s_lighting.lightmap.data() + offset, count * sizeof( float ) );
        lc.lm_cpu_cache_valid = true;
    }
}

auto clear_colored_light_levels( map const &m, std::vector<int> const &levels ) -> void
{
    for( const auto zlev : levels ) {
        auto &lc = const_cast<level_cache &>( m.get_cache_ref( zlev ) );
        std::ranges::fill( lc.colored_light_cache, 0U );
        lc.colored_light_cache_active = false;
    }
}

auto copy_colored_light_to_level_caches( map const &m, std::vector<int> const &levels ) -> void
{
    for( const auto zlev : levels ) {
        auto &lc = const_cast<level_cache &>( m.get_cache_ref( zlev ) );
        const auto offset = level_offset( zlev );
        const auto src = std::span {
            s_lighting.colored_light.data() + offset,
            static_cast<std::size_t>( s_lighting.cache_x * s_lighting.cache_y )
        };
        std::ranges::copy( src, lc.colored_light_cache.begin() );
        lc.colored_light_cache_active = std::ranges::any_of( src, []( const uint32_t value ) {
            return value != 0U;
        } );
    }
}

auto max_light_radius( std::span<cata_gpu::GpuLightSource const> sources ) -> int
{
    auto result = 0;
    for( const auto &source : sources ) {
        result = std::max( result, static_cast<int>( std::ceil( source.radius ) ) );
    }
    return result;
}

auto max_colored_light_radius( std::span<cata_gpu::GpuColoredLightSource const> sources ) -> int
{
    auto result = 0;
    for( const auto &source : sources ) {
        result = std::max( result, static_cast<int>( std::ceil( source.radius ) ) );
    }
    return result;
}

auto max_optic_radius( std::span<cata_gpu::GpuVehicleOptic const> optics ) -> int
{
    auto result = 0;
    for( const auto &optic : optics ) {
        result = std::max( result, optic.range );
    }
    return result;
}

auto make_ambient_params( const int cache_x, const int cache_y, const int cache_xy )
-> kernels::ambient_params
{
    auto params = kernels::ambient_params {
        .floor = s_lighting.floor.data(),
        .transparency = s_lighting.transparency.data(),
        .source_map = s_lighting.source_map.data(),
        .vehicle_floor = s_lighting.vehicle_floor.data(),
        .lightmap = s_lighting.lightmap.data(),
        .daylight_seed = s_lighting.daylight_seed.data(),
        .inside_light = LIGHT_AMBIENT_LOW,
        .cache_x = cache_x,
        .cache_y = cache_y,
        .cache_xy = cache_xy,
        .z_count = OVERMAP_LAYERS,
        .overmap_depth = OVERMAP_DEPTH,
        .solar_shadow_light = std::max( static_cast<float>( LIGHT_AMBIENT_LOW ),
                                        std::min( g->natural_light_level( 0 ),
                                                static_cast<float>( default_daylight_level() ) *
                                                solar_shadow_scatter ) ),
    };
    for( const auto z_idx : std::views::iota( 0, OVERMAP_LAYERS ) ) {
        params.natural_light[z_idx / 4][z_idx % 4] =
            g->natural_light_level( z_idx - OVERMAP_DEPTH );
    }
    return params;
}
#endif

} // namespace

auto init() -> void
{
    s_initialized = true;
    s_probe_ok = kernels::run_startup_probes();
}

auto shutdown() -> void
{
    s_initialized = false;
    s_probe_ok = false;
#if defined( CATA_SDL )
    s_lighting = {};
    s_pending_lighting = {};
    s_pending_visibility = {};
    s_pending_sight_pairs = {};
#endif
}

auto status() -> backend_status
{
    const auto available = s_initialized && kernels::generated_kernels_available();
    const auto detail =
        !s_initialized ? "Slang CPU backend is not initialized" :
        !kernels::generated_kernels_available() ? "Slang CPU backend was built without generated kernels" :
        !s_probe_ok ? "Slang CPU generated kernel probe failed" :
        "Slang CPU generated kernel probe passed";

    return {
        .kind = backend_kind::slang_cpu,
        .available = available,
        .supports_lighting = available,
        .supports_visibility = available,
        .supports_transparency = available,
        .supports_sight_pairs = available,
        .name = "slang_cpu",
        .detail = detail,
    };
}

#if defined( CATA_SDL )
auto resident_lighting_ready_for_visibility( resident_lighting_ready_params const &p ) -> bool
{
    if( p.cache_x <= 0 || p.cache_y <= 0 || p.z_count <= 0 ) {
        return false;
    }
    reset_residency_for_shape( p.cache_x, p.cache_y, p.z_count );
    return s_lighting.floor_valid && s_lighting.vehicle_floor_valid &&
           s_lighting.vehicle_obscured_valid && s_lighting.source_map_valid &&
           s_lighting.lighting_outputs_valid && refresh_transparency_valid();
}

auto resident_lighting_ready_for_sight_pairs( resident_sight_pair_inputs_params const &p )
-> bool
{
    if( p.m == nullptr || p.pairs == nullptr || p.pairs->empty() ) {
        return false;
    }
    const auto &lc = p.m->get_cache_ref( p.zlev );
    reset_residency_for_shape( lc.cache_x, lc.cache_y, OVERMAP_LAYERS );
    const auto levels = sight_pair_levels( *p.pairs );
    return s_lighting.floor_valid &&
           std::ranges::all_of( levels, []( const int zlev ) {
        return transparency_level_is_valid( zlev );
    } );
}

auto prepare_lighting_transparency_output( prepare_lighting_transparency_output_params const &p )
-> resident_transparency_output
{
    if( p.cache_x <= 0 || p.cache_y <= 0 || p.z_count <= 0 ) {
        return {};
    }

    reset_residency_for_shape( p.cache_x, p.cache_y, p.z_count );
    const auto z_idx = z_to_resident_index( p.zlev );
    if( !resident_index_is_valid( z_idx ) ) {
        return {};
    }

    const auto cache_xy = static_cast<uint32_t>( p.cache_x * p.cache_y );
    return {
        .backend = backend_kind::slang_cpu,
        .id = s_lighting.id,
        .output_offset = static_cast<uint32_t>( z_idx ) * cache_xy,
    };
}

auto mark_lighting_transparency_level_updated( const int zlev ) -> void
{
    const auto z_idx = z_to_resident_index( zlev );
    if( resident_index_is_valid( z_idx ) ) {
        s_lighting.transparency_valid_levels[static_cast<std::size_t>( z_idx )] = '\1';
    }
}

auto lighting_transparency_level_is_valid( const int zlev ) -> bool
{
    const auto z_idx = z_to_resident_index( zlev );
    return resident_index_is_valid( z_idx ) &&
           s_lighting.transparency_valid_levels[static_cast<std::size_t>( z_idx )] != '\0';
}

auto invalidate_lighting_transparency_levels( std::vector<int> const &levels ) -> void
{
    for( const auto zlev : levels ) {
        const auto z_idx = z_to_resident_index( zlev );
        if( resident_index_is_valid( z_idx ) ) {
            s_lighting.transparency_valid_levels[static_cast<std::size_t>( z_idx )] = '\0';
        }
    }
}

auto shift_lighting_resident_inputs( shift_lighting_residency_params const &p ) -> bool
{
    if( p.cache_x <= 0 || p.cache_y <= 0 || p.z_count <= 0 ) {
        return false;
    }
    reset_residency_for_shape( p.cache_x, p.cache_y, p.z_count );

    const auto shift_x_tiles = p.shift_x_submaps * SEEX;
    const auto shift_y_tiles = p.shift_y_submaps * SEEY;
    if( shift_x_tiles == 0 && shift_y_tiles == 0 ) {
        return true;
    }

    const auto cache_xy = p.cache_x * p.cache_y;
    auto shifted_float = std::vector<float>( s_lighting.transparency.size(), LIGHT_TRANSPARENCY_OPEN_AIR );
    auto shifted_uint = std::vector<uint32_t>( s_lighting.floor.size(), 0U );
    if( !kernels::shift_float( {
        .target = shifted_float.data(),
        .source = s_lighting.transparency.data(),
        .cache_x = p.cache_x,
        .cache_y = p.cache_y,
        .cache_xy = cache_xy,
        .z_count = p.z_count,
        .shift_x_tiles = shift_x_tiles,
        .shift_y_tiles = shift_y_tiles,
        .fill_value = LIGHT_TRANSPARENCY_OPEN_AIR,
    } ) ) {
        return false;
    }
    s_lighting.transparency.swap( shifted_float );

    const auto shift_uint_volume = [&]( std::vector<uint32_t> &values ) -> bool {
        if( !kernels::shift_uint( {
            .target = shifted_uint.data(),
            .source = values.data(),
            .cache_x = p.cache_x,
            .cache_y = p.cache_y,
            .cache_xy = cache_xy,
            .z_count = p.z_count,
            .shift_x_tiles = shift_x_tiles,
            .shift_y_tiles = shift_y_tiles,
            .fill_value = 0U,
        } ) ) {
            return false;
        }
        values.swap( shifted_uint );
        std::ranges::fill( shifted_uint, 0U );
        return true;
    };
    if( s_lighting.floor_valid && !shift_uint_volume( s_lighting.floor ) ) {
        return false;
    }
    if( s_lighting.vehicle_floor_valid && !shift_uint_volume( s_lighting.vehicle_floor ) ) {
        return false;
    }
    if( s_lighting.vehicle_obscured_valid && !shift_uint_volume( s_lighting.vehicle_obscured ) ) {
        return false;
    }

    reset_seen_residency();
    s_lighting.source_map_valid = false;
    s_lighting.lighting_outputs_valid = false;
    return true;
}

auto dispatch_transparency( dispatch_transparency_params const &p ) -> bool
{
    if( p.submaps == nullptr || p.out_buffer == nullptr || p.output.backend != backend_kind::slang_cpu ||
        p.output.id != s_lighting.id ) {
        return false;
    }

    auto push = p.push;
    push.output_offset = p.output_offset;
    return kernels::dispatch_transparency( {
        .luts = p.luts,
        .submaps = *p.submaps,
        .push = &push,
        .compact_output = p.out_buffer,
        .full_output = &s_lighting.transparency,
    } );
}

auto run_lighting( lighting_params const &p ) -> bool
{
    ZoneScopedN( "slang_cpu_run_lighting" );
    if( p.m == nullptr || p.dirty_levels == nullptr ) {
        return false;
    }

    auto lightmap_levels = sorted_unique( *p.dirty_levels );
    if( p.rebuild_seen_cache || p.download_seen_cache ) {
        return false;
    }
    if( lightmap_levels.empty() ) {
        return true;
    }

    const auto &lc0 = p.m->get_cache_ref( p.player_zlev );
    const auto cache_x = lc0.cache_x;
    const auto cache_y = lc0.cache_y;
    const auto cache_xy = cache_x * cache_y;
    reset_residency_for_shape( cache_x, cache_y, OVERMAP_LAYERS );
    const auto levels = all_levels();

    auto transparency_levels = choose_transparency_update_levels(
        p.transparency_dirty, p.transparency_dirty_levels, levels );
    const auto floor_levels = choose_resident_level_update( {
        .dirty = p.floor_dirty,
        .resident_valid = s_lighting.floor_valid,
        .dirty_levels = p.floor_dirty_levels,
        .all_levels = &levels,
    } );
    const auto vehicle_floor_levels = choose_resident_level_update( {
        .dirty = p.vehicle_floor_dirty,
        .resident_valid = s_lighting.vehicle_floor_valid,
        .dirty_levels = p.vehicle_floor_dirty_levels,
        .all_levels = &levels,
    } );
    const auto vehicle_obscured_levels = choose_resident_level_update( {
        .dirty = p.vehicle_obscured_dirty,
        .resident_valid = s_lighting.vehicle_obscured_valid,
        .dirty_levels = p.vehicle_obscured_dirty_levels,
        .all_levels = &levels,
    } );

    if( !transparency_levels.empty() ) {
        copy_transparency_levels( *p.m, transparency_levels, s_lighting.transparency );
    }
    if( !floor_levels.empty() ) {
        copy_floor_levels( *p.m, floor_levels, s_lighting.floor );
        s_lighting.floor_valid = true;
    }
    if( !vehicle_floor_levels.empty() ) {
        copy_vehicle_floor_levels( *p.m, vehicle_floor_levels, s_lighting.vehicle_floor );
        s_lighting.vehicle_floor_valid = true;
    }
    if( !vehicle_obscured_levels.empty() ) {
        copy_vehicle_obscured_levels( *p.m, vehicle_obscured_levels,
                                      s_lighting.vehicle_obscured );
        s_lighting.vehicle_obscured_valid = true;
    }

    if( !refresh_transparency_valid() || !s_lighting.floor_valid ||
        !s_lighting.vehicle_floor_valid || !s_lighting.vehicle_obscured_valid ) {
        return false;
    }

    if( !lightmap_levels.empty() ) {
        auto source_collection = cata_gpu::collect_lighting_sources( {
            .m = p.m,
            .levels = &levels,
            .collect_colored_sources = colored_lighting,
        } );
        copy_source_map_levels( *p.m, levels, s_lighting.source_map );

        auto ambient = make_ambient_params( cache_x, cache_y, cache_xy );
        ambient.angled_sunlight_shadows = p.angled_sunlight_shadows ? 1U : 0U;
        ambient.direct_sunlight = p.direct_sunlight ? 1U : 0U;
        ambient.sun_dx_per_z = p.sun_dx_per_z;
        ambient.sun_dy_per_z = p.sun_dy_per_z;
        if( !kernels::ambient( ambient ) ) {
            return false;
        }

        const auto total_tiles = static_cast<uint32_t>( s_lighting.lightmap.size() );
        if( p.direct_sunlight ) {
            auto *source = s_lighting.daylight_seed.data();
            auto *target = s_lighting.daylight_diffuse_a.data();
            for( const auto pass : std::views::iota( 0, daylight_diffusion_passes ) ) {
                if( !kernels::daylight_diffuse( {
                    .daylight_seed = s_lighting.daylight_seed.data(),
                    .daylight_src = source,
                    .transparency = s_lighting.transparency.data(),
                    .daylight_dst = target,
                    .lightmap = s_lighting.lightmap.data(),
                    .total_tiles = total_tiles,
                    .cache_x = cache_x,
                    .cache_y = cache_y,
                    .cache_xy = cache_xy,
                    .z_count = OVERMAP_LAYERS,
                    .diffuse_decay = daylight_diffusion_decay_for_pass( pass ),
                    .min_light = LIGHT_AMBIENT_LOW,
                } ) ) {
                    return false;
                }
                source = target;
                target = pass % 2 == 0 ? s_lighting.daylight_diffuse_b.data() :
                         s_lighting.daylight_diffuse_a.data();
            }
        }

        if( !source_collection.sources.empty() ) {
            auto sources = std::span<cata_gpu::GpuLightSource const> {
                source_collection.sources.data(), source_collection.sources.size()
            };
            if( !kernels::raytrace( {
                .transparency = s_lighting.transparency.data(),
                .floor = s_lighting.floor.data(),
                .vehicle_floor = s_lighting.vehicle_floor.data(),
                .sources = sources,
                .lightmap = s_lighting.lightmap.data(),
                .cache_x = cache_x,
                .cache_y = cache_y,
                .cache_xy = cache_xy,
                .z_count = OVERMAP_LAYERS,
                .z_scale = Z_LEVEL_SCALE,
                .source_offset = 0U,
                .num_sources = static_cast<uint32_t>( sources.size() ),
                .max_radius = max_light_radius( sources ),
            } ) ) {
                return false;
            }
        }

        if( colored_lighting && !source_collection.colored_sources.empty() ) {
            std::ranges::fill( s_lighting.colored_light, 0U );
            auto sources = std::span<cata_gpu::GpuColoredLightSource const> {
                source_collection.colored_sources.data(), source_collection.colored_sources.size()
            };
            if( !kernels::color_raytrace( {
                .transparency = s_lighting.transparency.data(),
                .floor = s_lighting.floor.data(),
                .vehicle_floor = s_lighting.vehicle_floor.data(),
                .sources = sources,
                .color = s_lighting.colored_light.data(),
                .cache_x = cache_x,
                .cache_y = cache_y,
                .cache_xy = cache_xy,
                .z_count = OVERMAP_LAYERS,
                .z_scale = Z_LEVEL_SCALE,
                .source_offset = 0U,
                .num_sources = static_cast<uint32_t>( sources.size() ),
                .max_radius = max_colored_light_radius( sources ),
            } ) ) {
                return false;
            }
            copy_colored_light_to_level_caches( *p.m, levels );
        } else {
            clear_colored_light_levels( *p.m, levels );
        }

        if( p.download_lightmap ) {
            copy_lightmap_to_level_caches( *p.m, lightmap_levels );
        }
        s_lighting.source_map_valid = true;
        s_lighting.lighting_outputs_valid = true;
    }
    return true;
}

auto begin_lighting( lighting_params const &p ) -> lighting_work
{
    if( s_pending_lighting.active || !slang_cpu::run_lighting( p ) ) {
        return {};
    }
    const auto id = next_work_id();
    s_pending_lighting = {
        .active = true,
        .id = id,
    };
    return {
        .backend = backend_kind::slang_cpu,
        .id = id,
    };
}

auto finish_lighting( lighting_work const &work ) -> bool
{
    if( !s_pending_lighting.active || work.backend != backend_kind::slang_cpu ||
        s_pending_lighting.id != work.id ) {
        return false;
    }
    s_pending_lighting = {};
    return true;
}

auto run_visibility( visibility_params const &p ) -> bool
{
    if( p.m == nullptr ) {
        return false;
    }
    const auto &lc = p.m->get_cache_ref( p.zlev );
    reset_residency_for_shape( lc.cache_x, lc.cache_y, OVERMAP_LAYERS );
    if( !resident_lighting_ready_for_visibility( {
        .cache_x = lc.cache_x,
        .cache_y = lc.cache_y,
        .z_count = OVERMAP_LAYERS,
    } ) ) {
        return false;
    }

    auto download_levels = p.download_levels == nullptr ? all_levels() : sorted_unique( *p.download_levels );
    const auto rebuild_seen = p.rebuild_seen_cache ||
                              !seen_levels_valid( download_levels, p.player_x, p.player_y );
    const auto dispatch = make_dispatch_range( download_levels );
    if( rebuild_seen ) {
        const auto total_tiles =
            static_cast<uint32_t>( static_cast<std::size_t>( dispatch.z_count ) *
                                   static_cast<std::size_t>( s_lighting.cache_x * s_lighting.cache_y ) );
        const auto cleared = s_lighting.seen_origin_valid ?
                             kernels::clear_seen_view( {
            .seen_raw = s_lighting.seen_raw.data(),
            .seen = s_lighting.seen.data(),
            .player_x = s_lighting.seen_origin_x,
            .player_y = s_lighting.seen_origin_y,
            .cache_x = s_lighting.cache_x,
            .cache_y = s_lighting.cache_y,
            .cache_xy = s_lighting.cache_x * s_lighting.cache_y,
            .z_count = s_lighting.z_count,
            .view_radius = g_max_view_distance,
            .z_start_idx = dispatch.z_start_idx,
            .dispatch_z_count = dispatch.z_count,
        } ) :
                             kernels::clear_seen( {
            .seen_raw = s_lighting.seen_raw.data(),
            .seen = s_lighting.seen.data(),
            .total_tiles = total_tiles,
            .cache_xy = s_lighting.cache_x * s_lighting.cache_y,
            .z_start_idx = dispatch.z_start_idx,
        } );
        if( !cleared || !kernels::seen( {
        .transparency = s_lighting.transparency.data(),
        .floor = s_lighting.floor.data(),
        .vehicle_floor = s_lighting.vehicle_floor.data(),
        .vehicle_obscured = s_lighting.vehicle_obscured.data(),
        .seen = s_lighting.seen_raw.data(),
        .player_x = p.player_x,
        .player_y = p.player_y,
        .player_z_idx = p.player_zlev + OVERMAP_DEPTH,
        .cache_x = s_lighting.cache_x,
        .cache_y = s_lighting.cache_y,
        .cache_xy = s_lighting.cache_x * s_lighting.cache_y,
        .z_count = s_lighting.z_count,
        .view_radius = g_max_view_distance,
        .z_scale = Z_LEVEL_SCALE,
        .z_start_idx = dispatch.z_start_idx,
        .dispatch_z_count = dispatch.z_count,
        .trigdist = trigdist ? 1U : 0U,
        .vision_block_mask = p.vision_block_mask,
    } ) || !kernels::seen_walls( {
        .transparency = s_lighting.transparency.data(),
        .seen_src = s_lighting.seen_raw.data(),
        .vehicle_floor = s_lighting.vehicle_floor.data(),
        .vehicle_obscured = s_lighting.vehicle_obscured.data(),
        .seen_dst = s_lighting.seen.data(),
        .player_x = p.player_x,
        .player_y = p.player_y,
        .player_z_idx = p.player_zlev + OVERMAP_DEPTH,
        .cache_x = s_lighting.cache_x,
        .cache_y = s_lighting.cache_y,
        .cache_xy = s_lighting.cache_x * s_lighting.cache_y,
        .z_count = s_lighting.z_count,
        .view_radius = g_max_view_distance,
        .z_scale = Z_LEVEL_SCALE,
        .z_start_idx = dispatch.z_start_idx,
        .dispatch_z_count = dispatch.z_count,
        .trigdist = trigdist ? 1U : 0U,
    } ) ) {
            return false;
        }
        mark_seen_levels_valid( download_levels, p.player_x, p.player_y, p.rebuild_seen_cache );
    }

    std::ranges::fill( s_lighting.camera, 0U );
    auto const origin = tripoint_bub_ms { p.player_x, p.player_y, p.player_zlev };
    auto vehicle_optics = cata_gpu::collect_lighting_vehicle_optics( {
        .m = p.m,
        .origin = &origin,
        .target_z = p.zlev,
    } );
    if( !vehicle_optics.empty() ) {
        auto optics = std::span<cata_gpu::GpuVehicleOptic const> {
            vehicle_optics.data(), vehicle_optics.size()
        };
        if( !kernels::vehicle_optics( {
            .transparency = s_lighting.transparency.data(),
            .seen = s_lighting.seen.data(),
            .optics = optics,
            .camera = s_lighting.camera.data(),
            .cache_x = s_lighting.cache_x,
            .cache_y = s_lighting.cache_y,
            .cache_xy = s_lighting.cache_x * s_lighting.cache_y,
            .z_count = s_lighting.z_count,
            .trigdist = trigdist ? 1U : 0U,
            .visible_threshold = g_visible_threshold,
            .max_view_distance = g_max_view_distance,
            .max_radius = max_optic_radius( optics ),
        } ) ) {
            return false;
        }
    }
    if( !kernels::final_visibility( {
        .transparency = s_lighting.transparency.data(),
        .lightmap = s_lighting.lightmap.data(),
        .seen = s_lighting.seen.data(),
        .camera = s_lighting.camera.data(),
        .source_map = s_lighting.source_map.data(),
        .visibility = s_lighting.visibility.data(),
        .player_x = p.player_x,
        .player_y = p.player_y,
        .player_z_idx = p.player_zlev + OVERMAP_DEPTH,
        .cache_x = s_lighting.cache_x,
        .cache_y = s_lighting.cache_y,
        .cache_xy = s_lighting.cache_x * s_lighting.cache_y,
        .z_count = s_lighting.z_count,
        .trigdist = trigdist ? 1U : 0U,
        .u_clairvoyance = p.u_clairvoyance,
        .u_unimpaired_range = p.u_unimpaired_range,
        .g_light_level = p.g_light_level,
        .vision_threshold = p.vision_threshold,
        .visibility_scale_factor = p.visibility_scale_factor,
        .visible_threshold = g_visible_threshold,
        .detail_range = p.detail_range,
        .z_start_idx = dispatch.z_start_idx,
        .dispatch_z_count = dispatch.z_count,
    } ) ) {
        return false;
    }

    copy_visibility_to_level_cache( *p.m, download_levels );
    return true;
}

auto begin_visibility( visibility_params const &p ) -> visibility_work
{
    if( s_pending_visibility.active || !slang_cpu::run_visibility( p ) ) {
        return {};
    }
    const auto id = next_work_id();
    s_pending_visibility = {
        .active = true,
        .id = id,
    };
    return {
        .backend = backend_kind::slang_cpu,
        .id = id,
    };
}

auto finish_visibility( visibility_work const &work ) -> bool
{
    if( !s_pending_visibility.active || work.backend != backend_kind::slang_cpu ||
        s_pending_visibility.id != work.id ) {
        return false;
    }
    s_pending_visibility = {};
    return true;
}

auto run_sight_pairs( run_sight_pairs_params const &p ) -> bool
{
    if( p.m == nullptr || p.pairs == nullptr || p.results == nullptr || p.pairs->empty() ) {
        return false;
    }
    const auto &lc = p.m->get_cache_ref( p.zlev );
    reset_residency_for_shape( lc.cache_x, lc.cache_y, OVERMAP_LAYERS );
    if( !resident_lighting_ready_for_sight_pairs( {
        .m = p.m,
        .pairs = p.pairs,
        .zlev = p.zlev,
    } ) ) {
        return false;
    }
    return kernels::sight_pairs( {
        .transparency = s_lighting.transparency.data(),
        .floor = s_lighting.floor.data(),
        .pairs = *p.pairs,
        .results = p.results,
        .cache_x = s_lighting.cache_x,
        .cache_y = s_lighting.cache_y,
        .cache_xy = s_lighting.cache_x * s_lighting.cache_y,
        .z_count = s_lighting.z_count,
    } );
}

auto begin_sight_pairs( begin_sight_pairs_params const &p ) -> sight_pairs_work
{
    if( s_pending_sight_pairs.active || p.pairs == nullptr || p.pairs->empty() ) {
        return {};
    }
    const auto id = next_work_id();
    s_pending_sight_pairs = {
        .active = true,
        .id = id,
        .m = p.m,
        .pairs = *p.pairs,
        .zlev = p.zlev,
    };
    return {
        .backend = backend_kind::slang_cpu,
        .id = id,
    };
}

auto finish_sight_pairs( sight_pairs_work const &work, std::vector<uint32_t> &results )
-> bool
{
    if( !s_pending_sight_pairs.active || work.backend != backend_kind::slang_cpu ||
        s_pending_sight_pairs.id != work.id ) {
        return false;
    }
    auto pending = std::move( s_pending_sight_pairs );
    s_pending_sight_pairs = {};
    return slang_cpu::run_sight_pairs( {
        .m = pending.m,
        .pairs = &pending.pairs,
        .results = &results,
        .zlev = pending.zlev,
    } );
}
#endif

} // namespace cata_compute::slang_cpu

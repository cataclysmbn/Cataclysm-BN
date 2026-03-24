#include "shadowcasting.h" // IWYU pragma: associated

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <cstdint>

#include "cached_options.h"
#include "cata_unreachable.h"
#include "game_constants.h"
#include "line.h"
#include "lightmap.h"
#include "point.h"
#include "profile.h"
#include "string_formatter.h"
#include "thread_pool.h"

// ── four_quadrants ────────────────────────────────────────────────────────────

std::string four_quadrants::to_string() const
{
    return string_format( "(%.2f,%.2f,%.2f,%.2f)",
                          ( *this )[quadrant::NE], ( *this )[quadrant::SE],
                          ( *this )[quadrant::SW], ( *this )[quadrant::NW] );
}

// ── exp_lookup ────────────────────────────────────────────────────────────────

void exp_lookup::reset( float t ) noexcept
{
    transparency = t;
    for( int i = 0; i < size; ++i ) {
        values[i] = 1.0f / std::exp( t * static_cast<float>( i ) );
    }
}

// Precomputed table for open-air transparency — always valid, never changes.
static const exp_lookup s_openair_lookup{ LIGHT_TRANSPARENCY_OPEN_AIR };

// ── Z-distance table (Proposals A + B) ───────────────────────────────────────
// Precomputes round(sqrt(dx² + dy² + (dz * Z_LEVEL_SCALE)²)) for every
// (dx, dy, dz) triple that cast_zlight_segment can encounter.  Replaces the
// per-tile sqrt() call and applies a 1.8× z-level scaling to correct the
// physics: one z-level is ~1.8 horizontal tiles in height.
//
// Table layout: [dy * (Z+1) * (R+1) + dz * (R+1) + dx]
// where R = g_max_view_distance, Z = fov_3d_z_range.
// Rebuilt whenever those two runtime values change.

// Z_LEVEL_SCALE is declared in shadowcasting.h (inline constexpr float).

static std::vector<uint16_t> s_zdist_table;
static int s_zdist_R = -1;
static int s_zdist_Z = -1;

static void rebuild_zdist_table()
{
    const int R = g_max_view_distance;
    const int Z = fov_3d_z_range;
    if( R == s_zdist_R && Z == s_zdist_Z ) {
        return;
    }
    s_zdist_R = R;
    s_zdist_Z = Z;
    s_zdist_table.resize( static_cast<size_t>( R + 1 ) * ( Z + 1 ) * ( R + 1 ) );
    for( int dy = 0; dy <= R; ++dy ) {
        for( int dz = 0; dz <= Z; ++dz ) {
            const float fz  = static_cast<float>( dz ) * Z_LEVEL_SCALE;
            const float fz2 = fz * fz;
            for( int dx = 0; dx <= R; ++dx ) {
                const float d = std::sqrt( static_cast<float>( dx * dx + dy * dy ) + fz2 );
                s_zdist_table[static_cast<size_t>( dy ) * ( Z + 1 ) * ( R + 1 ) +
                                                   static_cast<size_t>( dz ) * ( R + 1 ) + dx] =
                                  static_cast<uint16_t>( std::lround( d ) );
            }
        }
    }
}

static int zdist_lookup( int dx, int dy, int dz ) noexcept
{
    return s_zdist_table[static_cast<size_t>( dy ) * ( s_zdist_Z + 1 ) * ( s_zdist_R + 1 ) +
                                              static_cast<size_t>( dz ) * ( s_zdist_R + 1 ) + dx];
}

template<bool UseAtomic>
static void atomic_float_max( float &cell, float val ) noexcept
{
    if constexpr( UseAtomic ) {
#if defined(__cpp_lib_atomic_ref)
        std::atomic_ref<float> a( cell );
        float expected = a.load( std::memory_order_relaxed );
        while( expected < val &&
               !a.compare_exchange_weak( expected, val, std::memory_order_relaxed ) ) {
        }
#else
        // Fallback for toolchains without std::atomic_ref (e.g. older Android NDK).
        // __atomic_* builtins only accept integer/pointer types, so type-pun float through uint32_t.
        // Non-negative IEEE 754 floats preserve ordering as uint32_t, so bit comparison is valid.
        static_assert( sizeof( float ) == sizeof( uint32_t ) );
        uint32_t *cell_bits = reinterpret_cast<uint32_t *>( &cell );
        uint32_t val_bits;
        std::memcpy( &val_bits, &val, sizeof( float ) );
        uint32_t expected_bits = __atomic_load_n( cell_bits, __ATOMIC_RELAXED );
        while( expected_bits < val_bits &&
               !__atomic_compare_exchange_n( cell_bits, &expected_bits, val_bits, true,
                                             __ATOMIC_RELAXED, __ATOMIC_RELAXED ) ) {
        }
#endif
    } else {
        cell = std::max( cell, val );
    }
}

// ── Internal helpers ──────────────────────────────────────────────────────────
// Fast integer square-root for distances ≤ MAX_VIEW_DISTANCE.
// Schraudolph-Newton method; matches rl_dist() truncation behaviour.
// Only used when g_max_view_distance ≤ 60.  See lightmap.cpp for derivation.
template<int start, int iterations>
static int fast_rl_dist( tripoint to )
{
    if( !trigdist ) {
        return square_dist( tripoint_zero, to );
    }

    int val = to.x * to.x + to.y * to.y + to.z * to.z;
    if( val < 2 ) {
        return val;
    }

    int a = start;
    for( int i = 0; i < iterations; ++i ) {
        int b = val / a;
        a = ( a + b ) / 2;
    }
    if( a * a > val ) {
        --a;
    }
    return a;
}

// Maps an XY direction vector to the quadrant that *sources* light in that
// direction (i.e. the quadrant the viewer stands in relative to the target).
// Assumes x != 0 || y != 0.
// NOLINTNEXTLINE(cata-xy)
static constexpr quadrant quadrant_from_x_y( int x, int y )
{
    return ( x > 0 )
           ? ( ( y > 0 ) ? quadrant::NW : quadrant::SW )
           : ( ( y > 0 ) ? quadrant::NE : quadrant::SE );
}

// ── Octant transform structs ──────────────────────────────────────────────────
// Replace the compile-time integer template packs.  All 8 (16) instances are
// constexpr constants, so the compiler constant-folds the arithmetic at every
// call site exactly as it did with templates.

struct octant_xform {
    int xx, xy, yx, yy;

    // Map octant-local (dx, dy) to actual (x, y) relative to offset.
    constexpr point apply( int dx, int dy ) const noexcept {
        return { dx *xx + dy * xy, dx *yx + dy * yy };
    }

    // The quadrant from which light approaches tiles seen via this octant.
    // Formula: quadrant_from_x_y( -(xx+xy), -(yx+yy) )
    constexpr quadrant source_quadrant() const noexcept {
        return quadrant_from_x_y( -xx - xy, -yx - yy );
    }
};

struct octant_xform_3d {
    int xx, xy, xz, yx, yy, yz, zz;

    // Map octant-local (dx, dy, dz) to world offset.
    constexpr tripoint apply( int dx, int dy, int dz ) const noexcept {
        return {
            dx *xx + dy *xy + dz * xz,
            dx *yx + dy *yy + dz * yz,
            dz *zz           // xz == yz == 0 always in current usage
        };
    }

    // Quadrant for the XY plane component (same formula as cast_zlight).
    constexpr quadrant source_quadrant() const noexcept {
        return quadrant_from_x_y( xx + xy, yx + yy );
    }
};

// The 8 octant transforms for 2D casting.  Bit i of the octant_mask passed to
// castLightOctants_q selects k_octant_xforms[i].
static constexpr std::array<octant_xform, 8> k_octant_xforms = {{
        { 0,  1,  1,  0 },
        { 1,  0,  0,  1 },
        { 0, -1,  1,  0 },
        {-1,  0,  0,  1 },
        { 0,  1, -1,  0 },
        { 1,  0,  0, -1 },
        { 0, -1, -1,  0 },
        {-1,  0,  0, -1 },
    }
};

// 16 transforms for 3D casting: 8 down (zz=-1) then 8 up (zz=+1).
static constexpr std::array<octant_xform_3d, 16> k_zlight_xforms = {{
        // Down (zz = -1)
        { 0,  1, 0,  1,  0, 0, -1 },
        { 1,  0, 0,  0,  1, 0, -1 },
        { 0, -1, 0,  1,  0, 0, -1 },
        {-1,  0, 0,  0,  1, 0, -1 },
        { 0,  1, 0, -1,  0, 0, -1 },
        { 1,  0, 0,  0, -1, 0, -1 },
        { 0, -1, 0, -1,  0, 0, -1 },
        {-1,  0, 0,  0, -1, 0, -1 },
        // Up (zz = +1)
        { 0,  1, 0,  1,  0, 0,  1 },
        { 1,  0, 0,  0,  1, 0,  1 },
        { 0, -1, 0,  1,  0, 0,  1 },
        {-1,  0, 0,  0,  1, 0,  1 },
        { 0,  1, 0, -1,  0, 0,  1 },
        { 1,  0, 0,  0, -1, 0,  1 },
        { 0, -1, 0, -1,  0, 0,  1 },
        {-1,  0, 0,  0, -1, 0,  1 },
    }
};

// ── Internal 2D cast ──────────────────────────────────────────────────────────
//
// Casts light for one octant.
//
// @p lookup  Active fast-path table, or null for full exp() computation.
//            When a tile's transparency differs from lookup->transparency the
//            cast recurses with lookup=nullptr (slow path).
// @p Out     float or four_quadrants — selects which update_* to invoke.

template<typename Out>
static void castLight(
    Out *output_cache,
    const float *input_array,
    const diagonal_blocks *blocked_array,
    int sx, int sy,
    point offset, int offset_distance, float numerator,
    const light_model &model,
    octant_xform xf,
    int row, float start, float end,
    float cumulative_transparency,
    const exp_lookup *lookup )
{
    if( start < end ) {
        return;
    }

    const quadrant quad = xf.source_quadrant();

    const auto check_blocked = [&]( point p ) -> bool {
        switch( quad )
        {
            case quadrant::NW:
                return blocked_array[p.x * sy + p.y].nw;
            case quadrant::NE:
                return blocked_array[p.x * sy + p.y].ne;
            case quadrant::SE:
                return p.x < sx - 1 && p.y < sy - 1 &&
                blocked_array[( p.x + 1 ) * sy + p.y + 1].nw;
            case quadrant::SW:
                return p.x > 1 && p.y < sy - 1 &&
                blocked_array[( p.x - 1 ) * sy + p.y + 1].ne;
            default:
                cata::unreachable();
        }
    };

    const int radius = g_max_view_distance - offset_distance;
    float last_intensity = 0.0f;
    tripoint delta;
    delta.z = 0;

    for( int distance = row; distance <= radius; ++distance ) {
        delta.y = -distance;
        bool started_row = false;
        float current_transparency = 0.0f;

        // Trim the x-range to the visible sector.
        delta.x = static_cast<int>( std::ceil(
                                        std::max( static_cast<float>( -distance ),
                                                ( ( -distance - 0.5f ) * start ) - 0.5f ) ) );
        const int x_limit = static_cast<int>( std::floor(
                std::min( 0.0f,
                          ( ( -distance + 0.5f ) * end ) - 0.5f ) ) ) + 1;

        int last_dist = -1;

        for( ; delta.x <= x_limit; ++delta.x ) {
            const point current{
                offset.x + delta.x *xf.xx + delta.y * xf.xy,
                offset.y + delta.x *xf.yx + delta.y *xf.yy
            };

            if( current.x < 0 || current.y < 0 ||
                current.x >= sx || current.y >= sy ) {
                continue;
            }

            if( check_blocked( current ) ) {
                continue;
            }

            if( !started_row ) {
                started_row = true;
                current_transparency = input_array[current.x * sy + current.y];
            }

            const int idx = current.x * sy + current.y;

            // Compute intensity — use lookup table if on fast path.
            if( lookup != nullptr ) {
                const int dist = ( g_max_view_distance > 60
                                   ? rl_dist( tripoint_zero, delta )
                                   : fast_rl_dist<21, 4>( delta ) ) + offset_distance;
                last_intensity = model.lookup_calc( numerator, lookup->values[dist], dist );
            } else {
                const int dist = rl_dist( tripoint_zero, delta ) + offset_distance;
                if( last_dist != dist ) {
                    last_intensity = model.calc( numerator, cumulative_transparency, dist );
                    last_dist = dist;
                }
            }

            const float new_transparency = input_array[idx];

            // Write to the output cache.  Transparent tiles accept light from
            // all directions; opaque tiles only from the source quadrant.
            const quadrant update_quad = model.check( new_transparency, last_intensity )
                                         ? quadrant::default_ : quad;

            if constexpr( std::is_same_v<Out, float> ) {
                model.update_float( output_cache[idx], last_intensity, update_quad );
            } else {
                model.update_quadrants( output_cache[idx], last_intensity, update_quad );
            }

            if( new_transparency == current_transparency ) {
                continue;
            }

            // ── Transparency boundary: split the remaining sector ─────────────
            const float trailing_edge = ( delta.x - 0.5f ) / ( delta.y + 0.5f );

            if( model.check( current_transparency, last_intensity ) ) {
                // The span we just completed was transparent — recurse into it.
                const float next_cumulative = ( lookup != nullptr )
                                              ? model.accumulate( lookup->transparency, current_transparency, distance )
                                              : model.accumulate( cumulative_transparency, current_transparency, distance );

                // Stay on fast path only if current span matched the lookup.
                const exp_lookup *next_lookup = ( lookup != nullptr &&
                                                  current_transparency == lookup->transparency )
                                                ? lookup : nullptr;

                castLight<Out>( output_cache, input_array, blocked_array, sx, sy,
                                offset, offset_distance, numerator, model, xf,
                                distance + 1, start, trailing_edge,
                                next_cumulative, next_lookup );
            }

            // Advance the leading edge.
            if( !model.check( current_transparency, last_intensity ) ) {
                // Previous span was opaque — new start is the leading edge of that span.
                start = ( delta.x - 0.5f ) / ( delta.y - 0.5f );
            } else {
                start = trailing_edge;
            }

            if( start < end ) {
                return;
            }
            current_transparency = new_transparency;
        }

        if( !model.check( current_transparency, last_intensity ) ) {
            // Ended the row blocked — no more rows in this sector.
            break;
        }

        // End of row: check whether we need to leave the fast path.
        if( lookup != nullptr && current_transparency != lookup->transparency ) {
            castLight<Out>( output_cache, input_array, blocked_array, sx, sy,
                            offset, offset_distance, numerator, model, xf,
                            distance + 1, start, end,
                            model.accumulate( lookup->transparency, current_transparency, distance ),
                            nullptr );
            return;
        }

        // Update cumulative transparency on the slow path.
        if( lookup == nullptr ) {
            cumulative_transparency = model.accumulate(
                                          cumulative_transparency, current_transparency, distance );
        }
        // On the fast path with matching transparency, cumulative is implicit in the table.
    }
}

// ── castLightAll / castLightAll_q ─────────────────────────────────────────────

void castLightAll(
    float *output_cache,
    const float *input_array,
    const diagonal_blocks *blocked_array,
    int sx, int sy,
    point offset, int offset_distance, float numerator,
    const light_model &model,
    const exp_lookup *weather_lookup )
{
    ZoneScoped;

    for( const auto &xf : k_octant_xforms ) {
        // Determine whether the fast path is available for this octant.
        // The first tile for row=1, delta.x=0 is at offset + apply(0,-1) = offset - (xy, yy).
        const exp_lookup *fast = nullptr;
        if( model.lookup_calc != nullptr ) {
            const point first{ offset.x - xf.xy, offset.y - xf.yy };
            if( first.x >= 0 && first.y >= 0 && first.x < sx && first.y < sy ) {
                const float t = input_array[first.x * sy + first.y];
                if( t == LIGHT_TRANSPARENCY_OPEN_AIR ) {
                    fast = &s_openair_lookup;
                } else if( weather_lookup != nullptr && t == weather_lookup->transparency ) {
                    fast = weather_lookup;
                }
            }
        }

        castLight<float>( output_cache, input_array, blocked_array, sx, sy,
                          offset, offset_distance, numerator, model, xf,
                          1, 1.0f, 0.0f, LIGHT_TRANSPARENCY_OPEN_AIR, fast );
    }
}

void castLightAll_q(
    four_quadrants *output_cache,
    const float *input_array,
    const diagonal_blocks *blocked_array,
    int sx, int sy,
    point offset, int offset_distance, float numerator,
    const light_model &model,
    const exp_lookup *weather_lookup )
{
    ZoneScoped;

    for( const auto &xf : k_octant_xforms ) {
        const exp_lookup *fast = nullptr;
        if( model.lookup_calc != nullptr ) {
            const point first{ offset.x - xf.xy, offset.y - xf.yy };
            if( first.x >= 0 && first.y >= 0 && first.x < sx && first.y < sy ) {
                const float t = input_array[first.x * sy + first.y];
                if( t == LIGHT_TRANSPARENCY_OPEN_AIR ) {
                    fast = &s_openair_lookup;
                } else if( weather_lookup != nullptr && t == weather_lookup->transparency ) {
                    fast = weather_lookup;
                }
            }
        }

        castLight<four_quadrants>( output_cache, input_array, blocked_array, sx, sy,
                                   offset, offset_distance, numerator, model, xf,
                                   1, 1.0f, 0.0f, LIGHT_TRANSPARENCY_OPEN_AIR, fast );
    }
}

void castLightOctants_q(
    four_quadrants *output_cache,
    const float *input_array,
    const diagonal_blocks *blocked_array,
    int sx, int sy,
    point offset, int offset_distance, float numerator,
    const light_model &model,
    uint8_t octant_mask,
    const exp_lookup *weather_lookup )
{
    ZoneScoped;

    for( int i = 0; i < 8; ++i ) {
        if( !( octant_mask & ( static_cast<uint8_t>( 1u ) << i ) ) ) {
            continue;
        }
        const auto &xf = k_octant_xforms[i];
        const exp_lookup *fast = nullptr;
        if( model.lookup_calc != nullptr ) {
            const point first{ offset.x - xf.xy, offset.y - xf.yy };
            if( first.x >= 0 && first.y >= 0 && first.x < sx && first.y < sy ) {
                const float t = input_array[first.x * sy + first.y];
                if( t == LIGHT_TRANSPARENCY_OPEN_AIR ) {
                    fast = &s_openair_lookup;
                } else if( weather_lookup != nullptr && t == weather_lookup->transparency ) {
                    fast = weather_lookup;
                }
            }
        }
        castLight<four_quadrants>( output_cache, input_array, blocked_array, sx, sy,
                                   offset, offset_distance, numerator, model, xf,
                                   1, 1.0f, 0.0f, LIGHT_TRANSPARENCY_OPEN_AIR, fast );
    }
}

// ── Internal 3D cast ──────────────────────────────────────────────────────────
// Casts light through one 3D octant-segment.
//
// UseAtomic — when true, output writes use std::atomic_ref CAS so that 16
// octant segments can run in parallel (Proposal C).  When false, plain
// assignments are used (serial path).
template<bool UseAtomic>
static void cast_zlight_segment(
    const array_of_grids_of<float> &output_caches,
    const array_of_grids_of<const float> &input_arrays,
    const array_of_grids_of<const bool> &floor_caches,
    const array_of_grids_of<const diagonal_blocks> &blocked_caches,
    const tripoint &offset, int offset_distance,
    float numerator, const light_model &model,
    octant_xform_3d xf,
    int row = 1,
    float start_major = 0.0f, float end_major = 1.0f,
    float start_minor = 0.0f, float end_minor = 1.0f,
    float cumulative_transparency = LIGHT_TRANSPARENCY_OPEN_AIR,
    int x_skip = -1, int z_skip = -1 )
{
    if( start_major >= end_major || start_minor > end_minor ) {
        return;
    }

    const quadrant quad = xf.source_quadrant();

    const auto check_blocked = [&]( const tripoint & p ) -> bool {
        const auto &bc = blocked_caches[p.z + OVERMAP_DEPTH];
        switch( quad )
        {
            case quadrant::NW:
                return bc.at( p.x, p.y ).nw;
            case quadrant::NE:
                return bc.at( p.x, p.y ).ne;
            case quadrant::SE:
                return p.x < bc.sx - 1 && p.y < bc.sy - 1 &&
                bc.at( p.x + 1, p.y + 1 ).nw;
            case quadrant::SW:
                return p.x > 1 && p.y < bc.sy - 1 &&
                bc.at( p.x - 1, p.y + 1 ).ne;
            default:
                cata::unreachable();
        }
    };

    const int radius = g_max_view_distance - offset_distance;
    constexpr int min_z = -OVERMAP_DEPTH;
    constexpr int max_z = OVERMAP_HEIGHT;

    float last_intensity = 0.0f;
    tripoint delta;

    for( int distance = row; distance <= radius; ++distance ) {
        delta.y = distance;
        bool started_block = false;
        float current_transparency = 0.0f;
        bool current_floor = false;

        const int z_start_init = ( z_skip != -1 ) ? z_skip :
                                 std::max( 0, static_cast<int>(
                                               std::ceil( ( ( distance - 0.5f ) * start_major ) - 0.5f ) ) );
        const int z_limit = std::min( distance,
                                      static_cast<int>(
                                          std::ceil( ( ( distance + 0.5f ) * end_major ) + 0.5f ) ) - 1 );

        // z_start is mutable within the z loop (floor handling advances it).
        int z_start = z_start_init;

        for( delta.z = z_start; delta.z <= std::min( fov_3d_z_range, z_limit ); ++delta.z ) {
            const tripoint world_offset = xf.apply( 0, delta.y, delta.z );
            tripoint current;
            current.z = offset.z + world_offset.z;

            if( current.z > max_z || current.z < min_z ) {
                continue;
            }

            const int z_index = current.z + OVERMAP_DEPTH;

            const int x_start = ( x_skip != -1 ) ? x_skip :
                                std::max( 0, static_cast<int>(
                                              std::ceil( ( ( distance - 0.5f ) * start_minor ) - 0.5f ) ) );
            const int x_limit = std::min( distance,
                                          static_cast<int>(
                                              std::ceil( ( ( distance + 0.5f ) * end_minor ) + 0.5f ) ) - 1 );

            for( delta.x = x_start; delta.x <= x_limit; ++delta.x ) {
                const tripoint world_xy = xf.apply( delta.x, delta.y, delta.z );
                current.x = offset.x + world_xy.x;
                current.y = offset.y + world_xy.y;

                const auto &ic = input_arrays[z_index];
                if( !( current.x >= 0 && current.y >= 0 &&
                       current.x < ic.sx && current.y < ic.sy ) ) {
                    continue;
                }

                if( check_blocked( current ) ) {
                    return;
                }

                const float new_transparency = ic.at( current.x, current.y );
                const bool new_floor = ( ( xf.zz < 0 )
                                         ? floor_caches[z_index].at( current.x, current.y )
                                         : ( z_index < OVERMAP_LAYERS - 1
                                             ? floor_caches[z_index + 1].at( current.x, current.y )
                                             : false ) );

                if( !started_block ) {
                    started_block = true;
                    current_transparency = new_transparency;
                    current_floor = new_floor;
                }

                const int dist_2d  = zdist_lookup( delta.x, delta.y, 0 ) + offset_distance;

                if( cumulative_transparency == LIGHT_TRANSPARENCY_OPEN_AIR ) {
                    const int lookup_idx = std::min( dist_2d, exp_lookup::size - 1 );
                    last_intensity = numerator * s_openair_lookup.values[lookup_idx];
                } else {
                    last_intensity = model.calc( numerator, cumulative_transparency, dist_2d );
                }

                float &out_cell = output_caches[z_index].at( current.x, current.y );
                atomic_float_max<UseAtomic>( out_cell, last_intensity );

                if( new_transparency != current_transparency || new_floor != current_floor ) {
                    // ── Split: A (past rows), B (processed x so far), C (rest) ─
                    //
                    //  +---+---+  <- end_major
                    //  | B | C |
                    //  +---+---+  <- mid_major
                    //  |   A   |
                    //  +-------+  <- start_major
                    //  ^   ^   ^
                    //  |   |   end_minor
                    //  |   mid_minor
                    //  start_minor

                    const float mid_major = ( current_transparency < new_transparency )
                                            ? ( delta.z - 0.5f ) / ( delta.y - 0.5f )
                                            : ( delta.z - 0.5f ) / ( delta.y + 0.5f );

                    if( delta.z != z_start &&
                        model.check( current_transparency, last_intensity ) ) {
                        // Cast section A (rows already processed at this distance).
                        const float next_cumulative = model.accumulate(
                                                          cumulative_transparency, current_transparency, distance );
                        cast_zlight_segment<UseAtomic>(
                            output_caches, input_arrays, floor_caches, blocked_caches,
                            offset, offset_distance, numerator, model, xf,
                            distance + 1,
                            start_major, std::min( mid_major, end_major ),
                            start_minor, end_minor,
                            next_cumulative,
                            -1, -1 );
                    }

                    const float mid_minor = ( current_transparency < new_transparency )
                                            ? ( delta.x - 0.5f ) / ( delta.y - 0.5f )
                                            : ( delta.x - 0.5f ) / ( delta.y + 0.5f );

                    // Section C: always cast (handles the new transparency span).
                    cast_zlight_segment<UseAtomic>(
                        output_caches, input_arrays, floor_caches, blocked_caches,
                        offset, offset_distance, numerator, model, xf,
                        distance,
                        std::max( mid_major, start_major ), end_major,
                        std::max( mid_minor, start_minor ), end_minor,
                        cumulative_transparency, delta.x, delta.z );

                    // Continue with section B (already-processed x tiles).
                    if( delta.x == x_start ) {
                        return;  // B is zero-width.
                    }

                    start_major = std::max( start_major, mid_major );
                    end_minor   = std::min( end_minor, mid_minor );

                    if( start_major >= end_major || start_minor >= end_minor ) {
                        return;
                    }
                    z_start = delta.z;
                    break;  // Exit x loop; z loop continues from delta.z.
                }
            } // end x loop

            // ── Floor handling ────────────────────────────────────────────────
            if( current_floor ) {
                if( model.check( current_transparency, last_intensity ) ) {
                    const float next_cumulative = model.accumulate(
                                                      cumulative_transparency, current_transparency, distance );
                    const float top_edge = ( delta.z + 0.5f ) / ( delta.y + 0.5001f );
                    cast_zlight_segment<UseAtomic>(
                        output_caches, input_arrays, floor_caches, blocked_caches,
                        offset, offset_distance, numerator, model, xf,
                        distance + 1,
                        start_major, top_edge,
                        start_minor, end_minor,
                        next_cumulative,
                        -1, -1 );
                }
                start_major = ( delta.z + 0.5f ) / ( delta.y - 0.5001f );
                if( start_major >= end_major ) {
                    return;
                }
                // Continue z loop from the next level; reset block state.
                z_start = delta.z + 1;
                started_block = false;
                z_skip = -1;
            }
        } // end z loop

        if( !model.check( current_transparency, last_intensity ) ) {
            break;
        }
        cumulative_transparency = model.accumulate(
                                      cumulative_transparency, current_transparency, distance );
        z_skip = -1;
        x_skip = -1;
    } // end distance loop
}

// ── cast_zlight ───────────────────────────────────────────────────────────────

void cast_zlight(
    const array_of_grids_of<float> &output_caches,
    const array_of_grids_of<const float> &input_arrays,
    const array_of_grids_of<const bool> &floor_caches,
    const array_of_grids_of<const diagonal_blocks> &blocked_caches,
    const tripoint &origin, int offset_distance, float numerator,
    const light_model &model )
{
    ZoneScoped;

    // Ensure the z-distance lookup table matches current runtime settings.
    rebuild_zdist_table();

    if( parallel_enabled ) {
        parallel_for_chunked( 0, static_cast<int>( k_zlight_xforms.size() ), 1, [&]( int i ) {
            cast_zlight_segment<true>(
                output_caches, input_arrays, floor_caches, blocked_caches,
                origin, offset_distance, numerator, model, k_zlight_xforms[i],
                1, 0.0f, 1.0f, 0.0f, 1.0f, LIGHT_TRANSPARENCY_OPEN_AIR, -1, -1 );
        } );
    } else {
        std::ranges::for_each( k_zlight_xforms, [&]( const octant_xform_3d & xf ) {
            cast_zlight_segment<false>(
                output_caches, input_arrays, floor_caches, blocked_caches,
                origin, offset_distance, numerator, model, xf,
                1, 0.0f, 1.0f, 0.0f, 1.0f, LIGHT_TRANSPARENCY_OPEN_AIR, -1, -1 );
        } );
    }
}

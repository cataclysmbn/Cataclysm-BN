#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>

#include "game_constants.h"
#include "lightmap.h"

struct point;
struct tripoint;

// Tracks whether a diagonal between two tiles is blocked by a vehicle part.
// Stored per-tile in level_cache::vehicle_obscured_cache.
// Defined here (not in map.h) so shadowcasting.cpp can use the members directly.
struct diagonal_blocks {
    bool nw = false;
    bool ne = false;
};

// ── Quadrant ──────────────────────────────────────────────────────────────────
// Light arriving at an opaque tile is tagged with the quadrant the viewer
// occupies relative to that tile.  Non-opaque tiles use default_ (= NE):
// light from all directions is equivalent there.
enum class quadrant : int {
    NE = 0, SE = 1, SW = 2, NW = 3,
    default_ = NE
};

// ── four_quadrants ────────────────────────────────────────────────────────────
// Per-quadrant light intensities for a single tile.  Used in the lightmap (lm)
// to correctly shade opaque surfaces based on the viewer's approach angle.
struct four_quadrants {
    four_quadrants() = default;
    explicit constexpr four_quadrants( float v ) : values{ {v, v, v, v} } {}

    std::array<float, 4> values;

    float &operator[]( quadrant q ) {
        return values[static_cast<int>( q )];
    }
    float operator[]( quadrant q ) const {
        return values[static_cast<int>( q )];
    }
    void fill( float v ) {
        values.fill( v );
    }
    float max() const {
        return *std::ranges::max_element( values );
    }
    std::string to_string() const;

    friend four_quadrants operator*( const four_quadrants &l, const four_quadrants &r ) {
        four_quadrants result;
        std::ranges::transform( l.values, r.values, result.values.begin(),
        []( float a, float b ) { return a * b; } );
        return result;
    }

    friend four_quadrants elementwise_max( const four_quadrants &l, const four_quadrants &r ) {
        four_quadrants result;
        std::ranges::transform( l.values, r.values, result.values.begin(),
        []( float a, float b ) { return std::max( a, b ); } );
        return result;
    }

    friend four_quadrants elementwise_max( const four_quadrants &l, const float r ) {
        four_quadrants result( l );
        for( float &v : result.values ) {
            // This looks like it should be v = std::max( v, r ), but mingw-w64
            // crashes with constant propagation through that form — keep the branch.
            if( v < r ) {
                v = r;
            }
        }
        return result;
    }
};

// ── exp_lookup ────────────────────────────────────────────────────────────────
// Pre-computes { 1/exp( t * i ) : i in [0, size) } for a fixed transparency t.
// castLightAll selects a matching table when the first tile's transparency equals
// a known lookup value, replacing exp() with a table lookup in the inner loop.
//
// Two tables are used in practice:
//   openair  — always available internally in shadowcasting.cpp.
//   weather  — owned by map, passed into castLightAll each frame when weather
//              modifies visibility.  Pass nullptr for no weather fast path.
static constexpr int LIGHTMAP_LOOKUP_SIZE = MAX_VIEW_DISTANCE * 2;

struct exp_lookup {
    static constexpr int size = LIGHTMAP_LOOKUP_SIZE;

    float values[size] {};
    float transparency = LIGHT_TRANSPARENCY_OPEN_AIR;

    explicit exp_lookup( float t ) noexcept { reset( t ); }
    void reset( float t ) noexcept;
};

// ── light_model ───────────────────────────────────────────────────────────────
// Encapsulates the physics of a shadowcasting pass as raw function pointers.
// All existing functions (sight_calc, light_calc, shrapnel_calc, …) are
// directly compatible with these signatures — no adapters needed.
//
// Null fields are legal when the feature is unused:
//   update_float     — null when writing to four_quadrants output.
//   update_quadrants — null when writing to float output.
//   lookup_calc      — null to disable the exp-lookup fast path entirely.
struct light_model {
    using calc_fn  = float( * )( const float &numerator,
                                 const float &transparency,
                                 const int &distance );
    using check_fn = bool ( * )( const float &transparency,
                                 const float &intensity );
    using upd_f_fn = void ( * )( float &out,
                                 const float &val,
                                 quadrant q );
    using upd_q_fn = void ( * )( four_quadrants &out,
                                 const float &val,
                                 quadrant q );
    using accum_fn = float( * )( const float &cumulative,
                                 const float &current,
                                 const int &distance );

    /// Intensity = calc( numerator, cumulative_transparency, distance ).
    calc_fn  calc;
    /// True if the ray should continue past this tile (not fully blocked).
    check_fn check;
    /// Write intensity to a flat float cache cell.       Null if unused.
    upd_f_fn update_float;
    /// Write intensity to a four_quadrants cache cell.   Null if unused.
    upd_q_fn update_quadrants;
    /// Fast-path variant: second param is lookup->values[dist], not raw transparency.
    /// Null disables the exp-lookup optimisation for this model.
    calc_fn  lookup_calc;
    /// Accumulate a running transparency average along the ray.
    accum_fn accumulate;
};

// ── cache_grid_ref / array_of_grids_of ───────────────────────────────────────
/// Lightweight non-owning view of one z-level's flat tile-cache array.
/// Carries runtime dimensions so shadowcasting can compute x*sy+y without
/// relying on compile-time MAPSIZE strides.
template<typename T>
struct cache_grid_ref {
    T  *data = nullptr;
    int sx   = 0;  ///< tile width  = SEEX * g_mapsize
    int sy   = 0;  ///< tile height = SEEY * g_mapsize
    auto at( int x, int y ) const -> T & { return data[x * sy + y]; } // *NOPAD*
};

template<typename T>
using array_of_grids_of = std::array<cache_grid_ref<T>, OVERMAP_LAYERS>;

// ── Z-level physical scale ────────────────────────────────────────────────────
// One z-level corresponds to ~1.8 horizontal tiles in height.
// Used by cast_zlight and the 3D LoS DDA check in build_seen_cache.
inline constexpr float Z_LEVEL_SCALE = 1.8f;

// ── Built-in Beer-Lambert sight / model functions ─────────────────────────────
// Inlined here so both shadowcasting.cpp and lightmap.cpp can use them without
// a separate translation-unit dependency.

inline float sight_calc( const float &numerator, const float &transparency,
                         const int &distance )
{
    return numerator / std::exp( transparency * distance );
}

inline bool sight_check( const float &transparency, const float &/*intensity*/ )
{
    return transparency > LIGHT_TRANSPARENCY_SOLID;
}

inline void update_light( float &update, const float &new_value, quadrant )
{
    update = std::max( update, new_value );
}

inline void update_light_quadrants( four_quadrants &update, const float &new_value, quadrant q )
{
    update[q] = std::max( update[q], new_value );
}

inline float accumulate_transparency( const float &cumulative_transparency,
                                      const float &current_transparency,
                                      const int &distance )
{
    return ( ( distance - 1 ) * cumulative_transparency + current_transparency ) / distance;
}

inline float sight_from_lookup( const float &numerator, const float &transparency,
                                const int &/*distance*/ )
{
    return numerator * transparency;
}

// ── Public shadowcasting API ──────────────────────────────────────────────────

/// 2D FOV / light cast — writes to a flat float array (seen_cache, shrapnel).
///
/// @p weather_lookup  Optional precomputed table for weather-modified air
///                    transparency.  Null = no weather fast path.
///                    An open-air fast path is always attempted internally when
///                    model.lookup_calc is non-null.
void castLightAll(
    float *output_cache,
    const float *input_array,
    const diagonal_blocks *blocked_array,
    int sx, int sy,
    point offset, int offset_distance, float numerator,
    const light_model &model,
    const exp_lookup *weather_lookup = nullptr );

/// 2D light cast — writes to a flat four_quadrants array (lm).
void castLightAll_q(
    four_quadrants *output_cache,
    const float *input_array,
    const diagonal_blocks *blocked_array,
    int sx, int sy,
    point offset, int offset_distance, float numerator,
    const light_model &model,
    const exp_lookup *weather_lookup = nullptr );

// ── Octant bitmasks ───────────────────────────────────────────────────────────
// Bit i selects k_octant_xforms[i].  Used by map::apply_light_source and
// map::apply_directional_light to cast only the relevant half-space.
inline constexpr uint8_t OCTANT_NORTH = 0xA0u; ///< octants 5, 7  (yy = -1 half)
inline constexpr uint8_t OCTANT_EAST  = 0x44u; ///< octants 2, 6  (xy = -1 half)
inline constexpr uint8_t OCTANT_SOUTH = 0x0Au; ///< octants 1, 3  (yy = +1 half)
inline constexpr uint8_t OCTANT_WEST  = 0x11u; ///< octants 0, 4  (xy = +1 half)

/// 2D light cast — writes to a flat four_quadrants array, casting only the
/// octants selected by @p octant_mask.  Bit i of octant_mask enables
/// k_octant_xforms[i].  Use OCTANT_NORTH/EAST/SOUTH/WEST constants.
void castLightOctants_q(
    four_quadrants *output_cache,
    const float *input_array,
    const diagonal_blocks *blocked_array,
    int sx, int sy,
    point offset, int offset_distance, float numerator,
    const light_model &model,
    uint8_t octant_mask,
    const exp_lookup *weather_lookup = nullptr );

/// 3D FOV cast across all z-levels.
/// Only model.calc, model.check, and model.accumulate are consulted;
/// update_float, update_quadrants, and lookup_calc are ignored.
void cast_zlight(
    const array_of_grids_of<float> &output_caches,
    const array_of_grids_of<const float> &input_arrays,
    const array_of_grids_of<const char> &floor_caches,
    const array_of_grids_of<const diagonal_blocks> &blocked_caches,
    const tripoint &origin, int offset_distance, float numerator,
    const light_model &model );

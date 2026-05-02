#pragma once

// The CATA_NO_STL macro is used by the cata clang-tidy plugin tests so they
// can include this header when compiling with -nostdinc++
#ifndef CATA_NO_STL

#include <array>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <ostream>
#include <string>
#include <vector>
#include <optional>

#else

#define assert(...)

namespace std
{
class string;
class ostream;
}

#endif // CATA_NO_STL

#include "point_traits.h"

class JsonIn;
class JsonOut;

// NOLINTNEXTLINE(cata-xy)
struct point {
    static constexpr int dimension = 2;

    int x = 0;
    int y = 0;
    constexpr point() = default;
    constexpr point( int X, int Y ) : x( X ), y( Y ) {}

    constexpr point operator+( point rhs ) const {
        return point( x + rhs.x, y + rhs.y );
    }
    point &operator+=( point rhs ) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    constexpr point operator-() const {
        return point( -x, -y );
    }
    constexpr point operator-( point rhs ) const {
        return point( x - rhs.x, y - rhs.y );
    }
    point &operator-=( point rhs ) {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
    constexpr point operator*( const int rhs ) const {
        return point( x * rhs, y * rhs );
    }
    friend constexpr point operator*( int lhs, point rhs ) {
        return rhs * lhs;
    }
    point &operator*=( const int rhs ) {
        x *= rhs;
        y *= rhs;
        return *this;
    }
    constexpr point operator/( const int rhs ) const {
        return point( x / rhs, y / rhs );
    }

#ifndef CATA_NO_STL
    point abs() const {
        return point( std::abs( x ), std::abs( y ) );
    }
#endif

    /**
     * Rotate point clockwise @param turns times, 90 degrees per turn,
     * around the center of a rectangle with the dimensions specified
     * by @param dim
     * By default rotates around the origin (0, 0).
     * NOLINTNEXTLINE(cata-use-named-point-constants) */
    point rotate( int turns, point dim = { 1, 1 } ) const;

    std::string to_string() const;

    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );

    friend constexpr bool operator<( point a, point b ) {
        return a.x < b.x || ( a.x == b.x && a.y < b.y );
    }
    friend constexpr bool operator==( point a, point b ) {
        return a.x == b.x && a.y == b.y;
    }
    friend constexpr bool operator!=( point a, point b ) {
        return !( a == b );
    }

    friend std::ostream &operator<<( std::ostream &, point );
};

inline int divide_round_to_minus_infinity( int n, int d )
{
    // The NOLINT comments here are to suppress a clang-tidy warning that seems
    // to be a clang-tidy bug.  I'd like to get rid of them if the bug is ever
    // fixed.  The warning comes via a project_remain call in
    // mission_companion.cpp.
    if( n >= 0 ) {
        return n / d; // NOLINT(clang-analyzer-core.DivideZero)
    }
    return ( n - d + 1 ) / d; // NOLINT(clang-analyzer-core.DivideZero)
}

inline point multiply_xy( point p, int f )
{
    return point( p.x * f, p.y * f );
}

inline point divide_xy_round_to_minus_infinity( point p, int d )
{
    return point( divide_round_to_minus_infinity( p.x, d ),
                  divide_round_to_minus_infinity( p.y, d ) );
}

// NOLINTNEXTLINE(cata-xy)
struct tripoint {
    static constexpr int dimension = 3;

    int x = 0;
    int y = 0;
    int z = 0;
    constexpr tripoint() = default;
    constexpr tripoint( int X, int Y, int Z ) : x( X ), y( Y ), z( Z ) {}
    constexpr tripoint( point p, int Z ) : x( p.x ), y( p.y ), z( Z ) {}

    constexpr tripoint operator+( const tripoint &rhs ) const {
        return tripoint( x + rhs.x, y + rhs.y, z + rhs.z );
    }
    constexpr tripoint operator-( const tripoint &rhs ) const {
        return tripoint( x - rhs.x, y - rhs.y, z - rhs.z );
    }
    tripoint &operator+=( const tripoint &rhs ) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }
    constexpr tripoint operator-() const {
        return tripoint( -x, -y, -z );
    }
    constexpr tripoint operator*( const int rhs ) const {
        return tripoint( x * rhs, y * rhs, z * rhs );
    }
    friend constexpr tripoint operator*( int lhs, const tripoint &rhs ) {
        return rhs * lhs;
    }
    tripoint &operator*=( const int rhs ) {
        x *= rhs;
        y *= rhs;
        z *= rhs;
        return *this;
    }
    constexpr tripoint operator/( const int rhs ) const {
        return tripoint( x / rhs, y / rhs, z / rhs );
    }
    /*** some point operators and functions ***/
    constexpr tripoint operator+( point rhs ) const {
        return tripoint( x + rhs.x, y + rhs.y, z );
    }
    friend constexpr tripoint operator+( point lhs, const tripoint &rhs ) {
        return rhs + lhs;
    }
    constexpr tripoint operator-( point rhs ) const {
        return tripoint( x - rhs.x, y - rhs.y, z );
    }
    tripoint &operator+=( point rhs ) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    tripoint &operator-=( point rhs ) {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
    tripoint &operator-=( const tripoint &rhs ) {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

#ifndef CATA_NO_STL
    tripoint abs() const {
        return tripoint( std::abs( x ), std::abs( y ), std::abs( z ) );
    }
#endif

    constexpr point xy() const {
        return point( x, y );
    }

    /**
     * Rotates just the x,y component of the tripoint. See point::rotate()
     * NOLINTNEXTLINE(cata-use-named-point-constants) */
    tripoint rotate( int turns, const point &dim = { 1, 1 } ) const {
        return tripoint( xy().rotate( turns, dim ), z );
    }

    std::string to_string() const;

    /**
     * Rotate x and y components clockwise @param turns times,
     * 90 degrees per turn, around the center of a rectangle with
     * the dimensions specified by @param dim.
     * By default rotates around the origin (0, 0).
     * NOLINTNEXTLINE(cata-use-named-point-constants) */
    tripoint rotate_2d( int turns, point dim = { 1, 1 } ) const {
        return tripoint( xy().rotate( turns, dim ), z );
    }

    void serialize( JsonOut &jsout ) const;
    void deserialize( JsonIn &jsin );

    friend std::ostream &operator<<( std::ostream &, const tripoint & );

    friend constexpr bool operator==( const tripoint &a, const tripoint &b ) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    friend constexpr bool operator!=( const tripoint &a, const tripoint &b ) {
        return !( a == b );
    }
    friend bool operator<( const tripoint &a, const tripoint &b ) {
        if( a.x != b.x ) {
            return a.x < b.x;
        }
        if( a.y != b.y ) {
            return a.y < b.y;
        }
        if( a.z != b.z ) {
            return a.z < b.z;
        }
        return false;
    }
};

inline tripoint multiply_xy( const tripoint &p, int f )
{
    return tripoint( p.x * f, p.y * f, p.z );
}

inline tripoint divide_xy_round_to_minus_infinity( const tripoint &p, int d )
{
    return tripoint( divide_round_to_minus_infinity( p.x, d ),
                     divide_round_to_minus_infinity( p.y, d ),
                     p.z );
}

static constexpr tripoint tripoint_zero{};
static constexpr point point_zero{};

static constexpr point point_north{ 0, -1 };
static constexpr point point_north_east{ 1, -1 };
static constexpr point point_east{ 1, 0 };
static constexpr point point_south_east{ 1, 1 };
static constexpr point point_south{ 0, 1 };
static constexpr point point_south_west{ -1, 1 };
static constexpr point point_west{ -1, 0 };
static constexpr point point_north_west{ -1, -1 };

static constexpr tripoint tripoint_north{ point_north, 0 };
static constexpr tripoint tripoint_north_east{ point_north_east, 0 };
static constexpr tripoint tripoint_east{ point_east, 0 };
static constexpr tripoint tripoint_south_east{ point_south_east, 0 };
static constexpr tripoint tripoint_south{ point_south, 0 };
static constexpr tripoint tripoint_south_west{ point_south_west, 0 };
static constexpr tripoint tripoint_west{ point_west, 0 };
static constexpr tripoint tripoint_north_west{ point_north_west, 0 };

static constexpr tripoint tripoint_above{ 0, 0, 1 };
static constexpr tripoint tripoint_below{ 0, 0, -1 };

struct sphere {
    int radius = 0;
    tripoint center = tripoint_zero;

    sphere() = default;
    explicit sphere( const tripoint &center ) : radius( 1 ), center( center ) {}
    explicit sphere( const tripoint &center, int radius ) : radius( radius ), center( center ) {}
};

#ifndef CATA_NO_STL

/**
 * Following functions return points in a spiral pattern starting at center_x/center_y until it hits the radius. Clockwise fashion.
 * Credit to Tom J Nowell; http://stackoverflow.com/a/1555236/1269969
 */

namespace detail
{
class spiral_generator_impl
{
    private:
        const point center;
        const int min_dist;
        const int max_dist;
        const int min_edge;
        const int max_edge;
        const int n;
        const bool is_center_included;

        int i;
        int x, y;
        int dx, dy;
        point p;
    public:
        spiral_generator_impl( point center, int min_dist, int max_dist );
        static spiral_generator_impl exhausted( point center, int min_dist, int max_dist );

        explicit operator bool() const noexcept;
        bool next();
        const point &current() const;

        bool operator==( const spiral_generator_impl &other ) const;
};

template<typename _Point, int _Dim = _Point::dimension>
_Point convert_point( point p, _Point ref )
{
    if constexpr( _Dim == 3 ) {
        return _Point{ p.x, p.y, point_traits<_Point>::z( ref ) };
    } else {
        return _Point{ p.x, p.y };
    }
}

} // namespace detail

template<typename _Point>
class spiral_generator
{
    private:
        const _Point center;
        const int min_dist;
        const int max_dist;
    public:
        class iterator
        {
                friend class spiral_generator;
            private:
                detail::spiral_generator_impl generator;
                const _Point origin;
                _Point current;
            public:
                using value_type = _Point;
                using difference_type = std::ptrdiff_t;
                using pointer = value_type *;
                using reference = value_type &;
                using iterator_category = std::forward_iterator_tag;

                iterator( const spiral_generator &g, bool );
                iterator( const spiral_generator &g );
                explicit operator bool() const noexcept;
                iterator &operator++();

                const _Point &operator*() const;

                bool operator==( const iterator &other ) const;
        };

        spiral_generator( _Point center, int min_dist, int max_dist );
        iterator begin() const;
        iterator end() const;

};

template<typename _Point>
inline spiral_generator<_Point> closest_points_generator( _Point center, int min_dist,
        int max_dist )
{
    return spiral_generator<_Point>( center, min_dist, max_dist );
}

template<typename _Point>
inline std::vector<_Point> closest_points_first( _Point center, int min_dist, int max_dist )
{
    auto gen = closest_points_generator( center, min_dist, max_dist );
    return std::vector( gen.begin(), gen.end() );
}

template<typename _Point>
inline spiral_generator<_Point> closest_points_generator( _Point center, int max_dist )
{
    return closest_points_generator( center, 0, max_dist );
}

template<typename _Point>
inline std::vector<_Point> closest_points_first( _Point center, int max_dist )
{
    return closest_points_first( center, 0, max_dist );
}

template<typename _Point>
inline spiral_generator<_Point>::iterator::iterator( const spiral_generator &g, bool )
    : generator( detail::spiral_generator_impl::exhausted( point{ point_traits<_Point>::x( g.center ), point_traits<_Point>::y( g.center ) },
                 g.min_dist, g.max_dist ) )
    , origin( g.center )
    , current( detail::convert_point( generator.current(), g.center ) )
{
}

template<typename _Point>
inline spiral_generator<_Point>::iterator::iterator( const spiral_generator &g )
    : generator( point{ point_traits<_Point>::x( g.center ), point_traits<_Point>::y( g.center ) },
                 g.min_dist, g.max_dist )
    , origin( g.center )
    , current( detail::convert_point( generator.current(), g.center ) )
{

}

template<typename _Point>
inline spiral_generator<_Point>::iterator::operator bool() const noexcept
{
    return static_cast<bool>( generator );
}

template<typename _Point>
inline spiral_generator<_Point>::iterator &spiral_generator<_Point>::iterator::operator++()
{
    if( generator.next() ) {
        current = detail::convert_point( generator.current(), origin );
    }
    return *this;
}

template<typename _Point>
inline bool spiral_generator<_Point>::iterator::operator==( const iterator &other ) const
{
    return  generator == other.generator;
}

template<typename _Point>
inline spiral_generator<_Point>::spiral_generator( _Point center, int min_dist, int max_dist )
    : center( center ), min_dist( min_dist ), max_dist( max_dist )
{
}

template<typename _Point>
inline spiral_generator<_Point>::iterator spiral_generator<_Point>::begin() const
{
    return iterator( *this );
}

template<typename _Point>
inline spiral_generator<_Point>::iterator spiral_generator<_Point>::end() const
{
    return iterator( *this, false );
}

template<typename _Point>
inline const _Point &spiral_generator<_Point>::iterator::operator*() const
{
    return current;
}


static constexpr tripoint tripoint_min { INT_MIN, INT_MIN, INT_MIN };
static constexpr tripoint tripoint_max{ INT_MAX, INT_MAX, INT_MAX };

static constexpr point point_min{ tripoint_min.xy() };
static constexpr point point_max{ tripoint_max.xy() };

// Make point hashable so it can be used as an unordered_set or unordered_map key,
// or a component of one.
namespace std
{
template <>
struct hash<point> {
    std::size_t operator()( point k ) const noexcept {
        constexpr uint64_t a = 2862933555777941757;
        size_t result = k.y;
        result *= a;
        result += k.x;
        return result;
    }
};
} // namespace std

// Make tripoint hashable so it can be used as an unordered_set or unordered_map key,
// or a component of one.
namespace std
{
template <>
struct hash<tripoint> {
    std::size_t operator()( const tripoint &k ) const noexcept {
        constexpr uint64_t a = 2862933555777941757;
        size_t result = k.z;
        result *= a;
        result += k.y;
        result *= a;
        result += k.x;
        return result;
    }
};
} // namespace std

static constexpr std::array<point, 4> four_adjacent_offsets{{
        point_north, point_east, point_south, point_west
    }};

static constexpr std::array<point, 4> four_diagonal_offsets{{
        point_north_east, point_south_east, point_south_west, point_north_west
    }};

static constexpr std::array<point, 8> eight_adjacent_offsets{{
        point_north, point_north_east, point_east, point_south_east,
        point_south, point_south_west, point_west, point_north_west
    }};

static constexpr std::array<point, 4> neighborhood{ {
        point_south, point_east, point_west, point_north
    }};

static constexpr std::array<point, 4> offsets = {{
        point_south, point_east, point_west, point_north
    }
};

static constexpr std::array<point, 4> four_cardinal_directions{{
        point_west, point_east, point_north, point_south
    }};

static constexpr std::array<point, 5> five_cardinal_directions{{
        point_west, point_east, point_north, point_south, point_zero
    }};

static constexpr std::array<tripoint, 6> six_cardinal_directions{{
        tripoint_west, tripoint_east, tripoint_north, tripoint_south,
        tripoint_above, tripoint_below
    }};

static const std::array<tripoint, 8> eight_horizontal_neighbors = { {
        { tripoint_north_west },
        { tripoint_north },
        { tripoint_north_east },
        { tripoint_west },
        { tripoint_east },
        { tripoint_south_west },
        { tripoint_south },
        { tripoint_south_east },
    }
};

#endif // CATA_NO_STL



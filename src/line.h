#pragma once

#include <cmath>
#include <functional>
#include <string>
#include <vector>
#include <algorithm>

#include "math_defines.h"
#include "point.h"
#include "cached_options.h"
#include "units_angle.h"

template <typename T> struct enum_traits;

/**
 * Calculate base of an isosceles triangle
 * @param distance one of the equal lengths
 * @param vertex the unequal angle
 * @returns base in equivalent units to distance
 */
double iso_tangent( double distance, units::angle vertex );

//! This compile-time usable function combines the sign of each (x, y, z) component into a single integer
//! to allow simple runtime and compile-time mapping of (x, y, z) tuples to @ref direction enumerators.
//! Specifically, (0, -, +) => (0, 1, 2); a base-3 number.
//! This only works correctly for inputs between -1,-1,-1 and 1,1,1.
//! For numbers outside that range, use make_xyz().
constexpr unsigned make_xyz_unit( const tripoint &p ) noexcept
{
    return ( ( p.x > 0 ) ? 2u : ( p.x < 0 ) ? 1u : 0u ) * 1u +
           ( ( p.y > 0 ) ? 2u : ( p.y < 0 ) ? 1u : 0u ) * 3u +
           ( ( p.z > 0 ) ? 2u : ( p.z < 0 ) ? 1u : 0u ) * 9u;
}

// This more general version of this function gives correct values for larger inputs.
unsigned make_xyz( const tripoint & );

enum class direction : unsigned {
    ABOVENORTHWEST = make_xyz_unit( tripoint_above + tripoint_north_west ),
    NORTHWEST      = make_xyz_unit( tripoint_north_west ),
    BELOWNORTHWEST = make_xyz_unit( tripoint_below + tripoint_north_west ),
    ABOVENORTH     = make_xyz_unit( tripoint_above + tripoint_north ),
    NORTH          = make_xyz_unit( tripoint_north ),
    BELOWNORTH     = make_xyz_unit( tripoint_below + tripoint_north ),
    ABOVENORTHEAST = make_xyz_unit( tripoint_above + tripoint_north_east ),
    NORTHEAST      = make_xyz_unit( tripoint_north_east ),
    BELOWNORTHEAST = make_xyz_unit( tripoint_below + tripoint_north_east ),

    ABOVEWEST      = make_xyz_unit( tripoint_above + tripoint_west ),
    WEST           = make_xyz_unit( tripoint_west ),
    BELOWWEST      = make_xyz_unit( tripoint_below + tripoint_west ),
    ABOVECENTER    = make_xyz_unit( tripoint_above ),
    CENTER         = make_xyz_unit( tripoint_zero ),
    BELOWCENTER    = make_xyz_unit( tripoint_below ),
    ABOVEEAST      = make_xyz_unit( tripoint_above + tripoint_east ),
    EAST           = make_xyz_unit( tripoint_east ),
    BELOWEAST      = make_xyz_unit( tripoint_below + tripoint_east ),

    ABOVESOUTHWEST = make_xyz_unit( tripoint_above + tripoint_south_west ),
    SOUTHWEST      = make_xyz_unit( tripoint_south_west ),
    BELOWSOUTHWEST = make_xyz_unit( tripoint_below + tripoint_south_west ),
    ABOVESOUTH     = make_xyz_unit( tripoint_above + tripoint_south ),
    SOUTH          = make_xyz_unit( tripoint_south ),
    BELOWSOUTH     = make_xyz_unit( tripoint_below + tripoint_south ),
    ABOVESOUTHEAST = make_xyz_unit( tripoint_above + tripoint_south_east ),
    SOUTHEAST      = make_xyz_unit( tripoint_south_east ),
    BELOWSOUTHEAST = make_xyz_unit( tripoint_below + tripoint_south_east ),

    last = 27
};

template<>
struct enum_traits<direction> {
    static constexpr direction last = direction::last;
};

namespace std
{
template <> struct hash<direction> {
    std::size_t operator()( const direction &d ) const {
        return static_cast<std::size_t>( d );
    }
};
} // namespace std

template< class T >
constexpr direction operator%( const direction &lhs, const T &rhs )
{
    return static_cast<direction>( static_cast<T>( lhs ) % rhs );
}

template< class T >
constexpr T operator+( const direction &lhs, const T &rhs )
{
    return static_cast<T>( lhs ) + rhs;
}

template< class T >
constexpr bool operator==( const direction &lhs, const T &rhs )
{
    return static_cast<T>( lhs ) == rhs;
}

template< class T >
constexpr bool operator==( const T &lhs, const direction &rhs )
{
    return operator==( rhs, lhs );
}

template< class T >
constexpr bool operator!=( const T &lhs, const direction &rhs )
{
    return !operator==( rhs, lhs );
}

template< class T >
constexpr bool operator!=( const direction &lhs, const T &rhs )
{
    return !operator==( lhs, rhs );
}

direction direction_from( point p ) noexcept;
direction direction_from( const tripoint &p ) noexcept;
direction direction_from( point p1, point p2 ) noexcept;
direction direction_from( const tripoint &p, const tripoint &q );

tripoint displace( direction dir );
point displace_XY( direction dir );
std::string direction_name( direction dir );
std::string direction_name_short( direction dir );

/* Get suffix describing vector from p to q (e.g. 1NW, 2SE) or empty string if p == q */
std::string direction_suffix( const tripoint &p, const tripoint &q );

/**
 * The actual Bresenham algorithm in 2D and 3D, everything else should call these
 * and pass in an interact functor to iterate across a line between two points.
 */
void bresenham( point p1, point p2, int t,
                const std::function<bool( point )> &interact );
void bresenham( const tripoint &loc1, const tripoint &loc2, int t, int t2,
                const std::function<bool( const tripoint & )> &interact );

tripoint move_along_line( const tripoint &loc, const std::vector<tripoint> &line,
                          int distance );
// The "t" value decides WHICH Bresenham line is used.
std::vector<point> line_to( point p1, point p2, int t = 0 );
// t and t2 decide which Bresenham line is used.
std::vector<tripoint> line_to( const tripoint &loc1, const tripoint &loc2, int t = 0, int t2 = 0 );
// sqrt(dX^2 + dY^2)

inline int trig_dist_squared( const tripoint &loc1, const tripoint &loc2 )
{
    return ( ( loc1.x - loc2.x ) * ( loc1.x - loc2.x ) ) +
           ( ( loc1.y - loc2.y ) * ( loc1.y - loc2.y ) ) +
           ( ( loc1.z - loc2.z ) * ( loc1.z - loc2.z ) );
}
inline float trig_dist( const tripoint &loc1, const tripoint &loc2 )
{
    return std::sqrt( static_cast<double>( trig_dist_squared( loc1, loc2 ) ) );
}
inline float trig_dist( point loc1, point loc2 )
{
    return trig_dist( tripoint( loc1, 0 ), tripoint( loc2, 0 ) );
}

// Chebyshev distance; maximum of dX and dY
inline int square_dist( const tripoint &loc1, const tripoint &loc2 )
{
    const tripoint d = ( loc1 - loc2 ).abs();
    return std::max( { d.x, d.y, d.z } );
}
inline int square_dist( point loc1, point loc2 )
{
    const point d = ( loc1 - loc2 ).abs();
    return std::max( d.x, d.y );
}

// Choose between the above two according to the "circular distances" option
inline int rl_dist( const tripoint &loc1, const tripoint &loc2 )
{
    if( trigdist ) {
        return trig_dist( loc1, loc2 );
    }
    return square_dist( loc1, loc2 );
}
inline int rl_dist( point a, point b )
{
    return rl_dist( tripoint( a, 0 ), tripoint( b, 0 ) );
}

/**
 * Helper type for the return value of dist_fast().
 *
 * This lets us delay the sqrt() call of trigdist until the actual length is needed.
 */
struct FastDistanceApproximation {
    private:
        int value;
    public:
        FastDistanceApproximation( int value ) : value( value ) { }
        template<typename T>
        bool operator<=( const T &rhs ) const {
            if( trigdist ) {
                return value <= rhs * rhs;
            }
            return value <= rhs;
        }
        template<typename T>
        bool operator>=( const T &rhs ) const {
            if( trigdist ) {
                return value >= rhs * rhs;
            }
            return value >= rhs;
        }
        operator int() const {
            if( trigdist ) {
                return std::sqrt( value );
            }
            return value;
        }
};

inline FastDistanceApproximation trig_dist_fast( const tripoint &loc1, const tripoint &loc2 )
{
    return ( loc1.x - loc2.x ) * ( loc1.x - loc2.x ) +
           ( loc1.y - loc2.y ) * ( loc1.y - loc2.y ) +
           ( loc1.z - loc2.z ) * ( loc1.z - loc2.z );
}
inline FastDistanceApproximation square_dist_fast( const tripoint &loc1, const tripoint &loc2 )
{
    const tripoint d = ( loc1 - loc2 ).abs();
    return std::max( { d.x, d.y, d.z } );
}
inline FastDistanceApproximation rl_dist_fast( const tripoint &loc1, const tripoint &loc2 )
{
    if( trigdist ) {
        return trig_dist_fast( loc1, loc2 );
    }
    return square_dist_fast( loc1, loc2 );
}
inline FastDistanceApproximation rl_dist_fast( point a, point b )
{
    return rl_dist_fast( tripoint( a, 0 ), tripoint( b, 0 ) );
}

float rl_dist_exact( const tripoint &loc1, const tripoint &loc2 );
// Sum of distance in both axes
int manhattan_dist( point loc1, point loc2 );

// Travel distance between 2 points on a square grid, assuming diagonal moves
// cost sqrt(2) and cardinal moves cost 1.
int octile_dist( point loc1, point loc2, int multiplier = 1 );
float octile_dist_exact( point loc1, point loc2 );

// get angle of direction represented by point
units::angle atan2( point );

// Get the magnitude of the slope ranging from 0.0 to 1.0
float get_normalized_angle( point start, point end );
std::vector<tripoint> continue_line( const std::vector<tripoint> &line, int distance );
std::vector<point> squares_in_direction( point p1, point p2 );
// Returns a vector of squares adjacent to @from that are closer to @to than @from is.
std::vector<tripoint> squares_closer_to( const tripoint &from, const tripoint &to );
void calc_ray_end( units::angle, int range, const tripoint &p, tripoint &out );
/**
 * Calculates the horizontal angle between the lines from (0,0,0) to @p a and
 * the line from (0,0,0) to @p b.
 * Returned value is in degree and in the range 0....360.
 * Example: if @p a is (0,1) and @p b is (1,0), the result will 90 degree
 * The function currently ignores the z component.
 */
units::angle coord_to_angle( const tripoint &a, const tripoint &b );



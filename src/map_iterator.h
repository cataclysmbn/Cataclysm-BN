#pragma once

#include <cstddef>
#include <ranges>

#include "point.h"
#include "point_traits.h"

template<typename Tripoint>
class tripoint_range : public std::ranges::view_interface<tripoint_range<Tripoint>>
{
        static_assert( Tripoint::dimension == 3, "Requires tripoint type" );
    private:
        using traits = point_traits<Tripoint>;

        Tripoint minp;
        Tripoint maxp;

    public:
        /**
         * Generates points in a rectangle.
         */
        class point_generator
        {
                friend class tripoint_range;
            private:
                Tripoint p;
                Tripoint range_min;
                Tripoint range_max;

            public:
                using value_type = Tripoint;
                using difference_type = std::ptrdiff_t;
                using pointer = const Tripoint *;
                using reference = const Tripoint &;
                using iterator_category = std::forward_iterator_tag;
                using iterator_concept = std::forward_iterator_tag;

                point_generator() = default;

                point_generator( const Tripoint &_p, const tripoint_range *_range )
                    : p( _p ) {
                    if( _range ) {
                        range_min = _range->minp;
                        range_max = _range->maxp;
                    }
                }

                // Increment x, then if it goes outside range, "wrap around" and increment y
                // Same for y and z
                auto operator++() -> point_generator& { // *NOPAD*
                    traits::x( p )++;
                    if( traits::x( p ) <= traits::x( range_max ) ) {
                        return *this;
                    }

                    traits::y( p )++;
                    traits::x( p ) = traits::x( range_min );
                    if( traits::y( p ) <= traits::y( range_max ) ) {
                        return *this;
                    }

                    traits::z( p )++;
                    traits::y( p ) = traits::y( range_min );
                    return *this;
                }

                auto operator++( int ) -> point_generator {
                    auto tmp = *this;
                    ++( *this );
                    return tmp;
                }

                auto operator*() const -> reference { return p; }

                auto operator==( const point_generator &other ) const -> bool {
                    return p == other.p;
                }
        };

        using iterator = point_generator;
        using value_type = typename point_generator::value_type;
        using difference_type = typename point_generator::difference_type;
        using pointer = typename point_generator::pointer;
        using reference = typename point_generator::reference;
        using iterator_category = typename point_generator::iterator_category;

        tripoint_range( const Tripoint &_minp, const Tripoint &_maxp ) :
            minp( _minp ), maxp( _maxp ) {
        }

        auto begin() const -> point_generator {
            return point_generator( minp, this );
        }

        auto end() const -> point_generator {
            // Return the point AFTER the last one
            // That is, point under (in z-levels) the first one, but one z-level below the last one
            return point_generator( Tripoint( minp.xy(), traits::z( maxp ) + 1 ), this );
        }

        auto size() const -> size_t {
            Tripoint range( maxp - minp );
            return std::max( ++traits::x( range ) * ++traits::y( range ) * ++traits::z( range ), 0 );
        }

        bool is_point_inside( const Tripoint &point ) const {
            for( const Tripoint &current : *this ) {
                if( current == point ) {
                    return true;
                }
            }
            return false;
        }

        const Tripoint &min() const {
            return minp;
        }
        const Tripoint &max() const {
            return maxp;
        }
};

template<typename Tripoint>
inline tripoint_range<Tripoint> points_in_radius( const Tripoint &center, const int radius,
        const int radiusz = 0 )
{
    static_assert( Tripoint::dimension == 3, "Requires tripoint type" );
    const tripoint offset( radius, radius, radiusz );
    return tripoint_range<Tripoint>( center - offset, center + offset );
}

// C++20 ranges compatibility verification
static_assert( std::forward_iterator<tripoint_range<tripoint>::point_generator> );
static_assert( std::ranges::forward_range<tripoint_range<tripoint>> );
static_assert( std::ranges::view<tripoint_range<tripoint>> );

// 2D analog of tripoint_range for any Point type with dimension == 2.
// Iterates all points in [minp, maxp] (inclusive both ends), x-major order.
template<typename Point>
class point_range : public std::ranges::view_interface<point_range<Point>>
{
        static_assert( Point::dimension == 2, "Requires 2D point type" );
    private:
        using traits = point_traits<Point>;

        Point minp;
        Point maxp;

    public:
        class point_generator
        {
                friend class point_range;
            private:
                Point p;
                Point range_min;
                Point range_max;

            public:
                using value_type = Point;
                using difference_type = std::ptrdiff_t;
                using pointer = const Point *;
                using reference = const Point &;
                using iterator_category = std::forward_iterator_tag;
                using iterator_concept = std::forward_iterator_tag;

                point_generator() = default;

                point_generator( const Point &_p, const point_range *_range )
                    : p( _p ) {
                    if( _range ) {
                        range_min = _range->minp;
                        range_max = _range->maxp;
                    }
                }

                // Increment x first; when x exceeds range_max wrap to range_min and increment y.
                auto operator++() -> point_generator & { // *NOPAD*
                    traits::x( p )++;
                    if( traits::x( p ) <= traits::x( range_max ) ) {
                        return *this;
                    }
                    traits::y( p )++;
                    traits::x( p ) = traits::x( range_min );
                    return *this;
                }

                auto operator++( int ) -> point_generator {
                    auto tmp = *this;
                    ++( *this );
                    return tmp;
                }

                auto operator*() const -> reference { return p; }

                auto operator==( const point_generator &other ) const -> bool {
                    return p == other.p;
                }
        };

        using iterator = point_generator;
        using value_type = typename point_generator::value_type;
        using difference_type = typename point_generator::difference_type;
        using pointer = typename point_generator::pointer;
        using reference = typename point_generator::reference;
        using iterator_category = typename point_generator::iterator_category;

        point_range( const Point &_minp, const Point &_maxp ) :
            minp( _minp ), maxp( _maxp ) {}

        auto begin() const -> point_generator {
            return point_generator( minp, this );
        }

        auto end() const -> point_generator {
            // Sentinel: x resets to range_min, y goes one past range_max.
            Point end_p;
            traits::x( end_p ) = traits::x( minp );
            traits::y( end_p ) = traits::y( maxp ) + 1;
            return point_generator( end_p, this );
        }

        auto size() const -> size_t {
            const int w = traits::x( maxp ) - traits::x( minp ) + 1;
            const int h = traits::y( maxp ) - traits::y( minp ) + 1;
            return static_cast<size_t>( std::max( w * h, 0 ) );
        }

        const Point &min() const { return minp; }
        const Point &max() const { return maxp; }
};

// C++20 ranges compatibility verification for point_range
static_assert( std::forward_iterator<point_range<point>::point_generator> );
static_assert( std::ranges::forward_range<point_range<point>> );
static_assert( std::ranges::view<point_range<point>> );



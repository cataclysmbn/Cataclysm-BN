#include "drawing_primitives.h"

#include <functional>
#include <vector>
#include <utility>

#include "coordinates.h"
#include "line.h"
#include "rng.h"
#include "point.h"
#include "point_float.h"

template<typename Point, coords::origin Origin, coords::scale Scale>
void draw_line( const std::function<void( coords::coord_point<Point, Origin, Scale> )> &set,
                coords::coord_point<Point, Origin, Scale> p1, coords::coord_point<Point, Origin, Scale> p2 )
{
    std::vector<coords::coord_point<Point, Origin, Scale>> line = line_to( p1, p2 );
    for( auto &i : line ) {
        set( i );
    }
    set( p1 );
}

template<typename Point, coords::origin Origin, coords::scale Scale>
void draw_square( const std::function<void( coords::coord_point<Point, Origin, Scale> )> &set,
                  coords::coord_point<Point, Origin, Scale> p1, coords::coord_point<Point, Origin, Scale> p2 )
{
    if( p1.x() > p2.x() ) {
        std::swap( p1.x(), p2.x() );
    }
    if( p1.y() > p2.y() ) {
        std::swap( p1.y(), p2.y() );
    }
    for( int x = p1.x(); x <= p2.x(); x++ ) {
        for( int y = p1.y(); y <= p2.y(); y++ ) {
            set( coords::coord_point<Point, Origin, Scale>( x, y ) );
        }
    }
}

template<typename Point, coords::origin Origin, coords::scale Scale>
void draw_rough_circle( const std::function<void( coords::coord_point<Point, Origin, Scale> )> &set,
                        coords::coord_point<Point, Origin, Scale> p, int rad )
{
    for( int i = p.x() - rad; i <= p.x() + rad; i++ ) {
        for( int j = p.y() - rad; j <= p.y() + rad; j++ ) {
            if( trig_dist( p, coords::coord_point<Point, Origin, Scale>( i, j ) ) + rng( 0, 3 ) <= rad ) {
                set( coords::coord_point<Point, Origin, Scale>( i, j ) );
            }
        }
    }
}

template<typename Point, coords::origin Origin, coords::scale Scale>
void draw_circle( const std::function<void( coords::coord_point<Point, Origin, Scale> )> &set,
                  const rl_vec2d &p, double rad )
{
    for( int i = p.x - rad - 1; i <= p.x + rad + 1; i++ ) {
        for( int j = p.y - rad - 1; j <= p.y + rad + 1; j++ ) {
            if( ( p.x - i ) * ( p.x - i ) + ( p.y - j ) * ( p.y - j ) <= rad * rad ) {
                set( coords::coord_point<Point, Origin, Scale>( i, j ) );
            }
        }
    }
}

template<typename Point, coords::origin Origin, coords::scale Scale>
void draw_circle( const std::function<void( coords::coord_point<Point, Origin, Scale> )> &set,
                  coords::coord_point<Point, Origin, Scale> p, int rad )
{
    for( int i = p.x() - rad; i <= p.x() + rad; i++ ) {
        for( int j = p.y() - rad; j <= p.y() + rad; j++ ) {
            if( trig_dist( p, coords::coord_point<Point, Origin, Scale>( i, j ) ) <= rad ) {
                set( coords::coord_point<Point, Origin, Scale>( i, j ) );
            }
        }
    }
}

template void draw_rough_circle( const std::function<void( point_bub_ms )> &, point_bub_ms, int );

void draw_line( const std::function<void( point )> &set, point p1, point p2 )
{
    std::vector<point> line = line_to( p1, p2, 0 );
    for( auto &i : line ) {
        set( i );
    }
    set( p1 );
}

void draw_square( const std::function<void( point )> &set, point p1, point p2 )
{
    if( p1.x > p2.x ) {
        std::swap( p1.x, p2.x );
    }
    if( p1.y > p2.y ) {
        std::swap( p1.y, p2.y );
    }
    for( int x = p1.x; x <= p2.x; x++ ) {
        for( int y = p1.y; y <= p2.y; y++ ) {
            set( point( x, y ) );
        }
    }
}

void draw_rough_circle( const std::function<void( point )> &set, point p, int rad )
{
    for( int i = p.x - rad; i <= p.x + rad; i++ ) {
        for( int j = p.y - rad; j <= p.y + rad; j++ ) {
            if( trig_dist( p, point( i, j ) ) + rng( 0, 3 ) <= rad ) {
                set( point( i, j ) );
            }
        }
    }
}

void draw_circle( const std::function<void( point )> &set, const rl_vec2d &p, double rad )
{
    for( int i = p.x - rad - 1; i <= p.x + rad + 1; i++ ) {
        for( int j = p.y - rad - 1; j <= p.y + rad + 1; j++ ) {
            if( ( p.x - i ) * ( p.x - i ) + ( p.y - j ) * ( p.y - j ) <= rad * rad ) {
                set( point( i, j ) );
            }
        }
    }
}

void draw_circle( const std::function<void( point )> &set, point p, int rad )
{
    for( int i = p.x - rad; i <= p.x + rad; i++ ) {
        for( int j = p.y - rad; j <= p.y + rad; j++ ) {
            if( trig_dist( p, point( i, j ) ) <= rad ) {
                set( point( i, j ) );
            }
        }
    }
}

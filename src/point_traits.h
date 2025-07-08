#pragma once

#include <type_traits>

struct point;
struct tripoint;
struct rl_vec2d;
struct rl_vec3d;

template<typename Point, typename = void>
struct point_traits {
    static int &x( Point &p ) {
        return p.x();
    }
    static int x( const Point &p ) {
        return p.x();
    }
    static int &y( Point &p ) {
        return p.y();
    }
    static int y( const Point &p ) {
        return p.y();
    }
    static int &z( Point &p ) {
        return p.z();
    }
    static int z( const Point &p ) {
        return p.z();
    }
};

template<typename Point>
struct point_traits <
    Point,
    std::enable_if_t < std::is_same_v<Point, point> || std::is_same_v<Point, tripoint> >
    >  {
    static int &x( Point &p ) {
        return p.x;
    }
    static const int &x( const Point &p ) {
        return p.x;
    }
    static int &y( Point &p ) {
        return p.y;
    }
    static const int &y( const Point &p ) {
        return p.y;
    }
    static int &z( Point &p ) {
        return p.z;
    }
    static const int &z( const Point &p ) {
        return p.z;
    }
};

template<typename Point>
struct point_traits <
    Point,
    std::enable_if_t < std::is_same_v<Point, rl_vec2d> || std::is_same_v<Point, rl_vec3d> >
    >  {
    static float &x( Point &p ) {
        return p.x;
    }
    static const float &x( const Point &p ) {
        return p.x;
    }
    static float &y( Point &p ) {
        return p.y;
    }
    static const float &y( const Point &p ) {
        return p.y;
    }
    static float &z( Point &p ) {
        return p.z;
    }
    static const float &z( const Point &p ) {
        return p.z;
    }
    // TODO: Template
    constexpr static float &at( Point &p, std::size_t i ) {
        switch( i ) {
            case 0:
                return x( p );
            case 1:
                return y( p );
            case 2:
                return z( p );
        }
        // Template would make this impossible
        return x( p );
    }
    constexpr static const float &at( const Point &p, std::size_t i ) {
        switch( i ) {
            case 0:
                return x( p );
            case 1:
                return y( p );
            case 2:
                return z( p );
        }
        return x( p );
    }
};



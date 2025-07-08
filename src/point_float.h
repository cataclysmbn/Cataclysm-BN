#pragma once

#include "point_traits.h"

struct point;
struct tripoint;

struct rl_vec2d {
    static constexpr int dimension = 2;
    float x;
    float y;

    explicit rl_vec2d( float x = 0, float y = 0 ) : x( x ), y( y ) {}
    template<typename Point, typename Traits = point_traits<Point>>
    constexpr explicit rl_vec2d( const Point &p ) : x( Traits::x( p ) ), y( Traits::y( p ) ) {}

    float magnitude() const;
    rl_vec2d normalized() const;
    rl_vec2d rotated( float angle ) const;
    float dot_product( const rl_vec2d &v ) const;
    bool is_null() const;

    point as_point() const;

    // scale.
    rl_vec2d operator* ( float rhs ) const;
    rl_vec2d operator/ ( float rhs ) const;
    // subtract
    rl_vec2d operator- ( const rl_vec2d &rhs ) const;
    // unary negation
    rl_vec2d operator- () const;
    rl_vec2d operator+ ( const rl_vec2d &rhs ) const;
};

struct rl_vec3d {
    static constexpr int dimension = 3;
    float x;
    float y;
    float z;

    constexpr explicit rl_vec3d( float x = 0, float y = 0, float z = 0 ) : x( x ), y( y ), z( z ) {}
    template<typename Point, typename Traits = point_traits<Point>>
    constexpr explicit rl_vec3d( const Point &p ) : x( Traits::x( p ) ), y( Traits::y( p ) ),
        z( Traits::z( p ) ) {}

    float magnitude() const;
    rl_vec3d normalized() const;
    rl_vec3d rotated( float angle ) const;
    float dot_product( const rl_vec3d &v ) const;
    rl_vec3d cross_product( const rl_vec3d &v ) const;
    bool is_null() const;

    tripoint as_point() const;

    // scale.
    constexpr rl_vec3d operator* ( float rhs ) const {
        rl_vec3d ret;
        ret.x = x * rhs;
        ret.y = y * rhs;
        ret.z = z * rhs;
        return ret;
    }
    constexpr rl_vec3d operator/ ( float rhs ) const {
        rl_vec3d ret;
        ret.x = x / rhs;
        ret.y = y / rhs;
        ret.z = z / rhs;
        return ret;
    }
    // subtract
    constexpr rl_vec3d operator- ( const rl_vec3d &rhs ) const {
        rl_vec3d ret;
        ret.x = x - rhs.x;
        ret.y = y - rhs.y;
        ret.z = z - rhs.z;
        return ret;
    }
    // unary negation
    constexpr rl_vec3d operator- () const {
        rl_vec3d ret;
        ret.x = -x;
        ret.y = -y;
        ret.z = -z;
        return ret;
    }
    constexpr rl_vec3d operator+ ( const rl_vec3d &rhs ) const {
        rl_vec3d ret;
        ret.x = x + rhs.x;
        ret.y = y + rhs.y;
        ret.z = z + rhs.z;
        return ret;
    }
    friend constexpr bool operator==( const rl_vec3d &a, const rl_vec3d &b ) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
    friend constexpr bool operator!=( const rl_vec3d &a, const rl_vec3d &b ) {
        return !( a == b );
    }
    friend constexpr bool operator<( const rl_vec3d &a, const rl_vec3d &b ) {
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



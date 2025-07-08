#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <vector>
#include "cata_utility.h"
#include "cuboid_rectangle.h"
#include "line.h"
#include "matrix_math.h"
#include "shape.h"
#include "point.h"
#include "point_float.h"
#include "map_iterator.h"
#include "make_static.h"
#include "json.h"
#include "units_angle.h"

template<typename T>
constexpr T sign( T x )
{
    return x > 0 ? 1 : ( x < 0 ? -1 : 0 );
}

class shape_impl
{
    public:
        virtual ~shape_impl() = default;
        virtual double signed_distance( const rl_vec3d &p ) const = 0;

        virtual inclusive_cuboid<rl_vec3d> bounding_box() const = 0;
};

/**
 * Cone centered along y axis.
 * Tip is at zero, base at (0, -h, 0).
 * A 90 degree half angle will cause zero division.
 */
class cone : public shape_impl
{
    private:
        double h;
        double sin_angle;
        double cos_angle;

    public:
        cone( units::angle half_angle, double h )
            : h( h )
            , sin_angle( sin( half_angle ) )
            , cos_angle( cos( half_angle ) )
        {}

        double signed_distance( const rl_vec3d &p ) const override {
            // Based on https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm
            rl_vec2d q( h * sin_angle / cos_angle, -h );
            rl_vec2d w( std::sqrt( p.x * p.x + p.z * p.z ), p.y );
            rl_vec2d a = w - q * clamp( w.dot_product( q ) / q.dot_product( q ), 0.0f, 1.0f );
            rl_vec2d b = w - rl_vec2d( q.x * clamp( w.x / q.x, 0.0f, 1.0f ), q.y );
            float k = q.y >= 0.0f ? 1.0f : -1.0f;
            float d = std::min( a.dot_product( a ), b.dot_product( b ) );
            float s = std::max( k * ( w.x * q.y - w.y * q.x ), k * ( w.y - q.y ) );
            return std::sqrt( d ) * sign( s );
        }

        inclusive_cuboid<rl_vec3d> bounding_box() const override {
            return inclusive_cuboid<rl_vec3d>( rl_vec3d( -sin_angle * h, -h, 0 ),
                                               rl_vec3d( sin_angle * h, 0, 0 ) );
        }
};

class cylinder : public shape_impl
{
    private:
        double half_length;
        double radius;

    public:
        cylinder( double length, double radius )
            : half_length( 0.5 * length )
            , radius( radius )
        {}

        double signed_distance( const rl_vec3d &p ) const override {
            // https://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm
            // Plus adjustments (and fix!)
            double dx = std::sqrt( p.x * p.x + p.z * p.z ) - radius;
            double dy = std::abs( p.y + half_length ) - half_length;
            double lx = std::max( dx, 0.0 );
            double ly = std::max( dy, 0.0 );

            return std::min( std::max( dx, dy ), 0.0 ) + std::sqrt( lx * lx + ly * ly );
        }

        inclusive_cuboid<rl_vec3d> bounding_box() const override {
            return inclusive_cuboid<rl_vec3d>( rl_vec3d( radius, -2 * half_length, 0 ),
                                               rl_vec3d( radius, 0, 0 ) );
        }
};

class empty_shape : public shape_impl
{
    public:
        empty_shape() = default;

        double signed_distance( const rl_vec3d & ) const override {
            return 0.0;
        }

        inclusive_cuboid<rl_vec3d> bounding_box() const override {
            return inclusive_cuboid<rl_vec3d>( rl_vec3d(), rl_vec3d() );
        }
};

/**
 * Offsets a shape by a point.
 * Actually offsets the point by minus point.
 */
class offset_shape : public shape_impl
{
    public:
        offset_shape( const std::shared_ptr<shape_impl> &shape, const rl_vec3d &offset )
            : shape( shape ), offset( offset )
        {}

        double signed_distance( const rl_vec3d &p ) const override {
            return shape->signed_distance( p - offset );
        }

        inclusive_cuboid<rl_vec3d> bounding_box() const override {
            const inclusive_cuboid<rl_vec3d> &bb = shape->bounding_box();
            return inclusive_cuboid<rl_vec3d>( bb.p_min + offset, bb.p_max + offset );
        }

    private:
        std::shared_ptr<shape_impl> shape;
        rl_vec3d offset;
};

/**
 * Rotates a shape around z axis by a given angle.
 * Actually rotates input point around z axis by minus angle.
 */
class rotate_z_shape : public shape_impl
{
    private:
        std::shared_ptr<shape_impl> shape;
        matrix_3d mat;
        matrix_3d mat_inv;

    public:
        rotate_z_shape( const std::shared_ptr<shape_impl> &shape, units::angle angle_z )
            : shape( shape )
            , mat( matrices::rotation_z_axis( -angle_z ) )
            , mat_inv( matrices::rotation_z_axis( angle_z ) )
        {}

        double signed_distance( const rl_vec3d &p ) const override {
            return shape->signed_distance( mat * p );
        }

        inclusive_cuboid<rl_vec3d> bounding_box() const override {
            const inclusive_cuboid<rl_vec3d> &bb = shape->bounding_box();
            const std::set<rl_vec3d> pts = {
                rl_vec3d( bb.p_max.x, bb.p_min.y, bb.p_min.z ),
                rl_vec3d( bb.p_min.x, bb.p_max.y, bb.p_min.z ),
                rl_vec3d( bb.p_max.x, bb.p_max.y, bb.p_min.z ),
                rl_vec3d( bb.p_min.x, bb.p_min.y, bb.p_max.z ),
                rl_vec3d( bb.p_max.x, bb.p_min.y, bb.p_max.z ),
                rl_vec3d( bb.p_min.x, bb.p_max.y, bb.p_max.z )
            };
            rl_vec3d min = mat_inv * bb.p_min;
            rl_vec3d max = mat_inv * bb.p_max;
            for( const rl_vec3d &unrotated : pts ) {
                rl_vec3d v = mat_inv * unrotated;
                min.x = std::min( min.x, v.x );
                min.y = std::min( min.y, v.y );
                min.z = std::min( min.z, v.z );
                max.x = std::max( max.x, v.x );
                max.y = std::max( max.y, v.y );
                max.z = std::max( max.z, v.z );
            }
            return inclusive_cuboid<rl_vec3d>( min, max );
        }
};

class shape_min : public shape_impl
{
    private:
        std::vector<std::shared_ptr<shape_impl>> elements;
    public:

        shape_min( const std::shared_ptr<shape_impl> &l, const std::shared_ptr<shape_impl> &r )
            : elements( {l, r} )
        {}

        double signed_distance( const rl_vec3d &p ) const override {
            double min = HUGE_VAL;
            for( const auto &e : elements ) {
                min = std::min( min, e->signed_distance( p ) );
            }
            return min;
        }

        inclusive_cuboid<rl_vec3d> bounding_box() const override {
            rl_vec3d min;
            rl_vec3d max;
            std::set<rl_vec3d> pts;
            for( const auto &e : elements ) {
                auto bb = e->bounding_box();
                pts.emplace( bb.p_min );
                pts.emplace( bb.p_max );
            }
            for( const rl_vec3d &v : pts ) {
                min.x = std::min( min.x, v.x );
                min.y = std::min( min.y, v.y );
                min.z = std::min( min.z, v.z );
                max.x = std::max( max.x, v.x );
                max.y = std::max( max.y, v.y );
                max.z = std::max( max.z, v.z );
            }
            return inclusive_cuboid<rl_vec3d>( min, max );
        }
};

class shape_exclude_circle : public shape_impl
{
    private:
        std::shared_ptr<shape_impl> base;
        double excluded_radius_squared;
    public:
        shape_exclude_circle( const std::shared_ptr<shape_impl> &base, double excluded_radius )
            : base( base )
            , excluded_radius_squared( excluded_radius * excluded_radius )
        {}

        double signed_distance( const rl_vec3d &p ) const override {
            if( p.dot_product( p ) <= excluded_radius_squared ) {
                return 1.0;
            }

            return base->signed_distance( p );
        }

        inclusive_cuboid<rl_vec3d> bounding_box() const override {
            return base->bounding_box();
        }
};

class shape_factory_impl
{
    public:
        virtual std::shared_ptr<shape> create( const rl_vec3d &start, const rl_vec3d &end ) const = 0;
        virtual const std::string &get_type() const = 0;
        virtual ~shape_factory_impl() = default;

        virtual double get_range() const = 0;
        virtual std::string get_description() const = 0;

        virtual void serialize( JsonOut & ) const {};
        virtual void deserialize( JsonIn & ) {};
};

class cone_factory : public shape_factory_impl
{
    private:
        /** TODO: Angles deserve a proper unit type */
        units::angle half_angle;
        double length;
    public:
        cone_factory() = default;
        cone_factory( const cone_factory & ) = default;
        cone_factory( units::angle half_angle, double length )
            : half_angle( half_angle )
            , length( length )
        {}

        std::shared_ptr<shape> create( const rl_vec3d &start, const rl_vec3d &end ) const override {
            std::shared_ptr<cone> c = std::make_shared<cone>( half_angle, length );
            // Very thin cones may lack points close to origin after discretization, so let's hack it
            std::shared_ptr<cylinder> cyl = std::make_shared<cylinder>( length - 1.0, 0.5 );
            // Offset to prevent it reaching backwards when rotated
            std::shared_ptr<shape_impl> offset_cyl = std::make_shared<offset_shape>( cyl,
                    rl_vec3d( 0.0, -0.5, 0.0 ) );
            auto mindist = std::make_shared<shape_min>( c, offset_cyl );
            auto mindist_minus_origin = std::make_shared<shape_exclude_circle>( mindist, 0.5 );
            rl_vec3d diff = end - start;
            // Plus 90 deg because @ref cone extends in -y direction
            units::angle rotation_angle = units::atan2( diff.y, diff.x ) + 90_degrees;
            std::shared_ptr<rotate_z_shape> r = std::make_shared<rotate_z_shape>(
                                                    mindist_minus_origin, rotation_angle );
            std::shared_ptr<offset_shape> o = std::make_shared<offset_shape>( r,
                                              rl_vec3d( start.x, start.y, start.z ) );
            return std::make_shared<shape>( o, start.as_point() );
        }

        const std::string &get_type() const override {
            return STATIC( "cone" );
        }

        void serialize( JsonOut &jsout ) const override {
            jsout.start_object();
            jsout.member( "half_angle", static_cast<int>( std::round( units::to_degrees( half_angle ) ) ) );
            jsout.member( "length", length );
        }
        void deserialize( JsonIn &jsin ) override {
            JsonObject jo = jsin.get_object();
            half_angle = units::from_degrees( jo.get_int( "half_angle" ) );
            jo.read( "length", length );
        }

        double get_range() const override {
            return length;
        }

        std::string get_description() const override {
            return string_format( _( "Cone of length <info>%d</info>, apex angle <info>%d</info>" ),
                                  static_cast<int>( length ),
                                  static_cast<int>( std::round( units::to_degrees( half_angle * 2 ) ) ) );
        }
};



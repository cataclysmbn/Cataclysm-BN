#pragma once

#include <map>
#include <memory>

#include "cuboid_rectangle.h"
#include "point.h"
#include "point_float.h"
#include "translations.h"

class shape_impl;
class shape_factory_impl;
class JsonIn;
class JsonOut;

/**
 * Class describing shapes in 3D space. The shapes can cover some points partially.
 */
class shape
{
    public:
        double distance_at( const tripoint &p ) const;
        double distance_at( const rl_vec3d &p ) const;
        /**
         * Approximation of bounding box, guaranteed to contain all points.
         */
        inclusive_cuboid<tripoint> bounding_box() const;
        // TODO: Envelope instead of transformed bb
        inclusive_cuboid<rl_vec3d> bounding_box_float() const;
        const tripoint &get_origin() const;

        shape( const std::shared_ptr<shape_impl> &, const tripoint &origin );
        shape( const shape & );
    private:
        std::shared_ptr<shape_impl> impl;
        tripoint origin;
};

/**
 * A class that produces shapes from two points: origin and destination.
 */
class shape_factory
{
    public:
        shape_factory();
        shape_factory( const shape_factory & );
        ~shape_factory();

        void serialize( JsonOut &jsout ) const;
        void deserialize( JsonIn &jsin );

        std::shared_ptr<shape> create( const tripoint &start, const tripoint &end ) const;
        std::shared_ptr<shape> create( const rl_vec3d &start, const rl_vec3d &end ) const;
        double get_range() const;
        std::string get_description() const;

        shape_factory &operator=( const shape_factory &other );
    private:
        std::shared_ptr<shape_factory_impl> impl;
};



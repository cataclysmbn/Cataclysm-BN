#include <set>

#include "debug.h"
#include "json.h"
#include "make_static.h"
#include "shape.h"
#include "shape_impl.h"
#include "point_float.h"

shape::shape( const shape & ) = default;
shape::shape( const std::shared_ptr<shape_impl> &impl, const tripoint &origin )
    : impl( impl )
    , origin( origin )
{}

const tripoint &shape::get_origin() const
{
    return origin;
}

inclusive_cuboid<tripoint> shape::bounding_box() const
{
    const inclusive_cuboid<rl_vec3d> &bb_float = bounding_box_float();
    const tripoint min = tripoint( std::floor( bb_float.p_min.x ),
                                   std::floor( bb_float.p_min.y ),
                                   std::floor( bb_float.p_min.z ) );
    const tripoint max = tripoint( std::ceil( bb_float.p_max.x ),
                                   std::ceil( bb_float.p_max.y ),
                                   std::ceil( bb_float.p_max.z ) );
    return inclusive_cuboid<tripoint>( min, max );
}
inclusive_cuboid<rl_vec3d> shape::bounding_box_float() const
{
    return impl->bounding_box();
}

double shape::distance_at( const tripoint &p ) const
{
    return distance_at( rl_vec3d( p.x, p.y, p.z ) );
}
double shape::distance_at( const rl_vec3d &p ) const
{
    return impl->signed_distance( p );
}

shape_factory::shape_factory() = default;
shape_factory::shape_factory( std::shared_ptr<shape_factory_impl> impl ) : impl( std::move(
                impl ) ) {}
shape_factory::shape_factory( const shape_factory & ) = default;
shape_factory::~shape_factory() = default;

void shape_factory::serialize( JsonOut &jsout ) const
{
    // TODO: poly_serialized should handle this with some rewriting
    jsout.start_array();
    if( impl != nullptr ) {
        jsout.write( impl->get_type() );
        jsout.start_object();
        impl->serialize( jsout );
        jsout.end_object();
    } else {
        jsout.write_null();
    }
    jsout.end_array();
}

void shape_factory::deserialize( JsonIn &jsin )
{
    jsin.start_array();
    std::string type_string = jsin.get_string();
    if( type_string == "cone" ) {
        impl = std::make_shared<cone_factory>();
        impl->deserialize( jsin );
    } else {
        debugmsg( "Invalid type" );
        impl.reset();
    }
    jsin.end_array();
}

std::shared_ptr<shape> shape_factory::create( const tripoint &start, const tripoint &end ) const
{
    return create( rl_vec3d( start ), rl_vec3d( end ) );
}

std::shared_ptr<shape> shape_factory::create( const rl_vec3d &start, const rl_vec3d &end ) const
{
    if( impl == nullptr ) {
        return std::make_shared<shape>( std::make_shared<empty_shape>(), tripoint_zero );
    }
    return impl->create( start, end );
}

double shape_factory::get_range() const
{
    if( impl == nullptr ) {
        return 0.0;
    }
    return impl->get_range();
}
std::string shape_factory::get_description() const
{
    if( impl == nullptr ) {
        return "BUGGED SHAPE DESCRIPTION";
    }
    return impl->get_description();
}
shape_factory &shape_factory::operator=( const shape_factory &other ) = default;

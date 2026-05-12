#include "catalua_bindings.h"

#include "catalua_bindings_utils.h"
#include "catalua_luna_doc.h"
#include "catalua_luna.h"
#include "coordinates.h"
#include "json.h"
#include "line.h"
#include "point.h"
#include "sol/forward.hpp"

// Register points, not tripoints
template<coords::origin Origin, coords::scale Scale>
void register_point( sol::state &lua )
{
    using Point = coords::coord_point<point, Origin, Scale>;
    using RelPoint = coords::coord_point<point, coords::origin::relative, Scale>;

    sol::usertype<Point> ut = luna::new_usertype<Point>(
                                  lua,
                                  luna::no_bases,
                                  luna::constructors <
                                  Point(),
                                  Point( const point & ),
                                  Point( int, int )
                                  > ()
                              );

    DOC( "Gets x" );
    luna::set_fx( ut, "x", []( Point pt ) -> int & { return pt.x(); } );
    DOC( "Sets x" );
    luna::set_fx( ut, "set_x", []( Point pt, int x ) { pt.x() = x; } );
    DOC( "Gets y" );
    luna::set_fx( ut, "y", []( Point pt ) -> int & { return pt.y(); } );
    DOC( "Sets y" );
    luna::set_fx( ut, "set_y", []( Point pt, int y ) { pt.y() = y; } );

    luna::set_fx( ut, "rotate", &Point::rotate );

    // To string
    // We're using Lua meta function here to make it work seamlessly with native Lua tostring()
    luna::set_fx( ut, sol::meta_function::to_string, &Point::to_string );

    // Equality operator
    // It's defined as inline friend function inside Point class, we can't access it and so have to improvise
    luna::set_fx( ut, sol::meta_function::equal_to, []( const Point & a, const Point & b ) { return a == b; } );

    // Less-then operator
    // Same deal as with equality operator
    luna::set_fx( ut, sol::meta_function::less_than, []( const Point & a, const Point & b ) { return a < b; } );

    // Arithmetic operators
    // Point + RelPoint
    DOC( "Adds point and relative point" );
    luna::set_fx( ut, sol::meta_function::addition, []( const Point & a, const RelPoint & b ) { return a + b; } );
    // Point + point
    DOC( "Adds point and raw point" );
    luna::set_fx( ut, sol::meta_function::addition, []( const Point & a, const point & b ) { return a + b; } );
    // Point - RelPoint
    DOC( "Subtracts point and relative point" );
    luna::set_fx( ut, sol::meta_function::subtraction, []( const Point & a, const RelPoint & b ) { return a - b; } );
    // Point - point
    DOC( "Subtracts point and raw point" );
    luna::set_fx( ut, sol::meta_function::subtraction, []( const Point & a, const point & b ) { return a + b; } );

    reg_serde_functions( ut );

    if constexpr( Origin == coords::origin::relative ) {
        // Point * int
        luna::set_fx( ut, sol::meta_function::multiplication, []( const Point & a, const int &b ) { return a * b; } );
    } else {
        if constexpr( Scale != coords::scale::map_square ) {
            luna::set_fx( ut, "to_ms", []( const Point & a ) {
                return coords::project_to<coords::ms>( a );
            } );
        }
        if constexpr( Scale != coords::scale::submap ) {
            luna::set_fx( ut, "to_sm", []( const Point & a ) {
                return coords::project_to<coords::sm>( a );
            } );
        }
        if constexpr( Scale != coords::scale::overmap_terrain ) {
            luna::set_fx( ut, "to_omt", []( const Point & a ) {
                return coords::project_to<coords::omt>( a );
            } );
        }
        if constexpr( Scale != coords::scale::overmap ) {
            luna::set_fx( ut, "to_om", []( const Point & a ) {
                return coords::project_to<coords::om>( a );
            } );
        }
    }
    luna::set_fx( ut, "rl_dist", []( const Point & a, const Point & b ) {
        return rl_dist( a, b );
    } );
    luna::set_fx( ut, "trig_dist", []( const Point & a, const Point & b ) {
        return trig_dist( a, b );
    } );
    luna::set_fx( ut, "square_dist", []( const Point & a, const Point & b ) {
        return square_dist( a, b );
    } );
}

template<coords::origin Origin, coords::scale Scale>
void register_tripoint( sol::state &lua )
{
    using Point = coords::coord_point<tripoint, Origin, Scale>;
    using RelPoint = coords::coord_point<tripoint, coords::origin::relative, Scale>;

    sol::usertype<Point> ut = luna::new_usertype<Point>(
                                  lua,
                                  luna::no_bases,
                                  luna::constructors <
                                  Point(),
                                  Point( const tripoint & ),
                                  Point( int, int, int )
                                  > ()
                              );

    DOC( "Gets x" );
    luna::set_fx( ut, "x", []( Point pt ) -> int { return pt.x(); } );
    DOC( "Sets x" );
    luna::set_fx( ut, "set_x", []( Point pt, int x ) { pt.x() = x; } );
    DOC( "Gets y" );
    luna::set_fx( ut, "y", []( Point pt ) -> int { return pt.y(); } );
    DOC( "Sets y" );
    luna::set_fx( ut, "set_y", []( Point pt, int y ) { pt.y() = y; } );
    DOC( "Gets z" );
    luna::set_fx( ut, "z", []( Point pt ) -> int { return pt.z(); } );
    DOC( "Sets z" );
    luna::set_fx( ut, "set_z", []( Point pt, int z ) { pt.z() = z; } );

    luna::set_fx( ut, "xy", &Point::xy );

    // I honestly cant tell you why this isn't compiling
    // This is a not this moment problem
    // luna::set_fx( ut, "rotate", &Point::rotate );

    // To string
    // We're using Lua meta function here to make it work seamlessly with native Lua tostring()
    luna::set_fx( ut, sol::meta_function::to_string, &Point::to_string );

    // Equality operator
    // It's defined as inline friend function inside Point class, we can't access it and so have to improvise
    luna::set_fx( ut, sol::meta_function::equal_to, []( const Point & a, const Point & b ) { return a == b; } );

    // Less-then operator
    // Same deal as with equality operator
    luna::set_fx( ut, sol::meta_function::less_than, []( const Point & a, const Point & b ) { return a < b; } );

    // Arithmetic operators
    // Point + RelPoint
    DOC( "Adds point and relative point" );
    luna::set_fx( ut, sol::meta_function::addition, []( const Point & a, const RelPoint & b ) { return a + b; } );
    // Point + point
    DOC( "Adds point and raw point" );
    luna::set_fx( ut, sol::meta_function::addition, []( const Point & a, const point & b ) { return a + b; } );
    // Point - RelPoint
    DOC( "Subtracts point and relative point" );
    luna::set_fx( ut, sol::meta_function::subtraction, []( const Point & a, const RelPoint & b ) { return a - b; } );
    // Point - point
    DOC( "Subtracts point and raw point" );
    luna::set_fx( ut, sol::meta_function::subtraction, []( const Point & a, const point & b ) { return a + b; } );

    reg_serde_functions( ut );

    if constexpr( Origin == coords::origin::relative ) {
        // Point * int
        luna::set_fx( ut, sol::meta_function::multiplication, []( const Point & a, const int &b ) { return a * b; } );
    } else {
        if constexpr( Scale != coords::scale::map_square ) {
            luna::set_fx( ut, "to_ms", []( const Point & a ) {
                return coords::project_to<coords::ms>( a );
            } );
        }
        if constexpr( Scale != coords::scale::submap ) {
            luna::set_fx( ut, "to_sm", []( const Point & a ) {
                return coords::project_to<coords::sm>( a );
            } );
        }
        if constexpr( Scale != coords::scale::overmap_terrain ) {
            luna::set_fx( ut, "to_omt", []( const Point & a ) {
                return coords::project_to<coords::omt>( a );
            } );
        }
        if constexpr( Scale != coords::scale::overmap ) {
            luna::set_fx( ut, "to_om", []( const Point & a ) {
                return coords::project_to<coords::om>( a );
            } );
        }
    }
    luna::set_fx( ut, "rl_dist", []( const Point & a, const Point & b ) {
        return rl_dist( a, b );
    } );
    luna::set_fx( ut, "trig_dist", []( const Point & a, const Point & b ) {
        return trig_dist( a, b );
    } );
    luna::set_fx( ut, "square_dist", []( const Point & a, const Point & b ) {
        return square_dist( a, b );
    } );
}

void cata::detail::reg_point_tripoint( sol::state &lua )
{
    // Points

    // Submap
    register_point<coords::origin::relative, coords::sm>( lua );
    register_point<coords::origin::bubble, coords::sm>( lua );
    register_point<coords::origin::overmap_terrain, coords::sm>( lua );
    register_point<coords::origin::overmap, coords::sm>( lua );
    register_point<coords::origin::abs, coords::sm>( lua );

    // Map Square
    register_point<coords::origin::relative, coords::ms>( lua );
    register_point<coords::origin::bubble, coords::ms>( lua );
    register_point<coords::origin::submap, coords::ms>( lua );
    register_point<coords::origin::overmap_terrain, coords::ms>( lua ); // Do I even need you?
    register_point<coords::origin::abs, coords::ms>( lua );

    // Overmap Terrain
    register_point<coords::origin::relative, coords::omt>( lua );
    register_point<coords::origin::overmap, coords::omt>( lua );
    register_point<coords::origin::abs, coords::omt>( lua );

    // Overmap
    register_point<coords::origin::relative, coords::om>( lua );
    register_point<coords::origin::abs, coords::om>( lua );

    // Tripoints

    // Submap
    register_tripoint<coords::origin::relative, coords::sm>( lua );
    register_tripoint<coords::origin::bubble, coords::sm>( lua );
    register_tripoint<coords::origin::overmap_terrain, coords::sm>( lua );
    register_tripoint<coords::origin::overmap, coords::sm>( lua );
    register_tripoint<coords::origin::abs, coords::sm>( lua );

    // Map Square
    register_tripoint<coords::origin::relative, coords::ms>( lua );
    register_tripoint<coords::origin::bubble, coords::ms>( lua );
    register_tripoint<coords::origin::submap, coords::ms>( lua );
    register_tripoint<coords::origin::overmap_terrain, coords::ms>( lua ); // Do I even need you?
    register_tripoint<coords::origin::abs, coords::ms>( lua );

    // Overmap Terrain
    register_tripoint<coords::origin::relative, coords::omt>( lua );
    register_tripoint<coords::origin::overmap, coords::omt>( lua );
    register_tripoint<coords::origin::abs, coords::omt>( lua );

    // Overmap
    register_tripoint<coords::origin::relative, coords::om>( lua );
    register_tripoint<coords::origin::abs, coords::om>( lua );

    // Register 'point' class to be used in Lua
    {
        sol::usertype<point> ut =
            luna::new_usertype<point>(
                lua,
                luna::no_bases,
                luna::constructors <
                point(),
                point( const point & ),
                point( int, int )
                > ()
            );

        // Members
        luna::set( ut, "x", &point::x );
        luna::set( ut, "y", &point::y );

        // Methods
        luna::set_fx( ut, "abs", &point::abs );
        luna::set_fx( ut, "rotate", &point::rotate );

        // (De-)Serialization
        reg_serde_functions( ut );

        // To string
        // We're using Lua meta function here to make it work seamlessly with native Lua tostring()
        luna::set_fx( ut, sol::meta_function::to_string, &point::to_string );

        // Equality operator
        // It's defined as inline friend function inside point class, we can't access it and so have to improvise
        luna::set_fx( ut, sol::meta_function::equal_to, []( const point & a, const point & b ) { return a == b; } );

        // Less-then operator
        // Same deal as with equality operator
        luna::set_fx( ut, sol::meta_function::less_than, []( const point & a, const point & b ) { return a < b; } );

        // Arithmetic operators
        // point + point
        luna::set_fx( ut, sol::meta_function::addition, &point::operator+ );
        // point - point
        // sol::resolve here makes it possible to specify which overload to use
        luna::set_fx( ut, sol::meta_function::subtraction, sol::resolve< point( point ) const >
                      ( &point::operator- ) );
        // point * int
        luna::set_fx( ut, sol::meta_function::multiplication, &point::operator* );
        // point / float
        luna::set_fx( ut, sol::meta_function::division, &point::operator/ );
        // point / int
        luna::set_fx( ut, sol::meta_function::floor_division, &point::operator/ );
        // -point
        // sol::resolve here makes it possible to specify which overload to use
        luna::set_fx( ut, sol::meta_function::unary_minus,
                      sol::resolve< point() const >( &point::operator- ) );
    }

    // Register 'tripoint' class to be used in Lua
    {
        sol::usertype<tripoint> ut =
            luna::new_usertype<tripoint>(
                lua,
                luna::no_bases,
                luna::constructors <
                tripoint(),
                tripoint( const point &, int ),
                tripoint( const tripoint & ),
                tripoint( int, int, int )
                > ()
            );

        // Members
        luna::set( ut, "x", &tripoint::x );
        luna::set( ut, "y", &tripoint::y );
        luna::set( ut, "z", &tripoint::z );

        // Methods
        luna::set_fx( ut, "abs", &tripoint::abs );
        luna::set_fx( ut, "xy", &tripoint::xy );
        luna::set_fx( ut, "rotate_2d", &tripoint::rotate_2d );

        // (De-)Serialization
        reg_serde_functions( ut );

        // To string
        // We're using Lua meta function here to make it work seamlessly with native Lua tostring()
        luna::set_fx( ut, sol::meta_function::to_string, &tripoint::to_string );

        // Equality operator
        // It's defined as inline friend function inside point class, we can't access it and so have to improvise
        luna::set_fx( ut, sol::meta_function::equal_to, []( const tripoint & a, const tripoint & b ) { return a == b; } );

        // Less-then operator
        // Same deal as with equality operator
        luna::set_fx( ut, sol::meta_function::less_than, []( const tripoint & a, const tripoint & b ) { return a < b; } );

        // Arithmetic operators
        // tripoint + tripoint (overload 1)
        // tripoint + point (overload 2)
        luna::set_fx( ut, sol::meta_function::addition, sol::overload(
                          sol::resolve< tripoint( const tripoint & ) const > ( &tripoint::operator+ ),
                          sol::resolve< tripoint( point ) const > ( &tripoint::operator+ )
                      ) );
        // tripoint - tripoint (overload 1)
        // tripoint - point (overload 2)
        luna::set_fx( ut, sol::meta_function::subtraction, sol::overload(
                          sol::resolve< tripoint( const tripoint & ) const > ( &tripoint::operator- ),
                          sol::resolve< tripoint( point ) const > ( &tripoint::operator- )
                      ) );
        // tripoint * int
        luna::set_fx( ut, sol::meta_function::multiplication, &tripoint::operator* );
        // tripoint / float
        luna::set_fx( ut, sol::meta_function::division, &tripoint::operator/ );
        // tripoint / int
        luna::set_fx( ut, sol::meta_function::floor_division, &tripoint::operator/ );
        // -tripoint
        // sol::resolve here makes it possible to specify which overload to use
        luna::set_fx( ut, sol::meta_function::unary_minus,
                      sol::resolve< tripoint() const >( &tripoint::operator- ) );
    }
}

void cata::detail::reg_coords_library( sol::state &lua )
{
    DOC( "Methods for manipulating coord systems and calculating distance" );
    luna::userlib lib = luna::begin_lib( lua, "coords" );

    luna::set_fx( lib, "ms_to_sm", []( const tripoint & raw ) -> std::tuple<tripoint, point> {
        tripoint_rel_ms fine( raw );
        tripoint_rel_sm rough;
        point_sm_ms remain;
        std::tie( rough, remain ) = coords::project_remain<coords::sm>( fine );
        return std::make_pair( rough.raw(), remain.raw() );
    } );
    luna::set_fx( lib, "ms_to_omt", []( const tripoint & raw ) -> std::tuple<tripoint, point> {
        tripoint_rel_ms fine( raw );
        tripoint_rel_omt rough;
        point_omt_ms remain;
        std::tie( rough, remain ) = coords::project_remain<coords::omt>( fine );
        return std::make_pair( rough.raw(), remain.raw() );
    } );
    luna::set_fx( lib, "ms_to_om", []( const tripoint & raw ) -> std::tuple<point, tripoint> {
        tripoint_rel_ms fine( raw );
        point_rel_om rough;
        coords::coord_point<tripoint, coords::origin::overmap, coords::ms> remain;
        std::tie( rough, remain ) = coords::project_remain<coords::om>( fine );
        return std::make_pair( rough.raw(), remain.raw() );
    } );

    luna::set_fx( lib, "sm_to_ms", []( const tripoint & raw_rough,
    sol::optional<const point &> raw_remain ) -> tripoint {
        tripoint_rel_sm rough( raw_rough );
        point_sm_ms remain( raw_remain.value_or( point_zero ) );
        tripoint_rel_ms fine = coords::project_combine( rough, remain );
        return fine.raw();
    } );
    luna::set_fx( lib, "omt_to_ms", []( const tripoint & raw_rough,
    sol::optional<const point &> raw_remain ) -> tripoint {
        tripoint_rel_omt rough( raw_rough );
        point_omt_ms remain( raw_remain.value_or( point_zero ) );
        tripoint_rel_ms fine = coords::project_combine( rough, remain );
        return fine.raw();
    } );
    luna::set_fx( lib, "om_to_ms", []( const point & raw_rough,
    sol::optional<const tripoint &> raw_remain ) -> tripoint {
        point_rel_om rough( raw_rough );
        coords::coord_point<tripoint, coords::origin::overmap, coords::ms> remain(
            raw_remain.value_or( tripoint_zero )
        );
        tripoint_rel_ms fine = coords::project_combine( rough, remain );
        return fine.raw();
    } );

    luna::set_fx( lib, "rl_dist", sol::overload(
                      sol::resolve<int( const tripoint &, const tripoint & )>( rl_dist ),
                      sol::resolve<int( point, point )>( rl_dist )
                  ) );
    luna::set_fx( lib, "trig_dist", sol::overload(
                      sol::resolve<float( const tripoint &, const tripoint & )>( trig_dist ),
                      sol::resolve<float( point, point )>( trig_dist )
                  ) );
    luna::set_fx( lib, "square_dist", sol::overload(
                      sol::resolve<int( const tripoint &, const tripoint & )>( square_dist ),
                      sol::resolve<int( point, point )>( square_dist )
                  ) );

    luna::finalize_lib( lib );
}

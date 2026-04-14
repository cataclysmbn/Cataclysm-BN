#include "world_type.h"

#include "generic_factory.h"
#include "json.h"
#include "mapdata.h"

namespace
{
generic_factory<world_type> world_type_factory( "world_type" );
const world_type_id default_world_type_id( "default" );
} // namespace

template<>
const world_type &world_type_id::obj() const
{
    return world_type_factory.obj( *this );
}

template<>
bool string_id<world_type>::is_valid() const
{
    return world_type_factory.is_valid( *this );
}

void world_type::load( const JsonObject &jo, const std::string & )
{
    mandatory( jo, was_loaded, "name", name );

    optional( jo, was_loaded, "description", description );
    optional( jo, was_loaded, "region_settings", region_settings_id, "default" );
    optional( jo, was_loaded, "generate_overmap", generate_overmap, true );
    optional( jo, was_loaded, "infinite_bounds", infinite_bounds, true );

    if( jo.has_member( "boundary_terrain" ) ) {
        boundary_terrain = ter_str_id( jo.get_string( "boundary_terrain" ) );
    }

    optional( jo, was_loaded, "simulate_when_inactive", simulate_when_inactive, false );
    optional( jo, was_loaded, "save_prefix", save_prefix, "" );
    optional( jo, was_loaded, "allow_npc_travel", allow_npc_travel, false );
    optional( jo, was_loaded, "allow_vehicle_travel", allow_vehicle_travel, false );

    if( jo.has_member( "parent_dimension" ) ) {
        parent_dimension = world_type_id( jo.get_string( "parent_dimension" ) );
    }
}

void world_type::check() const
{
    if( boundary_terrain && !boundary_terrain->is_valid() ) {
        debugmsg( "World type \"%s\" has invalid boundary_terrain \"%s\"",
                  id.str(), boundary_terrain->str() );
    }

    if( parent_dimension && !parent_dimension->is_valid() ) {
        debugmsg( "World type \"%s\" has invalid parent_dimension \"%s\"",
                  id.str(), parent_dimension->str() );
    }

    // Bounded dimensions should have boundary terrain defined
    if( !infinite_bounds && !boundary_terrain ) {
        debugmsg( "World type \"%s\" has infinite_bounds=false but no boundary_terrain defined",
                  id.str() );
    }
}

void world_types::reset()
{
    world_type_factory.reset();
}

void world_types::finalize_all()
{
    world_type_factory.finalize();
}

const std::vector<world_type> &world_types::get_all()
{
    return world_type_factory.get_all();
}

void world_types::check_consistency()
{
    world_type_factory.check();
}

void world_types::load( const JsonObject &jo, const std::string &src )
{
    world_type_factory.load( jo, src );
}

const world_type_id &world_types::get_default()
{
    return default_world_type_id;
}

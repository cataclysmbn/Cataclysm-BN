#include "world_type.h"

#include "calendar.h"
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

    optional( jo, was_loaded, "save_prefix", save_prefix, "" );
    optional( jo, was_loaded, "allow_npc_travel", allow_npc_travel, false );
    optional( jo, was_loaded, "allow_vehicle_travel", allow_vehicle_travel, false );

    optional( jo, was_loaded, "scale_num", scale_num, 1 );
    optional( jo, was_loaded, "scale_den", scale_den, 1 );
    scale_num = std::max( scale_num, 1 );
    scale_den = std::max( scale_den, 1 );

    optional( jo, was_loaded, "sunrise_summer",  sunrise_summer,  -1 );
    optional( jo, was_loaded, "sunrise_winter",  sunrise_winter,  -1 );
    optional( jo, was_loaded, "sunrise_equinox", sunrise_equinox, -1 );
    optional( jo, was_loaded, "sunset_summer",   sunset_summer,   -1 );
    optional( jo, was_loaded, "sunset_winter",   sunset_winter,   -1 );
    optional( jo, was_loaded, "sunset_equinox",  sunset_equinox,  -1 );
    optional( jo, was_loaded, "permanent_daylight", permanent_daylight, false );
    optional( jo, was_loaded, "permanent_night",    permanent_night,    false );
}

void world_type::check() const
{
    if( boundary_terrain && !boundary_terrain->is_valid() ) {
        debugmsg( "World type \"%s\" has invalid boundary_terrain \"%s\"",
                  id.str(), boundary_terrain->str() );
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

    // Register per-dimension time configs with the calendar system.
    for( const auto &wt : world_type_factory.get_all() ) {
        calendar::dim_time_config cfg;
        cfg.sunrise_summer  = wt.sunrise_summer;
        cfg.sunrise_winter  = wt.sunrise_winter;
        cfg.sunrise_equinox = wt.sunrise_equinox;
        cfg.sunset_summer   = wt.sunset_summer;
        cfg.sunset_winter   = wt.sunset_winter;
        cfg.sunset_equinox  = wt.sunset_equinox;
        cfg.permanent_daylight = wt.permanent_daylight;
        cfg.permanent_night    = wt.permanent_night;
        // Only register if this world_type has any non-default time settings.
        const bool has_time_overrides = cfg.sunrise_summer  >= 0 || cfg.sunrise_winter  >= 0 ||
                                        cfg.sunrise_equinox >= 0 || cfg.sunset_summer   >= 0 ||
                                        cfg.sunset_winter   >= 0 || cfg.sunset_equinox  >= 0 ||
                                        cfg.permanent_daylight || cfg.permanent_night;
        if( has_time_overrides ) {
            calendar::register_dim_time_config( wt.id.str(), cfg );
        }
    }
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

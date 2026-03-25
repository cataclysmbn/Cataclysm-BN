#include "procgen/proc_schema.h"

#include <algorithm>
#include <string>

#include "debug.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "json.h"

namespace
{

generic_factory<proc::schema> proc_schema_factory( "proc schema" );

} // namespace

auto proc::hist_data::load( const JsonObject &jo ) -> void
{
    jo.read( "def", def );
    if( jo.has_array( "ok" ) ) {
        ok.clear();
        auto ok_arr = jo.get_array( "ok" );
        while( ok_arr.has_more() ) {
            ok.push_back( io::string_to_enum<proc::hist>( ok_arr.next_string() ) );
        }
    }
}

auto proc::hist_data::allows( const hist value ) const -> bool
{
    return std::ranges::find( ok, value ) != ok.end();
}

auto proc::lua_data::load( const JsonObject &jo ) -> void
{
    jo.read( "full", full );
    jo.read( "name", name );
    jo.read( "make", make );
    jo.read( "validate", validate );
}

auto proc::slot_data::load( const JsonObject &jo ) -> void
{
    jo.read( "id", id );
    jo.read( "role", role );
    jo.read( "min", min );
    jo.read( "max", max );
    jo.read( "rep", rep );

    if( jo.has_array( "ok" ) ) {
        ok.clear();
        auto ok_arr = jo.get_array( "ok" );
        while( ok_arr.has_more() ) {
            ok.push_back( ok_arr.next_string() );
        }
    }
    if( jo.has_array( "no" ) ) {
        no.clear();
        auto no_arr = jo.get_array( "no" );
        while( no_arr.has_more() ) {
            no.push_back( no_arr.next_string() );
        }
    }
}

auto proc::schema::load( const JsonObject &jo, const std::string & ) -> void
{
    jo.read( "cat", cat );
    jo.read( "res", res );

    if( jo.has_object( "hist" ) ) {
        hist.load( jo.get_object( "hist" ) );
    }
    if( jo.has_object( "lua" ) ) {
        lua.load( jo.get_object( "lua" ) );
    }
    if( jo.has_array( "slot" ) ) {
        slots.clear();
        auto slot_arr = jo.get_array( "slot" );
        while( slot_arr.has_more() ) {
            auto slot = slot_data{};
            slot.load( slot_arr.next_object() );
            slots.push_back( slot );
        }
    }
}

auto proc::schema::check() const -> void
{
    if( res.is_null() ) {
        debugmsg( "proc schema %s is missing res", id.c_str() );
    } else if( !res.is_valid() ) {
        debugmsg( "proc schema %s has invalid res %s", id.c_str(), res.c_str() );
    }

    if( !hist.ok.empty() && !hist.allows( hist.def ) ) {
        debugmsg( "proc schema %s hist.def is not in hist.ok", id.c_str() );
    }

    std::ranges::for_each( slots, [&]( const slot_data & slot ) {
        if( slot.id.is_null() ) {
            debugmsg( "proc schema %s has slot with null id", id.c_str() );
        }
        if( slot.max < slot.min ) {
            debugmsg( "proc schema %s slot %s has max < min", id.c_str(), slot.id.c_str() );
        }
    } );
}

auto proc::load( const JsonObject &jo, const std::string &src ) -> void
{
    proc_schema_factory.load( jo, src );
}

auto proc::check() -> void
{
    proc_schema_factory.check();
}

auto proc::reset() -> void
{
    proc_schema_factory.reset();
}

auto proc::all() -> const std::vector<schema>& // *NOPAD*
{
    return proc_schema_factory.get_all();
}

auto proc::get( const schema_id &id ) -> const schema& // *NOPAD*
{
    return proc_schema_factory.obj( id );
}

auto proc::has( const schema_id &id ) -> bool
{
    return proc_schema_factory.is_valid( id );
}

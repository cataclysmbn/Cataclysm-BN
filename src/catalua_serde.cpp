#include "catalua_serde.h"

#include "catalua_impl.h"
#include "catalua_sol.h"
#include "debug.h"
#include "json.h"
#include "string_formatter.h"

#include <algorithm>
#include <utility>
#include <vector>
#include <stdexcept>

namespace cata
{
void serialize_lua_table_internal( const sol::table &t, JsonOut &jsout,
                                   std::vector<sol::table> &stack );

static void serialize_lua_object( const sol::object &val, JsonOut &jsout,
                                  std::vector<sol::table> &stack )
{
    sol::state_view lua( val.lua_state() );

    jsout.start_object();
    switch( val.get_type() ) {
        case sol::type::boolean: {
            jsout.member_as_string( "type", "bool" );
            jsout.member( "data" );
            jsout.write( val.as<bool>() );
            break;
        }
        case sol::type::number: {
            // There's no clear difference in JSON between int and float,
            // so we have to be explicit to avoid subtle errors down the line
            if( is_number_integer( lua, val ) ) {
                jsout.member_as_string( "type", "int" );
                jsout.member( "data" );
                jsout.write( val.as<int64_t>() );
            } else {
                jsout.member_as_string( "type", "float" );
                jsout.member( "data" );
                jsout.write( val.as<double>() );
            }
            break;
        }
        case sol::type::string: {
            jsout.member_as_string( "type", "string" );
            jsout.member( "data" );
            jsout.write( val.as<std::string>() );
            break;
        }
        case sol::type::table: {
            jsout.member_as_string( "type", "lua_table" );
            jsout.member( "data" );
            serialize_lua_table_internal( val.as<sol::table>(), jsout, stack );
            break;
        }
        case sol::type::userdata: {
            std::optional<std::string> luna_type = get_luna_type( val );
            if( !luna_type ) {
                debugmsg( "Tried to serialize usertype that was not registered with luna." );
                break;
            }
            sol::table table_val = val.as<sol::table>();
            sol::protected_function serialize_func = table_val["serialize"];
            if( !serialize_func ) {
                debugmsg( "Tried to serialize usertype that does not allow serialization." );
                break;
            }
            jsout.member_as_string( "type", "userdata" );
            jsout.member_as_string( "kind", *luna_type );
            jsout.member( "data" );
            sol::protected_function_result res = serialize_func( val, jsout );
            if( res.status() != sol::call_status::ok ) {
                sol::error err = res;
                debugmsg( "Failed to serialize type '%s': %s", *luna_type, err.what() );
            }
            break;
        }
        default: {
            // TODO: throw error
            debugmsg( "Unsupported type encountered when serializing Lua table." );
            break;
        }
    }
    jsout.end_object();
}

void serialize_lua_table_internal( const sol::table &t, JsonOut &jsout,
                                   std::vector<sol::table> &stack )
{
    for( const sol::table &it : stack ) {
        if( it == t ) {
            debugmsg( "Tried to serialize recursive table structure." );
            jsout.write_null();
            return;
        }
    }

    stack.push_back( t );

    sol::state_view lua( t.lua_state() );
    jsout.start_object();

    if( !t.empty() ) {
        jsout.member( "entries" );
        jsout.start_array();

        // TODO: persistent key order?
        t.for_each( [&]( const sol::object & key, const sol::object & val ) {
            serialize_lua_object( key, jsout, stack );
            serialize_lua_object( val, jsout, stack );
        } );

        jsout.end_array();
    }
    jsout.end_object();

    stack.pop_back();
}

void serialize_lua_table( const sol::table &t, JsonOut &jsout )
{
    std::vector<sol::table> stack;
    serialize_lua_table_internal( t, jsout, stack );
}

static sol::object
deserialize_lua_object( sol::state_view lua, const JsonObject &jo )
{
    std::string entry_type = jo.get_member( "type" );
    if( entry_type == "string" ) {
        std::string data = jo.get_string( "data" );
        return sol::object( lua, sol::in_place, data );
    } else if( entry_type == "bool" ) {
        bool data = jo.get_bool( "data" );
        return sol::object( lua, sol::in_place, data );
    } else if( entry_type == "int" ) {
        int data = jo.get_int( "data" );
        return sol::object( lua, sol::in_place, data );
    } else if( entry_type == "float" ) {
        double data = jo.get_float( "data" );
        return sol::object( lua, sol::in_place, data );
    } else if( entry_type == "lua_table" ) {
        JsonObject data = jo.get_object( "data" );
        sol::table new_table = lua.create_table();
        deserialize_lua_table( new_table, data );
        return new_table;
    } else if( entry_type == "userdata" ) {
        std::string kind = jo.get_member( "kind" );
        JsonIn &data_raw = *jo.get_raw( "data" );
        // Horrible hack ahead
        std::string script = string_format( "return %s.new()", kind );
        sol::protected_function_result res = lua.script( script, "deserialize_init", sol::load_mode::any );
        if( res.status() != sol::call_status::ok ) {
            debugmsg( "Failed to init container for deserializable type '%s'", kind );
        } else {
            sol::object obj = res;
            sol::table obj_table = obj.as<sol::table>();
            if( !obj_table ) {
                debugmsg( "Failed to init container for deserializable type '%s'", kind );
            } else {
                sol::protected_function deserialize_func = obj_table["deserialize"];
                if( !deserialize_func ) {
                    debugmsg( "Failed to deserialize type '%s': not deserializable.", kind );
                } else {
                    sol::protected_function_result res = deserialize_func( obj, data_raw );
                    if( res.status() != sol::call_status::ok ) {
                        sol::error err = res;
                        debugmsg( "Failed to deserialize type '%s': %s", kind, err.what() );
                    } else {
                        return obj;
                    }
                }
            }
        }
    } else {
        debugmsg( "Deserialization of record type '%s' is not implemented.", entry_type );
    }
    return sol::nil;
}

void deserialize_lua_table( sol::table t, JsonObject &obj )
{
    sol::state_view lua( t.lua_state() );

    if( !obj.has_array( "entries" ) ) {
        return;
    }

    JsonArray arr = obj.get_array( "entries" );
    if( arr.size() % 2 != 0 ) {
        debugmsg( "invalid array size %d", arr.size() );
        return;
    }
    for( size_t idx = 0; idx < arr.size(); idx += 2 ) {
        JsonObject jo_key = arr.get_object( idx );
        JsonObject jo_val = arr.get_object( idx + 1 );
        sol::object key = deserialize_lua_object( lua, jo_key );
        sol::object val = deserialize_lua_object( lua, jo_val );
        t.set( key, val );
    }
}

} // namespace cata

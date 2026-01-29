#include "catalua_bindings.h"
#include "catalua_bindings_utils.h"
#include "catalua_impl.h"
#include "catalua_luna.h"
#include "catalua_luna_doc.h"

#include "debug.h"
#include "lua_mod_options.h"
#include "options.h"

namespace cata::detail
{

void reg_options_api( sol::state &lua )
{
    DOC( "Mod settings API for registering per-world options" );
    luna::userlib lib = luna::begin_lib( lua, "mod_settings" );

    DOC( "Register a new mod option. Must be called in preload.lua." );
    DOC( "Options will appear in the 'Mod Settings' page during world creation." );
    DOC( "" );
    DOC( "spec is a table with the following fields:" );
    DOC( "  id: string - Unique option identifier (will be prefixed with mod ident)" );
    DOC( "  name: string - Display name (translatable)" );
    DOC( "  tooltip: string - Description text (translatable)" );
    DOC( "  type: string - One of: 'bool', 'int', 'float', 'string_select', 'string_input'" );
    DOC( "  default: any - Default value (type-appropriate)" );
    DOC( "" );
    DOC( "For 'int' type:" );
    DOC( "  min: int - Minimum value" );
    DOC( "  max: int - Maximum value" );
    DOC( "" );
    DOC( "For 'float' type:" );
    DOC( "  min: float - Minimum value" );
    DOC( "  max: float - Maximum value" );
    DOC( "  step: float - Step increment" );
    DOC( "" );
    DOC( "For 'string_select' type:" );
    DOC( "  options: table - Array of {id=string, name=string} choices" );
    DOC( "" );
    DOC( "For 'string_input' type:" );
    DOC( "  max_length: int - Maximum input length" );
    luna::set_fx( lib, "register_option", []( sol::this_state lua_state,
    const sol::table & spec ) -> bool {
        sol::state_view lua( lua_state );

        // Get current mod ident
        sol::object current_mod_obj = lua["game"]["current_mod"];
        if( !current_mod_obj.valid() || current_mod_obj.get_type() != sol::type::string )
        {
            debugmsg( "mod_settings.register_option must be called from a mod context (e.g., preload.lua)" );
            return false;
        }
        const auto mod_ident = current_mod_obj.as<std::string>();

        lua_option_spec opt_spec;

        // Required fields
        auto id_obj = spec.get<sol::optional<std::string>>( "id" );
        if( !id_obj )
        {
            debugmsg( "mod_settings.register_option: 'id' field is required" );
            return false;
        }
        opt_spec.id = *id_obj;

        auto name_obj = spec.get<sol::optional<std::string>>( "name" );
        if( !name_obj )
        {
            debugmsg( "mod_settings.register_option: 'name' field is required" );
            return false;
        }
        opt_spec.name = to_translation( *name_obj );

        auto tooltip_obj = spec.get<sol::optional<std::string>>( "tooltip" );
        if( tooltip_obj )
        {
            opt_spec.tooltip = to_translation( *tooltip_obj );
        }

        auto type_obj = spec.get<sol::optional<std::string>>( "type" );
        if( !type_obj )
        {
            debugmsg( "mod_settings.register_option: 'type' field is required" );
            return false;
        }
        const auto &type_str = *type_obj;

        if( type_str == "bool" )
        {
            opt_spec.type = lua_option_type::boolean;
            opt_spec.default_bool = spec.get_or( "default", false );

        } else if( type_str == "int" )
        {
            opt_spec.type = lua_option_type::integer;
            opt_spec.default_int = spec.get_or( "default", 0 );
            opt_spec.min_int = spec.get_or( "min", 0 );
            opt_spec.max_int = spec.get_or( "max", 100 );

        } else if( type_str == "float" )
        {
            opt_spec.type = lua_option_type::floating;
            opt_spec.default_float = spec.get_or( "default", 0.0f );
            opt_spec.min_float = spec.get_or( "min", 0.0f );
            opt_spec.max_float = spec.get_or( "max", 1.0f );
            opt_spec.step_float = spec.get_or( "step", 0.1f );

        } else if( type_str == "string_select" )
        {
            opt_spec.type = lua_option_type::string_select;
            opt_spec.default_string = spec.get_or<std::string>( "default", "" );

            auto options_obj = spec.get<sol::optional<sol::table>>( "options" );
            if( !options_obj ) {
                debugmsg( "mod_settings.register_option: 'options' field is required for string_select type" );
                return false;
            }

            for( const auto &pair : *options_obj ) {
                if( pair.second.get_type() != sol::type::table ) {
                    continue;
                }
                sol::table choice = pair.second.as<sol::table>();
                lua_option_choice c;
                c.id = choice.get_or<std::string>( "id", "" );
                auto choice_name = choice.get<sol::optional<std::string>>( "name" );
                if( choice_name ) {
                    c.name = to_translation( *choice_name );
                } else {
                    c.name = to_translation( c.id );
                }
                opt_spec.choices.push_back( c );
            }

            if( opt_spec.choices.empty() ) {
                debugmsg( "mod_settings.register_option: 'options' must have at least one choice" );
                return false;
            }

            if( opt_spec.default_string.empty() ) {
                opt_spec.default_string = opt_spec.choices[0].id;
            }

        } else if( type_str == "string_input" )
        {
            opt_spec.type = lua_option_type::string_input;
            opt_spec.default_string = spec.get_or<std::string>( "default", "" );
            opt_spec.max_length = spec.get_or( "max_length", 64 );

        } else
        {
            debugmsg( "mod_settings.register_option: unknown type '%s'", type_str );
            return false;
        }

        return register_lua_option( mod_ident, opt_spec );
    } );

    luna::finalize_lib( lib );

    // Also add game.get_option() for reading option values
    sol::object game_obj = lua.globals()["game"];
    sol::table game_table;
    if( !game_obj.valid() || game_obj.get_type() == sol::type::lua_nil ) {
        game_table = lua.create_table();
        lua.globals()["game"] = game_table;
    } else {
        game_table = game_obj.as<sol::table>();
    }

    DOC( "Get the value of an option by name." );
    DOC( "Returns the option value, or nil if the option doesn't exist." );
    DOC( "For Lua mod options, the full ID is '<mod_ident>.<option_id>'." );
    game_table["get_option"] = []( sol::this_state lua_state,
    const std::string & name ) -> sol::object {
        sol::state_view lua( lua_state );
        auto &opts = get_options();
        if( !opts.has_option( name ) )
        {
            return sol::lua_nil;
        }
        auto &opt = opts.get_option( name );
        const auto &type = opt.getType();
        if( type == "bool" )
        {
            return sol::make_object( lua, opt.value_as<bool>() );
        } else if( type == "int" || type == "int_map" )
        {
            return sol::make_object( lua, opt.value_as<int>() );
        } else if( type == "float" )
        {
            return sol::make_object( lua, opt.value_as<float>() );
        } else
        {
            return sol::make_object( lua, opt.value_as<std::string>() );
        }
    };

    // Add a convenience method to get lua mod options with automatic prefix
    DOC( "Get the value of a Lua mod option." );
    DOC( "Automatically prefixes with '<current_mod>.' if called from mod context." );
    game_table["get_mod_option"] = []( sol::this_state lua_state,
    const std::string & option_id ) -> sol::object {
        sol::state_view lua( lua_state );

        sol::object current_mod_obj = lua["game"]["current_mod"];
        std::string full_id;
        if( current_mod_obj.valid() && current_mod_obj.get_type() == sol::type::string )
        {
            full_id = get_lua_option_id( current_mod_obj.as<std::string>(), option_id );
        } else
        {
            full_id = option_id;
        }

        auto &opts = get_options();
        if( !opts.has_option( full_id ) )
        {
            return sol::lua_nil;
        }
        auto &opt = opts.get_option( full_id );
        const auto &type = opt.getType();
        if( type == "bool" )
        {
            return sol::make_object( lua, opt.value_as<bool>() );
        } else if( type == "int" || type == "int_map" )
        {
            return sol::make_object( lua, opt.value_as<int>() );
        } else if( type == "float" )
        {
            return sol::make_object( lua, opt.value_as<float>() );
        } else
        {
            return sol::make_object( lua, opt.value_as<std::string>() );
        }
    };
}

} // namespace cata::detail

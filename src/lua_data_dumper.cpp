#include "lua_data_dumper.h"

#include <iostream>

#include "catalua_impl.h"
#include "json.h"
#include "lua_table_wrapper.h"

namespace cata
{

std::vector<std::string> get_dumpable_lua_data_types()
{
    return {
        "effect_type",
        "mutation",
        "bionic",
        "talk_topic",
        "finalize_effect_type",
        "finalize_mutation",
        "finalize_bionic",
        "finalize_talk_topic"
    };
}

static void dump_definition_table( sol::state &lua,
                                   const std::string &table_name,
                                   const std::string &type_name,
                                   JsonOut &jsout )
{
    sol::table define = lua["game"]["define"];
    if( !define.valid() ) {
        return;
    }

    sol::object tbl_obj = define[table_name];
    if( !tbl_obj.valid() || !tbl_obj.is<sol::table>() ) {
        return;
    }

    sol::table tbl = tbl_obj.as<sol::table>();

    for( auto it = tbl.begin(); it != tbl.end(); ++it ) {
        const auto ref = *it;
        if( !ref.first.is<std::string>() ) {
            continue;
        }
        if( !ref.second.is<sol::table>() ) {
            continue;
        }

        std::string id = ref.first.as<std::string>();
        sol::table def = ref.second.as<sol::table>();

        jsout.start_object();
        jsout.member( "type", type_name );
        jsout.member( "id", id );

        // Write all fields from the Lua table
        LuaTableWrapper wrapper( def );
        for( auto &pair : def ) {
            std::string key;
            if( pair.first.is<std::string>() ) {
                key = pair.first.as<std::string>();
            } else {
                continue;
            }

            // Skip metadata fields that aren't part of the data definition
            if( key == "copy_from" || key == "abstract" ) {
                continue;
            }

            // Use LuaTableWrapper to serialize the value properly
            jsout.member( key );
            write_lua_value_as_json( jsout, pair.second );
        }

        jsout.end_object();
    }
}

// write_lua_value_as_json is declared in lua_table_wrapper.h

bool dump_lua_data( lua_state &state,
                    const std::vector<std::string> &types,
                    std::ostream &output )
{
    sol::state &lua = state.lua;

    // Mapping from command-line type names to (table_name, json_type_name)
    struct type_info {
        std::string table_name;
        std::string json_type;
    };

    static const std::map<std::string, type_info> type_map = {
        { "effect_type", { "effect_type", "effect_type" } },
        { "mutation", { "mutation", "mutation" } },
        { "bionic", { "bionic", "bionic" } },
        { "talk_topic", { "talk_topic", "talk_topic" } },
        { "finalize_effect_type", { "finalize_effect_type", "effect_type" } },
        { "finalize_mutation", { "finalize_mutation", "mutation" } },
        { "finalize_bionic", { "finalize_bionic", "bionic" } },
        { "finalize_talk_topic", { "finalize_talk_topic", "talk_topic" } },
    };

    // Determine which types to dump
    std::vector<std::string> types_to_dump;
    if( types.empty() ) {
        // Dump all types
        for( const auto &pair : type_map ) {
            types_to_dump.push_back( pair.first );
        }
    } else {
        types_to_dump = types;
    }

    JsonOut jsout( output, true ); // Pretty-printed JSON
    jsout.start_array();

    for( const std::string &type : types_to_dump ) {
        auto it = type_map.find( type );
        if( it == type_map.end() ) {
            std::cerr << "Unknown data type: " << type << "\n";
            std::cerr << "Supported types:";
            for( const auto &t : get_dumpable_lua_data_types() ) {
                std::cerr << " " << t;
            }
            std::cerr << "\n";
            return false;
        }

        dump_definition_table( lua, it->second.table_name, it->second.json_type, jsout );
    }

    jsout.end_array();
    output << "\n";

    return true;
}

} // namespace cata

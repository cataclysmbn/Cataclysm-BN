#include "lua_data_modifier.h"

#include "bionics.h"
#include "catalua_sol.h"
#include "catalua_data_reg.h"
#include "debug.h"
#include "effect.h"
#include "mutation.h"

namespace cata
{

void register_data_modification_api( sol::state &lua )
{
    sol::table gt = lua.globals()["game"];
    sol::table modify = lua.create_table();
    gt["modify"] = modify;

    // game.modify.effect_type(id, modifications)
    modify["effect_type"] = [&lua]( const std::string & id, const sol::table & mod_table ) {
        auto reg_tbl = mod_table;
        if( mod_table.get_or<std::string>( "copy_from", "" ) == "" ) {
            reg_tbl["copy_from"] = id;
        }
        effect_type modified = lua_table_to_effect_type( id, reg_tbl );
        register_lua_effect_type( std::move( modified ) );
    };

    // game.modify.mutation(id, modifications)
    modify["mutation"] = [&lua]( const std::string & id, const sol::table & mod_table ) {
        auto reg_tbl = mod_table;
        if( mod_table.get_or<std::string>( "copy_from", "" ) == "" ) {
            reg_tbl["copy_from"] = id;
        }
        mutation_branch modified = lua_table_to_mutation( id, reg_tbl );
        mutation_branch::register_lua_mutation( std::move( modified ) );
    };

    // game.modify.bionic(id, modifications)
    modify["bionic"] = [&lua]( const std::string & id, const sol::table & mod_table ) {
        auto reg_tbl = mod_table;
        if( mod_table.get_or<std::string>( "copy_from", "" ) == "" ) {
            reg_tbl["copy_from"] = id;
        }
        bionic_data modified = lua_table_to_bionic( id, reg_tbl );
        bionic_data::register_lua_bionic( std::move( modified ) );
    };
}

} // namespace cata

#pragma once
#ifndef CATA_SRC_CATALUA_DATA_REG_H
#define CATA_SRC_CATALUA_DATA_REG_H

#include "catalua_sol_fwd.h"

#include <string>

// Forward declarations for data types
class effect_type;
struct bionic_data;
struct mutation_branch;

namespace cata
{
struct lua_state;

/**
 * Initialize the game.define tables for Lua data definitions.
 * Called during init_global_state_tables().
 */
void init_data_definition_tables( sol::state &lua );

/**
 * Register Lua-defined data from game.define.* tables.
 * Called after preload.lua scripts run, before JSON loading.
 */
void reg_lua_data_definitions( lua_state &state );

/**
 * Register Lua-defined data from game.define.finalize_* tables.
 * Called after finalize.lua scripts run, after JSON loading.
 */
void reg_lua_finalize_definitions( lua_state &state );

} // namespace cata

/**
 * Converter functions for Lua tables to game data types.
 * These are declared at global scope to match their definitions in catalua_data_reg.cpp
 * and are used by lua_data_modifier.cpp for runtime modifications.
 */
effect_type lua_table_to_effect_type( const std::string &id, const sol::table &def );
mutation_branch lua_table_to_mutation( const std::string &id, const sol::table &def );
bionic_data lua_table_to_bionic( const std::string &id, const sol::table &def );

#endif // CATA_SRC_CATALUA_DATA_REG_H

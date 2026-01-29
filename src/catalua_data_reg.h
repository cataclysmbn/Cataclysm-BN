#pragma once
#ifndef CATA_SRC_CATALUA_DATA_REG_H
#define CATA_SRC_CATALUA_DATA_REG_H

#include "catalua_sol_fwd.h"

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

#endif // CATA_SRC_CATALUA_DATA_REG_H

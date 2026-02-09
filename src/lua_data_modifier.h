#pragma once

#include "catalua_sol_fwd.h"

namespace cata
{

/**
 * Register the game.modify API for runtime Lua data modifications.
 * Called during init_global_state_tables().
 *
 * This provides callable functions for modifying existing game data at runtime,
 * usable from any Lua context (hooks, callbacks, main.lua, etc.):
 *
 * Lua usage:
 *   -- Modify an existing effect_type
 *   game.modify.effect_type("poison", {
 *       max_intensity = 20,
 *       proportional = { max_duration = 1.5 }
 *   })
 *
 *   -- Modify a mutation
 *   game.modify.mutation("NIGHTVISION", {
 *       night_vision_range = 10.0
 *   })
 *
 *   -- Modify a bionic
 *   game.modify.bionic("bio_power_storage", {
 *       capacity = "1000 kJ"
 *   })
 *
 * Changes take effect immediately. The modification table is merged with
 * the existing object's data (copy_from is automatically set to the target ID).
 */
void register_data_modification_api( sol::state &lua );

} // namespace cata

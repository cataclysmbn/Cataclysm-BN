gdebug.log_info("Resurrection: preload.")

-- Have to register iuse before data loading.
-- Actual implementation (function mod.iuse_function) will be defined later.

local mod = game.mod_runtime[game.current_mod]

-- Register our item use function

game.iuse_functions["resurrection_anchor_use"] = function(...) return mod.iuse_function_anchor(...) end

table.insert(game.hooks.on_character_death, function(...) return mod.on_character_death_hook(...) end)

table.insert(game.hooks.on_game_load, function(...) return mod.on_game_load_hook(...) end)

table.insert(game.hooks.on_game_save, function(...) return mod.on_game_save_hook(...) end)

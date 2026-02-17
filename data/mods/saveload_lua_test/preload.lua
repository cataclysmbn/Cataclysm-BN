gdebug.log_info("SLT: preload")

local mod = game.mod_runtime[game.current_mod]

table.insert(game.hooks.on_game_load, function(...) return mod.on_game_load_hook(...) end)
table.insert(game.hooks.on_game_save, function(...) return mod.on_game_save_hook(...) end)

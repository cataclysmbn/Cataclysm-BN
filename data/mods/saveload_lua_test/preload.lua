gdebug.log_info("SLT: preload")

local mod = game.mod_runtime[game.current_mod]

game.hooks.on_game_load[#game.hooks.on_game_load + 1] = function(...) return mod.on_game_load_hook(...) end

game.hooks.on_game_save[#game.hooks.on_game_save + 1] = function(...) return mod.on_game_save_hook(...) end

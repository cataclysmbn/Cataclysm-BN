gdebug.log_info("SLT: preload")

local mod = game.mod_runtime[game.current_mod]

game.add_hook("on_game_load", function(...) return mod.on_game_load_hook(...) end)
game.add_hook("on_game_save", function(...) return mod.on_game_save_hook(...) end)

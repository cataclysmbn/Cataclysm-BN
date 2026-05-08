gdebug.log_info("nuclear tear mod loaded")

local mod = game.mod_runtime[game.current_mod]

game.add_hook("on_explosion_start", function(params) return mod.on_explosion(params) end)

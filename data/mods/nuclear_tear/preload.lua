gdebug.log_info("nuclear tear mod loaded")

local mod = game.mod_runtime[game.current_mod]

table.insert(game.hooks.on_explosion_start, function(params) return mod.on_explosion(params) end)

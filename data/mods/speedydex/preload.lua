gdebug.log_info("SpeedyDex: Preload")

---@class ModSpeedyDex
local mod = game.mod_runtime[game.current_mod]

table.insert(game.hooks.on_character_reset_stats, function(...) return mod.speedydex(...) end)

gdebug.log_info("RPG System: Preload")

---@class ModRpgSystem
local mod = game.mod_runtime[game.current_mod]

table.insert(game.hooks.on_mon_death, function(...) return mod.on_monster_killed(...) end)
table.insert(game.hooks.on_game_started, function(...) return mod.on_game_started(...) end)
table.insert(game.hooks.on_game_load, function(...) return mod.on_game_load(...) end)
table.insert(game.hooks.on_character_reset_stats, function(...) return mod.on_character_reset_stats(...) end)
gapi.add_on_every_x_hook(TimeDuration.from_minutes(5), function(...) return mod.on_every_5_minutes(...) end)
game.iuse_functions["RPG_SYSTEM_MENU"] = function(...) return mod.open_rpg_menu(...) end

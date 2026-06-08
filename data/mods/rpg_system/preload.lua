gdebug.log_info("RPG System: Preload")

---@class ModRpgSystem
---@field on_monster_killed fun(params: OnMonDeathParams)
---@field on_game_started fun()
---@field on_game_load fun()
---@field on_dialogue_end fun(params: OnDialogueEndParams)
---@field on_character_reset_stats fun(params: OnCharacterResetStatsParams)
---@field on_every_5_minutes fun(...: any): any
---@field open_rpg_menu fun(params: ItemUseParams): integer
---@field add_mutation fun(config: RpgMutationConfig?): boolean
---@field manage_class_menu fun(player: Character)
---@field manage_traits_menu fun(player: Character)
---@field assign_stats_menu fun(player: Character)
---@field show_help_menu fun(player: Character)
---@field show_about_screen fun(player: Character)
---@field adjust_level_scaling fun(player: Character)
---@type ModRpgSystem
local mod = game.mod_runtime[game.current_mod]

game.add_hook("on_mon_death", function(...) return mod.on_monster_killed(...) end)
game.add_hook("on_game_started", function(...) return mod.on_game_started(...) end)
game.add_hook("on_game_load", function(...) return mod.on_game_load(...) end)
game.add_hook("on_dialogue_end", function(...) return mod.on_dialogue_end(...) end)
game.add_hook("on_character_reset_stats", function(...) return mod.on_character_reset_stats(...) end)
gapi.add_on_every_x_hook(TimeDuration.from_minutes(5), function(...) return mod.on_every_5_minutes(...) end)
game.iuse_functions["RPG_SYSTEM_MENU"] = function(...) return mod.open_rpg_menu(...) end

gdebug.log_info("Skills Through Kills: Preload")

---@class ModSkillsThroughKills
local mod = game.mod_runtime[game.current_mod]

game.add_hook("on_mon_death", function(...) return mod.on_mon_death(...) end)
game.add_hook("on_character_death", function(...) return mod.on_character_death(...) end)
game.add_hook("on_character_display_skill_action", function(...) return mod.on_character_display_skill_action(...) end)
game.add_hook("on_character_display_skill_info", function(...) return mod.on_character_display_skill_info(...) end)

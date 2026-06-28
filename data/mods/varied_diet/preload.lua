gdebug.log_info("Varied Diet: Preload")

---@class ModVariedDiet
local mod = game.mod_runtime[game.current_mod]

game.add_hook("on_character_consumed_food", function(...) return mod.on_character_consumed_food(...) end)

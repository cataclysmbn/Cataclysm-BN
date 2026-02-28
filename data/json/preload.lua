gdebug.log_info("bn: preloaded.")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.storage = storage

game.iuse_functions["VOLTMETER"] = function(...) return mod.voltmeter.menu(...) end
game.iuse_functions["ARTIFACT_ANALYZER"] = function(...) return mod.artifact_analyzer.menu(...) end
game.mapgen_functions["slimepit"] = function(...) return mod.slimepit.draw(...) end

gapi.add_on_every_x_hook(TimeDuration.from_turns(1), function(...)
  if mod.on_nyctophobia_tick then mod.on_nyctophobia_tick(...) end
  if mod.on_morale_traits_tick then mod.on_morale_traits_tick(...) end
end)

game.add_hook("on_character_try_move", function(...) return mod.on_character_try_move(...) end)
game.add_hook("on_game_started", function(...) return mod.on_achievements_game_started(...) end)
game.add_hook("on_game_load", function(...) return mod.on_achievements_game_load(...) end)
game.add_hook("on_mon_death", function(...) return mod.on_achievements_mon_death(...) end)

gapi.add_on_every_x_hook(TimeDuration.from_turns(1), function(...) return mod.on_achievements_tick(...) end)

gdebug.log_info("bn: preloaded.")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.storage = storage

game.iuse_functions["VOLTMETER"] = function(...) return mod.voltmeter.menu(...) end
game.iuse_functions["sonar_scan"] = function(...) return mod.sonar_scan(...) end
game.iuse_functions["ARTIFACT_ANALYZER"] = function(...) return mod.artifact_analyzer.menu(...) end
game.mapgen_functions["slimepit"] = function(...) return mod.slimepit.draw(...) end

gapi.add_on_every_x_hook(TimeDuration.from_turns(1), function(...)
  if mod.on_nyctophobia_tick then mod.on_nyctophobia_tick(...) end
  if mod.on_morale_traits_tick then mod.on_morale_traits_tick(...) end
end)

game.add_hook("on_character_try_move", function(...) return mod.on_character_try_move(...) end)

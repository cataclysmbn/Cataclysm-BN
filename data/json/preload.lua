gdebug.log_info("bn: preloaded.")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.storage = storage

game.iuse_functions["VOLTMETER"] = function(...) return mod.voltmeter.menu(...) end
game.iuse_functions["sonar_scan"] = function(...) return mod.sonar_scan(...) end
game.iuse_functions["ARTIFACT_ANALYZER"] = function(...) return mod.artifact_analyzer.menu(...) end
game.iuse_functions["OBJ_VAR_VIEWER"] = function(...) return mod.item_var_viewer.menu(...) end

gapi.add_on_every_x_hook(TimeDuration.from_turns(1), function(...)
  if mod.on_nyctophobia_tick then mod.on_nyctophobia_tick(...) end
  if mod.on_morale_traits_tick then mod.on_morale_traits_tick(...) end
end)

gapi.add_on_every_x_hook(TimeDuration.from_turns(300), function(...)
  if mod.on_clutter_intolerant_tick then mod.on_clutter_intolerant_tick(...) end
end)

game.add_hook("on_character_try_move", function(...) return mod.on_character_try_move(...) end)
game.add_hook("on_craft_result", function(...) return mod.cooking.on_craft_result(...) end)

-- Mapgen
game.mapgen_functions["slimepit"] = function(...) return mod.slimepit.draw(...) end
game.mapgen_functions["lab"] = function(...) return mod.lab.draw(...) end
game.mapgen_functions["lab_ice"] = function(...) return mod.lab.ice_draw(...) end
game.add_hook("on_make_mapgen_factory_list", function(params)
  params.results:insert(#params.results + 1, "lab_1side")
  params.results:insert(#params.results + 1, "lab_4side")
  params.results:insert(#params.results + 1, "lab_finale_1level")
  params.results:insert(#params.results + 1, "lab_1side_ice")
  params.results:insert(#params.results + 1, "lab_finale_1level_ice")
end)

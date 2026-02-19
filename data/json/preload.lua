gdebug.log_info("bn: preloaded.")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.storage = storage

game.iuse_functions["VOLTMETER"] = function(...) return mod.voltmeter.menu(...) end
game.iuse_functions["ARTIFACT_ANALYZER"] = function(...) return mod.artifact_analyzer.menu(...) end
game.iuse_functions["SURVIVOR_RADIO_BROADCAST"] = function(...) return mod.survivor_radio.menu(...) end
game.mapgen_functions["slimepit"] = function(...) return mod.slimepit.draw(...) end

gapi.add_on_every_x_hook(TimeDuration.from_turns(1), function(...) return mod.on_nyctophobia_tick(...) end)
gapi.add_on_every_x_hook(TimeDuration.from_hours(1), function(...) return mod.on_survivor_radio_tick(...) end)

game.add_hook("on_character_try_move", function(...) return mod.on_character_try_move(...) end)
game.add_hook("on_npc_spawn", function(...) return mod.on_npc_spawn(...) end)

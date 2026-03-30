gdebug.log_info("survivor_radio preloaded.")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.storage = storage

game.iuse_functions["SURVIVOR_RADIO_BROADCAST"] = function(...) return mod.survivor_radio.menu(...) end

gapi.add_on_every_x_hook(TimeDuration.from_hours(1), function(...) return mod.on_survivor_radio_tick(...) end)

game.add_hook("on_npc_spawn", function(...) return mod.on_npc_spawn(...) end)

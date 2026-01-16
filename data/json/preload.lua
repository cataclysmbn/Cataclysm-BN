gdebug.log_info("bn: preloaded.")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.storage = storage

game.iuse_functions["VOLTMETER"] = function(...) return mod.voltmeter.menu(...) end

gapi.add_on_every_x_hook(TimeDuration.from_turns(1), function(...) return mod.on_nyctophobia_tick(...) end)

table.insert(game.hooks.on_character_try_move, function(...) return mod.on_character_try_move(...) end)

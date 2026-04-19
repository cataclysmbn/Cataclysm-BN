local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.storage = storage

game.mapgen_functions["crt_lab_veh_house"] = function(...) return mod.crt_lab_veh_house.draw(...) end
game.mapgen_functions["crt_lab_veh_hall"] = function(...) return mod.crt_lab_veh_hall.draw(...) end
game.mapgen_functions["crt_lab_veh_main"] = function(...) return mod.crt_lab_veh_main.draw(...) end

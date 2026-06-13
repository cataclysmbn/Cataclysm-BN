local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.storage = storage

game.mapgen_functions["crt_lab_veh_house"] = function(...) return mod.crt_lab_veh_house.draw(...) end
game.mapgen_functions["crt_lab_veh_hall"] = function(...) return mod.crt_lab_veh_hall.draw(...) end
game.mapgen_functions["crt_lab_veh_main"] = function(...) return mod.crt_lab_veh_main.draw(...) end
game.add_hook("on_creature_melee_attacked", function(...) return mod.on_creature_melee_attacked(...) end)
game.add_hook("on_make_mapgen_factory_list", function(params)
  params.results:insert(#params.results + 1, "crt_lab_veh_house_backdrop")
  params.results:insert(#params.results + 1, "crt_lab_veh_hall_frame")
end)

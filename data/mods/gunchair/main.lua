local mod = game.mod_runtime[game.current_mod]

---@param params OnMapgenPostprocessParams
mod.on_mapgen_postprocess_hook = function(params)
  for _, vehicle in pairs(params.map:get_vehicles()) do
    params.map:replace_vehicle(vehicle, "swivel_chair", { status = 0, locks = false })
  end
end

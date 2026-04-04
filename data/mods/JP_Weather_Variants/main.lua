local mod = game.mod_runtime[game.current_mod]

mod.charge_per_tick_joule = 5000

mod.on_weather_updated = function(params)
  if params.weather_id ~= "geomagnetic_storm" then return end

  local player = gapi.get_avatar()
  if not player or not player:has_bionics() then return end

  local current_joules = player:get_power_level():to_joule()
  local max_joules = player:get_max_power_level():to_joule()
  local available_space = math.max(0, max_joules - current_joules)
  if available_space <= 0 then return end

  local charge_delta = math.min(available_space, mod.charge_per_tick_joule)
  player:mod_power_level(Energy.from_joule(charge_delta))
end

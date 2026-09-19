local mod = game.mod_runtime[game.current_mod]

mod.rain_functions = {}

local weather = "null"

mod.on_weather_updated = function(params) weather = params.weather_id end

mod.rain_functions.global = function(params)
  if weather == "rain" then return true end
  return false
end

game.add_lua_condition("rain", mod.rain_functions)

local mod = game.mod_runtime[game.current_mod]

game.add_hook("on_weather_updated", function(...) return mod.on_weather_updated(...) end)

local mod = game.mod_runtime[game.current_mod]

game.add_hook("on_mapgen_postprocess", function(...) return mod.on_mapgen_postprocess_hook(...) end)

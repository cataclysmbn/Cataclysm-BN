local mod = game.mod_runtime[game.current_mod]

-- Register the iuse function
game.iuse_functions["CHANGE_HAIRSTYLE_ACTION"] = function(...) return mod.change_hairstyle_function(...) end

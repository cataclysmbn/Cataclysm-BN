gdebug.log_info("SHR: preload.")

-- Have to register iuse before data loading.
-- Actual implementation (function mod.iuse_function) will be defined later.

local mod = game.mod_runtime[game.current_mod]

-- Register our map post-process hook
game.hooks.on_mapgen_postprocess[#game.hooks.on_mapgen_postprocess + 1] =
  function(...) return mod.on_mapgen_postprocess_hook(...) end

-- Register our item use function
game.iuse_functions["SMART_HOUSE_REMOTE"] = function(...) return mod.iuse_function(...) end

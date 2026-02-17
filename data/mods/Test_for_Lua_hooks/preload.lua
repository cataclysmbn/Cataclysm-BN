gdebug.log_info("LUA HOOKS TEST: PRELOAD ONLINE")

--@class LuaHooksTest
local mod = game.mod_runtime[game.current_mod]

table.insert(game.hooks.on_mon_death, function(...) return mod.chicken_death(...) end)

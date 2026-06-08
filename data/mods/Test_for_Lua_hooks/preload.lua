gdebug.log_info("LUA HOOKS TEST: PRELOAD ONLINE")

--@class LuaHooksTest
local mod = game.mod_runtime[game.current_mod]

game.add_hook("on_mon_death", function(...) return mod.chicken_death(...) end)

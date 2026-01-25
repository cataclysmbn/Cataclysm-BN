gdebug.log_info("LUA HOOKS TEST: PRELOAD ONLINE")

--@class LuaHooksTest
local mod = game.mod_runtime[game.current_mod]

game.add_hook("on_dialogue_start", function(...) return mod.on_dialogue_start(...) end)
game.add_hook("on_dialogue_option", function(...) return mod.on_dialogue_option(...) end)
game.add_hook("on_dialogue_end", function(...) return mod.on_dialogue_end(...) end)

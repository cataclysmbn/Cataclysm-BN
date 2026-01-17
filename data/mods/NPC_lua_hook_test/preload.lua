gdebug.log_info("LUA HOOKS TEST: PRELOAD ONLINE")

--@class LuaHooksTest
local mod = game.mod_runtime[game.current_mod]

table.insert(game.hooks.on_dialogue_start, function(...) return mod.on_dialogue_start(...) end)
table.insert(game.hooks.on_dialogue_option, function(...) return mod.on_dialogue_option(...) end)
table.insert(game.hooks.on_dialogue_end, function(...) return mod.on_dialogue_end(...) end)

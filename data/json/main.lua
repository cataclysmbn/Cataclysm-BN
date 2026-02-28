local voltmeter = require("./voltmeter")
local slimepit = require("./slimepit")
local artifact_analyzer = require("./artifact_analyzer")
local lua_traits = require("./lua_traits")
local action_menu_macros = require("lib.action_menu_macros")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.slimepit = slimepit
mod.artifact_analyzer = artifact_analyzer
mod.lua_traits = lua_traits
lua_traits.register(mod)
action_menu_macros.register_defaults()

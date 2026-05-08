local voltmeter = require("./voltmeter")
local sonar = require("./sonar")
local slimepit = require("./slimepit")
local artifact_analyzer = require("./artifact_analyzer")
local item_var_viewer = require("./item_var_viewer")
local lua_traits = require("./lua_traits")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.slimepit = slimepit
mod.artifact_analyzer = artifact_analyzer
mod.item_var_viewer = item_var_viewer
sonar.register(mod)
mod.lua_traits = lua_traits
lua_traits.register(mod)

local voltmeter = require("./voltmeter")
local sonar = require("./sonar")
local slimepit = require("./slimepit")
local artifact_analyzer = require("./artifact_analyzer")
local achievement_runtime = require("./achievement_runtime")
local lua_traits = require("./lua_traits")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.slimepit = slimepit
mod.artifact_analyzer = artifact_analyzer
mod.achievement_runtime = achievement_runtime
sonar.register(mod)
mod.lua_traits = lua_traits
lua_traits.register(mod)

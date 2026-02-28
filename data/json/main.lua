local voltmeter = require("./voltmeter")
local nyctophobia = require("./nyctophobia")

local sonar = require("./sonar")
local slimepit = require("./slimepit")
local artifact_analyzer = require("./artifact_analyzer")
local lua_traits = require("./lua_traits")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.slimepit = slimepit
mod.artifact_analyzer = artifact_analyzer
nyctophobia.register(mod)
sonar.register(mod)
mod.lua_traits = lua_traits
lua_traits.register(mod)

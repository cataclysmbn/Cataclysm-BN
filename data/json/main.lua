local voltmeter = require("./voltmeter")
local slimepit = require("./slimepit")
local artifact_analyzer = require("./artifact_analyzer")
local lua_traits = require("./lua_traits")
local achievements = require("./achievements")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.slimepit = slimepit
mod.artifact_analyzer = artifact_analyzer
mod.lua_traits = lua_traits
mod.achievements = achievements
lua_traits.register(mod)
achievements.register(mod)

game.cata_internal.get_lua_achievements_text = function() return achievements.get_ui_text(mod) end

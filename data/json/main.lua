local voltmeter = require("lua/iuse/voltmeter")
local sonar = require("lua/iuse/sonar")
local slimepit = require("lua/mapgen/slimepit")
local artifact_analyzer = require("lua/iuse/artifact_analyzer")
local item_var_viewer = require("lua/iuse/item_var_viewer")
local lua_traits = require("lua/traits/lua_traits")
local lab = require("lua/mapgen/lab")
local cooking = require("lua/cooking")
local achievements = require("lua/achievements")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.slimepit = slimepit
mod.lab = lab
mod.artifact_analyzer = artifact_analyzer
mod.item_var_viewer = item_var_viewer
sonar.register(mod)
mod.lua_traits = lua_traits
mod.achievements = achievements
lua_traits.register(mod)
mod.cooking = cooking
achievements.register(mod)

game.cata_internal.get_lua_achievements_text = function() return achievements.get_ui_text(mod) end

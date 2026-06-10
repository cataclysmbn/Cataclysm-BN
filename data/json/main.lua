local voltmeter = require("lua/iuse/voltmeter")
local sonar = require("lua/iuse/sonar")
local slimepit = require("lua/mapgen/slimepit")
local artifact_analyzer = require("lua/iuse/artifact_analyzer")
local item_var_viewer = require("lua/iuse/item_var_viewer")
local ebook_reader = require("lua/iuse/ebook_reader")
local lua_traits = require("lua/traits/lua_traits")
local lab = require("lua/mapgen/lab")
local cooking = require("lua/cooking")
local robofac = require("lua/robofac")
local nuclear_tear = require("lua/nuclear_tear")
local minirose = require("lua/minirose")
local plumbing = require("lua/plumbing")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.slimepit = slimepit
mod.lab = lab
mod.artifact_analyzer = artifact_analyzer
mod.item_var_viewer = item_var_viewer
mod.ebook_reader = ebook_reader
mod.plumbing = plumbing
sonar.register(mod)
mod.lua_traits = lua_traits
lua_traits.register(mod)
mod.cooking = cooking
mod.robofac = robofac
mod.nuclear_tear = nuclear_tear
mod.minirose = minirose

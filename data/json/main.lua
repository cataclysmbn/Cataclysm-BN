local voltmeter = require("./voltmeter")
local nyctophobia = require("./nyctophobia")
local artifact_analyzer = require("./artifact_analyzer")
local item_var_viewer = require("./item_var_viewer")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.artifact_analyzer = artifact_analyzer
mod.item_var_viewer = item_var_viewer
nyctophobia.register(mod)

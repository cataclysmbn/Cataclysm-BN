local voltmeter = require("./voltmeter")
local nyctophobia = require("./nyctophobia")
local artifact_analyzer = require("./artifact_analyzer")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.artifact_analyzer = artifact_analyzer
nyctophobia.register(mod)

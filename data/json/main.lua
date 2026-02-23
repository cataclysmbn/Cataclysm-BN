local voltmeter = require("./voltmeter")
local nyctophobia = require("./nyctophobia")
local door_key = require("./door_key")
local slimepit = require("./slimepit")
local artifact_analyzer = require("./artifact_analyzer")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.door_key = door_key
mod.slimepit = slimepit
mod.artifact_analyzer = artifact_analyzer
nyctophobia.register(mod)

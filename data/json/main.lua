local voltmeter = require("./voltmeter")
local nyctophobia = require("./nyctophobia")
local slimepit = require("./slimepit")
local artifact_analyzer = require("./artifact_analyzer")
local survivor_radio = require("./survivor_radio")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.slimepit = slimepit
mod.artifact_analyzer = artifact_analyzer
mod.survivor_radio = survivor_radio
nyctophobia.register(mod)
survivor_radio.register(mod)

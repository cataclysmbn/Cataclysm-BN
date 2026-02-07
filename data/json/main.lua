local voltmeter = require("./voltmeter")
local nyctophobia = require("./nyctophobia")
local artifact_compass = require("./artifact_compass")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.artifact_compass = artifact_compass
mod.on_artifact_compass_tick = artifact_compass.on_every_x
nyctophobia.register(mod)

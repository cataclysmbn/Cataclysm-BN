local voltmeter = require("./voltmeter")
local nyctophobia = require("./nyctophobia")
local door_key = require("./door_key")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.door_key = door_key
nyctophobia.register(mod)

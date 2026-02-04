local voltmeter = require("./voltmeter")
local nyctophobia = require("./nyctophobia")
local slimepit = require("./slimepit")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

mod.voltmeter = voltmeter
mod.slimepit = slimepit
nyctophobia.register(mod)

-- Test module: lib/ui_wrapper.lua
-- Tests absolute import from data/lua/lib
local ui = require("lib.ui")

local ui_wrapper = {}

ui_wrapper.show_test = function(message) ui.popup("Test: " .. message) end

return ui_wrapper

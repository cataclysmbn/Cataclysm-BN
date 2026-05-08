-- Test mod main.lua
-- Tests:
-- 1. Relative import from subdirectory: ./lib/calculator
-- 2. Nested relative import: lib/calculator -> ../util/math_helper
-- 3. Absolute import from data/lua: ui

local calculator = require("./lib/calculator")
local ui_wrapper = require("./lib/ui_wrapper")
local ui = require("ui")

local mod = game.mod_runtime[game.current_mod]

-- Test function that exercises all import types
mod.test_imports = function()
  -- Test relative imports work
  local result = calculator.sum_and_multiply(2, 3, 4)

  if result == 20 then
    ui.popup(
      "✓ Lua require test PASSED!\n\nRelative imports: ✓\nAbsolute imports: ✓\n\nResult: (2+3)*4 = " .. result
    )
  else
    ui.popup("✗ Lua require test FAILED!\n\nExpected: 20\nGot: " .. result)
  end
end

-- Auto-run test on mod load
mod.test_imports()

-- Test module: lib/calculator.lua
-- Tests relative import from sibling directory
local math_helper = require("../util/math_helper")

local calculator = {}

calculator.sum_and_multiply = function(a, b, c)
  local sum = math_helper.add(a, b)
  return math_helper.multiply(sum, c)
end

return calculator

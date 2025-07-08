-- Initialize function output table
test_data["out"] = {
  s = "",
  i = 0,
}

local func = function(i, s)
  -- Add to existing value
  test_data.out.i = test_data.out.i + i
  -- Append string, but only if string is passed
  if s ~= nil then test_data.out.s = test_data.out.s .. s end
  -- Return some new integer value
  return i * 2
end

-- Put function into table to be used by C++
test_data["func"] = func

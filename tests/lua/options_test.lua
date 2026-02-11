-- Test script for Lua mod options API
-- This script should be run from a test harness that sets up game.current_mod

-- Test registering a boolean option
local bool_result = mod_settings.register_option({
  id = "TEST_BOOL",
  name = "Test Boolean Option",
  tooltip = "A test boolean option",
  type = "bool",
  default = true,
})
test_data.bool_result = bool_result

-- Test registering an integer option
local int_result = mod_settings.register_option({
  id = "TEST_INT",
  name = "Test Integer Option",
  tooltip = "A test integer option",
  type = "int",
  default = 50,
  min = 0,
  max = 100,
})
test_data.int_result = int_result

-- Test registering a float option
local float_result = mod_settings.register_option({
  id = "TEST_FLOAT",
  name = "Test Float Option",
  tooltip = "A test float option",
  type = "float",
  default = 0.5,
  min = 0.0,
  max = 1.0,
  step = 0.1,
})
test_data.float_result = float_result

-- Test registering a string_select option
local select_result = mod_settings.register_option({
  id = "TEST_SELECT",
  name = "Test Select Option",
  tooltip = "A test string select option",
  type = "string_select",
  default = "option_a",
  options = {
    { id = "option_a", name = "Option A" },
    { id = "option_b", name = "Option B" },
    { id = "option_c", name = "Option C" },
  },
})
test_data.select_result = select_result

-- Test registering a string_input option
local input_result = mod_settings.register_option({
  id = "TEST_INPUT",
  name = "Test Input Option",
  tooltip = "A test string input option",
  type = "string_input",
  default = "default_value",
  max_length = 32,
})
test_data.input_result = input_result

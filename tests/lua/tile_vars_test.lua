---@type Map|MapgenConstructor
local target = test_data.target
---@type TripointBubMs|PointOmtMs
local pos = test_data.pos

test_data.furn_has_before = target:has_furn_var_at(pos, "lua_key")
test_data.furn_default = target:get_furn_var_at(pos, "lua_key", "fallback")
test_data.furn_set_ok = target:set_furn_var_at(pos, "lua_key", "furn_value")
test_data.furn_has_after_set = target:has_furn_var_at(pos, "lua_key")
test_data.furn_value = target:get_furn_var_at(pos, "lua_key")
test_data.cpp_furn_key_value = target:get_furn_var_at(pos, "cpp_key")

test_data.ter_has_before = target:has_ter_var_at(pos, "lua_key")
test_data.ter_default = target:get_ter_var_at(pos, "lua_key")
test_data.ter_set_ok = target:set_ter_var_at(pos, "lua_key", "ter_value")
test_data.ter_value = target:get_ter_var_at(pos, "lua_key")

test_data.ter_erase_ok = target:erase_ter_var_at(pos, "lua_key")
test_data.ter_erase_again = target:erase_ter_var_at(pos, "lua_key")
test_data.ter_has_after_erase = target:has_ter_var_at(pos, "lua_key")
test_data.furn_erase_ok = target:erase_furn_var_at(pos, "cpp_key")

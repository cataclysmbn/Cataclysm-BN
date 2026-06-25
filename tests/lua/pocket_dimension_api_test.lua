local map = gapi.get_map()

local dimension_id = test_data["target_dimension_id"]
local target_omt = test_data["target_omt"]
local return_ms = test_data["return_ms"]
local bounds_min_omt = test_data["bounds_min_omt"]
local bounds_max_omt = test_data["bounds_max_omt"]
local overmap_terrain = test_data["overmap_terrain"]

test_data["before_dim"] = gapi.get_current_dimension_id()
test_data["before_map_dim"] = map:get_bound_dimension()

test_data["missing_target_travel"] = gapi.place_player_dimension_at({
  dimension_id = "",
  target_omt = nil,
})

test_data["noop_travel"] = gapi.place_player_dimension_at({
  dimension_id = "",
  target_ms = return_ms,
})

test_data["invalid_bounds_travel"] = gapi.place_player_dimension_at({
  dimension_id = dimension_id,
  target_omt = target_omt,
  world_type = "pocket_dimension",
  bounds_min_omt = bounds_min_omt,
})

test_data["invalid_special_travel"] = gapi.place_player_dimension_at({
  dimension_id = dimension_id,
  target_omt = target_omt,
  world_type = "pocket_dimension",
  bounds_min_omt = bounds_min_omt,
  bounds_max_omt = bounds_max_omt,
  pregen_special_id = "lua_test_missing_special",
})

test_data["invalid_overmap_terrain_travel"] = gapi.place_player_dimension_at({
  dimension_id = dimension_id,
  target_omt = target_omt,
  world_type = "pocket_dimension",
  bounds_min_omt = bounds_min_omt,
  bounds_max_omt = bounds_max_omt,
  overmap_terrain = { { { "lua_test_missing_omt" } } },
})

test_data["after_invalid_dim"] = gapi.get_current_dimension_id()
test_data["after_invalid_map_dim"] = gapi.get_map():get_bound_dimension()

test_data["entered_travel"] = gapi.place_player_dimension_at({
  dimension_id = dimension_id,
  target_omt = target_omt,
  world_type = "pocket_dimension",
  bounds_min_omt = bounds_min_omt,
  bounds_max_omt = bounds_max_omt,
  overmap_terrain = overmap_terrain,
})

local dimension_map = gapi.get_map()
test_data["entered_dim"] = gapi.get_current_dimension_id()
test_data["entered_map_dim"] = dimension_map:get_bound_dimension()
test_data["entry_is_oob"] = dimension_map:is_out_of_bounds(gapi.get_avatar():bub_pos())
test_data["outside_is_oob"] = dimension_map:is_out_of_bounds(test_data["outside_local"])

test_data["return_travel"] = gapi.place_player_dimension_at({
  dimension_id = "",
  target_ms = return_ms,
})

test_data["after_return_dim"] = gapi.get_current_dimension_id()
test_data["after_return_map_dim"] = gapi.get_map():get_bound_dimension()
test_data["after_return_pos"] = gapi.get_avatar():abs_pos()
test_data["after_return_outside_is_oob"] = gapi.get_map():is_out_of_bounds(test_data["outside_local"])

test_data["reentered_travel"] = gapi.place_player_dimension_at({
  dimension_id = dimension_id,
  target_omt = target_omt,
})

test_data["reentered_dim"] = gapi.get_current_dimension_id()
test_data["reentered_map_dim"] = gapi.get_map():get_bound_dimension()
test_data["reentered_outside_is_oob"] = gapi.get_map():is_out_of_bounds(test_data["outside_local"])

test_data["final_return_travel"] = gapi.place_player_dimension_at({
  dimension_id = "",
  target_ms = return_ms,
})

test_data["final_dim"] = gapi.get_current_dimension_id()
test_data["final_map_dim"] = gapi.get_map():get_bound_dimension()
test_data["final_pos"] = gapi.get_avatar():abs_pos()

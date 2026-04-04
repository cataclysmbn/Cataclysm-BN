local map = gapi.get_map()

test_data["before_dim"] = gapi.get_current_dimension_id()
test_data["before_map_dim"] = map:get_bound_dimension()

local dimension_id = test_data["target_dimension_id"]
local target_omt = test_data["target_omt"]
local return_omt = test_data["return_omt"]

test_data["noop_travel"] = gapi.place_player_dimension_at({
  dimension_id = "",
  target_omt = return_omt,
})

test_data["invalid_bounds_travel"] = gapi.place_player_dimension_at({
  dimension_id = dimension_id,
  target_omt = target_omt,
  world_type = "pocket_dimension",
  bounds_min_omt = test_data["bounds_min_omt"],
})

test_data["after_dim"] = gapi.get_current_dimension_id()
test_data["after_map_dim"] = gapi.get_map():get_bound_dimension()
test_data["outside_is_oob"] = gapi.get_map():is_out_of_bounds(test_data["outside_local"])

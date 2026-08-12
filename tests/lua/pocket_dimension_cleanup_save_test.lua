local phase = test_data["phase"]

if phase == "enter" then
  test_data["entered"] = gapi.place_player_dimension_at({
    dimension_id = test_data["dimension_id"],
    target_omt = test_data["target_omt"],
    world_type = "pocket_dimension",
    bounds_min_omt = test_data["bounds_min_omt"],
    bounds_max_omt = test_data["bounds_max_omt"],
  })
  test_data["entered_pos"] = gapi.get_avatar():abs_pos()
elseif phase == "cleanup" then
  test_data["returned"] = gapi.place_player_dimension_at({
    dimension_id = "",
    target_ms = test_data["return_ms"],
  })
  test_data["cleanup_result"] = gapi[test_data["cleanup_function"]](test_data["dimension_id"])
elseif phase == "recreate" then
  local options = {
    dimension_id = test_data["dimension_id"],
    target_omt = test_data["target_omt"],
  }
  if test_data["include_generation_options"] then
    options.world_type = "pocket_dimension"
    options.bounds_min_omt = test_data["bounds_min_omt"]
    options.bounds_max_omt = test_data["bounds_max_omt"]
  end
  test_data["recreated"] = gapi.place_player_dimension_at(options)
elseif phase == "return" then
  test_data["final_return"] = gapi.place_player_dimension_at({
    dimension_id = "",
    target_ms = test_data["return_ms"],
  })
end

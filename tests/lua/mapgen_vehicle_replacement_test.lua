local vehicles_before = test_data.mapgen:get_vehicles()
test_data.vehicle_count_before = #vehicles_before

test_data.replace_ok = test_data.mapgen:replace_vehicle(vehicles_before[1], "swivel_chair", {
  orientation = Angle.from_degrees(180),
  status = 0,
  locks = false,
})

local vehicles_after = test_data.mapgen:get_vehicles()
test_data.vehicle_count_after = #vehicles_after

local vehicles_before = test_data.map:get_vehicles()
test_data.vehicle_count_before = #vehicles_before
test_data.vehicle_type_before = vehicles_before[1]:type()

test_data.replace_ok = test_data.map:replace_vehicle(vehicles_before[1], "swivel_chair")

local vehicles_after = test_data.map:get_vehicles()
test_data.vehicle_count_after = #vehicles_after
test_data.vehicle_type_after = vehicles_after[1]:type()

test_data.replace_with_opts_ok = test_data.map:replace_vehicle(vehicles_after[1], "swivel_chair", {
  orientation = Angle.from_degrees(180),
  status = 0,
  locks = false,
})

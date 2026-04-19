test_data.has_examine_functions = game.examine_functions ~= nil
test_data.has_activity_functions = game.activity_functions ~= nil

local avatar = gapi.get_avatar()
avatar:assign_lua_activity({
  activity = ActivityTypeId.new("ACT_WASH_SELF"),
  duration = TimeDuration.from_minutes(5),
  callback = "TEST_CALLBACK",
  name = "test wash",
  values = { 7 },
  str_values = { "extra" },
  coords = { Tripoint.new(9, 8, 0) },
})

local activity = avatar:get_activity()
test_data.activity_id = activity:id_str()
test_data.activity_name = activity.name
test_data.activity_callback = activity.str_values[1]
test_data.activity_str_value = activity.str_values[2]
test_data.activity_value = activity.values[1]
test_data.activity_coord = tostring(activity.coords[1])

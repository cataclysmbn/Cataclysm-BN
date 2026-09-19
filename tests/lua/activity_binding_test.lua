---@class ActivityBindingTestData
---@field has_examine_functions boolean
---@field has_activity_functions boolean
---@field activity_id string
---@field activity_name string
---@field activity_moves_total integer
---@field activity_interruptable boolean
---@field activity_coord string
---@field turn_called boolean
---@field turn_name string
---@field finish_called boolean
---@field finish_name string
---@field finish_pos_type string
---@field finish_mode string
---@field finish_is_warm boolean
---@field finish_cleaner_label string
---@field finish_nested_charges integer

---@type ActivityBindingTestData
test_data.has_examine_functions = game.examine_functions ~= nil
test_data.has_activity_functions = game.activity_functions ~= nil

---@param params LuaActivityCallbackParams
game.activity_functions["TEST_TURN"] = function(params)
  test_data.turn_called = true
  test_data.turn_name = params.name
end

---@param params LuaActivityCallbackParams
game.activity_functions["TEST_CALLBACK"] = function(params)
  test_data.finish_called = true
  test_data.finish_name = params.name
  test_data.finish_pos_type = tostring(params.pos):match("^([^%(]+)") or ""
  test_data.finish_mode = params.data.mode
  test_data.finish_is_warm = params.data.is_warm
  test_data.finish_cleaner_label = params.data.cleaner_label
  test_data.finish_nested_charges = params.data.nested.charges
end

local avatar = gapi.get_avatar()
avatar:assign_lua_activity({
  type = ActivityTypeId.new("ACT_WAIT"),
  duration = TimeDuration.from_minutes(5),
  on_finish = "TEST_CALLBACK",
  on_turn = "TEST_TURN",
  name = "test wash",
  pos = TripointBubMs.new(9, 8, 0),
  data = {
    mode = "test_shower",
    is_warm = true,
    cleaner_label = "soap",
    nested = { charges = 7 },
  },
})

local activity = avatar:get_activity()
test_data.activity_id = activity:id_str()
test_data.activity_name = activity.name
test_data.activity_moves_total = activity.moves_total
test_data.activity_interruptable = activity.interruptable_with_kb
test_data.activity_coord = tostring(activity.coords[1])

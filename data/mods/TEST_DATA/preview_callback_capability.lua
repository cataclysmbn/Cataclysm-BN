-- Loaded explicitly by preview_callback_capability_test.cpp, never by normal mod startup.

---@class PreviewCallbackAlias
---@field value integer

---@class PreviewCallbackObservation
---@field calls integer
---@field try_calls integer
---@field upvalue integer
---@field alias_value integer
---@field aliased boolean
---@field moves integer
---@field terrain string
---@field trace string[]

---@class PreviewCallbackWearParams
---@field user Character
---@field item Item

---@class PreviewCallbackTryWearParams
---@field who Character
---@field item Item

---@class PreviewCallbackFixture
---@field on_wear fun(params: PreviewCallbackWearParams)
---@field read fun(): PreviewCallbackObservation

---@param cell TripointBubMs
---@param interactive boolean
local function begin(cell, interactive)
  -- Capture authority before the first eligibility query, not inside the callback.
  local captured_avatar = gapi.get_avatar()
  local captured_map = gapi.get_map()
  local upvalue = 41
  ---@type PreviewCallbackAlias
  local runtime = { value = 3 }
  game.mod_runtime["test_data"].preview_callback_alias = runtime
  local alias = game.mod_runtime["test_data"].preview_callback_alias
  local calls = 0
  local try_calls = 0
  ---@type string[]
  local trace = {}

  ---@param params PreviewCallbackTryWearParams
  local function try_wear(params)
    if params.item:get_type():str() ~= "test_preview_callback_coat" then return { allowed = true } end
    try_calls = try_calls + 1
    if interactive then
      return {
        allowed = require("lib.ui").query_yn("Allow this fixture coat?"),
        message = "Fixture coat refused.",
      }
    end
    return { allowed = true }
  end
  game.add_hook("on_character_try_wear", try_wear)

  ---@param params PreviewCallbackWearParams
  local function on_wear(params)
    calls = calls + 1
    captured_avatar:mod_moves(-13)
    captured_map:set_ter_at(cell, TerId.new("t_dirt"):int_id())
    upvalue = upvalue + 7
    alias.value = alias.value + 3
    local content = string.format(
      "wear=%d;upvalue=%d;alias=%d;moves=%d;terrain=%s;temporary=%s",
      calls,
      upvalue,
      runtime.value,
      captured_avatar:get_moves(),
      captured_map:get_ter_at(cell):str_id():str(),
      tostring(params.user ~= captured_avatar)
    )
    trace[#trace + 1] = content
    params.item:set_var_str("item_note", content)
    params.item:set_var_str("preview_callback_trace", table.concat(trace, "|"))
    params.item:set_var_col("tint_color_fg", rgb_colors.try_parse("#FF0000"))
    gapi.add_msg("Preview callback " .. content)
  end

  local function read()
    return {
      calls = calls,
      try_calls = try_calls,
      upvalue = upvalue,
      alias_value = runtime.value,
      aliased = rawequal(runtime, alias) and rawequal(alias, game.mod_runtime["test_data"].preview_callback_alias),
      moves = captured_avatar:get_moves(),
      terrain = captured_map:get_ter_at(cell):str_id():str(),
      trace = trace,
    }
  end

  return { on_wear = on_wear, read = read }
end

return begin

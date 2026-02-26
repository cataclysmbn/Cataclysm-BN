local ui = require("lib.ui")

local action_menu_macros = {}

local function gettext(message)
  if type(locale) == "table" and type(locale.gettext) == "function" then
    local ok_text, text = pcall(locale.gettext, message)
    if ok_text and type(text) == "string" then return text end
  end
  return message
end

local function gapi_fn(name)
  if type(gapi) ~= "table" then return nil end
  local fn = gapi[name]
  if type(fn) ~= "function" then return nil end
  return fn
end

local function safe_add_msg(msg)
  local add_msg = gapi_fn("add_msg")
  if not add_msg then return end
  pcall(add_msg, msg)
end

local function popup_text(message)
  local ok_popup = pcall(ui.popup, message)
  if not ok_popup then safe_add_msg(message) end
end

local function interactive_macros_enabled()
  if type(os) ~= "table" or type(os.getenv) ~= "function" then return false end
  return os.getenv("CATA_ENABLE_INTERACTIVE_MACROS") == "1"
end

local function popup_recent_messages()
  local get_messages = gapi_fn("get_messages")
  if not get_messages then return end

  local ok_entries, entries = pcall(get_messages, 12)
  if not ok_entries or type(entries) ~= "table" then return end

  if #entries == 0 then
    popup_text(gettext("No recent messages."))
    return
  end

  local lines = { gettext("Recent Messages"), "" }
  for _, entry in ipairs(entries) do
    table.insert(lines, string.format("[%s] %s", entry.time, entry.text))
  end

  popup_text(table.concat(lines, "\n"))
end

local function popup_recent_lua_log()
  local get_lua_log = gapi_fn("get_lua_log")
  if not get_lua_log then return end

  local ok_entries, entries = pcall(get_lua_log, 20)
  if not ok_entries or type(entries) ~= "table" then return end

  if #entries == 0 then
    popup_text(gettext("No recent Lua log entries."))
    return
  end

  local lines = { gettext("Recent Lua Log"), "" }
  for _, entry in ipairs(entries) do
    local source_prefix = entry.from_user and "> " or ""
    table.insert(lines, string.format("[%s] %s%s", entry.level, source_prefix, entry.text))
  end

  popup_text(table.concat(lines, "\n"))
end

local function announce_current_turn()
  local current_turn = gapi_fn("current_turn")
  if not current_turn then return end

  local ok_turn, turn = pcall(current_turn)
  if not ok_turn or turn == nil then return end

  local ok_value, turn_value = pcall(turn.to_turn, turn)
  if not ok_value then return end

  safe_add_msg(string.format(gettext("Current turn: %d"), turn_value))
end

local function report_agent_context()
  local get_avatar = gapi_fn("get_avatar")
  local get_map = gapi_fn("get_map")
  local current_turn = gapi_fn("current_turn")
  if not get_avatar or not get_map or not current_turn then return end

  local ok_avatar, avatar = pcall(get_avatar)
  local ok_map, map = pcall(get_map)
  local ok_turn, turn = pcall(current_turn)
  if not ok_avatar or not ok_map or not ok_turn or avatar == nil or map == nil or turn == nil then return end

  local ok_pos, pos = pcall(avatar.get_pos_ms, avatar)
  local ok_turn_value, turn_value = pcall(turn.to_turn, turn)
  if not ok_pos or not ok_turn_value or pos == nil then return end

  local ok_outside, is_outside = pcall(map.is_outside, map, pos)
  local ok_sheltered, is_sheltered = pcall(map.is_sheltered, map, pos)
  if not ok_outside or not ok_sheltered then return end

  local details = string.format(
    "[AI] turn=%d local_ms=(%d,%d,%d) outside=%s sheltered=%s",
    turn_value,
    pos.x,
    pos.y,
    pos.z,
    tostring(is_outside),
    tostring(is_sheltered)
  )
  safe_add_msg(details)
end

local function report_look_target()
  local look_around = gapi_fn("look_around")
  if not look_around then return end

  local ok_target, target = pcall(look_around)
  if not ok_target then return end

  if not target then
    safe_add_msg(gettext("Look canceled."))
    return
  end

  local get_map = gapi_fn("get_map")
  if not get_map then return end

  local ok_map, map = pcall(get_map)
  if not ok_map or map == nil then return end

  local ok_abs, target_abs = pcall(map.get_abs_ms, map, target)
  if not ok_abs or target_abs == nil then return end

  safe_add_msg(
    string.format(
      "[AI] look local_ms=(%d,%d,%d) abs_ms=(%d,%d,%d)",
      target.x,
      target.y,
      target.z,
      target_abs.x,
      target_abs.y,
      target_abs.z
    )
  )
end

local function report_adjacent_choice()
  local choose_adjacent = gapi_fn("choose_adjacent")
  if not choose_adjacent then return end

  local ok_target, target = pcall(choose_adjacent, gettext("Choose adjacent tile for AI context"), true)
  if not ok_target then return end

  if not target then
    safe_add_msg(gettext("Adjacent selection canceled."))
    return
  end

  local get_map = gapi_fn("get_map")
  if not get_map then return end

  local ok_map, map = pcall(get_map)
  if not ok_map or map == nil then return end

  local ok_abs, target_abs = pcall(map.get_abs_ms, map, target)
  if not ok_abs or target_abs == nil then return end

  safe_add_msg(
    string.format(
      "[AI] adjacent local_ms=(%d,%d,%d) abs_ms=(%d,%d,%d)",
      target.x,
      target.y,
      target.z,
      target_abs.x,
      target_abs.y,
      target_abs.z
    )
  )
end

action_menu_macros.register_defaults = function()
  local register_action_menu_entry = gapi_fn("register_action_menu_entry")
  if not register_action_menu_entry then return end

  register_action_menu_entry({
    id = "bn_macro_recent_messages",
    name = gettext("Recent Messages"),
    description = gettext("Show the latest in-game messages in a popup."),
    category = "info",
    fn = popup_recent_messages,
  })

  register_action_menu_entry({
    id = "bn_macro_recent_lua_log",
    name = gettext("Recent Lua Log"),
    description = gettext("Show the latest Lua console log entries in a popup."),
    category = "info",
    fn = popup_recent_lua_log,
  })

  register_action_menu_entry({
    id = "bn_macro_current_turn",
    name = gettext("Current Turn"),
    description = gettext("Print the current absolute turn in the message log."),
    category = "info",
    fn = announce_current_turn,
  })

  register_action_menu_entry({
    id = "bn_macro_agent_context",
    name = gettext("AI Context Packet"),
    description = gettext("Print turn, local coordinates, and shelter/outside state."),
    category = "info",
    fn = report_agent_context,
  })

  if interactive_macros_enabled() then
    register_action_menu_entry({
      id = "bn_macro_look_target",
      name = gettext("AI Look Target"),
      description = gettext("Pick a tile via look-around and print local/absolute coordinates."),
      category = "info",
      fn = report_look_target,
    })

    register_action_menu_entry({
      id = "bn_macro_adjacent_target",
      name = gettext("AI Adjacent Target"),
      description = gettext("Pick an adjacent tile and print local/absolute coordinates."),
      category = "info",
      fn = report_adjacent_choice,
    })
  end
end

return action_menu_macros

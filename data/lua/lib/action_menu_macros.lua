local ui = require("lib.ui")

local action_menu_macros = {}

local function popup_recent_messages()
  local entries = gapi.get_messages(12)
  if #entries == 0 then
    ui.popup(locale.gettext("No recent messages."))
    return
  end

  local lines = { locale.gettext("Recent Messages"), "" }
  for _, entry in ipairs(entries) do
    table.insert(lines, string.format("[%s] %s", entry.time, entry.text))
  end

  ui.popup(table.concat(lines, "\n"))
end

local function popup_recent_lua_log()
  local entries = gapi.get_lua_log(20)
  if #entries == 0 then
    ui.popup(locale.gettext("No recent Lua log entries."))
    return
  end

  local lines = { locale.gettext("Recent Lua Log"), "" }
  for _, entry in ipairs(entries) do
    local source_prefix = entry.from_user and "> " or ""
    table.insert(lines, string.format("[%s] %s%s", entry.level, source_prefix, entry.text))
  end

  ui.popup(table.concat(lines, "\n"))
end

local function announce_current_turn()
  local turn_value = gapi.current_turn():to_turn()
  gapi.add_msg(string.format(locale.gettext("Current turn: %d"), turn_value))
end

local function report_agent_context()
  local avatar = gapi.get_avatar()
  local map = gapi.get_map()
  local pos = avatar:get_pos_ms()
  local turn_value = gapi.current_turn():to_turn()
  local details = string.format(
    "[AI] turn=%d local_ms=(%d,%d,%d) outside=%s sheltered=%s",
    turn_value,
    pos.x,
    pos.y,
    pos.z,
    tostring(map:is_outside(pos)),
    tostring(map:is_sheltered(pos))
  )
  gapi.add_msg(details)
end

local function report_look_target()
  local target = gapi.look_around()
  if not target then
    gapi.add_msg(locale.gettext("Look canceled."))
    return
  end

  local target_abs = gapi.get_map():get_abs_ms(target)
  gapi.add_msg(
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
  local target = gapi.choose_adjacent(locale.gettext("Choose adjacent tile for AI context"), true)
  if not target then
    gapi.add_msg(locale.gettext("Adjacent selection canceled."))
    return
  end

  local target_abs = gapi.get_map():get_abs_ms(target)
  gapi.add_msg(
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
  gapi.register_action_menu_entry({
    id = "bn_macro_recent_messages",
    name = locale.gettext("Recent Messages"),
    description = locale.gettext("Show the latest in-game messages in a popup."),
    category = "info",
    fn = popup_recent_messages,
  })

  gapi.register_action_menu_entry({
    id = "bn_macro_recent_lua_log",
    name = locale.gettext("Recent Lua Log"),
    description = locale.gettext("Show the latest Lua console log entries in a popup."),
    category = "info",
    fn = popup_recent_lua_log,
  })

  gapi.register_action_menu_entry({
    id = "bn_macro_current_turn",
    name = locale.gettext("Current Turn"),
    description = locale.gettext("Print the current absolute turn in the message log."),
    category = "info",
    fn = announce_current_turn,
  })

  gapi.register_action_menu_entry({
    id = "bn_macro_agent_context",
    name = locale.gettext("AI Context Packet"),
    description = locale.gettext("Print turn, local coordinates, and shelter/outside state."),
    category = "info",
    fn = report_agent_context,
  })

  gapi.register_action_menu_entry({
    id = "bn_macro_look_target",
    name = locale.gettext("AI Look Target"),
    description = locale.gettext("Pick a tile via look-around and print local/absolute coordinates."),
    category = "info",
    fn = report_look_target,
  })

  gapi.register_action_menu_entry({
    id = "bn_macro_adjacent_target",
    name = locale.gettext("AI Adjacent Target"),
    description = locale.gettext("Pick an adjacent tile and print local/absolute coordinates."),
    category = "info",
    fn = report_adjacent_choice,
  })
end

return action_menu_macros

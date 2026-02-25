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
end

return action_menu_macros

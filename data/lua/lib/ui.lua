local ui = {}

---@param str string
---@param color Color?
ui.query_any_key = function(str, color)
  local popup = QueryPopup.new()
  popup:message(str)
  if color then popup:message_color(color) end
  popup:allow_any_key(true)
  popup:query()
end

---@param str string
---@return boolean
ui.query_yn = function(str)
  local popup = QueryPopup.new()
  popup:message(str)
  return popup:query_yn() == "YES"
end

---@param str string
---@param color Color?
ui.popup = function(str, color) ui.query_any_key(str, color) end

return ui

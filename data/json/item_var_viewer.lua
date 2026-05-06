local ui = require("lib.ui")

---@alias StoredVars table<string, string>
---@alias ItemVarViewerSubject Item | Avatar | Character | Monster | Npc

---@class ItemVarViewerChoice
---@field type "item" | "player" | "monster" | "npc" | "ground_item"
---@field subject ItemVarViewerSubject
---@field subject_name string
---@field get_vars fun(): StoredVars

local viewer = {}

---@param text string?
---@param color string?
---@return string
local function color_text(text, color)
  if text == nil then return "" end
  return string.format("<color_%s>%s</color>", color or "white", text)
end

---@param vars StoredVars?
---@return integer
local function count_vars(vars)
  if type(vars) ~= "table" then return 0 end
  local sum = 0
  for _ in pairs(vars) do
    sum = sum + 1
  end
  return sum
end

---@param vars StoredVars?
---@return string[]
local function sorted_keys(vars)
  local keys = {}
  if type(vars) ~= "table" then return keys end
  for key in pairs(vars) do
    table.insert(keys, key)
  end
  table.sort(keys)
  return keys
end

---@param display_name string
---@param vars StoredVars?
---@return string
local function format_label(display_name, vars)
  local header = string.format(locale.gettext("Stored vars for %s"), display_name)
  local count_line = string.format(locale.gettext("Stored entries: %d"), count_vars(vars))
  return color_text(header, "light_green") .. "  " .. color_text(count_line, "light_gray")
end

---@param display_name string
---@param vars StoredVars
---@return string[]
local function build_display_lines(display_name, vars)
  local lines = {
    format_label(display_name, vars),
    "",
  }

  for _, key in ipairs(sorted_keys(vars)) do
    table.insert(
      lines,
      string.format("%s %s", color_text(string.format("%s:", key), "light_blue"), color_text(vars[key], "white"))
    )
  end

  return lines
end

---@param selected ItemVarViewerChoice
---@return boolean
local function is_item_choice(selected) return selected.type == "item" or selected.type == "ground_item" end

---@param selected ItemVarViewerChoice
---@param key string
---@param value string
---@return nil
local function set_subject_var(selected, key, value)
  if is_item_choice(selected) then
    selected.subject:set_var_str(key, value)
    return
  end
  selected.subject:set_value(key, value)
end

---@param selected ItemVarViewerChoice
---@param key string
---@return nil
local function remove_subject_var(selected, key)
  if is_item_choice(selected) then
    selected.subject:erase_var(key)
    return
  end
  selected.subject:remove_value(key)
end

---@param title string
---@param desc string
---@return string?
local function prompt_string(title, desc)
  local popup = PopupInputStr.new()
  popup:title(title)
  popup:desc(desc)
  local value = popup:query_str()
  if value == nil or value == "" then return nil end
  return value
end

---@param selected ItemVarViewerChoice
---@return nil
local function show_vars_popup(selected)
  local vars = selected.get_vars()
  if type(vars) ~= "table" or next(vars) == nil then
    ui.popup(
      color_text(string.format(locale.gettext("No stored variables found on %s."), selected.subject_name), "light_gray")
    )
    return
  end

  local lines = build_display_lines(selected.subject_name, vars)
  ui.popup(table.concat(lines, "\n"))
end

---@param selected ItemVarViewerChoice
---@return nil
local function add_or_update_var(selected)
  local key = prompt_string(
    locale.gettext("Variable name"),
    string.format(locale.gettext("Enter the variable name for %s."), selected.subject_name)
  )
  if not key then return end

  local value =
    prompt_string(locale.gettext("Variable value"), string.format(locale.gettext("Enter the value for %s."), key))
  if value == nil then return end

  set_subject_var(selected, key, value)
  gapi.add_msg(MsgType.good, string.format(locale.gettext("Set variable %s on %s."), key, selected.subject_name))
end

---@param selected ItemVarViewerChoice
---@return nil
local function remove_var(selected)
  local vars = selected.get_vars()
  if type(vars) ~= "table" or next(vars) == nil then
    ui.popup(
      color_text(string.format(locale.gettext("No stored variables found on %s."), selected.subject_name), "light_gray")
    )
    return
  end

  local keys = sorted_keys(vars)
  local menu = UiList.new()
  menu:title(color_text(locale.gettext("Remove variable"), "yellow"))
  menu:text(format_label(selected.subject_name, vars))

  for idx, key in ipairs(keys) do
    menu:add(
      idx - 1,
      string.format("%s %s", color_text(string.format("%s:", key), "light_blue"), color_text(vars[key], "white"))
    )
  end

  local choice = menu:query()
  if choice < 0 then return end

  local key = keys[choice + 1]
  if not key then return end
  if not ui.query_yn(string.format(locale.gettext("Remove variable %s from %s?"), key, selected.subject_name)) then
    return
  end

  remove_subject_var(selected, key)
  gapi.add_msg(MsgType.good, string.format(locale.gettext("Removed variable %s from %s."), key, selected.subject_name))
end

---@param selected ItemVarViewerChoice
---@return integer
local function manage_vars(selected)
  while true do
    local vars = selected.get_vars()
    local menu = UiList.new()
    menu:title(color_text(locale.gettext("Item var viewer"), "yellow"))
    if type(vars) ~= "table" or next(vars) == nil then
      menu:text(format_label(selected.subject_name, vars))
    else
      menu:text(table.concat(build_display_lines(selected.subject_name, vars), "\n"))
    end
    menu:add(0, locale.gettext("Add / update variable"))
    menu:add(1, locale.gettext("Remove variable"))

    if type(vars) ~= "table" or next(vars) == nil then menu.entries[2].enable = false end

    local action = menu:query()
    if action < 0 then return 0 end

    if action == 0 then
      add_or_update_var(selected)
    elseif action == 1 then
      remove_var(selected)
    end
  end
end

---@type fun(who: Character, item: Item, pos: Tripoint): integer
viewer.menu = function(params)
  local who = params.user
  local item = params.item
  local pos = params.pos
  local inventory = who:all_items(false)
  local menu = UiList.new()
  menu:title(color_text(locale.gettext("Item var viewer"), "yellow"))

  ---@type ItemVarViewerChoice[]
  local choices = {}
  local next_id = 0

  ---@param label_text string
  ---@param choice ItemVarViewerChoice
  local function push_choice(label_text, choice)
    menu:add(next_id, label_text)
    choices[next_id + 1] = choice
    next_id = next_id + 1
  end

  for _, candidate in ipairs(inventory) do
    ---@type ItemVarViewerChoice
    local choice = {
      type = "item",
      subject = candidate,
      subject_name = "",
      get_vars = function() return candidate:vars_table() end,
    }
    choice.subject_name = candidate:tname(1, false, 0)
    local count = count_vars(choice.get_vars())
    local label = string.format(
      "%s %s %s",
      color_text(locale.gettext("Item"), "magenta"),
      color_text(choice.subject_name, "white"),
      color_text(string.format("[%d vars]", count), "light_gray")
    )
    push_choice(label, choice)
  end

  ---@type ItemVarViewerChoice
  local player_choice = {
    type = "player",
    subject = who,
    subject_name = "",
    get_vars = function() return who:values_table() end,
  }
  player_choice.subject_name = who:disp_name(false, true)
  local player_count = count_vars(player_choice.get_vars())
  local player_label = string.format(
    "%s %s %s",
    color_text(locale.gettext("Player"), "yellow"),
    color_text(player_choice.subject_name, "white"),
    color_text(string.format("[%d vars]", player_count), "light_gray")
  )
  push_choice(player_label, player_choice)

  local map = gapi.get_map()
  ---@type Tripoint[]
  local points = map:points_in_radius(pos, 5)
  ---@type table<string, boolean>
  local seen_monsters = {}
  ---@type table<string, boolean>
  local seen_npcs = {}
  ---@type table<string, boolean>
  local seen_items = {}

  for _, pt in ipairs(points) do
    ---@type Monster?
    local monster_obj = gapi.get_monster_at(pt, false)
    if monster_obj then
      local key = tostring(monster_obj)
      if not seen_monsters[key] then
        seen_monsters[key] = true
        ---@type ItemVarViewerChoice
        local monster_choice = {
          type = "monster",
          subject = monster_obj,
          subject_name = "",
          get_vars = function() return monster_obj:values_table() end,
        }
        monster_choice.subject_name = monster_obj:disp_name(false, true)
        local monk_count = count_vars(monster_choice.get_vars())
        local monster_label = string.format(
          "%s %s %s",
          color_text(locale.gettext("Monster"), "red"),
          color_text(monster_choice.subject_name, "white"),
          color_text(string.format("[%d vars]", monk_count), "light_gray")
        )
        push_choice(monster_label, monster_choice)
      end
    end
    ---@type Npc?
    local npc_obj = gapi.get_npc_at(pt, false)
    if npc_obj then
      local key = tostring(npc_obj)
      if not seen_npcs[key] then
        seen_npcs[key] = true
        ---@type ItemVarViewerChoice
        local npc_choice = {
          type = "npc",
          subject = npc_obj,
          subject_name = "",
          get_vars = function() return npc_obj:values_table() end,
        }
        npc_choice.subject_name = npc_obj:disp_name(false, true)
        local npc_count = count_vars(npc_choice.get_vars())
        local npc_label = string.format(
          "%s %s %s",
          color_text(locale.gettext("NPC"), "light_blue"),
          color_text(npc_choice.subject_name, "white"),
          color_text(string.format("[%d vars]", npc_count), "light_gray")
        )
        push_choice(npc_label, npc_choice)
      end
    end
  end

  for _, pt in ipairs(points) do
    local stack = map:get_items_at(pt)
    for _, it in ipairs(stack:items()) do
      local key = tostring(it)
      if not seen_items[key] and it ~= item then
        seen_items[key] = true
        ---@type ItemVarViewerChoice
        local item_choice = {
          type = "ground_item",
          subject = it,
          subject_name = "",
          get_vars = function() return it:vars_table() end,
        }
        item_choice.subject_name = it:tname(1, false, 0)
        local map_item_count = count_vars(item_choice.get_vars())
        local label = string.format(
          "%s %s %s",
          color_text(locale.gettext("Ground"), "green"),
          color_text(item_choice.subject_name, "white"),
          color_text(string.format("[%d vars]", map_item_count), "light_gray")
        )
        push_choice(label, item_choice)
      end
    end
  end

  if next(choices) == nil then
    ui.popup(color_text(locale.gettext("You have nothing to inspect."), "light_gray"))
    return 0
  end

  local choice = menu:query()
  if choice < 0 then return 0 end

  ---@type ItemVarViewerChoice?
  local selected = choices[choice + 1]
  if not selected then return 0 end

  return manage_vars(selected)
end

return viewer

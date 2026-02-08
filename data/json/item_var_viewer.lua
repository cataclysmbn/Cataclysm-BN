local ui = require("lib.ui")

local viewer = {}

local function color_text(text, color)
  if text == nil then
    return ""
  end
  return string.format("<color_%s>%s</color>", color or "white", text)
end

local function count_vars(vars)
  if type(vars) ~= "table" then
    return 0
  end
  local sum = 0
  for _ in pairs(vars) do
    sum = sum + 1
  end
  return sum
end

local function format_label(display_name, vars)
  local header = string.format(locale.gettext("Stored vars for %s"), display_name)
  local count_line = string.format(locale.gettext("Stored entries: %d"), count_vars(vars))
  return color_text(header, "light_green") .. "  " .. color_text(count_line, "light_gray")
end

local function build_display_lines(display_name, vars)
  local lines = {
    format_label(display_name, vars),
    ""
  }

  local keys = {}
  for key in pairs(vars) do
    table.insert(keys, key)
  end
  table.sort(keys)

  for _, key in ipairs(keys) do
    table.insert(
      lines,
      string.format(
        "%s %s",
        color_text(string.format("%s:", key), "light_blue"),
        color_text(vars[key], "white")
      )
    )
  end

  return lines
end

viewer.menu = function(who, item, pos)
  local inventory = who:all_items(false)
  local menu = UiList.new()
  menu:title(color_text(locale.gettext("Item var viewer"), "yellow"))

  local choices = {}
  local next_id = 0

  local function push_choice(label_text, choice)
    menu:add(next_id, label_text)
    choices[next_id + 1] = choice
    next_id = next_id + 1
  end

  for _, candidate in ipairs(inventory) do
    local choice = {
      type = "item",
      subject = candidate,
      get_vars = function()
        return candidate:vars_table()
      end
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

  local player_choice = {
    type = "player",
    subject = who,
    get_vars = function()
      return who:values_table()
    end
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
  local points = map:points_in_radius(pos, 5)
  local seen_monsters = {}
  local seen_npcs = {}
  local seen_items = {}

  for _, pt in ipairs(points) do
    local monster_obj = gapi.get_monster_at(pt, false)
    if monster_obj then
      local key = tostring(monster_obj)
      if not seen_monsters[key] then
        seen_monsters[key] = true
        local monster_choice = {
          type = "monster",
          subject = monster_obj,
          get_vars = function()
            return monster_obj:values_table()
          end
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
    local npc_obj = gapi.get_npc_at(pt, false)
  if npc_obj then
    local key = tostring(npc_obj)
    if not seen_npcs[key] then
      seen_npcs[key] = true
        local npc_choice = {
          type = "npc",
          subject = npc_obj,
          get_vars = function()
            return npc_obj:values_table()
          end
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
      if not seen_items[key] then
        seen_items[key] = true
        local item_choice = {
          type = "ground_item",
          subject = it,
          get_vars = function()
            return it:vars_table()
          end
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
  if choice < 0 then
    return 0
  end

  local selected = choices[choice + 1]
  if not selected then
    return 0
  end

  local vars = selected.get_vars()
  if type(vars) ~= "table" or next(vars) == nil then
    ui.popup(
      color_text(
        string.format(
          locale.gettext("No stored variables found on %s."),
          selected.subject_name
        ),
        "light_gray"
      )
    )
    return 0
  end

  local lines = build_display_lines(selected.subject_name, vars)
  ui.popup(table.concat(lines, "\n"))
  return 0
end

return viewer

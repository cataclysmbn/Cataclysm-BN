local ui = require("lib.ui")

local function color_text(text, color) return string.format("<color_%s>%s</color>", color, text) end

local analyzer = {}

local function add_section(lines, label, entries)
  local label_clean = string.gsub(label, ":$", "")
  local label_text = color_text(label_clean, "green") .. color_text(":", "dark_gray")
  if #entries == 0 then
    table.insert(lines, string.format("%s %s", label_text, color_text(locale.gettext("None"), "white")))
    return
  end

  table.insert(lines, label_text)
  for _, entry in ipairs(entries) do
    table.insert(lines, string.format("  %s %s", color_text("-", "dark_gray"), color_text(entry, "white")))
  end
end

local function add_line(lines, label, value, value_color)
  local label_clean = string.gsub(label, ":$", "")
  local value_tint = value_color or "white"
  table.insert(
    lines,
    color_text(label_clean, "green") .. color_text(":", "dark_gray") .. " " .. color_text(value, value_tint)
  )
end

---@type fun(who: Character): Item[]
local function collect_artifacts(who)
  local items = who:all_items(false)
  local artifacts = {}

  for _, it in ipairs(items) do
    if it:is_artifact() then table.insert(artifacts, it) end
  end

  return artifacts
end

---@type fun(it: Item): string
local function describe_artifact(it)
  local itype_id = it:get_type()
  local itype = itype_id:obj()
  if not itype then return locale.gettext("Unable to read artifact data.") end

  local slot = itype:slot_artifact()
  if not slot then return locale.gettext("This item has no artifact effects.") end

  local lines = {}
  table.insert(lines, color_text("ARTIFACT ANALYSIS TERMINAL", "green"))
  add_line(lines, locale.gettext("SUBJECT"), it:tname(1, false, 0), "white")
  table.insert(lines, "")
  table.insert(lines, color_text("SCAN RESULTS", "green"))
  table.insert(lines, "")
  add_line(lines, locale.gettext("Charge"), slot:charge_type_description())
  add_line(lines, locale.gettext("Charge Requirement"), slot:charge_req_description())
  table.insert(lines, "")
  add_section(lines, locale.gettext("Activated"), slot:effects_activated_descriptions())
  add_section(lines, locale.gettext("Wielded"), slot:effects_wielded_descriptions())
  add_section(lines, locale.gettext("Worn"), slot:effects_worn_descriptions())
  add_section(lines, locale.gettext("Carried"), slot:effects_carried_descriptions())

  return table.concat(lines, "\n")
end

---@type fun(who: Character, item: Item, pos: Tripoint): integer
analyzer.menu = function(who, item, pos)
  local artifacts = collect_artifacts(who)

  if #artifacts == 0 then
    gapi.add_msg(locale.gettext("You have no artifacts to analyze."))
    return 0
  end

  local choice = 0
  if #artifacts > 1 then
    local menu = UiList.new()
    menu:title(locale.gettext("Artifact Analyzer"))
    for idx, it in ipairs(artifacts) do
      local name = it:tname(1, false, 0)
      menu:add(idx - 1, name)
    end
    choice = menu:query()
    if choice < 0 then return 0 end
  end

  local selected = artifacts[choice + 1]
  local report = describe_artifact(selected)
  ui.popup(report)

  return 0
end

return analyzer

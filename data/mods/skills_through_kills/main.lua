gdebug.log_info("Skills Through Kills: Main")

---@class ModSkillsThroughKills
local mod = game.mod_runtime[game.current_mod]
local gettext = locale.gettext
local ui = require("lib.ui")

local XP_VALUE_KEY = "skills_through_kills_xp"
local MAX_SKILL_LEVEL = 10

local SKILL_DEFS = {
  { id = "dodge", name = gettext("Dodging") },
  { id = "melee", name = gettext("Melee") },
  { id = "unarmed", name = gettext("Unarmed Combat") },
  { id = "bashing", name = gettext("Bashing Weapons") },
  { id = "cutting", name = gettext("Cutting Weapons") },
  { id = "stabbing", name = gettext("Piercing Weapons") },
  { id = "throw", name = gettext("Throwing") },
  { id = "gun", name = gettext("Marksmanship") },
  { id = "pistol", name = gettext("Handguns") },
  { id = "shotgun", name = gettext("Shotguns") },
  { id = "smg", name = gettext("Submachine Guns") },
  { id = "rifle", name = gettext("Rifles") },
  { id = "archery", name = gettext("Archery") },
  { id = "launcher", name = gettext("Launchers") },
  { id = "mechanics", name = gettext("Mechanics") },
  { id = "electronics", name = gettext("Electronics") },
  { id = "cooking", name = gettext("Cooking") },
  { id = "tailor", name = gettext("Tailoring") },
  { id = "firstaid", name = gettext("First Aid") },
  { id = "speech", name = gettext("Speaking") },
  { id = "barter", name = gettext("Bartering") },
  { id = "computer", name = gettext("Computers") },
  { id = "survival", name = gettext("Survival") },
  { id = "traps", name = gettext("Trapping") },
  { id = "swimming", name = gettext("Athletics") },
  { id = "driving", name = gettext("Driving") },
  { id = "fabrication", name = gettext("Fabrication") },
  { id = "spellcraft", name = gettext("Spellcraft") },
}

---@class SkillsThroughKillsSkill
---@field id string
---@field skill_id SkillId
---@field name string

---@type SkillsThroughKillsSkill[]|nil
local cached_skills = nil

---@param text string
---@param color string
---@return string
local function color_text(text, color) return string.format("<color_%s>%s</color>", color, text) end

---@param number integer
---@return string
local function number_shorthand(number)
  if math.abs(number) < 1000 then return tostring(number) end
  if math.abs(number) < 1000000 then return string.format("%dk", math.floor(number / 1000)) end
  return string.format("%dm", math.floor(number / 1000000))
end

---@param character Character
---@return integer
local function get_xp(character)
  local value = character:get_value(XP_VALUE_KEY)
  if value == "" then return 0 end
  return tonumber(value) or 0
end

---@param character Character
---@param value integer
local function set_xp(character, value) character:set_value(XP_VALUE_KEY, tostring(math.floor(value))) end

---@param character Character
---@param amount integer
local function add_xp(character, amount)
  if amount <= 0 then return end
  set_xp(character, get_xp(character) + amount)
end

---@return SkillsThroughKillsSkill[]
local function get_skills()
  if cached_skills then return cached_skills end

  cached_skills = {}
  for _, skill_def in ipairs(SKILL_DEFS) do
    local skill_id = SkillId.new(skill_def.id)
    if skill_id:is_valid() then
      table.insert(cached_skills, {
        id = skill_def.id,
        skill_id = skill_id,
        name = skill_def.name,
      })
    end
  end
  return cached_skills
end

---@param skill_id SkillId
---@return SkillsThroughKillsSkill|nil
local function find_skill(skill_id)
  local skill_id_string = skill_id:str()
  for _, skill in ipairs(get_skills()) do
    if skill.skill_id:str() == skill_id_string then return skill end
  end
  return nil
end

---@param levels table<string, integer>
---@return integer
local function spent_for_levels(levels)
  local total_levels = 0
  local total_pow4_levels = 0
  for _, level in pairs(levels) do
    total_levels = total_levels + level
    total_pow4_levels = total_pow4_levels + (level * level * level * level)
  end
  return 10 * total_levels + total_levels * total_levels + total_pow4_levels
end

---@param character Character
---@return table<string, integer>
local function skill_levels(character)
  local levels = {}
  for _, skill in ipairs(get_skills()) do
    levels[skill.id] = character:get_skill_level(skill.skill_id)
  end
  return levels
end

---@param character Character
---@return integer
local function spent_xp(character) return spent_for_levels(skill_levels(character)) end

---@param character Character
---@return integer
local function available_xp(character) return get_xp(character) - spent_xp(character) end

---@param character Character
---@param skill SkillsThroughKillsSkill
---@return integer
local function cost_to_raise(character, skill)
  local levels = skill_levels(character)
  local current_level = levels[skill.id] or 0
  levels[skill.id] = current_level + 1
  return spent_for_levels(levels) - spent_xp(character)
end

---@param monster Monster
---@return integer
local function monster_xp(monster)
  local monster_type = monster:get_type():obj()
  return math.max(0, monster_type.difficulty + monster_type.difficulty_base)
end

---@param character Character
---@param skill SkillsThroughKillsSkill
---@return boolean
local function try_raise_skill(character, skill)
  local level = character:get_skill_level(skill.skill_id)
  if level >= MAX_SKILL_LEVEL then
    ui.popup(gettext("This skill is already at the maximum level."))
    return false
  end

  local available = available_xp(character)
  local cost = cost_to_raise(character, skill)
  if available < cost then
    ui.popup(
      string.format(
        gettext("Not enough XP to raise %s (%d -> %d).\nNeeded XP: %d\nAvailable XP: %d"),
        skill.name,
        level,
        level + 1,
        cost,
        available
      )
    )
    return false
  end

  if
    not ui.query_yn(
      string.format(
        gettext("Raise %s from level %d to level %d?\nThis uses %d XP out of %d available XP."),
        skill.name,
        level,
        level + 1,
        cost,
        available
      )
    )
  then
    return false
  end

  character:set_skill_level(skill.skill_id, level + 1)
  gapi.add_msg(MsgType.good, string.format(gettext("You raise %s to level %d."), skill.name, level + 1))
  return true
end

---@param character Character
---@param skill SkillsThroughKillsSkill
---@return string
local function skill_info_text(character, skill)
  local level = character:get_skill_level(skill.skill_id)
  local available = available_xp(character)
  local total = get_xp(character)
  local spent = spent_xp(character)

  if level >= MAX_SKILL_LEVEL then
    return string.format(
      "%s\n%s",
      color_text(gettext("Skills Through Kills"), "light_green"),
      string.format(
        gettext("Total XP: %d | Spent XP: %d | Available XP: %d\nThis skill is at maximum level."),
        total,
        spent,
        available
      )
    )
  end

  local cost = cost_to_raise(character, skill)
  local percentage = math.floor(100 * available / cost)
  local readiness = available >= cost and color_text(gettext("ready"), "light_green")
    or string.format(gettext("%d%% funded"), percentage)

  return string.format(
    "%s\n%s",
    color_text(gettext("Skills Through Kills"), "light_green"),
    string.format(
      gettext(
        "Total XP: %d | Spent XP: %d | Available XP: %d\nCost to raise to level %d: %d XP (%s).\nPress Enter to spend XP on this skill."
      ),
      total,
      spent,
      available,
      level + 1,
      cost,
      readiness
    )
  )
end

---@param params table
mod.on_mon_death = function(params)
  ---@type Creature|nil
  local killer = params.killer
  ---@type Monster|nil
  local monster = params.mon
  if not killer or not monster then return end
  if not killer:is_avatar() then return end
  if monster:is_hallucination() then return end

  add_xp(killer:as_character(), monster_xp(monster))
end

---@param params table
mod.on_character_death = function(params)
  ---@type Creature|nil
  local killer = params.killer
  ---@type Character|nil
  local character = params.char
  if not killer or not character then return end
  if not killer:is_avatar() then return end
  if character:is_avatar() then return end

  add_xp(killer:as_character(), 10)
end

---@param params table
mod.on_character_display_skill_action = function(params)
  ---@type Character|nil
  local character = params.character
  ---@type SkillId|nil
  local skill_id = params.skill
  if not character or not skill_id then return end
  if not character:is_avatar() then return end

  local skill = find_skill(skill_id)
  if not skill then return end

  params.results.handled = true
  try_raise_skill(character, skill)
end

---@param params table
mod.on_character_display_skill_info = function(params)
  ---@type Character|nil
  local character = params.character
  ---@type SkillId|nil
  local skill_id = params.skill
  if not character or not skill_id then return end
  if not character:is_avatar() then return end

  local skill = find_skill(skill_id)
  if not skill then return end

  params.results.text = skill_info_text(character, skill)
end

---@return table
local function draw_xp_widget()
  local avatar = gapi.get_avatar()
  if not avatar then return {} end

  local available = available_xp(avatar)
  local total = get_xp(avatar)
  local available_color = available >= 0 and "light_green" or "light_red"
  return {
    { text = string.format("XP: %s", number_shorthand(available)), color = available_color },
    { text = string.format("Total: %s", number_shorthand(total)), color = "light_gray" },
  }
end

sidebar.register_widget({
  id = "skills_through_kills_xp",
  name = gettext("Skills Through Kills XP"),
  height = 2,
  order = 25,
  default_toggle = true,
  draw = draw_xp_widget,
})

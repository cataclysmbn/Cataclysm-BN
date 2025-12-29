gdebug.log_info("RPG System: Main")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]
local mutations = require("rpg_mutations")
local Mutation = mutations.Mutation
local MUTATIONS = mutations.MUTATIONS
local ALL_CLASS_IDS = mutations.ALL_CLASS_IDS
local ALL_TRAIT_IDS = mutations.ALL_TRAIT_IDS
local BASE_CLASS_IDS = mutations.BASE_CLASS_IDS
local PRESTIGE_CLASS_IDS = mutations.PRESTIGE_CLASS_IDS
local STAT_BONUS_IDS = mutations.STAT_BONUS_IDS
local PERIODIC_BONUS_IDS = mutations.PERIODIC_BONUS_IDS
local KILL_MONSTER_BONUS_IDS = mutations.KILL_MONSTER_BONUS_IDS
local register_mutation = mutations.register_mutation
local function color_text(text, color) return string.format("<color_%s>%s</color>", color, text) end

local function color_good(text) return color_text(text, "light_green") end

local function color_bad(text) return color_text(text, "light_red") end

local function color_warning(text) return color_text(text, "yellow") end

local function color_info(text) return color_text(text, "light_cyan") end

local function color_highlight(text) return color_text(text, "white") end

local function create_progress_bar(current, max, width)
  width = width or 20
  local filled = math.floor((current / max) * width)
  local empty = width - filled
  local bar = color_good(string.rep("█", filled)) .. color_text(string.rep("░", empty), "dark_gray")
  local percent = math.floor((current / max) * 100)
  return string.format("[%s] %d%%", bar, percent)
end

local function wrap_text(text, width, indent)
  width = width or 60
  indent = indent or ""

  if #text <= width then return indent .. text end

  local lines = {}
  local current_line = ""

  for word in text:gmatch("%S+") do
    local test_line = current_line == "" and word or (current_line .. " " .. word)
    if #test_line <= width then
      current_line = test_line
    else
      if current_line ~= "" then table.insert(lines, indent .. current_line) end
      current_line = word
    end
  end

  if current_line ~= "" then table.insert(lines, indent .. current_line) end

  return table.concat(lines, "\n")
end

-- Experience Curve Functions for Levels 1-40
-- Formula: XP = 2.2387 * level^3.65
local XP_COEFFICIENT = 2.2387
local XP_EXPONENT = 3.65

-- Progression Constants
local LEVELS_PER_STAT_POINT = 2
local LEVELS_PER_TRAIT_SLOT = 5

-- Function to register a new mutation with the RPG system (can be called by other mods)
-- config: table with mutation properties (id, type, symbol, requirements, stat_bonuses, etc.)
-- See rpg_mutations.lua Mutation class for full structure
mod.add_mutation = function(config)
  if not config or not config.id then
    gdebug.log_error("RPG System: add_mutation called without valid config or id")
    return false
  end

  if MUTATIONS[config.id] then
    -- If it's already there, just ignore.
    return false
  end

  local mutation = Mutation.new(config)
  MUTATIONS[config.id] = mutation
  register_mutation(mutation)

  gdebug.log_info("RPG System: Registered new mutation: " .. config.id)
  return true
end

local function get_rpg_level(exp)
  local level = math.floor((exp / XP_COEFFICIENT) ^ (1 / XP_EXPONENT))
  return math.min(level, 40)
end

local function rpg_xp_needed(level)
  if level >= 40 then return XP_COEFFICIENT * (40 ^ XP_EXPONENT) end
  return XP_COEFFICIENT * (level ^ XP_EXPONENT)
end

local function get_char_value(char, key, default)
  local val = char:get_value(key)
  if val == "" then return default end
  return tonumber(val) or default
end

local function set_char_value(char, key, value) char:set_value(key, tostring(value)) end

-- Common requirement checking and formatting
local function check_requirements(player, mutation, current_level)
  local reqs = mutation.requirements
  if not reqs or (not reqs.level and not reqs.stats and not reqs.skills) then return true, {} end

  local unmet = {}

  -- Check level requirement
  if reqs.level and current_level < reqs.level then
    table.insert(unmet, { type = "level", label = "Lv", current = current_level, required = reqs.level })
  end

  -- Check stat requirements
  if reqs.stats then
    local stats_map = {
      STR = player:get_str(),
      DEX = player:get_dex(),
      INT = player:get_int(),
      PER = player:get_per(),
    }
    for stat, required in pairs(reqs.stats) do
      local current = stats_map[stat]
      if current < required then
        table.insert(unmet, { type = "stat", label = stat, current = current, required = required })
      end
    end
  end

  -- Check skill requirements
  if reqs.skills then
    for skill_name, required in pairs(reqs.skills) do
      local current = player:get_skill_level(SkillId.new(skill_name))
      if current < required then
        local display_name = skill_name:gsub("^%l", string.upper)
        table.insert(unmet, { type = "skill", label = display_name, current = current, required = required })
      end
    end
  end

  return #unmet == 0, unmet
end

local function format_requirement(label, current, required, show_current)
  local met = current >= required
  local color_fn = met and color_good or color_bad
  if show_current then
    return color_fn(string.format("%s %d/%d", label, current, required))
  else
    return color_fn(string.format("%s %d+", label, required))
  end
end

local function format_requirements_list(unmet, show_current)
  local parts = {}
  for _, req in ipairs(unmet) do
    table.insert(parts, format_requirement(req.label, req.current, req.required, show_current))
  end
  return table.concat(parts, ", ")
end

local function has_class(char)
  for _, class_id in ipairs(ALL_CLASS_IDS) do
    if char:has_trait(class_id) then return true end
  end
  return false
end

local function get_class_info(char)
  for _, class_id in ipairs(ALL_CLASS_IDS) do
    if char:has_trait(class_id) then
      local class_obj = class_id:obj()
      return class_obj:name(), class_obj:desc()
    end
  end
  return "[None]", nil
end

local function get_current_traits(char)
  local current = {}
  for _, trait_id in ipairs(ALL_TRAIT_IDS) do
    if char:has_trait(trait_id) then
      local trait_obj = trait_id:obj()
      table.insert(current, { name = trait_obj:name(), desc = trait_obj:desc() })
    end
  end
  return current
end

mod.on_game_started = function()
  local player = gapi.get_avatar()

  set_char_value(player, "rpg_level", 0)
  set_char_value(player, "rpg_num_traits", 0)
  set_char_value(player, "rpg_max_traits", 1)
  set_char_value(player, "rpg_exp", 0)
  set_char_value(player, "rpg_xp_to_next_level", rpg_xp_needed(1))
  set_char_value(player, "rpg_stat_points", 0)
  set_char_value(player, "rpg_assigned_str", 0)
  set_char_value(player, "rpg_assigned_dex", 0)
  set_char_value(player, "rpg_assigned_int", 0)
  set_char_value(player, "rpg_assigned_per", 0)
  set_char_value(player, "rpg_level_scaling", 100)

  player:add_item_with_id(ItypeId.new("system_interface"), 1)

  gapi.add_msg(
    MsgType.mixed,
    color_info("[System]")
      .. " initialized. Welcome "
      .. color_highlight("[User]")
      .. " of world IB-73758-R. Use your "
      .. color_info("[System Interface]")
      .. " to access the "
      .. color_info("[System]")
      .. "."
  )
end

mod.on_game_load = function()
  local player = gapi.get_avatar()

  local level = get_char_value(player, "rpg_level", -1)
  if level < 0 then
    set_char_value(player, "rpg_level", 0)
    set_char_value(player, "rpg_num_traits", 0)
    set_char_value(player, "rpg_max_traits", 1)
    set_char_value(player, "rpg_exp", 0)
    set_char_value(player, "rpg_xp_to_next_level", rpg_xp_needed(1))
    set_char_value(player, "rpg_stat_points", 0)
    set_char_value(player, "rpg_assigned_str", 0)
    set_char_value(player, "rpg_assigned_dex", 0)
    set_char_value(player, "rpg_assigned_int", 0)
    set_char_value(player, "rpg_assigned_per", 0)
    set_char_value(player, "rpg_level_scaling", 100)

    player:add_item_with_id(ItypeId.new("system_interface"), 1)

    gapi.add_msg(
      MsgType.mixed,
      color_info("[System]")
        .. " initialized. Welcome "
        .. color_highlight("[User]")
        .. " of world IB-73758-R. Use your "
        .. color_info("[System Interface]")
        .. " to access the "
        .. color_info("[System]")
        .. "."
    )
    gdebug.log_info("RPG System: Initialized on game load")
    return
  end

  gdebug.log_info("RPG System: Loaded character at level " .. level)
end

mod.on_monster_killed = function(params)
  local killer = params.killer
  local monster = params.mon

  if not killer or not monster then return end
  if not killer:is_avatar() then return end

  local player = killer:as_character()
  if not player then return end

  local exp = get_char_value(player, "rpg_exp", 0)
  local old_level = get_char_value(player, "rpg_level", 0)

  local monster_hp = monster:get_hp_max()
  local xp_gain = math.max(1, math.floor(monster_hp / 10))
  exp = exp + xp_gain
  set_char_value(player, "rpg_exp", exp)

  local new_level = get_rpg_level(exp)
  if new_level > old_level then
    set_char_value(player, "rpg_level", new_level)

    local level_msg = color_good("★ ")
      .. color_highlight("LEVEL UP!")
      .. color_good(" ★")
      .. " You are now "
      .. color_info("Level " .. new_level)
      .. "!"
    gapi.add_msg(MsgType.good, level_msg)

    -- Calculate trait slots
    local old_max_traits = 1 + math.floor(old_level / LEVELS_PER_TRAIT_SLOT)
    local new_max_traits = 1 + math.floor(new_level / LEVELS_PER_TRAIT_SLOT)
    set_char_value(player, "rpg_max_traits", new_max_traits)

    if new_max_traits > old_max_traits then
      local traits_gained = new_max_traits - old_max_traits
      gapi.add_msg(
        MsgType.good,
        color_good(string.format("New trait slot%s unlocked!", traits_gained > 1 and "s" or ""))
          .. " You now have "
          .. color_highlight(new_max_traits)
          .. " trait slots."
      )
    end

    -- Calculate stat points earned by iterating through levels crossed
    local stat_points_earned = 0
    for level = old_level + 1, new_level do
      if level % LEVELS_PER_STAT_POINT == 0 then stat_points_earned = stat_points_earned + 1 end
    end

    if stat_points_earned > 0 then
      local stat_points = get_char_value(player, "rpg_stat_points", 0)
      stat_points = stat_points + stat_points_earned
      set_char_value(player, "rpg_stat_points", stat_points)
      gapi.add_msg(
        MsgType.good,
        color_good(
          string.format("✦ %d stat point%s earned!", stat_points_earned, stat_points_earned > 1 and "s" or "")
        )
          .. " You have "
          .. color_highlight(stat_points)
          .. " unassigned stat point"
          .. (stat_points > 1 and "s" or "")
          .. "."
      )
    end
  end

  local xp_needed = rpg_xp_needed(new_level + 1)
  local xp_to_next = xp_needed - exp
  set_char_value(player, "rpg_xp_to_next_level", xp_to_next)

  -- Apply kill monster bonuses (e.g., healing on kill)
  local level = get_char_value(player, "rpg_level", 0)
  local level_scaling = get_char_value(player, "rpg_level_scaling", 100) / 100.0

  for _, mutation_id in ipairs(KILL_MONSTER_BONUS_IDS) do
    if player:has_trait(mutation_id) then
      local mutation = MUTATIONS[mutation_id:str()]
      local bonuses = mutation.kill_monster_bonuses

      if bonuses.heal_percent then
        local heal_amount = math.max(1, math.floor(monster_hp * (bonuses.heal_percent * level * level_scaling / 100)))
        player:healall(heal_amount)
      end
    end
  end
end

mod.on_character_reset_stats = function(params)
  local character = params.character
  if not character then return end
  if not character:is_avatar() then return end

  local level = get_char_value(character, "rpg_level", 0)
  local level_scaling = get_char_value(character, "rpg_level_scaling", 100) / 100.0

  -- Apply manually assigned stats
  local assigned_str = get_char_value(character, "rpg_assigned_str", 0)
  local assigned_dex = get_char_value(character, "rpg_assigned_dex", 0)
  local assigned_int = get_char_value(character, "rpg_assigned_int", 0)
  local assigned_per = get_char_value(character, "rpg_assigned_per", 0)

  character:mod_str_bonus(assigned_str)
  character:mod_dex_bonus(assigned_dex)
  character:mod_int_bonus(assigned_int)
  character:mod_per_bonus(assigned_per)

  -- Apply stat bonuses from mutations that have them
  for _, mutation_id in ipairs(STAT_BONUS_IDS) do
    if character:has_trait(mutation_id) then
      local mutation = MUTATIONS[mutation_id:str()]
      local bonuses = mutation.stat_bonuses

      if bonuses.str then character:mod_str_bonus(math.floor(level * bonuses.str * level_scaling)) end
      if bonuses.dex then character:mod_dex_bonus(math.floor(level * bonuses.dex * level_scaling)) end
      if bonuses.int then character:mod_int_bonus(math.floor(level * bonuses.int * level_scaling)) end
      if bonuses.per then character:mod_per_bonus(math.floor(level * bonuses.per * level_scaling)) end
      if bonuses.speed then character:mod_speed_bonus(math.floor(level * bonuses.speed * level_scaling)) end
    end
  end
end

mod.on_every_5_minutes = function()
  local player = gapi.get_avatar()
  if not player then return end

  local level = get_char_value(player, "rpg_level", 0)
  if level <= 0 then return end

  local level_scaling = get_char_value(player, "rpg_level_scaling", 100) / 100.0

  -- Apply periodic bonuses from mutations that have them
  for _, mutation_id in ipairs(PERIODIC_BONUS_IDS) do
    if player:has_trait(mutation_id) then
      local mutation = MUTATIONS[mutation_id:str()]
      local bonuses = mutation.periodic_bonuses

      if bonuses.fatigue then player:mod_fatigue(math.floor(level * bonuses.fatigue * level_scaling)) end
      if bonuses.stamina then player:mod_stamina(math.floor(level * bonuses.stamina * level_scaling)) end
      if bonuses.thirst and player:get_thirst() >= 40 and math.random() > 0.75 then
        player:mod_thirst(math.floor(level * bonuses.thirst * level_scaling * 4))
      end
      if bonuses.rad then player:mod_rad(math.floor(level * bonuses.rad * level_scaling)) end
      if bonuses.healthy_mod then player:mod_healthy_mod(bonuses.healthy_mod * level * level_scaling, 100) end
      if bonuses.power_level then
        local power_regen = Energy.from_joule(math.floor(level * bonuses.power_level * 1000 * level_scaling))
        player:mod_power_level(power_regen)
      end
    end
  end
end

mod.open_rpg_menu = function(who, item, pos)
  if not who:is_avatar() then return 0 end

  local player = who
  local keep_open = true

  while keep_open do
    -- Refresh player stats to reflect any changes from class/trait selections
    player:reset()

    local exp = get_char_value(player, "rpg_exp", 0)
    local level = get_char_value(player, "rpg_level", 0)
    local num_traits = get_char_value(player, "rpg_num_traits", 0)
    local max_traits = get_char_value(player, "rpg_max_traits", 1)
    local xp_to_next = get_char_value(player, "rpg_xp_to_next_level", 0)
    local class_name, class_desc = get_class_info(player)
    local current_traits = get_current_traits(player)

    local ui = UiList.new()
    ui:title("=== [SYSTEM] ===")

    local current_level_xp = rpg_xp_needed(level)
    local next_level_xp = rpg_xp_needed(level + 1)
    local xp_in_level = exp - current_level_xp
    local xp_for_level = next_level_xp - current_level_xp

    local str_val = player:get_str()
    local dex_val = player:get_dex()
    local int_val = player:get_int()
    local per_val = player:get_per()
    local player_name = player:get_name()

    local info_text = ""

    info_text = info_text .. color_highlight("Name: ") .. color_good(player_name) .. "\n"
    info_text = info_text .. color_highlight("Level: ") .. color_good(string.format("%d", level))
    if level < 40 then
      info_text = info_text .. color_text(string.format(" (Next: %.0f XP)", xp_to_next), "light_gray")
    else
      info_text = info_text .. color_warning(" [MAX]")
    end
    info_text = info_text .. "\n"

    if level < 40 then
      local progress = create_progress_bar(xp_in_level, xp_for_level, 30)
      info_text = info_text .. "XP: " .. progress .. "\n"
    end

    info_text = info_text .. color_highlight("Total XP: ") .. string.format("%.0f", exp) .. "\n"
    info_text = info_text
      .. color_text(
        "─────────────────────────────────────────",
        "light_gray"
      )
      .. "\n"

    local stat_points = get_char_value(player, "rpg_stat_points", 0)
    info_text = info_text .. color_highlight("Stats:") .. "\n"
    info_text = info_text
      .. string.format(
        "  %s %s  %s %s  %s %s  %s %s\n",
        color_text("STR:", "light_gray"),
        color_good(str_val),
        color_text("DEX:", "light_gray"),
        color_good(dex_val),
        color_text("INT:", "light_gray"),
        color_good(int_val),
        color_text("PER:", "light_gray"),
        color_good(per_val)
      )
    if stat_points > 0 then
      info_text = info_text
        .. color_good(
          "  ✦ " .. stat_points .. " unassigned stat point" .. (stat_points > 1 and "s" or "") .. " available!\n"
        )
    end

    local all_skills = {
      { name = "Dodge", skill = "dodge" },
      { name = "Melee", skill = "melee" },
      { name = "Unarmed", skill = "unarmed" },
      { name = "Bashing", skill = "bashing" },
      { name = "Cutting", skill = "cutting" },
      { name = "Piercing", skill = "stabbing" },
      { name = "Archery", skill = "archery" },
      { name = "Handguns", skill = "gun" },
      { name = "Rifles", skill = "rifle" },
      { name = "Shotguns", skill = "shotgun" },
      { name = "SMGs", skill = "smg" },
      { name = "Launchers", skill = "launcher" },
      { name = "Throwing", skill = "throw" },
      { name = "Cooking", skill = "cooking" },
      { name = "Tailoring", skill = "tailor" },
      { name = "Mechanics", skill = "mechanics" },
      { name = "Electronics", skill = "electronics" },
      { name = "Fabrication", skill = "fabrication" },
      { name = "First Aid", skill = "firstaid" },
      { name = "Computers", skill = "computer" },
      { name = "Survival", skill = "survival" },
      { name = "Trapping", skill = "traps" },
      { name = "Swimming", skill = "swimming" },
      { name = "Driving", skill = "driving" },
      { name = "Bartering", skill = "barter" },
      { name = "Speech", skill = "speech" },
      { name = "Spellcraft", skill = "spellcraft" },
    }

    info_text = info_text .. color_highlight("Skills:") .. "\n"

    local displayed_skills = {}
    for _, skill_info in ipairs(all_skills) do
      local skill_level = player:get_skill_level(SkillId.new(skill_info.skill))
      if skill_level > 0 then table.insert(displayed_skills, { name = skill_info.name, level = skill_level }) end
    end

    -- Display trained skills
    if #displayed_skills > 0 then
      local skill_count = 0
      for _, skill_data in ipairs(displayed_skills) do
        local skill_color = skill_data.level >= 5 and "light_green"
          or (skill_data.level >= 3 and "white" or "light_gray")
        info_text = info_text
          .. string.format(
            "  %s %s",
            color_text(skill_data.name .. ":", "light_gray"),
            color_text(skill_data.level, skill_color)
          )
        skill_count = skill_count + 1
        if skill_count % 3 == 0 then
          info_text = info_text .. "\n"
        else
          info_text = info_text .. "  "
        end
      end
      if skill_count % 3 ~= 0 then info_text = info_text .. "\n" end
    else
      info_text = info_text .. color_text("  No skills trained yet.", "dark_gray") .. "\n"
    end

    info_text = info_text
      .. color_text(
        "─────────────────────────────────────────",
        "light_gray"
      )
      .. "\n"

    -- Class info
    if class_name == "[None]" then
      info_text = info_text .. color_highlight("Class: ") .. color_warning(class_name) .. "\n"
    else
      info_text = info_text .. color_highlight("Class: ") .. color_good(class_name) .. "\n"
      if class_desc then
        local wrapped = wrap_text(class_desc, 60, "  ")
        info_text = info_text .. color_text(wrapped, "light_gray") .. "\n"
      end
    end

    info_text = info_text
      .. color_text(
        "─────────────────────────────────────────",
        "light_gray"
      )
      .. "\n"

    -- Trait section
    info_text = info_text
      .. color_highlight("Trait Slots: ")
      .. color_good(string.format("%d", num_traits))
      .. color_text("/", "light_gray")
      .. color_highlight(string.format("%d", max_traits))
      .. "\n"

    if #current_traits > 0 then
      info_text = info_text .. color_highlight("Active Traits:") .. "\n"
      for _, trait in ipairs(current_traits) do
        info_text = info_text .. color_good("  • " .. trait.name) .. "\n"
        local wrapped = wrap_text(trait.desc, 58, "    ")
        info_text = info_text .. color_text(wrapped, "light_gray") .. "\n"
      end
    else
      info_text = info_text .. color_text("  No traits selected yet.", "dark_gray") .. "\n"
    end

    info_text = info_text .. "\n"

    ui:text(info_text)

    local menu_items = {}
    table.insert(menu_items, { text = "Manage Class", action = "class" })

    if stat_points > 0 then
      table.insert(
        menu_items,
        { text = color_good("Assign Stats (" .. stat_points .. " available)"), action = "stats" }
      )
    end

    -- Always show the Assign Traits menu (allows resetting traits)
    local available_slots = max_traits - num_traits
    if available_slots > 0 then
      table.insert(
        menu_items,
        { text = color_good("Assign Traits (" .. available_slots .. " available)"), action = "traits" }
      )
    else
      table.insert(menu_items, { text = "Manage Traits", action = "traits" })
    end

    table.insert(menu_items, { text = "Help", action = "help" })
    table.insert(menu_items, { text = "Close", action = "close" })

    for i, item_entry in ipairs(menu_items) do
      ui:add(i, item_entry.text)
    end

    local choice_index = ui:query()

    if choice_index > 0 and choice_index <= #menu_items then
      local chosen = menu_items[choice_index]

      if chosen.action == "class" then
        mod.manage_class_menu(player)
      elseif chosen.action == "stats" then
        mod.assign_stats_menu(player)
      elseif chosen.action == "traits" then
        mod.manage_traits_menu(player)
      elseif chosen.action == "help" then
        mod.show_help_menu(player)
      elseif chosen.action == "close" then
        keep_open = false
      end
    else
      keep_open = false
    end
  end

  return 0
end

mod.manage_class_menu = function(player)
  local level = get_char_value(player, "rpg_level", 0)

  local ui = UiList.new()
  ui:title("=== Select Class ===")
  ui:desc_enabled(true)

  local options = {}
  local index = 1

  -- Show base classes if player doesn't have a class
  if not has_class(player) then
    for id, mutation in pairs(MUTATIONS) do
      if mutation.type == "class" and not mutation.is_prestige then
        local mutation_id = mutation:get_mutation_id()
        local class_obj = mutation_id:obj()
        local can_select, unmet = check_requirements(player, mutation, level)

        local display_text
        if can_select then
          display_text = color_good(mutation.symbol .. " [" .. class_obj:name() .. "]")
        else
          display_text = color_text(mutation.symbol .. " [" .. class_obj:name() .. "]", "dark_gray")
            .. " - "
            .. format_requirements_list(unmet, true)
        end

        table.insert(options, {
          text = display_text,
          desc = class_obj:desc(),
          id = mutation_id,
          mutation = mutation,
          can_select = can_select,
          index = index,
        })
        index = index + 1
      end
    end
  end

  -- Show prestige classes if player has the appropriate base class
  for id, mutation in pairs(MUTATIONS) do
    if mutation.type == "class" and mutation.is_prestige then
      local base_class_id = MUTATIONS[mutation.base_class]:get_mutation_id()
      if player:has_trait(base_class_id) then
        local mutation_id = mutation:get_mutation_id()
        local class_obj = mutation_id:obj()
        local can_select, unmet = check_requirements(player, mutation, level)

        local display_text
        if can_select then
          display_text = color_good(mutation.symbol .. " [" .. class_obj:name() .. "]")
            .. color_text(" (Prestige)", "yellow")
        else
          display_text = color_text(mutation.symbol .. " [" .. class_obj:name() .. "]", "dark_gray")
            .. " - "
            .. format_requirements_list(unmet, true)
        end

        table.insert(options, {
          text = display_text,
          desc = class_obj:desc(),
          id = mutation_id,
          mutation = mutation,
          remove_first = base_class_id,
          can_select = can_select,
          index = index,
        })
        index = index + 1
      end
    end
  end

  -- Show abandon option if player has a class
  if has_class(player) then
    table.insert(options, {
      text = color_bad("✖ Abandon class"),
      desc = color_warning("WARNING: ") .. "Damages your soul for 3 days, with severe penalties.",
      action = "abandon",
      can_select = true,
      index = index,
    })
    index = index + 1
  end

  -- Back option
  table.insert(options, {
    text = color_text("← Back", "light_gray"),
    desc = "Return to main menu",
    action = "back",
    can_select = true,
    index = index,
  })

  for i, opt in ipairs(options) do
    ui:add_w_desc(opt.index, opt.text, opt.desc or "")
    if ui.entries and ui.entries[i] then ui.entries[i].enable = opt.can_select end
  end

  local choice_index = ui:query()

  if choice_index > 0 and choice_index <= #options then
    local chosen = options[choice_index]

    if not chosen.can_select then
      gapi.add_msg(MsgType.warning, color_warning("You don't meet the requirements for this class."))
      return
    end

    if chosen.action == "back" then
      return
    elseif chosen.action == "abandon" then
      -- Remove all classes
      for _, class_id in ipairs(ALL_CLASS_IDS) do
        if player:has_trait(class_id) then player:remove_mutation(class_id, true) end
      end

      player:add_effect(EffectTypeId.new("damaged_soul"), TimeDuration.from_hours(72))
      gapi.add_msg(
        MsgType.bad,
        color_bad("✖ You have abandoned your class!") .. " Your soul is " .. color_bad("damaged") .. " for 3 days."
      )
    elseif chosen.id then
      if chosen.remove_first then player:remove_mutation(chosen.remove_first, true) end
      player:set_mutation(chosen.id)
      local class_name = chosen.id:obj():name()
      gapi.add_msg(
        MsgType.good,
        color_good("✓ You have chosen your path as ") .. color_highlight(class_name) .. color_good("!")
      )
    end
  end
end

mod.manage_traits_menu = function(player)
  local level = get_char_value(player, "rpg_level", 0)
  local num_traits = get_char_value(player, "rpg_num_traits", 0)
  local max_traits = get_char_value(player, "rpg_max_traits", 1)
  local has_available_slots = num_traits < max_traits

  local ui = UiList.new()
  ui:title(string.format("=== Select Trait (%d/%d) ===", num_traits, max_traits))
  ui:desc_enabled(true)

  local traits = {}
  local index = 1

  -- Loop through all trait mutations
  for id, mutation in pairs(MUTATIONS) do
    if mutation.type == "trait" then
      local trait_id = mutation:get_mutation_id()
      local already_has = player:has_trait(trait_id)
      local trait_obj = trait_id:obj()
      local trait_name = trait_obj:name()
      local trait_desc = trait_obj:desc()

      local can_select_reqs, unmet = check_requirements(player, mutation, level)
      -- Can only select if you have available slots, meet requirements, and don't already have it
      local can_select = has_available_slots and not already_has and can_select_reqs

      local display_name
      if already_has then
        display_name = color_text("✓ " .. trait_name, "dark_gray") .. color_text(" (Owned)", "dark_gray")
      elseif not has_available_slots then
        display_name = color_text("◆ " .. trait_name, "dark_gray") .. color_text(" (No slots available)", "dark_gray")
      elseif not can_select_reqs then
        display_name = color_text("◆ " .. trait_name, "dark_gray") .. " - " .. format_requirements_list(unmet, false)
      else
        display_name = color_good("◆ " .. trait_name)
      end

      table.insert(traits, {
        id = trait_id,
        name = display_name,
        desc = trait_desc,
        can_select = can_select,
        index = index,
      })
      index = index + 1
    end
  end

  -- Reset Traits option
  local current_traits = get_current_traits(player)
  local can_reset = #current_traits > 0
  local reset_text
  if can_reset then
    reset_text = color_text("✖ Reset Traits", "red")
  else
    reset_text = color_text("✖ Reset Traits", "dark_gray") .. color_text(" (No traits to reset)", "dark_gray")
  end

  table.insert(traits, {
    name = reset_text,
    desc = "Remove all traits and damage your soul for 3 days, with severe penalties. You will keep your trait slots.",
    action = "reset",
    can_select = can_reset,
    index = index,
  })
  index = index + 1

  -- Back option
  table.insert(traits, {
    name = color_text("← Back", "light_gray"),
    desc = "Return to main menu",
    action = "back",
    can_select = true,
    index = index,
  })

  for i, trait in ipairs(traits) do
    ui:add_w_desc(trait.index, trait.name, trait.desc or "")
    if ui.entries and ui.entries[i] then ui.entries[i].enable = trait.can_select end
  end

  local choice_index = ui:query()

  if choice_index > 0 and choice_index <= #traits then
    local chosen = traits[choice_index]

    if not chosen.can_select then
      if chosen.id and player:has_trait(chosen.id) then
        gapi.add_msg(MsgType.warning, color_warning("You already have this trait."))
      else
        gapi.add_msg(MsgType.warning, color_warning("You don't meet the requirements for this trait."))
      end
      return
    end

    if chosen.action == "back" then
      return
    elseif chosen.action == "reset" then
      -- Remove all traits
      for _, trait_id in ipairs(ALL_TRAIT_IDS) do
        if player:has_trait(trait_id) then player:remove_mutation(trait_id, true) end
      end

      -- Reset trait counter but keep max traits (slots)
      set_char_value(player, "rpg_num_traits", 0)

      player:add_effect(EffectTypeId.new("damaged_soul"), TimeDuration.from_hours(72))
      gapi.add_msg(
        MsgType.bad,
        color_bad("✖ You have abandoned your traits!") .. " Your soul is " .. color_bad("damaged") .. " for 3 days."
      )
    elseif chosen.id then
      player:set_mutation(chosen.id)
      set_char_value(player, "rpg_num_traits", num_traits + 1)
      local trait_name = chosen.id:obj():name()
      gapi.add_msg(MsgType.good, color_good("✓ You have gained ") .. color_highlight(trait_name) .. color_good("!"))
    end
  end
end

mod.assign_stats_menu = function(player)
  local stat_points = get_char_value(player, "rpg_stat_points", 0)

  if stat_points <= 0 then
    gapi.add_msg(MsgType.warning, color_warning("You have no stat points to assign."))
    return
  end

  local ui = UiList.new()
  ui:title(string.format("=== Assign Stat Points (%d available) ===", stat_points))
  ui:desc_enabled(true)

  local str_val = player:get_str()
  local dex_val = player:get_dex()
  local int_val = player:get_int()
  local per_val = player:get_per()

  local options = {
    {
      name = string.format("Strength (Current: %d)", str_val),
      desc = "Strength affects your melee damage, the amount of weight you can carry, your total HP, your resistance to many diseases, and the effectiveness of actions which require brute force.",
      stat = "STR",
    },
    {
      name = string.format("Dexterity (Current: %d)", dex_val),
      desc = "Dexterity affects your chance to hit in melee combat, helps you steady your gun for ranged combat, and enhances many actions that require finesse.",
      stat = "DEX",
    },
    {
      name = string.format("Intelligence (Current: %d)", int_val),
      desc = "Intelligence is less important in most situations, but it is vital for more complex tasks like electronics crafting.  It also affects how much skill you can pick up from reading a book.",
      stat = "INT",
    },
    {
      name = string.format("Perception (Current: %d)", per_val),
      desc = "Perception is the most important stat for ranged combat.  It's also used for detecting traps and other things of interest.",
      stat = "PER",
    },
    {
      name = color_text("← Back", "light_gray"),
      desc = "Return to main menu",
      stat = "BACK",
    },
  }

  for i, opt in ipairs(options) do
    ui:add_w_desc(i, opt.name, opt.desc)
  end

  local choice_index = ui:query()

  if choice_index > 0 and choice_index <= #options then
    local chosen = options[choice_index]

    if chosen.stat == "BACK" then
      return
    elseif chosen.stat then
      local key = "rpg_assigned_" .. chosen.stat:lower()
      local current = get_char_value(player, key, 0)
      set_char_value(player, key, current + 1)

      set_char_value(player, "rpg_stat_points", stat_points - 1)

      gapi.add_msg(MsgType.good, color_good("✓ +1 ") .. color_highlight(chosen.stat) .. color_good(" assigned!"))
    end
  end
end

mod.show_help_menu = function(player)
  local ui = UiList.new()
  ui:title("=== [System] Information ===")
  ui:desc_enabled(true)

  local options = {
    {
      name = "About the System",
      desc = "[System] Query: Basic information",
      action = "about",
    },
    {
      name = "Adjust Level Scaling",
      desc = "Fine-tune power scaling for balance",
      action = "scaling",
    },
    {
      name = color_text("← Back", "light_gray"),
      desc = "Return to main menu",
      action = "back",
    },
  }

  for i, opt in ipairs(options) do
    ui:add_w_desc(i, opt.name, opt.desc)
  end

  local choice_index = ui:query()

  if choice_index > 0 and choice_index <= #options then
    local chosen = options[choice_index]

    if chosen.action == "back" then
      return
    elseif chosen.action == "about" then
      mod.show_about_screen(player)
    elseif chosen.action == "scaling" then
      mod.adjust_level_scaling(player)
    end
  end
end

mod.show_about_screen = function(player)
  local ui = UiList.new()
  ui:title("=== [System] Database Entry ===")

  local help_text = ""
  help_text = help_text .. color_info("[System]") .. " Protocol IB-73758-R\n"
  help_text = help_text
    .. color_text(
      "─────────────────────────────────────────",
      "light_gray"
    )
    .. "\n\n"

  help_text = help_text .. "Welcome to the " .. color_info("[System]") .. "! You, among\n"
  help_text = help_text .. "all inhabitants of world IB-73758-R, have\n"
  help_text = help_text .. "been blessed with a chance at greatness\n"
  help_text = help_text .. "for your great deeds doing " .. color_warning("[REASON NOT FOUND]") .. ".\n"
  help_text = help_text .. "Admire below the power made available:\n\n"

  help_text = help_text .. color_highlight("PROGRESSION") .. "\n"
  help_text = help_text .. "• Level 10: Prestige Class unlocks\n"
  help_text = help_text .. string.format("• +1 stat point per %d levels\n", LEVELS_PER_STAT_POINT)
  help_text = help_text .. string.format("• +1 trait slot per %d levels\n\n", LEVELS_PER_TRAIT_SLOT)

  help_text = help_text .. color_highlight("BALANCE") .. "\n"
  help_text = help_text .. "~25 point value (much less early, more late)\n"
  help_text = help_text .. color_warning("⚠") .. " Not meant to be used with Stats through Kills\n\n"

  help_text = help_text .. color_highlight("PRESTIGE PATHS") .. "\n"
  help_text = help_text .. "Warrior → Berserker / Guardian\n"
  help_text = help_text .. "Mage → Mystic / Scholar\n"
  help_text = help_text .. "Rogue → Acrobat / Assassin\n"
  help_text = help_text .. "Scout → Ranger / Craftsman\n"

  ui:text(help_text)
  ui:add(1, color_text("← Back", "light_gray"))

  ui:query()
end

mod.adjust_level_scaling = function(player)
  local current_scaling = get_char_value(player, "rpg_level_scaling", 100)

  local ui = UiList.new()
  ui:title("=== Adjust Level Scaling ===")
  ui:desc_enabled(true)

  local info_text = ""
  info_text = info_text .. "Current setting: " .. color_highlight(current_scaling .. "%") .. "\n"
  info_text = info_text
    .. color_text(
      "Affects class bonuses and periodic effects. Things that don't scale with level are unaffected.",
      "light_gray"
    )
    .. "\n"
  info_text = info_text .. color_text("Does not affect assigned stats.", "light_gray") .. "\n\n"

  ui:text(info_text)

  local options = {
    {
      name = "0% (Disable scaling)",
      desc = "Disables all level-based class bonuses and periodic effects.",
      value = 0,
    },
    {
      name = "50% (Half power)",
      desc = "Half effectiveness for balanced gameplay.",
      value = 50,
    },
    {
      name = "100% (Full power)",
      desc = "Default intended [System] experience.",
      value = 100,
    },
    {
      name = color_text("← Back", "light_gray"),
      desc = "Return to help menu",
      value = nil,
    },
  }

  for i, opt in ipairs(options) do
    local display_name = opt.name
    if opt.value == current_scaling then display_name = color_good("✓ " .. opt.name) end
    ui:add_w_desc(i, display_name, opt.desc)
  end

  local choice_index = ui:query()

  if choice_index > 0 and choice_index <= #options then
    local chosen = options[choice_index]

    if chosen.value ~= nil then
      set_char_value(player, "rpg_level_scaling", chosen.value)
      gapi.add_msg(MsgType.good, color_good("Level scaling set to ") .. color_highlight(chosen.value .. "%"))
    end
  end
end

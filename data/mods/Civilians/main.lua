gdebug.log_info("Civilians: Initializing mod...")
local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]
local faction_civ_id = MonsterFactionId.new("civilians"):int_id()
local faction_zombie_id = MonsterFactionId.new("zombie"):int_id()

local function merge_config(default_config, stored_config)
  local new_config = {}
  for key, default_value in pairs(default_config) do
    local stored_value = stored_config[key]
    if stored_value ~= nil then
      new_config[key] = stored_value
    else
      new_config[key] = default_value
    end
  end
  return new_config
end

---@param table_name string
---@param table_val table
local function print_table(table_name, table_val)
  gdebug.log_info("----------")
  gdebug.log_info(table_name)
  for key, val in pairs(table_val) do
    gdebug.log_info(string.format("%s: %s", key, val))
  end
  gdebug.log_info("----------")
end
-- ============================================================================
-- Mod parameter configuration area (SYSTEM CONFIG)
-- ============================================================================

local _stored_config = storage.config
if _stored_config == nil then
  storage.config = {}
  _stored_config = storage.config
end
print_table("Stored Config:", _stored_config)

local _default_config = {
  SPAWN_CHANCE = 15, -- Base spawn chance (15%)
  RARE_CHANCE = 10, -- Rare unit chance (10%): If spawned, 10% chance to be a police officer or fighter

  -- Vanish Configuration
  VANISH_PERIOD_DAYS = 14.0, -- Time unit: 14 days
  VANISH_BASE_RATE = 0.1, -- Chance of "creature still exists" after the above time period (0.0 ~ 1.0)

  TRY_TRIES = 5, -- Number of attempts to find a nearby empty tile

  -- NPC exclusive area list (avoid spawning wild civilians in these areas)
  NPC_TERRAINS = {
    "refctr", -- Refugee center related
    "evac_center", -- Evac center related
    "robofachq", -- Hub 01 HQ
    "isherwood", -- Isherwood Farm
    "_ocu", -- Occupied Stronghold
    "cabin_strange", -- Strange Cabin
    "cabin_lapin", -- Lapin Cabin
    "lab",
    "microlab",
    "necropolis",
    "mil_base",
    "bunker",
    "outpost",
    "prison",
    "aircraft_carrier",

    -- MOD location
    "fema_evac", -- FEMA Evac
    "GKB_SCRAPBASE", -- Scrapper Base faction base
    "FO_PLAYER_VAULT", -- Vault-Tec friendly vault
    "FO_WASTETOWN", -- Wastetown settlement
    "ZhighSchool", -- Zombie High School map
    "Survivor_Holdout", -- Survivor Holdout defense line
    "Survivor_Encampment", -- Survivor various encampments
    "surv_camp", -- Wilderness special survivor camp
    "forest_slaghter", -- Forest slaughterhouse
    "makeshift_command_center", -- Makeshift command center
    "Plain_Slaughter", -- Plain slaughterhouse
  },

  -- Furniture list where civilians can spawn
  TARGET_FURNITURE = {
    ["f_locker"] = true,
    ["f_wardrobe"] = true,
    ["f_chair"] = true,
    ["f_sofa"] = true,
    ["f_stool"] = true,
    ["f_bench"] = true,
    ["f_bed"] = true,
    ["f_chair_folding"] = true,
    ["f_armchair"] = true,
  },

  -- List of civilians allowed to pulp corpses (excludes panic, stationary, parent, and normal child)
  PULPING_ENABLED = true,
  PULPING_CIV_LIMIT = 25,
  PULPING_RADIUS = 4,
  PULPING_CHANCE = 50,
  CAN_PULP_CIVILIANS = {
    ["mon_civilian_zombiefighter"] = true,
    ["mon_civilian_police"] = true,
    ["mon_civilian_survivor_bow"] = true,
    ["mon_civilian_survivor_crossbow"] = true,
    ["mon_civilian_survivor_pistol"] = true,
    ["mon_civilian_survivor_fighter"] = true,
    ["mon_civilian_survivor_guardian_shotgun"] = true,
    ["mon_civilian_survivor_guardian_smg"] = true,
    ["mon_civilian_survivor_child"] = true,
    ["mon_civilian_survivor_elite_bow"] = true,
    ["mon_civilian_survivor_elite_crossbow"] = true,
    ["mon_civilian_survivor_elite_pistol"] = true,
    ["mon_civilian_survivor_elite_fighter"] = true,
    ["mon_civilian_survivor_guardian_elite_rifle"] = true,
    ["mon_civilian_survivor_guardian_elite_AR"] = true,
    ["mon_civilian_survivor_child_elite"] = true,
  },
}

local CONFIG = merge_config(_default_config, _stored_config)
print_table("Current Config:", CONFIG)

local FLAG_PULPED = JsonFlagId.new("PULPED")
local FLAG_FIELD_DRESS_FAILED = JsonFlagId.new("FIELD_DRESS_FAILED")

-- ============================================================================
-- Corpse Pulping Function Area
-- ============================================================================

local function pos_as_key(tripoint) return string.format("%d:%d:%d", tripoint.x, tripoint.y, tripoint.z) end

--- Process civilian corpse pulping behavior
local function process_civilian_corpse_pulping(monster, map, checked_positions)
  local m_pos = monster:get_pos_ms()
  ---@type Item?
  local found_corpse = nil
  ---@type TripointBubMs?
  local corpse_pos = nil

  -- 2. Scan surroundings for unpulped corpses (radius 8 tiles)
  local points = map:points_in_radius(m_pos, CONFIG.PULPING_RADIUS, 0)
  for _, pt in ipairs(points) do
    local pos_key = pos_as_key(pt)
    if checked_positions[pos_key] == nil then
      checked_positions[pos_key] = true -- Do not check same position twice
      if map:has_items_at(pt) then
        local map_stack = map:get_items_at(pt)
        for _, item in ipairs(map_stack:items()) do
          if item and not item:is_null() and item:is_corpse() then
            -- Determine if the corpse has not been pulped yet
            local is_pulped = item:has_flag(FLAG_PULPED) or item:has_flag(FLAG_FIELD_DRESS_FAILED)
            local is_max_damage = item:get_damage() >= item:get_max_damage()

            if not (is_pulped or is_max_damage) then
              found_corpse = item
              corpse_pos = pt
              break
            end
          end
        end
      end
    end
    if found_corpse then break end
  end

  if not found_corpse or found_corpse == nil or not corpse_pos or corpse_pos == nil then return end
  ---@cast corpse_pos TripointBubMs

  -- 3. Determine distance and execute action
  local dist = coords.rl_dist(m_pos, corpse_pos) or math.maxinteger
  if dist <= 1 then
    -- Close enough, execute pulping action
    found_corpse:set_damage(found_corpse:get_max_damage())
    found_corpse:set_flag(FLAG_PULPED)

    -- Issue system message (only when the player can see this civilian)
    if gapi.get_avatar():sees(monster:get_pos_ms()) then
      gapi.add_msg(
        MsgType.info,
        string.format("<color_light_red>%s pulped the corpse on the ground!</color>", monster:get_name())
      )
    end

    -- Deduct some moves to simulate attack action
    monster:mod_moves(-100)
  else
    -- Too far, let the civilian walk over there
    monster:wander_to(corpse_pos, 100)
  end
end

-- Execute corpse pulping check for all civilians every 10 turns
function mod.on_every_10_turns_civilian_update()
  if not CONFIG.PULPING_ENABLED then return end
  local map = gapi.get_map()
  local civilians = gapi.get_monsters_if({ ["faction_ids"] = { faction_civ_id }, ["limit"] = CONFIG.PULPING_CIV_LIMIT })
  local hostiles = gapi.get_monsters_if({
    ["faction_ids"] = { faction_zombie_id },
    ["within_range_of"] = { ["range"] = 10, ["monsters"] = civilians },
    ["sees"] = civilians,
    ["hostile_to"] = civilians,
    ["limit"] = 1,
  })

  -- Dont process if no civilians or hostiles in sight
  if not map or not civilians then return end
  if hostiles and #hostiles > 0 then return end

  local checked_positions = {}
  for _, mon in ipairs(civilians) do
    if mon and not mon:is_dead() then
      local mon_id = mon:get_type():str()
      -- Only civilians in the whitelist will execute corpse pulping
      if CONFIG.CAN_PULP_CIVILIANS[mon_id] then
        -- This means not all civilians will be pulping at the same time
        if gapi.rng(1, 100) <= CONFIG.PULPING_CHANCE then
          process_civilian_corpse_pulping(mon, map, checked_positions)
        end
      end
    end
  end
end

-- ============================================================================
-- Native Mapgen and Civilian Placement
-- ============================================================================

local function is_valid_spawn_spot(map, p)
  local ter_id = map:get_ter_at(p)
  if ter_id:obj():get_movecost() <= 0 then return false end
  return true
end

local function find_nearby_free_tile(map, center_p)
  local map_size = map:get_map_size()
  for i = 1, CONFIG.TRY_TRIES do
    local dx = gapi.rng(-2, 2)
    local dy = gapi.rng(-2, 2)
    local tx = center_p.x + dx
    local ty = center_p.y + dy
    if tx >= 0 and tx < map_size and ty >= 0 and ty < map_size then
      local p = PointOmtMs.new(tx, ty)
      if is_valid_spawn_spot(map, p) then return p end
    end
  end
  return nil
end

local function decide_spawn_group()
  local days = (gapi.current_turn() - gapi.turn_zero()):to_days()
  local survival_chance = CONFIG.VANISH_BASE_RATE ^ (days / CONFIG.VANISH_PERIOD_DAYS)

  if gapi.rng(1, 10000) > (survival_chance * 10000) then return nil end
  if gapi.rng(1, 100) <= CONFIG.RARE_CHANCE then
    return "GROUP_LUA_RARE_HUMANS"
  else
    return "GROUP_LUA_COMMON_HUMANS"
  end
end

mod.on_mapgen_postprocess = function(params)
  local map = params.map
  local omt_pos = params.omt

  -- Check if the currently generated map matches any NPC exclusive area prefix, if so skip directly
  if omt_pos then
    for _, prefix in ipairs(CONFIG.NPC_TERRAINS) do
      if overmapbuffer.check_ot(prefix, OtMatchType.CONTAINS, omt_pos) then return end
    end
  end

  local size = map:get_map_size()
  local current_chance = CONFIG.SPAWN_CHANCE
  for x = 0, size - 1 do
    for y = 0, size - 1 do
      local local_p = PointOmtMs.new(x, y)
      local furn = map:get_furn_at(local_p)

      if furn and furn:is_valid() then
        local furn_str = furn:str_id():str()
        if CONFIG.TARGET_FURNITURE[furn_str] then
          if gapi.rng(1, 100) <= current_chance then
            local group_id = decide_spawn_group()
            if group_id then
              local spawn_local_p = find_nearby_free_tile(map, local_p)
              if spawn_local_p then map:place_spawns(group_id, 1, spawn_local_p, spawn_local_p, 1.0, true) end
            end
          end
        end
      end
    end
  end
end

---@class mod_option
--- @field key string
--- @field name string
--- @field type type
--- @field min_val number
--- @field max_val number
--- @field desc string

---@type mod_option
local OPT_SPAWN_CHANCE = {
  key = "SPAWN_CHANCE",
  name = "Spawn Chance",
  type = "number",
  min_val = 1,
  max_val = 100,
  desc = "Base spawn chance",
}
---@type mod_option
local OPT_RARE_CHANCE = {
  key = "RARE_CHANCE",
  name = "Rare Chance",
  type = "number",
  min_val = 1,
  max_val = 100,
  desc = "Rare spawn chance",
}
---@type mod_option
local OPT_VANISH_PERIOD_DAYS = {
  key = "VANISH_PERIOD_DAYS",
  name = "Vanish Period Days",
  type = "number",
  min_val = 1,
  max_val = 100,
  desc = "Days until minimal spawn chance",
}
---@type mod_option
local OPT_VANISH_BASE_RATE = {
  key = "VANISH_BASE_RATE",
  name = "Vanish Base Rate",
  type = "number",
  min_val = 0.01,
  max_val = 1.0,
  desc = "Lower bound spawn chance",
}
---@type mod_option
local OPT_PULPING_ENABLED = {
  key = "PULPING_ENABLED",
  name = "Pulping Enabled",
  type = "boolean",
  desc = "Enable civilian corpse pulping",
  min_val = 0,
  max_val = 1,
}
local OPT_PULPING_CIV_LIMIT = {
  key = "PULPING_CIV_LIMIT",
  name = "Pulping Civ Limit",
  type = "number",
  min_val = 1,
  max_val = 200,
  desc = "Amount of cilians to evaluate during pulping.",
}
---@type mod_option
local OPT_PULPING_RADIUS = {
  key = "PULPING_RADIUS",
  name = "Pulping Radius",
  type = "number",
  min_val = 1,
  max_val = 100,
  desc = "The distance a civilian will look for a pulpable corpse.",
}
---@type mod_option
local OPT_PULPING_CHANCE = {
  key = "PULPING_CHANCE",
  name = "Pulping Chance",
  type = "number",
  min_val = 0,
  max_val = 100,
  desc = "Chance of pulping each turn",
}

local ALL_OPTIONS = {
  OPT_SPAWN_CHANCE,
  OPT_RARE_CHANCE,
  OPT_VANISH_PERIOD_DAYS,
  OPT_VANISH_BASE_RATE,
  OPT_PULPING_ENABLED,
  OPT_PULPING_CIV_LIMIT,
  OPT_PULPING_RADIUS,
  OPT_PULPING_CHANCE,
}

---@param opt mod_option
---@return integer | string | boolean | nil
local prompt_setting = function(opt)
  local value = nil
  if opt.type == "number" then
    local prompt = PopupInputStr.new()
    prompt:desc(
      string.format(
        "%s\n\r<color_white>Min:%s\n\rMax:%s\n\rDefault:%s\n\rCurrent: %s</color>\n",
        opt.desc,
        opt.min_val,
        opt.max_val,
        _default_config[opt.key],
        CONFIG[opt.key]
      )
    )
    prompt:title(opt.name)
    value = prompt:query_str()
    value = tonumber(value)
    if value == 0 or value == nil then return tonumber(_stored_config[opt.key]) end
    if value < opt.min_val then return nil end
    if value > opt.max_val then return nil end
  elseif opt.type == "boolean" then
    local prompt = QueryPopup.new()
    prompt:message(string.format("%s\n--------\n%s", opt.name, opt.desc))
    value = prompt:query_ynq()
    if value == "QUIT" then return nil end
    if value == "YES" then return true end
    if value == "NO" then return false end
  end

  return value
end

mod.configure_options = function()
  while true do
    local menu = UiList.new()
    local id_map = {}
    menu:title("Configure Civilians Mod")
    for i, opt in ipairs(ALL_OPTIONS) do
      menu:add_w_desc(i, string.format("%s: %s", opt.name, CONFIG[opt.key]), opt.desc)
      id_map[i] = opt
    end

    local choice = menu:query()
    if choice < 0 then return end
    local choice_param = id_map[choice]
    local value = prompt_setting(choice_param)
    if value ~= nil then
      CONFIG[choice_param.key] = value
      _stored_config[choice_param.key] = value
      storage.config[choice_param.key] = value
    end
  end
end

gdebug.log_info("Civilians: Ready")

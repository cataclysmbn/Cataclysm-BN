gdebug.log_info("Civilians: Initializing mod...")
local options = require("options")
local mod = game.mod_runtime[game.current_mod]

local faction_civ_id = MonsterFactionId.new("civilians"):int_id()
local faction_zombie_id = MonsterFactionId.new("zombie"):int_id()

local FLAG_PULPED = JsonFlagId.new("PULPED")
local FLAG_FIELD_DRESS_FAILED = JsonFlagId.new("FIELD_DRESS_FAILED")

-- ============================================================================
-- Hardcoded Options
-- ============================================================================
TRY_TRIES = 5 -- Number of attempts to find a nearby empty tile
-- NPC exclusive area list (avoid spawning wild civilians in these areas)
---@type table<string>
local NPC_TERRAINS = {
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
}

-- Furniture list where civilians can spawn
---@type table<string, boolean>
local TARGET_FURNITURE = {
  ["f_locker"] = true,
  ["f_wardrobe"] = true,
  ["f_chair"] = true,
  ["f_sofa"] = true,
  ["f_stool"] = true,
  ["f_bench"] = true,
  ["f_bed"] = true,
  ["f_chair_folding"] = true,
  ["f_armchair"] = true,
}

-- List of civilians allowed to pulp corpses (excludes panic, stationary, parent, and normal child)
---@type table<string, boolean>
local CAN_PULP_CIVILIANS = {
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
}

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
  local pulping_radius = options.pulping_radius.get_value()
  local points = map:points_in_radius(m_pos, pulping_radius, 0)
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
  if not options.pulping_enabled.get_value() then return end
  local map = gapi.get_map()
  if not map then return end
  local pulping_civ_limit = options.pulping_civ_limit.get_value()
  local civilians = gapi.get_monsters_if({ ["faction_ids"] = { faction_civ_id }, ["limit"] = pulping_civ_limit })
  -- Dont process if no civilians in sight
  if not civilians then return end

  local hostiles = gapi.get_monsters_if({
    ["faction_ids"] = { faction_zombie_id },
    ["within_range_of"] = { ["range"] = 10, ["monsters"] = civilians },
    ["sees"] = civilians,
    ["hostile_to"] = civilians,
    ["limit"] = 1,
  })

  -- Dont process if hostiles in sight
  if hostiles and #hostiles > 0 then return end

  local pulping_chance = options.pulping_chance.get_value()
  local checked_positions = {}
  for _, mon in ipairs(civilians) do
    if mon and not mon:is_dead() then
      local mon_id = mon:get_type():str()
      -- Only civilians in the whitelist will execute corpse pulping
      if CAN_PULP_CIVILIANS[mon_id] then
        -- This means not all civilians will be pulping at the same time
        if gapi.rng(1, 100) <= pulping_chance then process_civilian_corpse_pulping(mon, map, checked_positions) end
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
  for i = 1, TRY_TRIES do
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
  local vanish_base_rate = options.vanish_base_rate.get_value()
  local vanish_period_days = options.vanish_period_days.get_value()
  local survival_chance = vanish_base_rate ^ (days / vanish_period_days)
  local rare_chance = options.rare_chance.get_value()

  if gapi.rng(1, 10000) > (survival_chance * 10000) then return nil end
  if gapi.rng(1, 100) <= rare_chance then
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
    for _, prefix in ipairs(NPC_TERRAINS) do
      if overmapbuffer.check_ot(prefix, OtMatchType.CONTAINS, omt_pos) then return end
    end
  end

  local size = map:get_map_size()
  local current_chance = options.spawn_chance.get_value()
  for x = 0, size - 1 do
    for y = 0, size - 1 do
      local local_p = PointOmtMs.new(x, y)
      local furn = map:get_furn_at(local_p)

      if furn and furn:is_valid() then
        local furn_str = furn:str_id():str()
        if TARGET_FURNITURE[furn_str] then
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

---@param opt mod_option
---@return integer | string | boolean | nil
local prompt_setting = function(opt)
  local value = nil
  if opt.type == "number" then
    local prompt = PopupInputStr.new()
    local current_value = opt.get_value()
    prompt:desc(
      string.format(
        "%s\n\r<color_white>Min:%s\n\rMax:%s\n\rDefault:%s\n\rCurrent: %s</color>\n",
        opt.desc,
        opt.min_val,
        opt.max_val,
        opt.default,
        current_value
      )
    )
    prompt:title(opt.name)
    value = prompt:query_str()
    value = tonumber(value)
    if value == 0 or value == nil then return current_value end
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
    local opt_i = 1
    for _, opt in pairs(options) do
      menu:add_w_desc(opt_i, string.format("%s: %s", opt.name, opt.get_value()), opt.desc)
      id_map[opt_i] = opt
      opt_i = opt_i + 1
    end

    local choice = menu:query()
    if choice < 0 then return end
    local chosen_option = id_map[choice]
    local value = prompt_setting(chosen_option)
    if value ~= nil then chosen_option.set_value(value) end
  end
end

gdebug.log_info("Civilians: Ready")

gdebug.log_info("Civilians: Initializing mod...")
local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

function merge_config(default_config, stored_config)
  if not stored_config then return default_config end

  local new_config = {}
  for key, value in pairs(stored_config) do
    local default_value = default_config[key]
    if default_value ~= nil then
      if value == nil then
        new_config[key] = default_value
      else
        new_config[key] = value
      end
    end
  end
  return new_config
end
-- ============================================================================
-- Mod parameter configuration area (SYSTEM CONFIG)
-- ============================================================================

local _stored_config = storage.config
local _default_config = {
  SPAWN_CHANCE = 15, -- Base spawn chance (15%)
  RARE_CHANCE = 10, -- Rare unit chance (10%): If spawned, 10% chance to be a police officer or fighter

  -- Vanish Configuration
  VANISH_PERIOD_DAYS = 14.0, -- Time unit: 14 days
  VANISH_BASE_RATE = 1, -- Chance of "creature still exists" after the above time period (0.0 ~ 1.0)

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
  PULPING_RADIUS = 2,
  PULPING_CHANCE = 25,
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

local FLAG_PULPED = JsonFlagId.new("PULPED")
local FLAG_FIELD_DRESS_FAILED = JsonFlagId.new("FIELD_DRESS_FAILED")

-- ============================================================================
-- Corpse Pulping Function Area
-- ============================================================================

--- Process civilian corpse pulping behavior
local function process_civilian_corpse_pulping(monster, all_creatures, map)
  -- 1. Check if in combat (If enemies are in sight, prioritize enemies, ignore corpses)
  for _, cr in ipairs(all_creatures) do
    if cr and cr ~= monster and not cr:is_dead() then
      if monster:attitude_to(cr) == Attitude.Hostile and monster:sees(cr:get_pos_ms()) then
        return -- In combat, terminate corpse pulping logic
      end
    end
  end

  local m_pos = monster:get_pos_ms()
  ---@type Item?
  local found_corpse = nil
  ---@type TripointBubMs?
  local corpse_pos = nil

  -- 2. Scan surroundings for unpulped corpses (radius 8 tiles)
  local points = map:points_in_radius(m_pos, CONFIG.PULPING_RADIUS, 0)
  for _, pt in ipairs(points) do
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
    if found_corpse then break end
  end

  if not found_corpse or corpse_pos == nil then return end
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
  -- Ideally, we change the API to filter creatures/monsters in C++ first.
  local all_creatures = gapi.get_all_creatures()
  local monsters = gapi.get_all_monsters()
  if not map or not all_creatures or not monsters then return end

  for _, mon in ipairs(monsters) do
    if mon and not mon:is_dead() then
      local mon_id = mon:get_type():str()
      -- Only civilians in the whitelist will execute corpse pulping
      if CONFIG.CAN_PULP_CIVILIANS[mon_id] then
        -- This means not all civilians will be pulping at the same time
        if gapi.rng(1, 100) <= CONFIG.PULPING_CHANCE then process_civilian_corpse_pulping(mon, all_creatures, map) end
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
      local p = TripointBubMs.new(tx, ty, center_p.z)
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

  for x = 0, size - 1 do
    for y = 0, size - 1 do
      local local_p = TripointBubMs.new(x, y, omt_pos.z)
      local furn = map:get_furn_at(local_p)

      if furn and furn:is_valid() then
        local furn_str = furn:str_id():str()
        if CONFIG.TARGET_FURNITURE[furn_str] then
          if gapi.rng(1, 100) <= CONFIG.SPAWN_CHANCE then
            local group_id = decide_spawn_group()
            if group_id then
              local spawn_local_p = find_nearby_free_tile(map, local_p)
              if spawn_local_p then
                local p2d = spawn_local_p:xy()
                map:place_spawns(group_id, 1, p2d, p2d, 1.0, true)
              end
            end
          end
        end
      end
    end
  end
end
gdebug.log_info("Civilians: Ready")

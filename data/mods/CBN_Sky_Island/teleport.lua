-- Sky Islands BN Port - Teleportation System
-- Handles warp obelisk, return obelisk, and teleportation logic

local teleport = {}
local util = require("util")

-- Temporary storage for red room items during teleportation
-- Items are detached from map, held here, then placed at home
local red_room_item_storage = {}

-- Red room bounds relative to obelisk position
-- The interior of the room is 3 tiles in one axis and 2 in the other from the obelisk
-- Since the room can rotate, we use 3 for both axes to be safe
local RED_ROOM_RANGE = 3  -- tiles in each direction

-- These items don't disappear when you die. Table tracks how many
-- the player had when they died
local items_preserved_on_death = {
  skyisland_warp_shard = 0,
  skyisland_material_token = 0,
  skyisland_vortex_token = 0,
}

-- Helper: Store items from the red room in temporary storage (survives map changes)
-- Returns the number of items stored
local function store_red_room_items(obelisk_pos)
  local game_map = gapi.get_map()
  local items_stored = 0

  -- Clear any previous storage
  red_room_item_storage = {}

  util.debug_log(string.format("Scanning red room around obelisk at (%d, %d, %d)",
    obelisk_pos.x, obelisk_pos.y, obelisk_pos.z))

  -- Scan area inside the red room (not including walls)
  for dx = -RED_ROOM_RANGE, RED_ROOM_RANGE do
    for dy = -RED_ROOM_RANGE, RED_ROOM_RANGE do
      local check_pos = Tripoint.new(
        obelisk_pos.x + dx,
        obelisk_pos.y + dy,
        obelisk_pos.z
      )

      -- Check if there are items at this position
      if game_map:has_items_at(check_pos) then
        local items_stack = game_map:get_items_at(check_pos)

        -- Get frozen copy of items for safe iteration while modifying
        local items_to_store = items_stack:items()

        -- Detach each item from map and store in Lua table
        for _, it in ipairs(items_to_store) do
          local detached = game_map:detach_item_at(check_pos, it)
          if detached then
            table.insert(red_room_item_storage, detached)
            items_stored = items_stored + 1
          end
        end
      end
    end
  end

  if items_stored > 0 then
    util.debug_log(string.format("Total: %d items stored from red room", items_stored))
  else
    util.debug_log("No items found in red room")
  end

  return items_stored
end

-- Helper: Retrieve stored items and place them at home position
local function retrieve_stored_items_at_home(home_pos)
  local game_map = gapi.get_map()
  local stored_count = #red_room_item_storage
  local items_retrieved = 0

  util.debug_log(string.format("Retrieving %d stored items at home (%d, %d, %d)",
    stored_count, home_pos.x, home_pos.y, home_pos.z))

  -- Place each stored item on the map at home position
  for _, detached in ipairs(red_room_item_storage) do
    local failed = game_map:add_item(home_pos, detached)
    if not failed then
      items_retrieved = items_retrieved + 1
    end
  end

  -- Clear the storage (items now owned by map or GC'd if placement failed)
  red_room_item_storage = {}

  if items_retrieved > 0 then
    gapi.add_msg(string.format(locale.gettext("%d items from the red room were teleported home with you!"), items_retrieved))
    util.debug_log(string.format("Retrieved %d items at home", items_retrieved))
  end

  return items_retrieved
end

-- Raid configuration
-- Distances are in OMT (overmap terrain tiles). One overmap is 180x180 OMT.
-- Timing: Base pulse interval is 15 minutes. Multiplier extends this.
-- DDA timing: 8 pulses grace, then sickness stages 9-12, disintegration at 12+
-- Short (1x): 15 min pulses = 2 hour grace, 3 hour disintegration
-- Medium (2x): 30 min pulses = 4 hour grace, 6 hour disintegration
-- Long (3x): 45 min pulses = 6 hour grace, 9 hour disintegration
local RAID_CONFIG = {
  short = {
    name = locale.gettext("Quick Expedition"),
    description = locale.gettext("Normal time limit (2 hours grace). Exits will be close together."),
    pulse_multiplier = 1,
    material_tokens = 50,
    token_cost = 0,  -- Free - no softlock risk
    -- Landing spot distance
    min_distance = 200,
    max_distance = 1200,
    -- Mission/exit distances (from landing spot)
    mission_min = 5,
    mission_max = 30,
    exit_min = 5,
    exit_max = 35
  },
  medium = {
    name = locale.gettext("Large Expedition"),
    description = locale.gettext("Double time limit (4 hours grace). Exits scattered over wider area."),
    pulse_multiplier = 2,
    material_tokens = 125,
    min_distance = 200,
    max_distance = 1200,
    mission_min = 20,
    mission_max = 60,
    exit_min = 30,
    exit_max = 90
  },
  long = {
    name = locale.gettext("Extended Expedition"),
    description = locale.gettext("Triple time limit (6 hours grace). Exits very far away."),
    pulse_multiplier = 3,
    material_tokens = 200,
    min_distance = 200,
    max_distance = 1200,
    mission_min = 30,
    mission_max = 80,
    exit_min = 50,
    exit_max = 160
  }
}

-- Starting location configuration
local LOCATION_CONFIG = {
  field = {
    name = locale.gettext("Field"),
    description = locale.gettext("Arrive in a barren field. Usually wilderness, but could be an empty lot in town."),
    terrain_type = "field",
    match_type = OtMatchType.EXACT,
    z_level = 0,
    unlock_key = nil,  -- Always available
    catalyst_item = nil
  },
  basement = {
    name = locale.gettext("Basement"),
    description = locale.gettext("Arrive inside an underground basement. Usually suburbia."),
    terrain_type = "basement",
    match_type = OtMatchType.CONTAINS,
    z_level = -1,
    unlock_key = "basements_unlocked",
    catalyst_item = nil
  },
  roof = {
    name = locale.gettext("Rooftop"),
    description = locale.gettext("Arrive on a building roof. Usually houses, one story up."),
    terrain_type = "roof",
    match_type = OtMatchType.CONTAINS,
    z_level = 1,
    unlock_key = "roofs_unlocked",
    catalyst_item = nil
  },
  labs = {
    name = locale.gettext("Science Lab"),
    description = locale.gettext("Arrive in a subterranean science lab. DANGEROUS! Costs 1 Labs Catalyst. You will be sealed inside - bring an exit strategy!"),
    terrain_type = "lab",
    match_type = OtMatchType.CONTAINS,
    z_level = -2,
    unlock_key = "labs_unlocked",
    catalyst_item = "skyisland_labs_catalyst"
  }
}

-- Helper: Get player position in OMT coordinates
local function get_player_omt()
  local player = gapi.get_avatar()
  if not player then return nil end

  local pos_ms = player:get_pos_ms()
  local abs_ms = gapi.get_map():get_abs_ms(pos_ms)
  local omt, _ = coords.ms_to_omt(abs_ms)
  return omt
end

-- Filter: Require ROOF flag (for rooftop spawns)
local function roof_filter(game_map, pos)
  local ter_id = game_map:get_ter_at(pos)
  if not ter_id then return false end
  local ter_data = ter_id:obj()
  if not ter_data then return false end
  local has_roof = ter_data:has_flag("ROOF")
  -- util.debug_log(string.format("roof_filter: has_roof=%s", tostring(has_roof)))
  return has_roof
end

-- Helper: Check if a position is safe to spawn
-- additional_filter is an optional func(game_map, pos) for spawn-type-specific checks
local function is_safe_spawn(game_map, pos, additional_filter)
  local ter_id = game_map:get_ter_at(pos)
  if not ter_id then return false end

  local ter_data = ter_id:obj()
  if not ter_data then return false end

  local movecost = ter_data:get_movecost()
  if movecost <= 0 then return false end

  for _, unsafe_flag in pairs({ "DEEP_WATER", "NO_FLOOR" }) do
    if ter_data:has_flag(unsafe_flag) then return false end
  end

  -- local ter_id_str = tostring(ter_id)
  -- util.debug_log(string.format("is_safe_spawn check: ter='%s' movecost=%d", ter_id_str, movecost))

  -- Also check furniture
  local furn_id = game_map:get_furn_at(pos)
  if furn_id then
    local furn_data = furn_id:obj()
    if furn_data then
      local furn_movecost = furn_data:get_movecost()
      -- Furniture movecost of -1 means impassable
      if furn_movecost < 0 then return false end
    end
  end

  -- Apply additional filter if provided
  if additional_filter and not additional_filter(game_map, pos) then
    return false
  end

  return true
end

-- Helper: Find a safe position near the player after teleport
-- additional_filter is an optional func(game_map, pos) for spawn-type-specific checks
local function find_safe_position(player, additional_filter)
  local game_map = gapi.get_map()
  local current_pos = player:get_pos_ms()

  util.debug_log(string.format("find_safe_position: checking at pos (%d, %d, %d)",
    current_pos.x, current_pos.y, current_pos.z))

  -- If current position is safe, no need to move
  if is_safe_spawn(game_map, current_pos, additional_filter) then
    util.debug_log("Current position is safe, no move needed")
    return nil
  end

  util.debug_log("Player landed in unsafe terrain, searching for safe position...")

  -- Search in expanding squares around current position
  for radius = 1, 10 do
    for dx = -radius, radius do
      for dy = -radius, radius do
        -- Only check the edge of the square (optimization)
        if math.abs(dx) == radius or math.abs(dy) == radius then
          local check_pos = Tripoint.new(
            current_pos.x + dx,
            current_pos.y + dy,
            current_pos.z
          )
          if is_safe_spawn(game_map, check_pos, additional_filter) then
            util.debug_log(string.format("Found safe position at offset (%d, %d)", dx, dy))
            return check_pos
          end
        end
      end
    end
  end

  util.debug_log("WARNING: Could not find safe position within radius 10!")
  return nil
end

-- Helper: Teleport player to OMT coordinates with offset
-- spawn_filter is an optional func(game_map, pos) for spawn-type-specific safety checks
local function teleport_to_omt(omt, offset_tiles, spawn_filter)
  local player = gapi.get_avatar()

  if player then
    -- we need noclip so that we can avoid falling before we find a safe position
    player:set_mutation(MutationBranchId.new("DEBUG_NOCLIP"))
    util.debug_log(string.format("Teleporting to OMT: %s, %s, %s", omt.x, omt.y, omt.z))
    gapi.place_player_overmap_at(omt)
    -- DEBUG: Show actual position after teleport
    local actual_pos = player:get_pos_ms()
    util.debug_log(string.format("After teleport: pos.z=%d (requested omt.z=%d)", actual_pos.z, omt.z))

    -- If offset specified, move player after teleport
    if offset_tiles then
      local current_pos = player:get_pos_ms()
      local new_pos = Tripoint.new(
        current_pos.x + offset_tiles.x,
        current_pos.y + offset_tiles.y,
        current_pos.z + offset_tiles.z
      )
      player:set_pos_ms(new_pos)
      util.debug_log(string.format("Applied offset: %d, %d, %d", offset_tiles.x, offset_tiles.y, offset_tiles.z))
    end

    -- Find safe position if landed in wall or unsafe terrain
    local safe_pos = find_safe_position(player, spawn_filter)
    if safe_pos then
      player:set_pos_ms(safe_pos)
      util.debug_log("Moved player to safe position")
    end
    player:unset_mutation(MutationBranchId.new("DEBUG_NOCLIP"))
  end

  gapi.add_msg(locale.gettext("You feel reality shift around you..."))
end

-- Helper: Apply warpcloak landing protection effects
-- Base effect: 60 seconds of invisibility + feather fall
-- Conditional bonuses based on upgrades
local function apply_landing_protection(storage)
  local player = gapi.get_avatar()
  if not player then return end

  -- Base warpcloak duration: 60 seconds
  local cloak_duration = TimeDuration.from_seconds(60)

  -- Apply base warpcloak (invisibility + feather fall)
  local warpcloak_id = EffectTypeId.new("skyisland_warpcloak")
  player:add_effect(warpcloak_id, cloak_duration)
  util.debug_log("Applied warpcloak landing protection (60s)")

  -- Check for landing upgrades and apply bonus effects

  -- Scouting clairvoyance bonus (based on upgrade level)
  local clairvoyance_time = storage.scouting_clairvoyance_time or 0
  if clairvoyance_time > 0 then
    local clairvoyance_id = EffectTypeId.new("skyisland_clairvoyance")
    local clairvoyance_duration = TimeDuration.from_seconds(clairvoyance_time)
    player:add_effect(clairvoyance_id, clairvoyance_duration)
    util.debug_log(string.format("Applied scouting clairvoyance (%ds)", clairvoyance_time))
  end

  -- Landing flight bonus (real flight via mutation)
  if storage.landing_flight_unlocked then
    local flight_trait = MutationBranchId.new("SKYISLAND_WARP_FLIGHT")
    player:set_mutation(flight_trait)
    gapi.add_msg(locale.gettext("Warp energy lifts you into the air!"))
    util.debug_log("Applied landing flight mutation (60s)")

    -- Schedule removal of flight mutation after 60 seconds
    gapi.add_on_every_x_hook(cloak_duration, function()
      local p = gapi.get_avatar()
      if p then
        p:unset_mutation(flight_trait)
        gapi.add_msg(locale.gettext("Your feet touch the ground as the warp flight fades."))
        util.debug_log("Removed landing flight mutation")
      end
      return false  -- One-shot: stop after first execution
    end)
  end
end

-- Save friendly animals currently on the island before departing
-- Scans visible creatures and stores friendly monsters in storage.island_animals
local function save_island_animals(storage)
  local player = gapi.get_avatar()
  if not player then return end

  storage.island_animals = {}

  -- Use get_visible_creatures to find all creatures in range
  local creatures = player:get_visible_creatures(60)
  local saved_count = 0

  for _, creature in ipairs(creatures) do
    if creature:is_monster() then
      local monster = creature:as_monster()
      if monster and monster.friendly > 0 then
        local type_id = monster:get_type():str()
        local hp = monster:get_hp()
        local name = monster:get_name()

        table.insert(storage.island_animals, {
          type_id = type_id,
          hp = hp,
          name = name
        })
        saved_count = saved_count + 1
        util.debug_log(string.format("Saved island animal: %s (HP: %d)", type_id, hp))
      end
    end
  end

  if saved_count > 0 then
    util.debug_log(string.format("Total island animals saved: %d", saved_count))
  end
end

-- Restore friendly animals that were on the island before departure
-- Uses delayed spawn (same pattern as spawn_warped_animals)
local function restore_island_animals(storage)
  if not storage.island_animals or #storage.island_animals == 0 then
    return 0
  end

  -- Copy the list and clear storage immediately
  local animals_to_restore = {}
  for _, animal in ipairs(storage.island_animals) do
    table.insert(animals_to_restore, animal)
  end
  storage.island_animals = {}

  util.debug_log(string.format("Queuing %d island animals for delayed restore", #animals_to_restore))

  -- Delay spawn by 2 seconds to let map fully load after teleport
  gapi.add_on_every_x_hook(TimeDuration.from_seconds(2), function()
    local player = gapi.get_avatar()
    if not player then return false end

    local player_pos = player:get_pos_ms()
    local restored_count = 0

    for _, animal_data in ipairs(animals_to_restore) do
      local mtype_id = MonsterTypeId.new(animal_data.type_id)
      local spawned_monster = gapi.place_monster_around(mtype_id, player_pos, 5)

      if spawned_monster then
        spawned_monster:set_hp(animal_data.hp)
        spawned_monster:make_friendly()
        restored_count = restored_count + 1
        util.debug_log(string.format("Restored island animal: %s with HP %d", animal_data.type_id, animal_data.hp))
      else
        util.debug_log(string.format("FAILED to restore island animal: %s", animal_data.type_id))
      end
    end

    if restored_count > 0 then
      gapi.add_msg(string.format(locale.gettext("%d island animal%s returned home safely!"),
        restored_count, restored_count > 1 and "s" or ""))
    end

    return false  -- One-shot: stop after first execution
  end)

  return #animals_to_restore
end

-- Spawn warped animals at home location
-- Called when player successfully returns home
-- Note: Uses delayed spawn to ensure map is fully loaded after teleport
function teleport.spawn_warped_animals(storage)
  if not storage.warped_animals or #storage.warped_animals == 0 then
    return 0
  end

  -- Copy the list and clear storage immediately
  local animals_to_spawn = {}
  for _, animal in ipairs(storage.warped_animals) do
    table.insert(animals_to_spawn, animal)
  end
  storage.warped_animals = {}

  util.debug_log(string.format("Queuing %d animals for delayed spawn", #animals_to_spawn))

  -- Delay spawn by 1 second to let map fully load after teleport
  gapi.add_on_every_x_hook(TimeDuration.from_seconds(1), function()
    local player = gapi.get_avatar()
    if not player then return false end

    local player_pos = player:get_pos_ms()
    local spawned_count = 0

    util.debug_log(string.format("Delayed spawn executing at (%d,%d,%d)",
      player_pos.x, player_pos.y, player_pos.z))

    for _, animal_data in ipairs(animals_to_spawn) do
      -- Handle old format "MonsterTypeId[mon_cow]" -> "mon_cow"
      local type_str = animal_data.type_id
      local extracted = type_str:match("MonsterTypeId%[(.+)%]")
      if extracted then
        type_str = extracted
      end

      -- Spawn monster near player
      local mtype_id = MonsterTypeId.new(type_str)

      -- Try place_monster_around for flexible positioning
      local spawned_monster = gapi.place_monster_around(mtype_id, player_pos, 3)

      if spawned_monster then
        -- Set the HP to what it was when captured
        spawned_monster:set_hp(animal_data.hp)
        -- Make it friendly again
        spawned_monster:make_friendly()

        spawned_count = spawned_count + 1
        util.debug_log(string.format("Spawned: %s with HP %d", type_str, animal_data.hp))
      else
        util.debug_log(string.format("FAILED to spawn: %s", type_str))
      end
    end

    if spawned_count > 0 then
      gapi.add_msg(string.format(locale.gettext("%d warped animal%s arrived at your island!"),
        spawned_count, spawned_count > 1 and "s" or ""))
    end

    return false  -- One-shot: stop after first execution
  end)

  return #animals_to_spawn  -- Return count of queued animals
end

-- Use warp obelisk - start expedition
function teleport.use_warp_obelisk(who, item, pos, storage, missions, warp_sickness)
  if storage.is_away_from_home then
    gapi.add_msg(locale.gettext("You are already on an expedition!"))
    return 0
  end

  -- Store home location as absolute MS coordinates (for resurrection) - only once
  if not storage.home_location then
    local player_pos_ms = who:get_pos_ms()
    local home_abs_ms = gapi.get_map():get_abs_ms(player_pos_ms)
    storage.home_location = { x = home_abs_ms.x, y = home_abs_ms.y, z = home_abs_ms.z }
    util.debug_log(string.format("Home location set to: %d, %d, %d", home_abs_ms.x, home_abs_ms.y, home_abs_ms.z))
  end

  -- Also get OMT for teleportation
  local home_omt = get_player_omt()
  if not home_omt then
    gapi.add_msg(locale.gettext("ERROR: Could not determine position!"))
    return 0
  end

  -- Check what raid lengths are unlocked
  local longer_raids = storage.longer_raids_unlocked or 0

  -- Show raid type menu
  local ui = UiList.new()
  ui:title(locale.gettext("Select Expedition Type"))

  local menu_index = 1
  local raid_options = {}

  -- Short raids always available
  ui:add(menu_index, string.format(locale.gettext("%s (reward: %d tokens)"), RAID_CONFIG.short.name, RAID_CONFIG.short.material_tokens))
  raid_options[menu_index] = "short"
  menu_index = menu_index + 1

  -- Medium raids require upgrade
  if longer_raids >= 1 then
    ui:add(menu_index, string.format(locale.gettext("%s (reward: %d tokens)"), RAID_CONFIG.medium.name, RAID_CONFIG.medium.material_tokens))
    raid_options[menu_index] = "medium"
    menu_index = menu_index + 1
  end

  -- Long raids require further upgrade
  if longer_raids >= 2 then
    ui:add(menu_index, string.format(locale.gettext("%s (reward: %d tokens)"), RAID_CONFIG.long.name, RAID_CONFIG.long.material_tokens))
    raid_options[menu_index] = "long"
    menu_index = menu_index + 1
  end

  ui:add(menu_index, locale.gettext("Cancel"))

  local choice = ui:query()
  local selected_raid = raid_options[choice]

  if not selected_raid then
    gapi.add_msg(locale.gettext("Warp cancelled."))
    return 0
  end

  local config = RAID_CONFIG[selected_raid]

  -- Store raid type for token rewards on return
  storage.current_raid_type = selected_raid
  storage.current_raid_pulse_multiplier = config.pulse_multiplier
  -- Store distance values for mission spawning
  storage.current_mission_min = config.mission_min
  storage.current_mission_max = config.mission_max
  storage.current_exit_min = config.exit_min
  storage.current_exit_max = config.exit_max

  -- Show location type menu
  local loc_ui = UiList.new()
  loc_ui:title(locale.gettext("Select Starting Location"))

  local loc_index = 1
  local loc_options = {}
  local loc_order = { "field", "basement", "roof", "labs" }

  for _, loc_key in ipairs(loc_order) do
    local loc_config = LOCATION_CONFIG[loc_key]
    local is_unlocked = true
    local has_catalyst = true
    local suffix = ""

    -- Check if location type is unlocked
    if loc_config.unlock_key then
      is_unlocked = storage[loc_config.unlock_key] or false
    end

    -- Check if catalyst is available (for labs)
    if loc_config.catalyst_item and is_unlocked then
      has_catalyst = who:has_item_with_id(ItypeId.new(loc_config.catalyst_item), false)
      if not has_catalyst then
        suffix = locale.gettext(" [No Catalyst]")
      end
    end

    if is_unlocked then
      local display_name = loc_config.name
      if loc_config.catalyst_item then
        display_name = display_name .. locale.gettext(" (requires catalyst)")
      end
      loc_ui:add(loc_index, locale.gettext(display_name .. suffix))
      if has_catalyst then
        loc_options[loc_index] = loc_key
      else
        loc_options[loc_index] = nil  -- Can't select without catalyst
      end
      loc_index = loc_index + 1
    end
  end

  loc_ui:add(loc_index, locale.gettext("Cancel"))

  local loc_choice = loc_ui:query()
  local selected_location = loc_options[loc_choice]

  if not selected_location then
    gapi.add_msg(locale.gettext("Warp cancelled."))
    return 0
  end

  local loc_config = LOCATION_CONFIG[selected_location]

  -- Consume catalyst if required
  if loc_config.catalyst_item then
    local catalyst_id = ItypeId.new(loc_config.catalyst_item)
    local catalyst_item = who:get_item_with_id(catalyst_id, false)
    if catalyst_item then
      if catalyst_item:is_stackable() and catalyst_item.charges > 1 then
        catalyst_item:mod_charges(-1)
      else
        who:remove_item(catalyst_item)
      end
    end
    gapi.add_msg(locale.gettext("The Labs Catalyst crumbles to dust as dimensional barriers part..."))
  end

  gapi.add_msg(string.format(locale.gettext("Initiating %s to %s..."), config.name, loc_config.name))
  gapi.add_msg(locale.gettext("Searching for suitable location..."))

  -- Build search parameters based on location type
  local params = OmtFindParams.new()
  params:add_type(loc_config.terrain_type, loc_config.match_type)

  -- For field, also add forest as fallback options
  if selected_location == "field" then
    params:add_type("forest", OtMatchType.EXACT)
    params:add_type("forest_thick", OtMatchType.EXACT)
  end

  -- Set search range
  params:set_search_range(config.min_distance, config.max_distance)
  -- Search at the appropriate z-level
  params:set_search_layers(loc_config.z_level, loc_config.z_level)

  -- Use ground-level origin for searching (sky islands are at z > 0)
  local search_origin = Tripoint.new(home_omt.x, home_omt.y, loc_config.z_level)

  util.debug_log(string.format("Searching for %s terrain in range %d-%d at z=%d from (%d, %d, %d)",
    loc_config.terrain_type, config.min_distance, config.max_distance, loc_config.z_level,
    search_origin.x, search_origin.y, search_origin.z))

  -- Debug: Try find_all to see how many results we get
  -- local all_results = overmapbuffer.find_all(search_origin, params)
  -- util.debug_log(string.format("find_all returned %d results", #all_results))

  -- Find a random location matching parameters
  local dest_omt = overmapbuffer.find_random(search_origin, params)

  if dest_omt then
    util.debug_log(string.format("Found raid location at (%d, %d, %d)", dest_omt.x, dest_omt.y, dest_omt.z))
    util.debug_log(string.format("Found %s at z=%d (searched z=%d)", loc_config.terrain_type, dest_omt.z, loc_config.z_level))
  else
    -- Fallback: widen the search range significantly
    util.debug_log("Primary search failed, trying wider range...")
    local fallback_params = OmtFindParams.new()
    fallback_params:add_type(loc_config.terrain_type, loc_config.match_type)
    if selected_location == "field" then
      fallback_params:add_type("forest", OtMatchType.EXACT)
    end
    fallback_params:set_search_range(10, 2000)  -- Much wider range
    fallback_params:set_search_layers(loc_config.z_level, loc_config.z_level)

    local fallback_results = overmapbuffer.find_all(search_origin, fallback_params)
    util.debug_log(string.format("Fallback find_all returned %d results", #fallback_results))

    dest_omt = overmapbuffer.find_random(search_origin, fallback_params)

    if dest_omt then
      util.debug_log(string.format("Fallback found terrain at (%d, %d, %d)", dest_omt.x, dest_omt.y, dest_omt.z))
    else
      -- Absolute last resort - this shouldn't happen but just in case
      gapi.add_msg("WARNING: Could not find suitable terrain. Aborting warp.")
      util.debug_log("ERROR: All terrain searches failed!")
      -- Refund catalyst if we consumed one
      if loc_config.catalyst_item then
        who:add_item_with_id(ItypeId.new(loc_config.catalyst_item), 1)
        gapi.add_msg("Your Labs Catalyst is returned.")
      end
      return 0
    end
  end

  -- Two-stage teleport to prevent map revelation from z=10
  -- First teleport: Move to z=0 at home x,y (ground level should exist below sky island)
  -- Second teleport: Move to actual destination
  local intermediate_omt = Tripoint.new(home_omt.x, home_omt.y, 0)
  util.debug_log("Intermediate teleport to z=0 to prevent map revelation")
  gapi.place_player_overmap_at(intermediate_omt)

  -- Determine spawn filter based on location type
  local spawn_filter = nil
  if selected_location == "roof" then
    spawn_filter = roof_filter
  end

  -- Now teleport to actual destination
  teleport_to_omt(dest_omt, nil, spawn_filter)

  -- Apply warpcloak landing protection (invisibility + feather fall + bonuses)
  apply_landing_protection(storage)

  -- Reveal overmap if scouting is unlocked
  local scouting_level = storage.scouting_unlocked or 0
  if scouting_level > 0 then
    local reveal_radius = scouting_level == 1 and 1 or 2  -- 3x3 or 5x5
    for dx = -reveal_radius, reveal_radius do
      for dy = -reveal_radius, reveal_radius do
        local reveal_pos = Tripoint.new(dest_omt.x + dx, dest_omt.y + dy, dest_omt.z)
        overmapbuffer.set_seen(reveal_pos, true)
      end
    end
    local area_size = (reveal_radius * 2 + 1)
    gapi.add_msg(string.format(locale.gettext("Your scouting reveals a %dx%d area around the landing zone."), area_size, area_size))
  end

  -- Save island animals before leaving
  save_island_animals(storage)

  -- Set away status
  storage.is_away_from_home = true
  storage.warp_pulse_count = 0
  storage.raids_total = (storage.raids_total or 0) + 1

  -- Create missions
  missions.create_extraction_mission(dest_omt, storage)

  -- Bonus missions: tier determines how many missions per expedition
  local bonus_tier = storage.bonus_missions_tier or 0
  for i = 1, bonus_tier do
    missions.create_bonus_mission(dest_omt, storage)
  end

  -- Slaughter missions (kill X of species) require upgrade
  if storage.slaughter_unlocked then
    missions.create_slaughter_mission(dest_omt, storage)
  end

  -- Start warp sickness (reset counters for new expedition)
  -- The global hook is already running and will start accumulating time
  warp_sickness.start(storage)

  gapi.add_msg(string.format(locale.gettext("You arrive at the %s!"), loc_config.name:lower()))
  gapi.add_msg(locale.gettext("Find the red room exit portal to return home before warp sickness kills you."))

  return 1
end

-- Use return obelisk - return home
function teleport.use_return_obelisk(who, item, pos, storage, missions, warp_sickness)
  if not storage.is_away_from_home then
    gapi.add_msg(locale.gettext("You are already home!"))
    return 0
  end

  if not storage.home_location then
    gapi.add_msg(locale.gettext("ERROR: Home location not set!"))
    return 0
  end

  local confirm_ui = UiList.new()
  confirm_ui:title(locale.gettext("Return home?"))
  confirm_ui:add(1, locale.gettext("Yes, return home"))
  confirm_ui:add(2, locale.gettext("No, stay"))
  local confirm = confirm_ui:query()

  if confirm == 1 then
    -- Check return behavior setting
    -- 0 = whole_room (always), 1 = whole_room_cost (needs vortex token), 2 = self_only (never)
    local return_behavior = storage.difficulty_return_behavior or 1
    local items_stored = 0
    local used_vortex_token = false

    if return_behavior == 0 then
      -- Whole Room: Always teleport room contents
      items_stored = store_red_room_items(pos)
    elseif return_behavior == 1 then
      -- Whole Room for Cost: Only if player has vortex token
      local vortex_id = ItypeId.new("skyisland_vortex_token")
      if who:has_item_with_id(vortex_id, false) then
        local vortex_item = who:get_item_with_id(vortex_id, false)
        if vortex_item then
          if vortex_item:is_stackable() and vortex_item.charges > 1 then
            vortex_item:mod_charges(-1)
          else
            who:remove_item(vortex_item)
          end
        end
        items_stored = store_red_room_items(pos)
        used_vortex_token = true
        gapi.add_msg(locale.gettext("The Vortex Token crumbles as it pulls the room's contents into the warp!"))
      else
        gapi.add_msg(locale.gettext("Without a Vortex Token, only items you carry will return with you."))
      end
    else
      -- Self Only: Never teleport room contents
      gapi.add_msg(locale.gettext("Only items you carry will return with you."))
    end

    -- Convert stored abs_ms coordinates to OMT for teleportation
    local home_abs_ms = Tripoint.new(
      storage.home_location.x,
      storage.home_location.y,
      storage.home_location.z
    )
    local home_omt = coords.ms_to_omt(home_abs_ms)

    -- Offset 1 tile north (negative Y in map coordinates)
    teleport_to_omt(home_omt, Tripoint.new(0, -1, 0))

    -- Retrieve stored items and place them at home
    if items_stored > 0 then
      local player = gapi.get_avatar()
      local player_pos = player:get_pos_ms()
      retrieve_stored_items_at_home(player_pos)
    end

    -- Complete missions when returning home
    local player = gapi.get_avatar()
    if player then
      missions.complete_or_fail_missions(player, storage)
    end

    -- Award material tokens for successful return based on raid type
    local raid_type = storage.current_raid_type or "short"
    local config = RAID_CONFIG[raid_type]
    local material_tokens = config and config.material_tokens or 50

    player:add_item_with_id(ItypeId.new("skyisland_material_token"), material_tokens)
    gapi.add_msg(string.format(locale.gettext("You've returned home safely! Earned %d material tokens."), material_tokens))

    -- Clear away status and increment wins
    storage.is_away_from_home = false
    storage.warp_pulse_count = 0
    local old_raids_won = storage.raids_won or 0
    storage.raids_won = old_raids_won + 1

    -- Stop warp sickness (remove all effects)
    warp_sickness.stop(storage)

    -- Spawn any warped animals
    teleport.spawn_warped_animals(storage)

    -- Restore island animals that were saved before departure
    restore_island_animals(storage)

    -- Check for progress gate rank-ups (automatic at 10 and 20 wins)
    if old_raids_won < 10 and storage.raids_won >= 10 then
      gapi.add_msg(locale.gettext("=== RANK UP ==="))
      gapi.add_msg(locale.gettext("You have survived 10 expeditions and achieved Adept rank!"))
      gapi.add_msg(locale.gettext("New features and recipes may now be available."))
    elseif old_raids_won < 20 and storage.raids_won >= 20 then
      gapi.add_msg(locale.gettext("=== RANK UP ==="))
      gapi.add_msg(locale.gettext("You have survived 20 expeditions and achieved Master rank!"))
      gapi.add_msg(locale.gettext("New features and recipes may now be available."))
    end

    gapi.add_msg(string.format(locale.gettext(
      "Stats: %d/%d raids completed successfully"),
      storage.raids_won,
      storage.raids_total
    ))

    return 1
  else
    gapi.add_msg(locale.gettext("Cancelled."))
    return 0
  end
end

-- Return home successfully (used by return obelisk and skyward beacon)
-- Does NOT prompt for confirmation - caller should handle that
function teleport.return_home_success(storage, missions, warp_sickness)
  if not storage.home_location then
    gapi.add_msg(locale.gettext("ERROR: Home location not set!"))
    return
  end

  -- Convert stored abs_ms coordinates to OMT for teleportation
  local home_abs_ms = Tripoint.new(
    storage.home_location.x,
    storage.home_location.y,
    storage.home_location.z
  )
  local home_omt = coords.ms_to_omt(home_abs_ms)

  -- Offset 1 tile north (negative Y in map coordinates)
  teleport_to_omt(home_omt, Tripoint.new(0, -1, 0))

  -- Complete missions when returning home
  local player = gapi.get_avatar()
  if player then
    missions.complete_or_fail_missions(player, storage)
  end

  -- Award material tokens for successful return based on raid type
  local raid_type = storage.current_raid_type or "short"
  local config = RAID_CONFIG[raid_type]
  local material_tokens = config and config.material_tokens or 50

  player:add_item_with_id(ItypeId.new("skyisland_material_token"), material_tokens)
  gapi.add_msg(string.format(locale.gettext("You've returned home safely! Earned %d material tokens."), material_tokens))

  -- Clear away status and increment wins
  storage.is_away_from_home = false
  storage.warp_pulse_count = 0
  local old_raids_won = storage.raids_won or 0
  storage.raids_won = old_raids_won + 1

  -- Stop warp sickness (remove all effects)
  warp_sickness.stop(storage)

  -- Spawn any warped animals
  teleport.spawn_warped_animals(storage)

  -- Restore island animals that were saved before departure
  restore_island_animals(storage)

  -- Check for progress gate rank-ups (automatic at 10 and 20 wins)
  if old_raids_won < 10 and storage.raids_won >= 10 then
    gapi.add_msg(locale.gettext("=== RANK UP ==="))
    gapi.add_msg(locale.gettext("You have survived 10 expeditions and achieved Adept rank!"))
    gapi.add_msg(locale.gettext("New features and recipes may now be available."))
  elseif old_raids_won < 20 and storage.raids_won >= 20 then
    gapi.add_msg(locale.gettext("=== RANK UP ==="))
    gapi.add_msg(locale.gettext("You have survived 20 expeditions and achieved Master rank!"))
    gapi.add_msg(locale.gettext("New features and recipes may now be available."))
  end

  gapi.add_msg(string.format(locale.gettext(
    "Stats: %d/%d raids completed successfully"),
    storage.raids_won,
    storage.raids_total
  ))
end

-- Helper: Count stackable items by ID
local function count_item_charges(player, item_id)
  local total = 0
  local all_items = player:all_items(false)
  for _, item in ipairs(all_items) do
    if item:get_type():str() == item_id then
      if item:is_stackable() then
        total = total + item.charges
      else
        total = total + 1
      end
    end
  end
  return total
end

-- Resurrection handler - teleport player home after death
function teleport.resurrect_at_home(storage, missions, warp_sickness)
  if not storage.home_location then
    return  -- No home to resurrect at
  end

  util.debug_log("Sky Islands: Resurrecting at home")

  local player = gapi.get_avatar()
  if not player then return end

  -- Check for token bag and preserve tokens before dropping items
  for item in pairs(items_preserved_on_death) do
    items_preserved_on_death[item] = count_item_charges(player, item)
  end

  -- DROP ALL ITEMS before teleporting - this is the death penalty!
  -- Items are dropped at the death location (lost forever)
  player:drop_all_items()
  gapi.add_msg(locale.gettext("Your belongings scatter as reality tears you away..."))

  -- Clear any warped animals - they're lost on death
  if storage.warped_animals and #storage.warped_animals > 0 then
    local lost_count = #storage.warped_animals
    storage.warped_animals = {}
    gapi.add_msg(string.format(locale.gettext("%d warped animal%s lost in the void..."),
      lost_count, lost_count > 1 and "s were" or " was"))
  end

  -- Build home position from stored abs_ms coordinates
  local home_abs_ms = Tripoint.new(
    storage.home_location.x,
    storage.home_location.y,
    storage.home_location.z
  )

  -- Convert abs_ms to OMT for overmap placement
  local home_omt = coords.ms_to_omt(home_abs_ms)
  gapi.place_player_overmap_at(home_omt)

  -- Convert abs_ms to local_ms for exact positioning
  local local_pos = gapi.get_map():get_local_ms(home_abs_ms)
  gapi.place_player_local_at(local_pos)

  -- Fail all raid missions on death
  missions.fail_all_raid_missions(player)

  -- Mark raid as failed
  storage.is_away_from_home = false
  storage.warp_pulse_count = 0
  storage.raids_lost = (storage.raids_lost or 0) + 1

  -- Stop warp sickness (remove all effects before applying resurrection sickness)
  warp_sickness.stop(storage)

  -- Apply resurrection sickness
  warp_sickness.apply_resurrection_sickness()

  gapi.add_msg(locale.gettext("You respawn at home, naked and wounded!"))
  util.debug_log(string.format("Resurrected at home abs_ms: %d, %d, %d", home_abs_ms.x, home_abs_ms.y, home_abs_ms.z))
  util.debug_log(string.format("local pos: %d, %d, %d", local_pos.x, local_pos.y, local_pos.z))

  -- Restore preserved items
  local player_pos = player:get_pos_ms()
  for item, count in pairs(items_preserved_on_death) do
    if count > 0 then
      util.debug_log(local_pos)
      local item_id = ItypeId.new(item)
      local items = gapi.create_item(item_id, count)
      gapi.get_map():add_item(player_pos, items)
    end
    items_preserved_on_death[item] = 0
  end
end

return teleport

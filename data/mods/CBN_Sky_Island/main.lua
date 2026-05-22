-- Sky Islands BN Port - Main Module
-- Hooks, initialization, and module integration

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]

-- Load modules
local missions = require("missions")
local warp_sickness = require("warp_sickness")
local teleport = require("teleport")
local heart = require("heart")
local upgrades = require("upgrades")
local util = require("util")
local iuse = require("iuse")

-- Initialize storage defaults (only for new games)
-- These will be overwritten by saved data on load
storage.home_location = storage.home_location or nil
storage.is_away_from_home = storage.is_away_from_home or false
storage.warp_pulse_count = storage.warp_pulse_count or 0
storage.raids_total = storage.raids_total or 0
storage.raids_won = storage.raids_won or 0
storage.raids_lost = storage.raids_lost or 0

-- Difficulty settings defaults
-- pulse_interval: "casual" (30min), "normal" (15min), "hard" (10min), "impossible" (5min)
-- return_behavior: 0 (whole room), 1 (whole room for cost), 2 (self only)
-- emergency_return: 0 (free), 1 (costs shard to use), 2 (costs shards to craft), 3 (extraction only)
storage.difficulty_pulse_interval = storage.difficulty_pulse_interval or "normal"
storage.difficulty_return_behavior = storage.difficulty_return_behavior or 1
storage.difficulty_emergency_return = storage.difficulty_emergency_return or 2  -- Default: craft cost

-- Use warp obelisk - start expedition
mod.use_warp_obelisk = function(who, item, pos)
  return teleport.use_warp_obelisk(who, item, pos, storage, missions, warp_sickness)
end

-- Use return obelisk - return home
mod.use_return_obelisk = function(who, item, pos)
  return teleport.use_return_obelisk(who, item, pos, storage, missions, warp_sickness)
end

-- Use Heart of the Island - show interactive menu
mod.use_heart_menu = function(who, item, pos)
  return heart.use_heart(who, item, pos, storage)
end

-- Upgrade item activations
mod.use_upgrade_stability1 = function(who, item, pos)
  return upgrades.use_stability1(who, item, pos, storage)
end

mod.use_upgrade_stability2 = function(who, item, pos)
  return upgrades.use_stability2(who, item, pos, storage)
end

mod.use_upgrade_stability3 = function(who, item, pos)
  return upgrades.use_stability3(who, item, pos, storage)
end

mod.use_upgrade_scouting1 = function(who, item, pos)
  return upgrades.use_scouting1(who, item, pos, storage)
end

mod.use_upgrade_scouting2 = function(who, item, pos)
  return upgrades.use_scouting2(who, item, pos, storage)
end

mod.use_upgrade_exits1 = function(who, item, pos)
  return upgrades.use_exits1(who, item, pos, storage)
end

mod.use_upgrade_raidlength1 = function(who, item, pos)
  return upgrades.use_raidlength1(who, item, pos, storage)
end

mod.use_upgrade_raidlength2 = function(who, item, pos)
  return upgrades.use_raidlength2(who, item, pos, storage)
end

mod.use_upgrade_basements = function(who, item, pos)
  return upgrades.use_basements(who, item, pos, storage)
end

mod.use_upgrade_roofs = function(who, item, pos)
  return upgrades.use_roofs(who, item, pos, storage)
end

mod.use_upgrade_labs = function(who, item, pos)
  return upgrades.use_labs(who, item, pos, storage)
end

mod.use_upgrade_scouting_clairvoyance1 = function(who, item, pos)
  return upgrades.use_scouting_clairvoyance1(who, item, pos, storage)
end

mod.use_upgrade_scouting_clairvoyance2 = function(who, item, pos)
  return upgrades.use_scouting_clairvoyance2(who, item, pos, storage)
end

mod.use_upgrade_bonusmissions1 = function(who, item, pos)
  return upgrades.use_bonusmissions1(who, item, pos, storage)
end

mod.use_upgrade_bonusmissions2 = function(who, item, pos)
  return upgrades.use_bonusmissions2(who, item, pos, storage)
end

mod.use_upgrade_bonusmissions3 = function(who, item, pos)
  return upgrades.use_bonusmissions3(who, item, pos, storage)
end

mod.use_upgrade_bonusmissions4 = function(who, item, pos)
  return upgrades.use_bonusmissions4(who, item, pos, storage)
end

mod.use_upgrade_bonusmissions5 = function(who, item, pos)
  return upgrades.use_bonusmissions5(who, item, pos, storage)
end

mod.use_upgrade_hardmissions1 = function(who, item, pos)
  return upgrades.use_hardmissions1(who, item, pos, storage)
end

mod.use_upgrade_hardmissions2 = function(who, item, pos)
  return upgrades.use_hardmissions2(who, item, pos, storage)
end

mod.use_upgrade_slaughter = function(who, item, pos)
  return upgrades.use_slaughter(who, item, pos, storage)
end

mod.use_upgrade_landing_flight = function(who, item, pos)
  return upgrades.use_landing_flight(who, item, pos, storage)
end

mod.use_proof_determination = function(who, item, pos)
  return upgrades.use_proof_determination(who, item, pos, storage)
end

mod.use_proof_mastery = function(who, item, pos)
  return upgrades.use_proof_mastery(who, item, pos, storage)
end

-- Utility item activations
mod.use_quickheal = function(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end

  -- Only works on the island (not away from home)
  if storage.is_away_from_home then
    gapi.add_msg(locale.gettext("The quickheal pill only works on your sanctuary island."))
    return 0
  end

  -- Heal all body parts to max
  player:healall(999)
  gapi.add_msg(locale.gettext("A warm sensation washes over you as your wounds heal."))
  return 1  -- Consume the item
end

mod.use_earthbound_pill = function(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end

  -- Only works during expedition
  if not storage.is_away_from_home then
    gapi.add_msg(locale.gettext("You're not on an expedition. Save this for when you need more time earthside."))
    return 0
  end

  -- Reduce pulse count by 4 (extend time)
  storage.warp_pulse_count = math.max(0, (storage.warp_pulse_count or 0) - 4)
  gapi.add_msg(locale.gettext("The pill dissolves on your tongue. You feel the warp's grip on you loosen. (+4 pulses of time)"))
  util.debug_log(string.format("Earthbound pill used. Pulse count now: %d", storage.warp_pulse_count))
  return 1  -- Consume the item
end

-- Warp Home Focus - reusable spell-like return (for free/shard difficulty modes)
mod.use_warp_focus = function(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end

  -- Check emergency return setting
  -- 0 = free, 1 = costs shard to use, 2+ = focus doesn't work
  local emergency_setting = storage.difficulty_emergency_return or 2

  if emergency_setting >= 2 then
    -- Focus doesn't work in craft-cost or extraction-only modes
    gapi.add_msg(locale.gettext("The focus pulses weakly but cannot establish a connection."))
    gapi.add_msg(locale.gettext("Your difficulty settings require a Skyward Beacon or extraction point."))
    return 0
  end

  -- Only works during expedition
  if not storage.is_away_from_home then
    gapi.add_msg(locale.gettext("You're already home. The focus has no effect here."))
    return 0
  end

  -- Mode 1: Costs a warp shard to use, and cannot be used near enemies
  if emergency_setting == 1 then
    -- Check for nearby enemies (10 tile radius)
    local player_pos = player:get_pos_ms()
    local has_nearby_enemy = false

    for dx = -10, 10 do
      for dy = -10, 10 do
        if not has_nearby_enemy then
          local check_pos = Tripoint.new(player_pos.x + dx, player_pos.y + dy, player_pos.z)
          local monster = gapi.get_monster_at(check_pos)
          if monster and monster.friendly <= 0 then
            has_nearby_enemy = true
          end
        end
      end
    end

    if has_nearby_enemy then
      gapi.add_msg(locale.gettext("Hostile creatures nearby disrupt the focus's energy. You cannot use it here!"))
      return 0
    end

    -- Check for warp shard
    local shard_id = ItypeId.new("skyisland_warp_shard")
    if not player:has_item_with_id(shard_id, false) then
      gapi.add_msg(locale.gettext("The focus requires a warp shard to activate!"))
      return 0
    end

    -- Consume the shard (get item then remove it)
    local shard_item = player:get_item_with_id(shard_id, false)
    if shard_item then
      -- For stackable items, reduce charges
      if shard_item:is_stackable() and shard_item.charges > 1 then
        shard_item:mod_charges(-1)
      else
        player:remove_item(shard_item)
      end
    end
    gapi.add_msg("A warp shard crumbles to dust as the focus activates!")
  end

  -- Return home with all items (focus is NOT consumed)
  gapi.add_msg(locale.gettext("The focus flares with brilliant light. You feel yourself being pulled skyward..."))
  teleport.return_home_success(storage, missions, warp_sickness)
  return 0  -- Don't consume the focus
end

-- Skyward Beacon - consumable emergency return (works in all modes except extraction-only)
mod.use_skyward_beacon = function(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end

  -- Check emergency return setting
  -- 0 = free focus, 1 = shard focus, 2 = beacon only, 3 = extraction only
  local emergency_setting = storage.difficulty_emergency_return or 2

  if emergency_setting == 3 then
    -- Extraction only mode - beacon doesn't work
    gapi.add_msg(locale.gettext("The beacon flickers weakly but nothing happens. Emergency returns are disabled."))
    gapi.add_msg(locale.gettext("You must find an extraction point (return obelisk) to escape."))
    return 0
  end

  -- Only works during expedition
  if not storage.is_away_from_home then
    gapi.add_msg(locale.gettext("You're already home. The beacon has no effect here."))
    return 0
  end

  -- Beacon always works (it's the craftable 5-shard item)
  gapi.add_msg(locale.gettext("The beacon flares with brilliant light. You feel yourself being pulled skyward..."))
  teleport.return_home_success(storage, missions, warp_sickness)
  return 1  -- Consume the item
end

-- Warp status crystal - show detailed expedition status
mod.use_warp_crystal = function(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end

  -- Get warp status info
  local status_info = warp_sickness.get_status_info(storage)

  -- Create status display menu
  local menu = UiList.new()
  menu:title(locale.gettext("Warp Status Crystal"))
  menu:desc_enabled(true)

  if not storage.is_away_from_home then
    menu:text(locale.gettext("You are HOME SAFE on your sanctuary island.\n\nThe warp has no hold on you here."))
    menu:add(0, locale.gettext("Close"))
  else
    -- Build status text
    -- Get base interval from difficulty setting
    local difficulty = storage.difficulty_pulse_interval or "normal"
    local base_intervals = { casual = 30, normal = 15, hard = 10, impossible = 5 }
    local base_interval = base_intervals[difficulty] or 15
    local pulse_multiplier = storage.current_raid_pulse_multiplier or 1
    local interval_minutes = base_interval * pulse_multiplier
    local raid_type = storage.current_raid_type or "short"
    local raid_name = raid_type:sub(1,1):upper() .. raid_type:sub(2)
    local difficulty_name = difficulty:sub(1,1):upper() .. difficulty:sub(2)

    local status_text = string.format(
      locale.gettext("Current Expedition: %s\n") ..
      locale.gettext("Difficulty: %s\n") ..
      locale.gettext("Pulse Interval: %d minutes\n") ..
      locale.gettext("Current Pulse: %d\n") ..
      locale.gettext("Grace Period: %d pulses\n\n"),
      raid_name,
      difficulty_name,
      interval_minutes,
      status_info.current_pulse,
      status_info.grace_period
    )

    if status_info.in_grace_period then
      status_text = status_text .. string.format(
        locale.gettext("STATUS: STABLE\n") ..
        locale.gettext("Safe pulses remaining: %d\n") ..
        locale.gettext("Time until sickness begins: ~%d minutes"),
        status_info.pulses_remaining,
        status_info.pulses_remaining * interval_minutes
      )
    else
      status_text = status_text .. string.format(
        locale.gettext("STATUS: %s (Intensity %d/%d)\n") ..
        locale.gettext("Pulses until disintegration: %d"),
        status_info.status_name,
        status_info.current_intensity,
        6,
        status_info.pulses_to_disintegration
      )
    end

    menu:text(status_text)

    -- Add entries showing what happens at each stage
    menu:add_w_desc(1, locale.gettext("Stage 1: Warp Stability"), locale.gettext("Grace period. No effects. You feel fine."))
    menu:add_w_desc(2, locale.gettext("Stage 2: Warp Sickness"), locale.gettext("Intensity 1. -2 to all stats. Mild discomfort."))
    menu:add_w_desc(3, locale.gettext("Stage 3: Warp Nausea"), locale.gettext("Intensity 2. -4 to all stats. Growing pain."))
    menu:add_w_desc(4, locale.gettext("Stage 4: Warp Debilitation"), locale.gettext("Intensity 3. -6 to all stats. Severely impaired."))
    menu:add_w_desc(5, locale.gettext("Stage 5: Warp Necrosis"), locale.gettext("Intensity 4. -8 to all stats. Body failing."))
    menu:add_w_desc(6, locale.gettext("Stage 6: Warp Disintegration!"), locale.gettext("Intensity 5+. -10+ to stats. Constant damage. DEATH IMMINENT."))
    menu:add(0, locale.gettext("Close"))
  end

  menu:query()
  return 0  -- Don't consume
end

-- Animal teleporter - warp friendly creatures directly to island
mod.use_animal_teleporter = function(who, item, pos)
  local player = gapi.get_avatar()
  if not player then return 0 end

  -- Check if home location is set
  if not storage.home_location then
    gapi.add_msg(locale.gettext("You need to set your home location first by using the warp obelisk."))
    return 0
  end

  -- Prompt player to choose adjacent tile
  local target_pos = gapi.choose_adjacent(locale.gettext("Select a creature to warp home:"))
  if not target_pos then
    gapi.add_msg(locale.gettext("Cancelled."))
    return 0
  end

  -- Check for monster at target position
  local monster = gapi.get_monster_at(target_pos)
  if not monster then
    gapi.add_msg(locale.gettext("There's no creature there."))
    return 0
  end

  -- Capture check (matches vanilla pet carrier behavior):
  -- Friendly monsters are captured automatically
  -- Non-friendly monsters have a chance to resist based on HP%
  if monster.friendly <= 0 then
    -- hp_percentage returns 0-100, chance = hp% / 10
    -- one_in(chance) means 1/chance probability of success
    -- So a 100% HP monster has 1/10 chance, 10% HP has 1/1 (guaranteed)
    local hp_percent = monster:hp_percentage()
    local chance = math.max(1, math.floor(hp_percent / 10))
    local roll = gapi.rng(1, chance)

    if roll ~= 1 then
      local monster_name = monster:get_name()
      gapi.add_msg(string.format(locale.gettext("The %s avoids your attempt to warp it!"), monster_name))
      return 0
    end
  end

  -- Get monster info before removing it
  local monster_type = monster:get_type()
  local monster_hp = monster:get_hp()
  local monster_name = monster:get_name()

  -- Initialize storage for warped animals if needed
  if not storage.warped_animals then
    storage.warped_animals = {}
  end

  -- Store the animal data (use :str() to get raw ID string)
  table.insert(storage.warped_animals, {
    type_id = monster_type:str(),
    hp = monster_hp,
    name = monster_name
  })

  -- Move the monster far away to remove it from the reality bubble
  -- It should despawn once outside the active area
  -- Use a moderate offset (500 tiles) and clamp magnitude to under 50k
  local monster_pos = monster:get_pos_ms()
  local dest_x = math.min(50000, math.max(-50000, monster_pos.x + 500))
  local dest_y = math.min(50000, math.max(-50000, monster_pos.y + 500))
  local far_away = Tripoint.new(dest_x, dest_y, monster_pos.z)
  monster:spawn(far_away)

  gapi.add_msg(string.format(locale.gettext("The %s vanishes in a shimmer of light, warped to your island!"), monster_name))
  gapi.add_msg(string.format(locale.gettext("Animals queued for arrival: %d"), #storage.warped_animals))
  util.debug_log(string.format("Warped animal: %s (HP: %d) - total queued: %d",
    tostring(monster_type), monster_hp, #storage.warped_animals))

  return 1  -- Consume the teleporter
end

-- Copyplate imprint functions (delegated to iuse module)
mod.use_imprint_autodoc = function(who, item, pos)
  return iuse.imprint_autodoc(who, item, pos)
end

mod.use_imprint_autodoc_couch = function(who, item, pos)
  return iuse.imprint_autodoc_couch(who, item, pos)
end

mod.use_imprint_nanofab_body = function(who, item, pos)
  return iuse.imprint_nanofab_body(who, item, pos)
end

mod.use_imprint_nanofab_panel = function(who, item, pos)
  return iuse.imprint_nanofab_panel(who, item, pos)
end

mod.use_imprint_cvd_body = function(who, item, pos)
  return iuse.imprint_cvd_body(who, item, pos)
end

mod.use_imprint_cvd_panel = function(who, item, pos)
  return iuse.imprint_cvd_panel(who, item, pos)
end

-- Game started hook - initialize for new games only
mod.on_game_started = function()
  -- Reset to defaults for new game
  storage.home_location = nil
  storage.is_away_from_home = false
  storage.warp_pulse_count = 0
  storage.warp_pulse_accumulated = 0
  storage.raids_total = 0
  storage.raids_won = 0
  storage.raids_lost = 0

  -- Register the global warp sickness hook (runs every minute, checks conditions)
  warp_sickness.register_global_hook(storage)

  util.debug_log("Sky Islands: New game started")
  gapi.add_msg(locale.gettext("Sky Islands PoC loaded! Use warp remote to start."))
end

-- Game load hook - restore state (storage auto-loaded)
mod.on_game_load = function()
  util.debug_log("Sky Islands: Game loaded")
  util.debug_log(string.format("  Away from home: %s", tostring(storage.is_away_from_home)))
  util.debug_log(string.format("  Warp pulse count: %d", storage.warp_pulse_count or 0))

  -- Register the global warp sickness hook (runs every minute, checks conditions)
  warp_sickness.register_global_hook(storage)

  if storage.is_away_from_home then
    gapi.add_msg(locale.gettext("Resuming expedition..."))
  end
end

-- Game save hook
mod.on_game_save = function()
  util.debug_log("Sky Islands: Game saving")
  util.debug_log(string.format("  Saving state: Away=%s, Pulses=%d",
    tostring(storage.is_away_from_home), storage.warp_pulse_count or 0))
end

-- Character death hook (early) - clear effects and heal before broken limbs lock in
-- mod.on_char_death = function()
--   util.debug_log("Sky Islands: on_char_death fired")
--
--   if storage.home_location then
--     local player = gapi.get_avatar()
--     if not player then return end
--
--     -- Clear all effects (including broken limb effects) EARLY
--     player:clear_effects()
--     -- Heal everything to prevent broken limbs from locking in
--     player:set_all_parts_hp_cur(10)
--
--     util.debug_log("Sky Islands: Cleared effects and healed in on_char_death")
--   end
-- end

-- Character death hook - resurrection and teleportation
-- Two call sites trigger this hook:
--   1. avatar::is_dead_state() - calls WITHOUT params (player about to die)
--   2. character::die() - calls WITH params.char set (any character dying)
-- We handle case 1 (nil params.char) and case 2 only if params.char is the player.
mod.on_character_death = function(params)
  local dying_char = params and params.char
  local player = gapi.get_avatar()

  util.debug_log("Sky Islands: on_character_death hook called")
  util.debug_log(string.format("  dying_char: %s, player: %s", tostring(dying_char), tostring(player)))

  if not player then
    util.debug_log("  -> Skipping: player is nil")
    return
  end

  -- If dying_char is set (from character::die()), check if it's the player
  -- If dying_char is nil (from avatar::is_dead_state()), it's the player
  if dying_char ~= nil and dying_char ~= player then
    util.debug_log("  -> Skipping: dying character is not the player (NPC death)")
    return
  end

  util.debug_log("Sky Islands: on_character_death fired for player")
  util.debug_log(string.format("  home_location: %s", tostring(storage.home_location)))

  if not storage.home_location then
    util.debug_log("  -> No home_location set, cannot resurrect")
    return false  -- No home set, can't resurrect - let normal death happen
  end

  -- CRITICAL: Heal the player IMMEDIATELY to prevent is_dead_state() from returning true
  -- The game will call is_dead_state() again right after this hook returns, and we need
  -- the player to no longer be in a dead state to prevent the death confirmation prompt.
  player:set_all_parts_hp_to_max()
  player:clear_effects()
  util.debug_log("  -> Immediately healed player to prevent death state")

  -- Check for homeward mote (life insurance)
  local mote_id = ItypeId.new("skyisland_homeward_mote")
  local has_mote = player:has_item_with_id(mote_id, false)

  if has_mote then
    -- Homeward mote saves you! Keep all items, consume mote
    util.debug_log("Sky Islands: Homeward mote activated!")
    local mote_item = player:get_item_with_id(mote_id, false)
    if mote_item then
      player:remove_item(mote_item)
    end
    gapi.add_msg(locale.gettext("The homeward mote flares brilliantly, yanking you from death's grasp!"))
    gapi.add_msg(locale.gettext("You arrive home alive, with all your belongings intact."))

    -- Teleport home WITH items (use the success path)
    teleport.return_home_success(storage, missions, warp_sickness)

    -- Mote save doesn't count as win or loss - undo the win increment from return_home_success
    storage.raids_won = (storage.raids_won or 1) - 1
  else
    -- Normal death - lose items, get resurrected at low HP
    teleport.resurrect_at_home(storage, missions, warp_sickness)

    -- resurrect_at_home applies resurrection sickness which sets HP to 10
  end

  util.debug_log("Sky Islands: Death handled, returning true to prevent normal death")
  return true  -- Tell the game we handled the death
end

util.debug_log("Sky Islands PoC main.lua loaded")

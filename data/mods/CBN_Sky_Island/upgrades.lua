-- Sky Islands BN Port - Upgrades System
-- Handles upgrade item activation and storage tracking

local upgrades = {}
local util = require("util")

-- Upgrade definitions
-- Maps upgrade key to: storage_field, required_level (current level must equal this), new_level, success_message
-- DDA has 4 stability upgrades, each adding +1 pulse to grace period (8 -> 9 -> 10 -> 11 -> 12)
-- We implement 3 levels for now (matching our recipes)
local UPGRADE_DEFS = {
  stability1 = {
    field = "stability_unlocked",
    required = 0,
    new_level = 1,
    message = locale.gettext("Your resistance to the chaos of the warpstream increases. In every future expedition, you will be able to endure an additional warp pulse and stay earthside longer. (9 pulses)")
  },
  stability2 = {
    field = "stability_unlocked",
    required = 1,
    new_level = 2,
    message = locale.gettext("Your resistance to the chaos of the warpstream increases. In every future expedition, you will be able to endure an additional warp pulse and stay earthside longer. (10 pulses)")
  },
  stability3 = {
    field = "stability_unlocked",
    required = 2,
    new_level = 3,
    message = locale.gettext("Your resistance to the chaos of the warpstream increases. In every future expedition, you will be able to endure an additional warp pulse and stay earthside longer. (11 pulses)")
  },
  scouting1 = {
    field = "scouting_unlocked",
    required = 0,
    new_level = 1,
    message = locale.gettext("Your vision expands. A 3x3 area around your landing zone will be revealed on future expeditions.")
  },
  scouting2 = {
    field = "scouting_unlocked",
    required = 1,
    new_level = 2,
    message = locale.gettext("Your foresight sharpens. A 5x5 area around your landing zone will be revealed on future expeditions.")
  },
  exits1 = {
    field = "multiple_exits_unlocked",
    required = 0,
    new_level = 1,
    message = locale.gettext("The paths multiply. Two return obelisks will now spawn per expedition, giving you more options to escape.")
  },
  raidlength1 = {
    field = "longer_raids_unlocked",
    required = 0,
    new_level = 1,
    message = locale.gettext("Large Expeditions are now available at the warp obelisk. You will have double the grace period before warp sickness sets in.")
  },
  raidlength2 = {
    field = "longer_raids_unlocked",
    required = 1,
    new_level = 2,
    message = locale.gettext("Extended Expeditions are now available. You will have triple the grace period for maximum exploration time.")
  },
  basements = {
    field = "basements_unlocked",
    required = nil,  -- No prerequisite, just a boolean unlock
    new_level = true,
    message = locale.gettext("Basement starts are now available at the warp obelisk. You can choose to begin expeditions in underground basements.")
  },
  roofs = {
    field = "roofs_unlocked",
    required = nil,
    new_level = true,
    message = locale.gettext("Rooftop starts are now available at the warp obelisk. You can choose to begin expeditions on building rooftops.")
  },
  labs = {
    field = "labs_unlocked",
    required = nil,
    new_level = true,
    message = locale.gettext("Lab starts are now available at the warp obelisk. You can choose to begin expeditions in underground science labs. Be warned: you will need a Labs Catalyst each time, and you will start sealed inside!")
  },
  -- Scouting clairvoyance (see everything near you on landing)
  scouting_clairvoyance1 = {
    field = "scouting_clairvoyance_time",
    required = 0,
    new_level = 20,
    message = locale.gettext("Scouting Clairvoyance I unlocked. You will see everything near you for 20 seconds when beginning an expedition.")
  },
  scouting_clairvoyance2 = {
    field = "scouting_clairvoyance_time",
    required = 20,
    new_level = 60,
    message = locale.gettext("Scouting Clairvoyance II unlocked. You will see everything near you for 60 seconds when beginning an expedition.")
  },
  -- Bonus mission unlocks (number of missions per expedition)
  bonusmissions1 = {
    field = "bonus_missions_tier",
    required = 0,
    new_level = 1,
    message = locale.gettext("Expedition Missions unlocked. Every expedition now offers 1 randomly assigned bonus mission, which rewards warp shards on completion.")
  },
  bonusmissions2 = {
    field = "bonus_missions_tier",
    required = 1,
    new_level = 2,
    message = locale.gettext("Two Missions unlocked. Every expedition now offers 2 randomly assigned bonus missions.")
  },
  bonusmissions3 = {
    field = "bonus_missions_tier",
    required = 2,
    new_level = 3,
    message = locale.gettext("Three Missions unlocked. Every expedition now offers 3 randomly assigned bonus missions.")
  },
  bonusmissions4 = {
    field = "bonus_missions_tier",
    required = 3,
    new_level = 4,
    message = locale.gettext("Four Missions unlocked. Every expedition now offers 4 randomly assigned bonus missions.")
  },
  bonusmissions5 = {
    field = "bonus_missions_tier",
    required = 4,
    new_level = 5,
    message = locale.gettext("Five Missions unlocked. Every expedition now offers 5 randomly assigned bonus missions.")
  },
  hardmissions1 = {
    field = "hard_missions_tier",
    required = 0,
    new_level = 1,
    message = locale.gettext("Harder Missions unlocked. More challenging bonus missions with greater rewards are now available.")
  },
  hardmissions2 = {
    field = "hard_missions_tier",
    required = 1,
    new_level = 2,
    message = locale.gettext("Hardest Missions unlocked. The most dangerous bonus missions with the greatest rewards are now available.")
  },
  -- Slaughter missions (kill X of species)
  slaughter = {
    field = "slaughter_unlocked",
    required = nil,
    new_level = true,
    message = locale.gettext("Slaughter Missions unlocked. You will now receive a slaughter mission during each expedition, rewarding warp shards for killing specific types of enemies.")
  },
  -- Landing bonuses (active during warpcloak effect on arrival)
  landing_flight = {
    field = "landing_flight_unlocked",
    required = nil,
    new_level = true,
    message = locale.gettext("Landing Flight unlocked. While under the warpcloak effect on landing, you gain the power of flight for 60 seconds.")
  },
  proof_determination = {
    field = "proof_determination_complete",
    required = nil,
    new_level = true,
    message = locale.gettext("Your determination is proven! You have achieved the rank of Adept. Rank challenge complete!")
  },
  proof_mastery = {
    field = "proof_mastery_complete",
    required = nil,
    new_level = true,
    message = locale.gettext("Your mastery is proven! You have achieved the highest rank. Rank challenge complete!")
  }
}

-- Check if near the Heart of the Island
local function is_near_heart(player)
  -- The crafting already requires being near the heart (fakeitem_heart tool),
  -- so if they crafted the item, they were near the heart.
  -- For activation, we trust that they crafted it properly.
  -- In the future, we could add a proximity check here.
  return true
end

-- Generic upgrade activation function
local function activate_upgrade(who, item, pos, storage, upgrade_key)
  local def = UPGRADE_DEFS[upgrade_key]
  if not def then
    gapi.add_msg(locale.gettext("ERROR: Unknown upgrade key: ") .. upgrade_key)
    return 0
  end

  local player = gapi.get_avatar()
  if not player then
    return 0
  end

  -- Handle boolean upgrades (no prerequisite, just on/off)
  if def.required == nil then
    local current_value = storage[def.field]
    if current_value then
      gapi.add_msg(locale.gettext("You have already unlocked this upgrade."))
      return 0
    end
    -- Apply the boolean upgrade
    storage[def.field] = true
    gapi.add_msg(locale.gettext("=== UPGRADE UNLOCKED ==="))
    gapi.add_msg(def.message)
    util.debug_log(string.format("Upgrade %s activated: %s = true", upgrade_key, def.field))
    return 1
  end

  -- Handle leveled upgrades (original logic)
  local current_level = storage[def.field] or 0
  if current_level ~= def.required then
    if current_level >= def.new_level then
      gapi.add_msg(locale.gettext("You have already unlocked this upgrade."))
    else
      gapi.add_msg(locale.gettext("You must unlock the previous tier of this upgrade first."))
    end
    return 0
  end

  -- Apply the upgrade
  storage[def.field] = def.new_level
  gapi.add_msg(locale.gettext("=== UPGRADE UNLOCKED ==="))
  gapi.add_msg(def.message)
  util.debug_log(string.format("Upgrade %s activated: %s = %d", upgrade_key, def.field, def.new_level))
  return 1
end

-- Individual upgrade handlers (called by main.lua iuse registrations)
function upgrades.use_stability1(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "stability1")
end

function upgrades.use_stability2(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "stability2")
end

function upgrades.use_stability3(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "stability3")
end

function upgrades.use_scouting1(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "scouting1")
end

function upgrades.use_scouting2(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "scouting2")
end

function upgrades.use_exits1(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "exits1")
end

function upgrades.use_raidlength1(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "raidlength1")
end

function upgrades.use_raidlength2(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "raidlength2")
end

function upgrades.use_basements(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "basements")
end

function upgrades.use_roofs(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "roofs")
end

function upgrades.use_labs(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "labs")
end

function upgrades.use_scouting_clairvoyance1(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "scouting_clairvoyance1")
end

function upgrades.use_scouting_clairvoyance2(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "scouting_clairvoyance2")
end

function upgrades.use_bonusmissions1(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "bonusmissions1")
end

function upgrades.use_bonusmissions2(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "bonusmissions2")
end

function upgrades.use_bonusmissions3(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "bonusmissions3")
end

function upgrades.use_bonusmissions4(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "bonusmissions4")
end

function upgrades.use_bonusmissions5(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "bonusmissions5")
end

function upgrades.use_hardmissions1(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "hardmissions1")
end

function upgrades.use_hardmissions2(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "hardmissions2")
end

function upgrades.use_slaughter(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "slaughter")
end

function upgrades.use_landing_flight(who, item, pos, storage)
  return activate_upgrade(who, item, pos, storage, "landing_flight")
end

function upgrades.use_proof_determination(who, item, pos, storage)
  local raids_won = storage.raids_won or 0
  if raids_won < 10 then
    gapi.add_msg(locale.gettext("You need at least 10 successful expeditions to use this."))
    return 0
  end
  return activate_upgrade(who, item, pos, storage, "proof_determination")
end

function upgrades.use_proof_mastery(who, item, pos, storage)
  local raids_won = storage.raids_won or 0
  if raids_won < 20 then
    gapi.add_msg(locale.gettext("You need at least 20 successful expeditions to use this."))
    return 0
  end
  return activate_upgrade(who, item, pos, storage, "proof_mastery")
end

return upgrades

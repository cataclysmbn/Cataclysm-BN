---@class ItrapParams
---@field target Character
---@field trap any
---@field pos TripointBubMs

---@class SprinklerLiquid
---@field liquid ItypeId
---@field field string

local traps = {}

---@param str string
---@return FieldTypeIntId
local function field_int_id(str)
  -- map:add_field_at takes the int_id form of field_type, built from the string
  -- id. Must be called at trigger time, not at preload: field types are not
  -- loaded yet when this file is required.
  return FieldTypeIntId.new(FieldTypeId.new(str))
end

-- Sprinkler: sprays whatever liquid the connected fluid grid currently holds.
-- One liquid family per grid, so probing a few known liquids finds the stored one.
---@type SprinklerLiquid[]
local sprinkler_liquids = {
  { liquid = ItypeId.new("acid"), field = "fd_acid" },
  { liquid = ItypeId.new("water"), field = "fd_water" },
  { liquid = ItypeId.new("water_clean"), field = "fd_water" },
}

local sprinkler_spray_charges = 50
local sprinkler_spray_radius = 1

---@param omt TripointAbsOmt
---@return SprinklerLiquid?
---@return integer
local function find_sprinkler_liquid(omt)
  for _, entry in ipairs(sprinkler_liquids) do
    local charges = overmapbuffer.fluid_grid_liquid_charges_at(omt, entry.liquid)
    if charges > 0 then
      return entry, charges
    end
  end
  return nil, 0
end

---@param params ItrapParams
---@return boolean
function traps.sprinkler_can_trigger(params)
  local omt = gapi.bub_to_abs(params.pos):to_omt()
  local entry = find_sprinkler_liquid(omt)
  return entry ~= nil
end

---@param params ItrapParams
function traps.sprinkler_on_trigger(params)
  local pos = params.pos
  local omt = gapi.bub_to_abs(pos):to_omt()
  local entry = find_sprinkler_liquid(omt)
  if entry == nil then return end
  local drained = overmapbuffer.drain_fluid_grid_liquid_charges(omt, entry.liquid, sprinkler_spray_charges)
  if drained <= 0 then return end
  local m = gapi.get_map()
  local field = field_int_id(entry.field)
  for _, p in ipairs(m:points_in_radius(pos, sprinkler_spray_radius)) do
    m:add_field_at(p, field, 2, TimeDuration.from_turns(0))
  end
  gapi.add_msg(MsgType.bad, locale.gettext("The sprinkler sprays liquid everywhere!"))
end

game.itrap_functions["tr_sprinkler"] = {
  can_trigger = traps.sprinkler_can_trigger,
  on_trigger = traps.sprinkler_on_trigger,
}

-- Incendiary mine: no explosion, just paints incendiary fire (the same field
-- an incendiary grenade leaves behind, fd_incendiary intensity 3) on its tile
-- and the surrounding tiles.
local incendiary_mine_field = "fd_incendiary"
local incendiary_mine_intensity = 3
local incendiary_mine_radius = 1

---@param params ItrapParams
function traps.incendiary_mine_on_trigger(params)
  local pos = params.pos
  local m = gapi.get_map()
  local field = field_int_id(incendiary_mine_field)
  for _, p in ipairs(m:points_in_radius(pos, incendiary_mine_radius)) do
    m:add_field_at(p, field, incendiary_mine_intensity, TimeDuration.from_turns(0))
  end
  gapi.add_msg(MsgType.bad, locale.gettext("The incendiary mine bursts into flames!"))
end

game.itrap_functions["tr_mine_incendiary"] = {
  on_trigger = traps.incendiary_mine_on_trigger,
}
game.itrap_functions["tr_mine_incendiary_buried"] = {
  on_trigger = traps.incendiary_mine_on_trigger,
}

-- Manhack hatch: single tile, fires once (remove_on_trigger) and releases a
-- manhack that is permanently friendly to the player.
local manhack_hatch_monster = MonsterTypeId.new("mon_manhack")

---@param params ItrapParams
function traps.manhack_hatch_on_trigger(params)
  local pos = params.pos
  -- place_monster_around searches closest-first from r=1 up to the radius, so
  -- radius 3 still prefers the tiles right next to the hatch but falls back to
  -- farther tiles when the immediate ones are blocked or occupied.
  local bot = gapi.place_monster_around(manhack_hatch_monster, pos, 3)
  if bot ~= nil then
    -- -1 = permanently friendly to the player (base defense bot)
    bot.friendly = -1
    gapi.add_msg(MsgType.bad, locale.gettext("The manhack hatch opens and a manhack flies out!"))
  else
    gapi.add_msg(MsgType.warning, locale.gettext("The manhack hatch opens, but there is no room for the manhack to emerge!"))
  end
end

game.itrap_functions["tr_manhack_hatch"] = {
  on_trigger = traps.manhack_hatch_on_trigger,
}

-- Tesla wire: drains the base distribution grid (furniture batteries, charge
-- in kJ) and shocks the triggerer. Fires as long as the grid holds enough
-- charge. No can_trigger gate: a can_trigger of false is completely silent, so
-- an unpowered wire would look broken — instead we fire and say when the grid
-- is dead.
local tesla_power_cost = 100
local tesla_damage = 30

---@param params ItrapParams
function traps.tesla_wire_on_trigger(params)
  local who = params.target
  local abs = gapi.bub_to_abs(params.pos)
  local grid = gapi.get_distribution_grid_tracker():grid_at(abs)
  if not grid:is_valid() or grid:get_resource() < tesla_power_cost then
    gapi.add_msg(MsgType.info, locale.gettext("The tesla wire crackles — the grid has no charge to spare."))
    return
  end
  grid:mod_resource(-tesla_power_cost)
  local dmg = DamageInstance.new()
  dmg:add_damage(DamageType.DT_ELECTRIC, tesla_damage, 0, 0, 1)
  who:deal_damage(nil, BodyPartTypeIntId.new(BodyPartTypeId.new("torso")), dmg)
  gapi.add_msg(MsgType.bad, locale.gettext("The tesla wire discharges into %s!"):format(who:disp_name(false, false)))
end

game.itrap_functions["tr_tesla_wire"] = {
  on_trigger = traps.tesla_wire_on_trigger,
}

-- Spear snare: spring-loaded spears pierce the triggerer and pin it in place.
-- Lua instead of the hardcoded snare_heavy / spell actions so the message can
-- say what actually happened (spell damage prints "<empty> shoots <target>"
-- because trap spells are cast by an unnamed dummy npc).
local snare_spear_damage_min = 25
local snare_spear_damage_max = 45

---@param params ItrapParams
---@return boolean
function traps.snare_spear_can_trigger(_params)
  return true
end

---@param params ItrapParams
function traps.snare_spear_on_trigger(params)
  local who = params.target
  local leg = gapi.rng(1, 2) == 1 and "leg_l" or "leg_r"
  local dmg = DamageInstance.new()
  dmg:add_damage(DamageType.DT_STAB, gapi.rng(snare_spear_damage_min, snare_spear_damage_max), 0, 0, 1)
  who:deal_damage(nil, BodyPartTypeIntId.new(BodyPartTypeId.new(leg)), dmg)
  who:add_effect(EffectTypeId.new("heavysnare"), TimeDuration.from_turns(1000), BodyPartTypeId.new(leg), 1)
  -- disp_name already includes the article ("the crawling zombie", "you")
  gapi.add_msg(MsgType.bad, locale.gettext("Spring-loaded spears pierce %s!"):format(who:disp_name(false, false)))
end

game.itrap_functions["tr_snare_spear"] = {
  can_trigger = traps.snare_spear_can_trigger,
  on_trigger = traps.snare_spear_on_trigger,
}

-- Electric tripwire: Lua instead of a spell so the message names the trap
-- (spell damage prints "<empty> shoots <target>" from the unnamed dummy caster).
local tripwire_shock_damage_min = 25
local tripwire_shock_damage_max = 40

---@param params ItrapParams
---@return boolean
function traps.tripwire_shock_can_trigger(_params)
  return true
end

---@param params ItrapParams
function traps.tripwire_shock_on_trigger(params)
  local who = params.target
  local dmg = DamageInstance.new()
  dmg:add_damage(DamageType.DT_ELECTRIC, gapi.rng(tripwire_shock_damage_min, tripwire_shock_damage_max), 0, 0, 1)
  who:deal_damage(nil, BodyPartTypeIntId.new(BodyPartTypeId.new("torso")), dmg)
  who:add_effect(EffectTypeId.new("stunned"), TimeDuration.from_turns(gapi.rng(100, 300)), nil, 1)
  gapi.add_msg(MsgType.bad, locale.gettext("The electric tripwire zaps %s!"):format(who:disp_name(false, false)))
end

game.itrap_functions["tr_tripwire_shock"] = {
  can_trigger = traps.tripwire_shock_can_trigger,
  on_trigger = traps.tripwire_shock_on_trigger,
}

-- Elemental tripwires. Lua instead of spells so the message names the trap
-- (spell damage prints "<empty> shoots <target>" from the unnamed dummy caster).
-- Fields are painted on the trap tile like the spell field_id mechanism did.

---@param who Character
---@param dmg_type DamageType
---@param dmg_min integer
---@param dmg_max integer
---@param effect EffectTypeId
---@param dur_min integer
---@param dur_max integer
---@param field FieldTypeIntId?
---@param field_min integer
---@param field_max integer
---@param message string
local function elemental_tripwire_on_trigger(params, dmg_type, dmg_min, dmg_max,
    effect, dur_min, dur_max, field, field_min, field_max, message)
  local who = params.target
  local dmg = DamageInstance.new()
  dmg:add_damage(dmg_type, gapi.rng(dmg_min, dmg_max), 0, 0, 1)
  who:deal_damage(nil, BodyPartTypeIntId.new(BodyPartTypeId.new("torso")), dmg)
  who:add_effect(effect, TimeDuration.from_turns(gapi.rng(dur_min, dur_max)), nil, 1)
  if field ~= nil then
    gapi.get_map():add_field_at(params.pos, field, gapi.rng(field_min, field_max), TimeDuration.from_turns(0))
  end
  gapi.add_msg(MsgType.bad, locale.gettext(message):format(who:disp_name(false, false)))
end

---@param params ItrapParams
---@return boolean
function traps.tripwire_incendiary_can_trigger(_params)
  return true
end

---@param params ItrapParams
function traps.tripwire_incendiary_on_trigger(params)
  elemental_tripwire_on_trigger(params, DamageType.DT_HEAT, 20, 30,
    EffectTypeId.new("onfire"), 1200, 2400,
    field_int_id("fd_fire"), 1, 2,
    "The incendiary tripwire sets %s on fire!")
end

game.itrap_functions["tr_tripwire_incendiary"] = {
  can_trigger = traps.tripwire_incendiary_can_trigger,
  on_trigger = traps.tripwire_incendiary_on_trigger,
}

---@param params ItrapParams
---@return boolean
function traps.tripwire_acid_can_trigger(_params)
  return true
end

---@param params ItrapParams
function traps.tripwire_acid_on_trigger(params)
  elemental_tripwire_on_trigger(params, DamageType.DT_ACID, 15, 25,
    EffectTypeId.new("corroding"), 3000, 6000,
    field_int_id("fd_acid"), 2, 3,
    "The acid tripwire sprays %s with acid!")
end

game.itrap_functions["tr_tripwire_acid"] = {
  can_trigger = traps.tripwire_acid_can_trigger,
  on_trigger = traps.tripwire_acid_on_trigger,
}

---@param params ItrapParams
---@return boolean
function traps.emp_can_trigger(_params)
  return true
end

---@param params ItrapParams
function traps.emp_on_trigger(params)
  elemental_tripwire_on_trigger(params, DamageType.DT_ELECTRIC, 5, 15,
    EffectTypeId.new("emp"), 200, 400,
    nil, 0, 0,
    "The EMP trap pulses %s!")
end

game.itrap_functions["tr_emp"] = {
  can_trigger = traps.emp_can_trigger,
  on_trigger = traps.emp_on_trigger,
}

return traps

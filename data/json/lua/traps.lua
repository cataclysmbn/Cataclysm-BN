---@class ItrapParams
---@field target Character
---@field trap any
---@field pos TripointBubMs

---@class SprinklerLiquid
---@field liquid ItypeId
---@field field FieldTypeId

local traps = {}

-- Sprinkler: sprays whatever liquid the connected fluid grid currently holds.
-- One liquid family per grid, so probing a few known liquids finds the stored one.
---@type SprinklerLiquid[]
local sprinkler_liquids = {
  { liquid = ItypeId.new("acid"), field = FieldTypeId.new("fd_acid") },
  { liquid = ItypeId.new("water"), field = FieldTypeId.new("fd_water") },
  { liquid = ItypeId.new("water_clean"), field = FieldTypeId.new("fd_water") },
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
  for _, p in ipairs(m:points_in_radius(pos, sprinkler_spray_radius)) do
    m:add_field_at(p, entry.field, 2, TimeDuration.from_turns(0))
  end
  gapi.add_msg(MsgType.bad, locale.gettext("The sprinkler sprays liquid everywhere!"))
end

game.itrap_functions["tr_sprinkler"] = {
  can_trigger = traps.sprinkler_can_trigger,
  on_trigger = traps.sprinkler_on_trigger,
}

-- Bot hatch: 3x3 footprint, center tile holds tr_bot_hatch, ring tiles hold
-- tr_bot_hatch_ring. Fires once: the first triggered tile releases the bot and
-- removes the center, remaining ring tiles silently clear themselves.
local bot_hatch_center_id = "tr_bot_hatch"
local bot_hatch_monster = MonsterTypeId.new("mon_manhack")

---@param pos TripointBubMs
---@return TripointBubMs?
local function find_hatch_center(pos)
  local m = gapi.get_map()
  for _, p in ipairs(m:points_in_radius(pos, 1)) do
    if m:get_trap_at(p):str() == bot_hatch_center_id then
      return p
    end
  end
  return nil
end

---@param params ItrapParams
---@return boolean
function traps.bot_hatch_can_trigger(params)
  return find_hatch_center(params.pos) ~= nil
end

---@param params ItrapParams
function traps.bot_hatch_on_trigger(params)
  local pos = params.pos
  local center = find_hatch_center(pos)
  if center == nil then return end
  gapi.get_map():remove_trap_at(center)
  local bot = gapi.place_monster_around(bot_hatch_monster, center, 1)
  if bot ~= nil then
    gapi.add_msg(MsgType.bad, locale.gettext("The bot hatch opens and a manhack flies out!"))
  end
end

game.itrap_functions["tr_bot_hatch"] = {
  can_trigger = traps.bot_hatch_can_trigger,
  on_trigger = traps.bot_hatch_on_trigger,
}
-- Ring tiles always "fire" so leftover ones clear themselves once the center
-- is gone; on_trigger is a no-op when no center remains.
---@param params ItrapParams
---@return boolean
function traps.bot_hatch_ring_can_trigger(_params)
  return true
end

game.itrap_functions["tr_bot_hatch_ring"] = {
  can_trigger = traps.bot_hatch_ring_can_trigger,
  on_trigger = traps.bot_hatch_on_trigger,
}

-- Tesla wire: drains the base distribution grid (furniture batteries) and
-- shocks the triggerer. Fires as long as the grid holds enough charge.
local tesla_power_cost = 100
local tesla_damage = 30

---@param params ItrapParams
---@return boolean
function traps.tesla_wire_can_trigger(params)
  local abs = gapi.bub_to_abs(params.pos)
  local grid = gapi.get_distribution_grid_tracker():grid_at(abs)
  return grid:is_valid() and grid:get_resource() >= tesla_power_cost
end

---@param params ItrapParams
function traps.tesla_wire_on_trigger(params)
  local who = params.target
  local abs = gapi.bub_to_abs(params.pos)
  local grid = gapi.get_distribution_grid_tracker():grid_at(abs)
  if not grid:is_valid() or grid:get_resource() < tesla_power_cost then return end
  grid:mod_resource(-tesla_power_cost)
  local dmg = DamageInstance.new()
  dmg:add_damage(DamageType.DT_ELECTRIC, tesla_damage, 0, 0, 1)
  who:deal_damage(nil, BodyPartTypeId.new("torso"), dmg)
  gapi.add_msg(MsgType.bad, locale.gettext("The tesla wire discharges into you!"))
end

game.itrap_functions["tr_tesla_wire"] = {
  can_trigger = traps.tesla_wire_can_trigger,
  on_trigger = traps.tesla_wire_on_trigger,
}

return traps

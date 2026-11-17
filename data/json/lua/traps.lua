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

return traps

---@class ZlopeModule
---@field on_mon_death fun(params: OnMonDeathParams)
local zlope = {}

local min_corpse_count = 5
local min_volume_ml = 500000
local min_weight_g = 500000
local zlope_terrain = TerId.new("t_zlope"):int_id()
local horizontal_cardinals = {
  Tripoint.new(1, 0, 0),
  Tripoint.new(-1, 0, 0),
  Tripoint.new(0, 1, 0),
  Tripoint.new(0, -1, 0),
}

---@param map Map
---@param pos Tripoint
---@return boolean
local function is_zlope_tile(map, pos) return map:get_ter_at(pos):str_id():str() == "t_zlope" end

---@param map Map
---@param pos Tripoint
---@return boolean
local function has_adjacent_wall(map, pos)
  for _, pos in ipairs(map:points_in_radius(pos, 1)) do
    if map:has_flag_ter_or_furn("WALL", pos) then return true end
  end
  return false
end

---@param map Map
---@param pos Tripoint
---@return boolean
local function has_open_upper_tile(map, pos)
  local above = pos + Tripoint.new(0, 0, 1)
  return not map:impassable(above) and map:has_flag_ter_or_furn("NO_FLOOR", above)
end

---@param corpse Item
---@return boolean
local function is_mergeable_zombie_corpse(corpse)
  local mon_type = corpse:get_mtype()
  return gapi.monster_type_default_faction(mon_type) == "zombie"
    and not gapi.monster_type_has_harvest_entry_type(mon_type, "bionic_group")
end

---@param map Map
---@param pos Tripoint
---@return Item[], integer, integer
local function collect_valid_corpses(map, pos)
  ---@type Item[]
  local valid_corpses = {}
  local total_volume_ml = 0
  local total_weight_g = 0

  for _, corpse in pairs(map:get_items_at(pos):items()) do
    if corpse:is_corpse() and is_mergeable_zombie_corpse(corpse) then
      valid_corpses[#valid_corpses + 1] = corpse
      total_volume_ml = total_volume_ml + corpse:volume():to_milliliter()
      total_weight_g = total_weight_g + corpse:weight():to_gram()
    end
  end

  return valid_corpses, total_volume_ml, total_weight_g
end

---@param params OnMonDeathParams
function zlope.on_mon_death(params)
  local dead_mon = params.mon
  local map = gapi.get_map()
  local pos = dead_mon:get_pos_ms()

  if is_zlope_tile(map, pos) or not has_adjacent_wall(map, pos) or not has_open_upper_tile(map, pos) then return end

  local valid_corpses, total_volume_ml, total_weight_g = collect_valid_corpses(map, pos)
  if #valid_corpses < min_corpse_count or total_volume_ml < min_volume_ml or total_weight_g < min_weight_g then
    return
  end

  for _, corpse in pairs(valid_corpses) do
    map:remove_item_at(pos, corpse)
  end

  map:set_ter_at(pos, zlope_terrain)
end

return zlope

local utils = {}

---@param pt TripointBubMs
---@return string
function utils.serialize_tripoint_bub_ms(pt) return string.format("%d,%d,%d", pt.x, pt.y, pt.z) end

---@param pt TripointAbsMs
---@return string
function utils.serialize_tripoint_abs_ms(pt) return string.format("%d,%d,%d", pt.x, pt.y, pt.z) end

---@param str string|nil
---@return integer|nil, integer|nil, integer|nil
function utils.parse_tripoint_components(str)
  local x, y, z = string.match(str or "", "(-?%d+),(-?%d+),(-?%d+)")
  if x == nil then return nil, nil, nil end
  return tonumber(x), tonumber(y), tonumber(z)
end

---@param str string|nil
---@return TripointBubMs|nil
function utils.deserialize_tripoint_bub_ms(str)
  local x, y, z = utils.parse_tripoint_components(str)
  if x ~= nil then return TripointBubMs.new(x, y, z) end
  return nil
end

---@param str string|nil
---@return TripointAbsMs|nil
function utils.deserialize_tripoint_abs_ms(str)
  local x, y, z = utils.parse_tripoint_components(str)
  if x ~= nil then return TripointAbsMs.new(x, y, z) end
  return nil
end

---@param mon Monster
---@param dest TripointBubMs
---@return boolean
function utils.safe_step(mon, dest)
  local occupant = gapi.get_creature_at(dest, true)
  if occupant ~= nil and occupant ~= mon then return false end
  return mon:move_to(dest, false, false, 1.0)
end

---@param val integer
---@return integer
function utils.sign(val)
  if val > 0 then return 1 end
  if val < 0 then return -1 end
  return 0
end

return utils

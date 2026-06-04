local M = {}

local ui = require("lib.ui")

local robofac_faction = FactionId.new("robofac")
local robofac_auxiliaries = FactionId.new("robofac_auxiliaries")
---@return MonsterFactionIntId
local get_robofac_authorized_monster_faction = function() return MonsterFactionId.new("robofac_authorized"):int_id() end
local legacy_light_turret = "mon_turret_light"
local hub01_turret = "mon_robofac_turret_light"
local hub01_turret_id = MonsterTypeId.new(hub01_turret)
local hub01_light_retrieval_complete = "npctalk_var_dialogue_intercom_completed_robofac_intercom_3"
local nearby_hub01_scan_radius_omt = 4
---@type PointOmtMs[]
local hub01_tile_offsets = coords.overmap_terrain_tiles()

---@class RobofacElevatorTryUseParams
---@field player Avatar?
---@field om_terrain string?

---@class RobofacNpcParams
---@field npc NPC?

---@class RobofacMonsterParams
---@field monster Monster?

---@param text string
---@param prefix string
---@return boolean
local starts_with = function(text, prefix) return text:sub(1, #prefix) == prefix end

---@param ch Character?
---@return boolean
local has_hub01_clearance = function(ch) return ch ~= nil and ch:get_value(hub01_light_retrieval_complete) == "yes" end

---@param ch Character?
---@return boolean
local is_in_hub01 = function(ch)
  if ch == nil then return false end
  return overmapbuffer.check_ot("robofachq_", OtMatchType.PREFIX, ch:global_square_location():to_omt())
end

---@return TripointBubMs[]
local nearby_hub01_points = function()
  ---@type Avatar?
  local player = gapi.get_avatar()
  if not has_hub01_clearance(player) then return {} end
  if not is_in_hub01(player) then return {} end

  ---@type Map
  local here = gapi.get_map()
  ---@type integer
  local map_size = here:get_map_size()
  ---@type TripointAbsOmt
  local center_omt = player:global_square_location():to_omt()
  ---@type TripointBubMs[]
  local points = {}

  for omt_x = center_omt.x - nearby_hub01_scan_radius_omt, center_omt.x + nearby_hub01_scan_radius_omt do
    for omt_y = center_omt.y - nearby_hub01_scan_radius_omt, center_omt.y + nearby_hub01_scan_radius_omt do
      local omt = TripointAbsOmt.new(omt_x, omt_y, center_omt.z)
      if overmapbuffer.check_ot("robofachq_", OtMatchType.PREFIX, omt) then
        for _, offset in ipairs(hub01_tile_offsets) do
          ---@type TripointAbsMs
          local abs_pos = coords.project_combine(omt, offset)
          local point = here:abs_to_bub(abs_pos)
          if point.x >= 0 and point.y >= 0 and point.x < map_size and point.y < map_size then
            points[#points + 1] = point
          end
        end
      end
    end
  end
  return points
end

---@param params RobofacElevatorTryUseParams
---@return boolean?
M.on_elevator_try_use = function(params)
  local player = params.player
  if not player then return true end

  local om_terrain = params.om_terrain or ""
  if not starts_with(om_terrain, "robofachq_") then return true end
  if has_hub01_clearance(player) then
    M.authorize_active_hub01_turrets()
    return true
  end

  ui.popup(locale.gettext('The control panels\' screen flashes before displaying "UNAUTHORIZED" in bold red letters.'))
  return false
end

---@param params RobofacNpcParams
---@return boolean?
M.authorize_hub01_security = function(params)
  local npc = params.npc
  if not npc then return true end

  local player = gapi.get_avatar()
  if not has_hub01_clearance(player) then return true end
  if npc:get_faction_id():str() ~= robofac_faction:str() then return true end
  if npc:get_first_topic() ~= "TALK_HUB_SECURITY" then return true end

  npc:set_faction_id(robofac_auxiliaries)
  if npc:get_attitude() == NpcAttitude.NPCATT_KILL or npc:get_attitude() == NpcAttitude.NPCATT_TALK then
    npc:set_attitude(NpcAttitude.NPCATT_NULL)
  end
  return true
end

---@param points TripointBubMs[]?
---@return boolean?
M.authorize_active_hub01_security = function(points)
  for _, point in ipairs(points or nearby_hub01_points()) do
    local npc = gapi.get_npc_at(point, false)
    if npc then M.authorize_hub01_security({ npc = npc }) end
  end
  return true
end

---@param params RobofacMonsterParams
---@return boolean?
M.authorize_hub01_turret = function(params)
  local monster = params.monster
  if not monster then return true end
  local monster_type = monster:get_type():str()
  if monster_type ~= hub01_turret and monster_type ~= legacy_light_turret then return true end

  local player = gapi.get_avatar()
  if not has_hub01_clearance(player) then return true end

  if monster_type == legacy_light_turret then
    if not is_in_hub01(player) then return true end
    local pos = monster:get_pos_ms()
    monster:erase()
    monster = gapi.place_monster_at(hub01_turret_id, pos)
    if not monster then return true end
  end
  monster.faction = get_robofac_authorized_monster_faction()
  return true
end

---@param points TripointBubMs[]?
---@return boolean?
M.authorize_active_hub01_turrets = function(points)
  for _, point in ipairs(points or nearby_hub01_points()) do
    local monster = gapi.get_monster_at(point, false)
    if monster then M.authorize_hub01_turret({ monster = monster }) end
  end
  return true
end

---@return boolean?
M.authorize_active_hub01 = function()
  local player = gapi.get_avatar()
  if not has_hub01_clearance(player) then return true end
  if not is_in_hub01(player) then return true end

  local points = nearby_hub01_points()
  M.authorize_active_hub01_security(points)
  M.authorize_active_hub01_turrets(points)
  return true
end

---@return boolean?
M.authorize_hub01_after_dialogue = function()
  local points = nearby_hub01_points()
  M.authorize_active_hub01_security(points)
  M.authorize_active_hub01_turrets(points)
end

return M

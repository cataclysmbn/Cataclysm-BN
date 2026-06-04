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

---@class RobofacElevatorTryUseParams
---@field player Avatar?
---@field om_terrain string?

---@class RobofacNpcParams
---@field npc NPC?

---@class RobofacMonsterParams
---@field monster Monster?

---@class RobofacMissionParams
---@field mission Mission?
---@field mission_type MissionType?

---@class NearbyOmtCreatureQuery
---@field center TripointAbsOmt
---@field radius integer
---@field ignore_z boolean?

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
  return overmapbuffer.check_ot("robofachq", OtMatchType.PREFIX, ch:global_square_location():to_omt())
end

---@return TripointAbsOmt?
local hub01_scan_center = function()
  ---@type Avatar?
  local player = gapi.get_avatar()
  if not has_hub01_clearance(player) then return nil end
  if not is_in_hub01(player) then return nil end
  return player:global_square_location():to_omt()
end

---@return NPC[]
local nearby_hub01_npcs = function()
  local center = hub01_scan_center()
  if center == nil then return {} end
  return gapi.get_npcs_near_omt({ center = center, radius = nearby_hub01_scan_radius_omt, ignore_z = true })
end

---@return Monster[]
local nearby_hub01_monsters = function()
  local center = hub01_scan_center()
  if center == nil then return {} end
  return gapi.get_monsters_near_omt({ center = center, radius = nearby_hub01_scan_radius_omt, ignore_z = true })
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

---@param npcs NPC[]?
---@return boolean?
M.authorize_active_hub01_security = function(npcs)
  for _, npc in ipairs(npcs or nearby_hub01_npcs()) do
    M.authorize_hub01_security({ npc = npc })
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

---@param monsters Monster[]?
---@return boolean?
M.authorize_active_hub01_turrets = function(monsters)
  for _, monster in ipairs(monsters or nearby_hub01_monsters()) do
    M.authorize_hub01_turret({ monster = monster })
  end
  return true
end

---@return boolean?
M.authorize_active_hub01 = function()
  local player = gapi.get_avatar()
  if not has_hub01_clearance(player) then return true end
  if not is_in_hub01(player) then return true end

  M.authorize_active_hub01_security()
  M.authorize_active_hub01_turrets()
  return true
end

---@return boolean?
M.authorize_hub01_after_dialogue = function()
  M.authorize_active_hub01_security()
  M.authorize_active_hub01_turrets()
end

---@param _params RobofacMissionParams
---@return boolean?
M.authorize_hub01_after_mission = function(_params)
  M.authorize_active_hub01_security()
  M.authorize_active_hub01_turrets()
  return true
end

return M

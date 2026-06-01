local M = {}

local ui = require("lib.ui")

local robofac_faction = FactionId.new("robofac")
local robofac_auxiliaries = FactionId.new("robofac_auxiliaries")
local robofac_authorized_monster_faction = MonsterFactionId.new("robofac_authorized"):int_id()
local legacy_light_turret = "mon_turret_light"
local hub01_turret = "mon_robofac_turret_light"
local hub01_turret_id = MonsterTypeId.new(hub01_turret)
local hub01_light_retrieval_complete = "npctalk_var_dialogue_intercom_completed_robofac_intercom_3"

local starts_with = function(text, prefix) return text:sub(1, #prefix) == prefix end

local has_hub01_clearance = function(ch) return ch and ch:get_value(hub01_light_retrieval_complete) == "yes" end

local is_in_hub01 = function(ch) return overmapbuffer.check_ot("robofachq_", OtMatchType.PREFIX, ch:global_square_location():to_omt()) end

---@param params table
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

---@param params table
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

---@param params table
---@return boolean?
M.authorize_hub01_turret = function(params)
  local monster = params.monster
  if not monster then return true end
  local monster_type = monster:get_type():str()
  if monster_type ~= hub01_turret and monster_type ~= legacy_light_turret then return true end

  local player = gapi.get_avatar()
  if not has_hub01_clearance(player) or not is_in_hub01(player) then return true end

  if monster_type == legacy_light_turret then
    local pos = monster:get_pos_ms()
    monster:erase()
    monster = gapi.place_monster_at(hub01_turret_id, pos)
    if not monster then return true end
  end
  monster.faction = robofac_authorized_monster_faction
  monster.friendly = 100
  return true
end

M.authorize_active_hub01_turrets = function()
  local player = gapi.get_avatar()
  if not has_hub01_clearance(player) or not is_in_hub01(player) then return true end
  for _, monster in ipairs(gapi.get_all_monsters()) do
    M.authorize_hub01_turret({ monster = monster })
  end
  return true
end

return M

---@param _ string
---@return nil
local popup = function(_) end
package.loaded["lib.ui"] = { popup = popup }

---@class FakeStringId
---@field value string

---@param id string
---@return FakeStringId
local make_string_id = function(id)
  local result = { value = id }

  ---@param self FakeStringId
  ---@return string
  result.str = function(self) return self.value end

  return result
end

---@class FakeMonsterFactionId: FakeStringId

---@param id string
---@return FakeMonsterFactionId
local make_monster_faction_id = function(id)
  local result = make_string_id(id)

  ---@param self FakeMonsterFactionId
  ---@return string
  result.int_id = function(self) return self.value .. ":int" end

  return result
end

_G.FactionId = { new = make_string_id }
_G.MonsterFactionId = { new = make_monster_faction_id }
_G.MonsterTypeId = { new = make_string_id }
_G.OtMatchType = { PREFIX = 1 }
_G.NpcAttitude = { NPCATT_KILL = 1, NPCATT_TALK = 2, NPCATT_NULL = 3 }

---@param text string
---@return string
local gettext = function(text) return text end
_G.locale = { gettext = gettext }

---@class FakeCoord
---@field x integer
---@field y integer
---@field z integer

---@param x integer
---@param y integer
---@param z integer
---@return FakeCoord
local make_tripoint = function(x, y, z) return { x = x, y = y, z = z } end

---@class FakeCharacter

---@class FakePlayer: FakeCharacter
local player = {}

---@class FakeGlobalSquareLocation

---@param _self FakePlayer
---@param _ string
---@return string
player.get_value = function(_self, _) return "yes" end

---@param _self FakePlayer
---@return FakeGlobalSquareLocation
player.global_square_location = function(_self)
  ---@param _location FakeGlobalSquareLocation
  ---@return FakeCoord
  local to_omt = function(_location) return make_tripoint(11, 20, 0) end
  return { to_omt = to_omt }
end

local player_is_in_hub01 = true

---@param _otype string
---@param _match_type integer
---@param omt FakeCoord
---@return boolean
local check_ot = function(otype, _match_type, omt)
  test_data.hub01_prefix = otype
  return player_is_in_hub01 and otype == "robofachq" and omt.x == 11 and omt.y == 20 and omt.z == 0
end
_G.overmapbuffer = { check_ot = check_ot }

---@class FakeNpc
local npc = {}
local npc_authorized = false
local npc_attitude_cleared = false

---@param _self FakeNpc
---@return FakeStringId
npc.get_faction_id = function(_self) return make_string_id("robofac") end

---@param _self FakeNpc
---@return string
npc.get_first_topic = function(_self) return "TALK_HUB_SECURITY" end

---@param _self FakeNpc
---@param id FakeStringId
---@return nil
npc.set_faction_id = function(_self, id) npc_authorized = id:str() == "robofac_auxiliaries" end

---@param _self FakeNpc
---@return integer
npc.get_attitude = function(_self) return NpcAttitude.NPCATT_KILL end

---@param _self FakeNpc
---@param attitude integer
---@return nil
npc.set_attitude = function(_self, attitude) npc_attitude_cleared = attitude == NpcAttitude.NPCATT_NULL end

---@class FakeMonster
local monster = {}

---@param _self FakeMonster
---@return FakeStringId
monster.get_type = function(_self) return make_string_id("mon_robofac_turret_light") end

local npc_omt_queries = 0
local monster_omt_queries = 0
local npc_query_radius = -1
local monster_query_radius = -1
local npc_query_ignores_z = false
local monster_query_ignores_z = false

---@class FakeNearbyOmtCreatureQuery
---@field center FakeCoord
---@field radius integer
---@field ignore_z boolean?

---@param params FakeNearbyOmtCreatureQuery
---@return FakeNpc[]
local get_npcs_near_omt = function(params)
  npc_omt_queries = npc_omt_queries + 1
  npc_query_radius = params.radius
  npc_query_ignores_z = params.ignore_z == true
  if params.center.x == 11 and params.center.y == 20 and params.center.z == 0 then return { npc } end
  return {}
end

---@param params FakeNearbyOmtCreatureQuery
---@return FakeMonster[]
local get_monsters_near_omt = function(params)
  monster_omt_queries = monster_omt_queries + 1
  monster_query_radius = params.radius
  monster_query_ignores_z = params.ignore_z == true
  if params.center.x == 11 and params.center.y == 20 and params.center.z == 0 then return { monster } end
  return {}
end

---@return nil
local fail_slow_scan = function() error("regression: slow creature scan was used") end

---@return FakePlayer
local get_avatar = function() return player end

_G.gapi = {
  get_avatar = get_avatar,
  get_npcs_near_omt = get_npcs_near_omt,
  get_monsters_near_omt = get_monsters_near_omt,
  get_all_npcs = fail_slow_scan,
  get_all_monsters = fail_slow_scan,
  get_npc_at = fail_slow_scan,
  get_monster_at = fail_slow_scan,
}

_G.game = _G.game or {}
_G.game.current_mod_path = "data/json"
package.path = package.path .. ";data/json/?.lua"
package.loaded["lua.robofac"] = nil

local robofac = require("lua.robofac")
robofac.authorize_hub01_after_dialogue()

test_data.npc_authorized = npc_authorized
test_data.npc_attitude_cleared = npc_attitude_cleared
test_data.monster_authorized = monster.faction == "robofac_authorized:int"
test_data.npc_omt_queries = npc_omt_queries
test_data.monster_omt_queries = monster_omt_queries
test_data.npc_query_radius = npc_query_radius
test_data.monster_query_radius = monster_query_radius
test_data.npc_query_ignores_z = npc_query_ignores_z
test_data.monster_query_ignores_z = monster_query_ignores_z

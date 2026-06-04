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

_G.TripointAbsOmt = { new = make_tripoint }

local hub_omt_min_x = 10
local hub_omt_min_y = 20
local map_squares_per_omt = 24
local hub_omts = {}
local hub_zlevels = { 0, -2 }
for _, z in ipairs(hub_zlevels) do
  for x = 10, 13 do
    for y = 20, 21 do
      hub_omts[x .. "," .. y .. "," .. z] = true
    end
  end
end

---@param _otype string
---@param _match_type integer
---@param omt FakeCoord
---@return boolean
local check_ot = function(_otype, _match_type, omt) return hub_omts[omt.x .. "," .. omt.y .. "," .. omt.z] == true end
_G.overmapbuffer = { check_ot = check_ot }

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

---@class FakeMap
local map = {}

---@param _self FakeMap
---@return integer
map.get_map_size = function(_self) return 96 end

---@param _self FakeMap
---@param abs_pos FakeCoord
---@return FakeCoord
map.abs_to_bub = function(_self, abs_pos)
  return make_tripoint(abs_pos.x - hub_omt_min_x * map_squares_per_omt, abs_pos.y - hub_omt_min_y * map_squares_per_omt, abs_pos.z)
end

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

local npc_point_lookups = 0
local monster_point_lookups = 0

---@param point FakeCoord
---@param _allow_hallucination boolean
---@return FakeNpc?
local get_npc_at = function(point, _allow_hallucination)
  npc_point_lookups = npc_point_lookups + 1
  if point.x == 25 and point.y == 1 and point.z == -2 then return npc end
  return nil
end

---@param point FakeCoord
---@param _allow_hallucination boolean
---@return FakeMonster?
local get_monster_at = function(point, _allow_hallucination)
  monster_point_lookups = monster_point_lookups + 1
  if point.x == 26 and point.y == 1 and point.z == -2 then return monster end
  return nil
end

---@return nil
local fail_get_all = function() error("regression: global active creature scan was used") end

---@return FakePlayer
local get_avatar = function() return player end

---@return FakeMap
local get_map = function() return map end

_G.gapi = {
  get_avatar = get_avatar,
  get_map = get_map,
  get_npc_at = get_npc_at,
  get_monster_at = get_monster_at,
  get_all_npcs = fail_get_all,
  get_all_monsters = fail_get_all,
}

---@return FakeCoord[]
local overmap_terrain_tiles = function()
  local offsets = {}
  for x = 0, map_squares_per_omt - 1 do
    for y = 0, map_squares_per_omt - 1 do
      offsets[#offsets + 1] = make_tripoint(x, y, 0)
    end
  end
  return offsets
end

---@param omt FakeCoord
---@param offset FakeCoord
---@return FakeCoord
local project_combine = function(omt, offset)
  return make_tripoint(omt.x * map_squares_per_omt + offset.x, omt.y * map_squares_per_omt + offset.y, omt.z)
end

_G.coords = { overmap_terrain_tiles = overmap_terrain_tiles, project_combine = project_combine }
_G.game = _G.game or {}
_G.game.current_mod_path = "data/json"
package.path = package.path .. ";data/json/?.lua"
package.loaded["lua.robofac"] = nil

local robofac = require("lua.robofac")
robofac.authorize_hub01_after_dialogue()

local expected_hub_points = 16 * map_squares_per_omt * map_squares_per_omt
test_data.npc_authorized = npc_authorized
test_data.npc_attitude_cleared = npc_attitude_cleared
test_data.monster_authorized = monster.faction == "robofac_authorized:int"
test_data.npc_point_lookups = npc_point_lookups
test_data.monster_point_lookups = monster_point_lookups
test_data.expected_hub_points = expected_hub_points

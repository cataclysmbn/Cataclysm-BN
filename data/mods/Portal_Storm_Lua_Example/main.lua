gdebug.log_info("Portal Storm Lua Example: main")

---@class PortalStormWeatherUpdatedParams
---@field weather_id string
---@field old_weather_id string

---@class PortalStormLuaExample
---@field spawn_cooldown_turns integer
---@field spawn_radius integer
---@field spawn_count_min integer
---@field spawn_count_max integer
---@field spawn_pool string[]
---@field last_spawn_turn integer
local mod = game.mod_runtime[game.current_mod]

mod.spawn_cooldown_turns = 30 * 60
mod.spawn_radius = 8
mod.spawn_count_min = 1
mod.spawn_count_max = 3
mod.spawn_pool = {
  "mon_mi_go",
  "mon_blank",
  "mon_yugg",
}
mod.last_spawn_turn = mod.last_spawn_turn or -1000000

---@return Avatar|nil
local function get_avatar()
  return gapi.get_avatar()
end

---@return integer
local function current_turn()
  return gapi.current_turn():to_turn()
end

---@return string
local function random_spawn_id()
  local index = math.random( 1, #mod.spawn_pool )
  return mod.spawn_pool[index]
end

---@param player Avatar
---@return TripointBubMs
local function player_pos( player )
  return player:get_pos_ms()
end

---@param player Avatar
---@return integer
local function spawn_near_player( player )
  local spawned = 0
  local count = math.random( mod.spawn_count_min, mod.spawn_count_max )
  local pos = player_pos( player )

  for _ = 1, count do
    local mon = gapi.place_monster_around( MonsterTypeId.new( random_spawn_id() ), pos, mod.spawn_radius )
    if mon ~= nil then
      spawned = spawned + 1
    end
  end

  return spawned
end

---@param params PortalStormWeatherUpdatedParams
---@return nil
mod.on_weather_updated = function( params )
  if params.weather_id ~= "portal_storm" then
    return
  end

  local now = current_turn()
  if now - mod.last_spawn_turn < mod.spawn_cooldown_turns then
    return
  end

  local player = get_avatar()
  if player == nil then
    return
  end

  local spawned = spawn_near_player( player )
  if spawned <= 0 then
    return
  end

  mod.last_spawn_turn = now
  gapi.add_msg( MsgType.warning, "The portal storm tears reality open nearby!" )
end

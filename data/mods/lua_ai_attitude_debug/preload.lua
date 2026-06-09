gdebug.log_info("lua_ai_attitude_debug: preload online.")

local mod = game.mod_runtime[game.current_mod]
local storage = game.mod_storage[game.current_mod]
storage.mode = storage.mode or "attack"
storage.mine_center = storage.mine_center or ""

---@class LuaAiDebugMode
---@field id string
---@field label string
---@field desc string

---@type LuaAiDebugMode[]
local modes = {
  { id = "attack", label = "Attack", desc = "Engage target (pick tile optional)." },
  { id = "follow", label = "Follow", desc = "Trail the avatar, holding 1–2 tiles away." },
  { id = "ignore", label = "Ignore", desc = "Idle in place." },
  { id = "fetch", label = "Fetch", desc = "Pick tile/item; grab and return it." },
  { id = "mine", label = "Mine", desc = "Pick center; walk 5x5 and bash tiles." },
}

---@param id string
---@return LuaAiDebugMode|nil
local function mode_by_id(id)
  for _, m in ipairs(modes) do
    if m.id == id then return m end
  end
  return nil
end

---@param pt Tripoint
---@return string
local function serialize_tripoint(pt) return string.format("%d,%d,%d", pt.x, pt.y, pt.z) end

---@param str string|nil
---@return Tripoint|nil
local function deserialize_tripoint(str)
  local x, y, z = string.match(str or "", "(-?%d+),(-?%d+),(-?%d+)")
  if x ~= nil then return Tripoint.new(tonumber(x), tonumber(y), tonumber(z)) end
  return nil
end

---@param mon Monster
---@param dest Tripoint
---@return boolean
local function safe_step(mon, dest)
  local occupant = gapi.get_creature_at(dest, true)
  if occupant ~= nil and occupant ~= mon then return false end
  return mon:move_to(dest, false, false, 1.0)
end

---@return string|nil
local function choose_mode()
  local menu = UiList.new()
  menu:title("Select Lua AI debug mode")
  for idx, m in ipairs(modes) do
    local label = string.format("%s (%s)", m.label, m.id)
    if menu.add_w_desc ~= nil then
      menu:add_w_desc(idx, label, m.desc)
    else
      menu:add(idx, label)
    end
  end
  local chosen = menu:query()
  if chosen < 0 or modes[chosen] == nil then return nil end
  return modes[chosen].id
end

---@param val integer
---@return integer
local function sign(val)
  if val > 0 then return 1 end
  if val < 0 then return -1 end
  return 0
end

local ai_behaviors = require("ai_behaviors")
---@class LuaAiDebugContext
---@field storage table
---@field serialize_tripoint fun(pt: Tripoint): string
---@field deserialize_tripoint fun(str: string|nil): Tripoint|nil
---@field safe_step fun(mon: Monster, dest: Tripoint): boolean
---@field sign fun(val: integer): integer

---@type LuaAiDebugContext
ai_behaviors.register({
  storage = storage,
  serialize_tripoint = serialize_tripoint,
  deserialize_tripoint = deserialize_tripoint,
  safe_step = safe_step,
  sign = sign,
})

mod.get_mode = function() return storage.mode end

mod.set_mode = function(new_mode)
  local m = mode_by_id(new_mode)
  if m ~= nil then
    storage.mode = new_mode
    return new_mode
  end
  return storage.mode
end

mod.spawn_demo_monster = function()
  local avatar = gapi.get_avatar()
  if avatar == nil then
    gdebug.log_warn("Lua AI debug: no avatar to anchor spawn.")
    return nil
  end
  local spawn = gapi.place_monster_around(MonsterTypeId.new("mon_lua_ai_debug"), avatar:get_pos_ms(), 1)
  if spawn ~= nil then
    local default_mode = mode_by_id(storage.mode) and storage.mode or modes[1].id
    spawn:set_value("lua_ai_mode", default_mode)
    spawn:set_value("lua_ai_fetch_target", storage.fetch_target or "")
    spawn:set_value("lua_ai_fetch_item", storage.fetch_item or "")
    spawn:set_value("lua_ai_drop_target", storage.drop_target or "")
    spawn:set_value("lua_ai_attack_target", storage.attack_target or "")
    spawn:set_value("lua_ai_mine_center", storage.mine_center or "")
    spawn:set_value("lua_ai_mine_idx", "1")
    spawn:set_value("lua_ai_remote", "0")
  end
  return spawn
end

mod.use_remote = function(_who, _item, _pos)
  local new_mode = choose_mode()
  if new_mode == nil then
    gapi.add_msg(MsgType.info, "Lua AI mode selection cancelled.")
    return 0
  end
  local fetch = nil
  local attack_target = ""
  local mine_center = nil
  if new_mode == "fetch" then
    fetch = mod.prompt_fetch_target()
    if fetch == nil then
      new_mode = "follow"
      storage.mode = new_mode
      storage.fetch_target = ""
      storage.fetch_item = ""
      storage.drop_target = ""
    else
      storage.fetch_target = fetch.target
      storage.fetch_item = fetch.item_id
      storage.drop_target = fetch.drop_target
    end
  elseif new_mode == "mine" then
    mine_center = mod.prompt_mine_area()
    if mine_center == nil then
      new_mode = "follow"
      storage.mode = new_mode
    else
      storage.mine_center = mine_center
    end
  else
    storage.fetch_target = ""
    storage.fetch_item = ""
    storage.drop_target = ""
    if new_mode ~= "mine" then storage.mine_center = "" end
  end
  if new_mode == "attack" then
    attack_target = mod.prompt_attack_target()
    storage.attack_target = attack_target
  end
  mod.set_nearby_modes(new_mode, fetch, attack_target, mine_center)
  local spawned = mod.spawn_demo_monster()
  if spawned ~= nil then
    gapi.add_msg(MsgType.info, string.format("Lua AI debug: mode '%s', spawned %s.", new_mode, spawned:name(0)))
  else
    gapi.add_msg(MsgType.info, string.format("Lua AI debug: mode '%s'. Spawn the drone manually if needed.", new_mode))
  end
  return 0
end

local function set_mon_mode(mon, mode, fetch, drop_target, attack_target, mine_center)
  mon:set_value("lua_ai_mode", mode)
  if fetch ~= nil then
    mon:set_value("lua_ai_fetch_target", fetch.target or "")
    mon:set_value("lua_ai_fetch_item", fetch.item_id or "")
  else
    mon:set_value("lua_ai_fetch_target", "")
    mon:set_value("lua_ai_fetch_item", "")
  end
  mon:set_value("lua_ai_drop_target", drop_target or "")
  mon:set_value("lua_ai_attack_target", attack_target or "")
  if mode == "mine" then
    mon:set_value("lua_ai_mine_center", mine_center or "")
    mon:set_value("lua_ai_mine_idx", "1")
  else
    mon:set_value("lua_ai_mine_center", "")
    mon:set_value("lua_ai_mine_idx", "")
  end
  mon:set_value("lua_ai_remote", "0")
end

mod.set_nearby_modes = function(mode, fetch, attack_target, mine_center)
  storage.mode = mode
  storage.fetch_target = fetch and fetch.target or ""
  storage.fetch_item = fetch and fetch.item_id or ""
  storage.drop_target = fetch and fetch.drop_target or ""
  storage.attack_target = attack_target or ""
  storage.mine_center = mine_center or storage.mine_center or ""
  local avatar = gapi.get_avatar()
  if avatar == nil then return end
  local here = gapi.get_map()
  for _, pt in ipairs(here:points_in_radius(avatar:get_pos_ms(), 15)) do
    local mon = gapi.get_monster_at(pt, true)
    if mon ~= nil and mon:get_type():str() == "mon_lua_ai_debug" then
      set_mon_mode(mon, mode, fetch, storage.drop_target, storage.attack_target, storage.mine_center)
    end
  end
end

mod.prompt_fetch_target = function()
  local here = gapi.get_map()
  local where = gapi.look_around()
  if where == nil then
    gapi.add_msg(MsgType.info, "Lua AI fetch cancelled.")
    return nil
  end
  local stack = here:get_items_at(where)
  local items = {}
  for idx, it in ipairs(stack) do
    items[#items + 1] = it
  end
  if #items == 0 then
    gapi.add_msg(MsgType.info, "Lua AI fetch: no items there.")
    return nil
  end
  local menu = UiList.new()
  menu:title("Fetch which item?")
  for idx, it in ipairs(items) do
    menu:add(idx, it:tname(1, true, 0))
  end
  local chosen = menu:query()
  if chosen < 0 or items[chosen] == nil then
    gapi.add_msg(MsgType.info, "Lua AI fetch cancelled.")
    return nil
  end
  return {
    target = serialize_tripoint(where),
    item_id = items[chosen]:get_type():str(),
    raw_pos = where,
    drop_target = serialize_tripoint(gapi.get_avatar():get_pos_ms()),
  }
end

mod.prompt_attack_target = function()
  local where = gapi.look_around()
  if where == nil then
    gapi.add_msg(MsgType.info, "Lua AI attack target cancelled.")
    return ""
  end
  return serialize_tripoint(where)
end

mod.prompt_mine_area = function()
  local where = gapi.look_around()
  if where == nil then
    gapi.add_msg(MsgType.info, "Lua AI mine cancelled.")
    return nil
  end
  local abs = gapi.get_map():get_abs_ms(where)
  return serialize_tripoint(abs)
end

mod.use_selector = function(_who, _item, _pos)
  local new_mode = choose_mode()
  if new_mode == nil then
    gapi.add_msg(MsgType.info, "Lua AI mode selection cancelled.")
    return 0
  end
  local fetch = nil
  local attack_target = ""
  local mine_center = nil
  if new_mode == "fetch" then
    fetch = mod.prompt_fetch_target()
    if fetch == nil then
      new_mode = "follow"
      storage.mode = new_mode
      storage.fetch_target = ""
      storage.fetch_item = ""
      storage.drop_target = ""
    else
      storage.fetch_target = fetch.target
      storage.fetch_item = fetch.item_id
      storage.drop_target = fetch.drop_target
    end
  elseif new_mode == "mine" then
    mine_center = mod.prompt_mine_area()
    if mine_center == nil then
      new_mode = "follow"
      storage.mode = new_mode
    else
      storage.mine_center = mine_center
    end
  else
    storage.fetch_target = ""
    storage.fetch_item = ""
    storage.drop_target = ""
    if new_mode ~= "mine" then storage.mine_center = "" end
  end
  if new_mode == "attack" then
    attack_target = mod.prompt_attack_target()
    storage.attack_target = attack_target
  end
  mod.set_nearby_modes(new_mode, fetch, attack_target, mine_center)
  gapi.add_msg(MsgType.info, string.format("Lua AI selector: set nearby debug drones to '%s'.", new_mode))
  return 0
end

game.iuse_functions["LUA_AI_DEBUG_REMOTE"] = function(...) return mod.use_remote(...) end

game.iuse_functions["LUA_AI_DEBUG_SELECTOR"] = function(...) return mod.use_selector(...) end

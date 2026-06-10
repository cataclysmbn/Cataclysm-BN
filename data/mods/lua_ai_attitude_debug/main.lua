local main = {}

local ai_behaviors = require("ai_behaviors")
local utils = require("lib.utils")

---@class LuaAiDebugMode
---@field id string
---@field label string
---@field desc string

---@class LuaAiDebugFetchTarget
---@field target string
---@field item_id string
---@field raw_pos TripointBubMs
---@field drop_target string

---@class LuaAiDebugContext
---@field storage table
---@field serialize_tripoint_bub_ms fun(pt: TripointBubMs): string
---@field serialize_tripoint_abs_ms fun(pt: TripointAbsMs): string
---@field deserialize_tripoint_bub_ms fun(str: string|nil): TripointBubMs|nil
---@field deserialize_tripoint_abs_ms fun(str: string|nil): TripointAbsMs|nil
---@field safe_step fun(mon: Monster, dest: TripointBubMs): boolean
---@field sign fun(val: integer): integer

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
  for _, mode in ipairs(modes) do
    if mode.id == id then return mode end
  end
  return nil
end

---@return string|nil
local function choose_mode()
  local menu = UiList.new()
  menu:title("Select Lua AI debug mode")
  for idx, mode in ipairs(modes) do
    local label = string.format("%s (%s)", mode.label, mode.id)
    if menu.add_w_desc ~= nil then
      menu:add_w_desc(idx, label, mode.desc)
    else
      menu:add(idx, label)
    end
  end
  local chosen = menu:query()
  if chosen < 0 or modes[chosen] == nil then return nil end
  return modes[chosen].id
end

---@param mon Monster
---@param mode string
---@param fetch LuaAiDebugFetchTarget|nil
---@param drop_target string|nil
---@param attack_target string|nil
---@param mine_center string|nil
---@return nil
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

---@param storage table
---@return nil
local function register_ai_behaviors(storage)
  ---@type LuaAiDebugContext
  local ctx = {
    storage = storage,
    serialize_tripoint_bub_ms = utils.serialize_tripoint_bub_ms,
    serialize_tripoint_abs_ms = utils.serialize_tripoint_abs_ms,
    deserialize_tripoint_bub_ms = utils.deserialize_tripoint_bub_ms,
    deserialize_tripoint_abs_ms = utils.deserialize_tripoint_abs_ms,
    safe_step = utils.safe_step,
    sign = utils.sign,
  }
  ai_behaviors.register(ctx)
end

---@param mod table
---@param storage table
---@return nil
local function register_runtime_functions(mod, storage)
  ---@return string
  local function get_mode() return storage.mode end

  ---@param new_mode string
  ---@return string
  local function set_mode(new_mode)
    local mode = mode_by_id(new_mode)
    if mode ~= nil then
      storage.mode = new_mode
      return new_mode
    end
    return storage.mode
  end

  ---@return Monster|nil
  local function spawn_demo_monster()
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

  ---@param mode string
  ---@param fetch LuaAiDebugFetchTarget|nil
  ---@param attack_target string|nil
  ---@param mine_center string|nil
  ---@return nil
  local function set_nearby_modes(mode, fetch, attack_target, mine_center)
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

  ---@return LuaAiDebugFetchTarget|nil
  local function prompt_fetch_target()
    local here = gapi.get_map()
    local where = gapi.look_around()
    if where == nil then
      gapi.add_msg(MsgType.info, "Lua AI fetch cancelled.")
      return nil
    end
    local stack = here:get_items_at(where)
    local items = {}
    for _, item in ipairs(stack) do
      items[#items + 1] = item
    end
    if #items == 0 then
      gapi.add_msg(MsgType.info, "Lua AI fetch: no items there.")
      return nil
    end
    local menu = UiList.new()
    menu:title("Fetch which item?")
    for idx, item in ipairs(items) do
      menu:add(idx, item:tname(1, true, 0))
    end
    local chosen = menu:query()
    if chosen < 0 or items[chosen] == nil then
      gapi.add_msg(MsgType.info, "Lua AI fetch cancelled.")
      return nil
    end
    return {
      target = utils.serialize_tripoint_bub_ms(where),
      item_id = items[chosen]:get_type():str(),
      raw_pos = where,
      drop_target = utils.serialize_tripoint_bub_ms(gapi.get_avatar():get_pos_ms()),
    }
  end

  ---@return string
  local function prompt_attack_target()
    local where = gapi.look_around()
    if where == nil then
      gapi.add_msg(MsgType.info, "Lua AI attack target cancelled.")
      return ""
    end
    return utils.serialize_tripoint_bub_ms(where)
  end

  ---@return string|nil
  local function prompt_mine_area()
    local where = gapi.look_around()
    if where == nil then
      gapi.add_msg(MsgType.info, "Lua AI mine cancelled.")
      return nil
    end
    local abs = gapi.get_map():bub_to_abs(where)
    return utils.serialize_tripoint_abs_ms(abs)
  end

  ---@param _who Character|nil
  ---@param _item Item|nil
  ---@param _pos TripointBubMs|nil
  ---@return integer
  local function use_remote(_who, _item, _pos)
    local new_mode = choose_mode()
    if new_mode == nil then
      gapi.add_msg(MsgType.info, "Lua AI mode selection cancelled.")
      return 0
    end
    local fetch = nil
    local attack_target = ""
    local mine_center = nil
    if new_mode == "fetch" then
      fetch = prompt_fetch_target()
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
      mine_center = prompt_mine_area()
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
      attack_target = prompt_attack_target()
      storage.attack_target = attack_target
    end
    set_nearby_modes(new_mode, fetch, attack_target, mine_center)
    local spawned = spawn_demo_monster()
    if spawned ~= nil then
      gapi.add_msg(MsgType.info, string.format("Lua AI debug: mode '%s', spawned %s.", new_mode, spawned:name(0)))
    else
      gapi.add_msg(
        MsgType.info,
        string.format("Lua AI debug: mode '%s'. Spawn the drone manually if needed.", new_mode)
      )
    end
    return 0
  end

  ---@param _who Character|nil
  ---@param _item Item|nil
  ---@param _pos TripointBubMs|nil
  ---@return integer
  local function use_selector(_who, _item, _pos)
    local new_mode = choose_mode()
    if new_mode == nil then
      gapi.add_msg(MsgType.info, "Lua AI mode selection cancelled.")
      return 0
    end
    local fetch = nil
    local attack_target = ""
    local mine_center = nil
    if new_mode == "fetch" then
      fetch = prompt_fetch_target()
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
      mine_center = prompt_mine_area()
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
      attack_target = prompt_attack_target()
      storage.attack_target = attack_target
    end
    set_nearby_modes(new_mode, fetch, attack_target, mine_center)
    gapi.add_msg(MsgType.info, string.format("Lua AI selector: set nearby debug drones to '%s'.", new_mode))
    return 0
  end

  mod.get_mode = get_mode
  mod.set_mode = set_mode
  mod.spawn_demo_monster = spawn_demo_monster
  mod.set_nearby_modes = set_nearby_modes
  mod.prompt_fetch_target = prompt_fetch_target
  mod.prompt_attack_target = prompt_attack_target
  mod.prompt_mine_area = prompt_mine_area
  mod.use_remote = use_remote
  mod.use_selector = use_selector

  game.iuse_functions["LUA_AI_DEBUG_REMOTE"] = use_remote
  game.iuse_functions["LUA_AI_DEBUG_SELECTOR"] = use_selector
end

---@return nil
function main.register()
  local mod = game.mod_runtime[game.current_mod]
  local storage = game.mod_storage[game.current_mod]
  storage.mode = storage.mode or "attack"
  storage.mine_center = storage.mine_center or ""
  register_ai_behaviors(storage)
  register_runtime_functions(mod, storage)
end

return main

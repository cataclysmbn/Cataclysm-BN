local ai_behaviors = {}

---@class LuaAiDebugContext
---@field storage table
---@field serialize_tripoint_bub_ms fun(pt: TripointBubMs): string
---@field serialize_tripoint_abs_ms fun(pt: TripointAbsMs): string
---@field deserialize_tripoint_bub_ms fun(str: string|nil): TripointBubMs|nil
---@field deserialize_tripoint_abs_ms fun(str: string|nil): TripointAbsMs|nil
---@field safe_step fun(mon: Monster, dest: TripointBubMs): boolean
---@field sign fun(val: integer): integer

---@class LuaAiDebugRuntimeContext: LuaAiDebugContext
---@field mon Monster

---@class LuaAiDebugTurnContext
---@field mon Monster
---@field avatar Avatar
---@field here Map
---@field pos TripointBubMs
---@field target TripointBubMs
---@field sign fun(val: integer): integer

---@class LuaAiModeRoute
---@field label string
---@field run fun(runtime_ctx: LuaAiDebugRuntimeContext, turn_ctx: LuaAiDebugTurnContext): boolean

---@param radius integer
---@return table[]
local function build_mine_offsets(radius)
  local out = {}
  for dx = -radius, radius do
    for dy = -radius, radius do
      out[#out + 1] = { dx, dy }
    end
  end
  return out
end

local mine_offsets = build_mine_offsets(2)

---@param mon Monster
---@param storage table
---@return string
local function get_debug_mode(mon, storage)
  local mode = mon:get_value("lua_ai_mode")
  if mode == "" then return storage.mode or "attack" end
  return mode
end

---@param mon Monster
---@param target Creature|nil
---@return MonsterAttitude
local function lua_attitude_dance(mon, target)
  local effect_hit_by_player = EffectTypeId.new("hit_by_player")
  if mon:get_value("lua_dance_angry") == "1" or mon:has_effect(effect_hit_by_player) then
    mon:set_value("lua_dance_angry", "1")
    if target == nil or target:is_avatar() then return MonsterAttitude.MATT_ATTACK end
  end
  return MonsterAttitude.MATT_IGNORE
end

---@param ctx LuaAiDebugContext|LuaAiDebugRuntimeContext
---@param mon Monster
---@param target Creature|nil
---@return MonsterAttitude
local function lua_attitude_debug_mode(ctx, mon, target)
  local mode = get_debug_mode(mon, ctx.storage)
  if target == nil or mode == "ignore" then return MonsterAttitude.MATT_IGNORE end
  if mode == "follow" or mode == "fetch" then return MonsterAttitude.MATT_FOLLOW end
  return MonsterAttitude.MATT_ATTACK
end

---@param ctx LuaAiDebugTurnContext
---@param dest TripointBubMs
---@param allow_attack boolean
---@return boolean
local function try_step(ctx, dest, allow_attack)
  local occupant = gapi.get_creature_at(dest, true)
  if occupant ~= nil and occupant ~= ctx.mon then
    if allow_attack and (occupant:is_avatar() or occupant:is_npc()) then
      return ctx.mon:move_to(dest, false, true, 1.0)
    end
    return false
  end
  return ctx.mon:move_to(dest, false, false, 1.0)
end

---@param ctx LuaAiDebugTurnContext
---@param dest TripointBubMs
---@param allow_attack boolean
---@return boolean
local function maybe_step_toward(ctx, dest, allow_attack)
  local step =
    TripointBubMs.new(ctx.pos.x + ctx.sign(dest.x - ctx.pos.x), ctx.pos.y + ctx.sign(dest.y - ctx.pos.y), ctx.pos.z)
  ctx.mon:set_target(ctx.avatar)
  if step.x == ctx.pos.x and step.y == ctx.pos.y and step.z == ctx.pos.z then return false end
  return try_step(ctx, step, allow_attack)
end

---@param ctx LuaAiDebugTurnContext
---@param serialize_tripoint_bub_ms fun(pt: TripointBubMs): string
---@param deserialize_tripoint_bub_ms fun(str: string|nil): TripointBubMs|nil
---@return boolean
local function maybe_deliver_carried_items(ctx, serialize_tripoint_bub_ms, deserialize_tripoint_bub_ms)
  local drop_target_str = ctx.mon:get_value("lua_ai_drop_target")
  local drop_target = deserialize_tripoint_bub_ms(drop_target_str)
  local carrying = ctx.mon:get_items()

  if #carrying == 0 then return false end
  if drop_target == nil then
    drop_target = ctx.avatar:get_pos_ms()
    ctx.mon:set_value("lua_ai_drop_target", serialize_tripoint_bub_ms(drop_target))
  end

  local dist = math.max(math.abs(drop_target.x - ctx.pos.x), math.abs(drop_target.y - ctx.pos.y))
  if dist <= 1 then
    for _, item in ipairs(carrying) do
      local detached = ctx.mon:remove_item(item)
      if detached ~= nil then ctx.here:add_item(drop_target, detached) end
    end
    ctx.mon:set_value("lua_ai_drop_target", "")
    ctx.mon:set_value("lua_ai_fetch_target", "")
    ctx.mon:set_value("lua_ai_fetch_item", "")
    ctx.mon:set_value("lua_ai_mode", "follow")
    gapi.add_msg(MsgType.info, "Lua AI debug drone delivered items.")
    return true
  end

  maybe_step_toward(ctx, drop_target, false)
  return true
end

---@param runtime_ctx LuaAiDebugRuntimeContext
---@param ctx LuaAiDebugTurnContext
---@return boolean
local function run_ignore_mode(runtime_ctx, ctx)
  local _ = runtime_ctx
  ctx.mon:mod_moves(-50)
  return true
end

---@param runtime_ctx LuaAiDebugRuntimeContext
---@param ctx LuaAiDebugTurnContext
---@return boolean
local function run_follow_mode(runtime_ctx, ctx)
  local _ = runtime_ctx
  local dist = math.max(math.abs(ctx.target.x - ctx.pos.x), math.abs(ctx.target.y - ctx.pos.y))
  if dist <= 2 then
    ctx.mon:mod_moves(-50)
    return true
  end
  if maybe_step_toward(ctx, ctx.target, false) then return true end
  ctx.mon:mod_moves(-50)
  return true
end

---@param runtime_ctx LuaAiDebugRuntimeContext
---@param ctx LuaAiDebugTurnContext
---@return boolean
local function run_fetch_mode(runtime_ctx, ctx)
  local target_str = ctx.mon:get_value("lua_ai_fetch_target")
  local item_id = ctx.mon:get_value("lua_ai_fetch_item")
  local fetch_pos = runtime_ctx.deserialize_tripoint_bub_ms(target_str)
  if fetch_pos == nil or item_id == "" then
    ctx.mon:set_value("lua_ai_mode", "follow")
    ctx.mon:set_value("lua_ai_fetch_target", "")
    ctx.mon:set_value("lua_ai_fetch_item", "")
    return true
  end

  if fetch_pos.x == ctx.pos.x and fetch_pos.y == ctx.pos.y and fetch_pos.z == ctx.pos.z then
    local stack = ctx.here:get_items_at(fetch_pos)
    local picked = nil
    for _, item in ipairs(stack) do
      if item:get_type():str() == item_id then
        picked = item
        break
      end
    end
    if picked ~= nil then
      local taken = ctx.here:detach_item_at(fetch_pos, picked)
      if taken ~= nil then
        ctx.mon:add_detached_item(taken)
        gapi.add_msg(MsgType.info, "Lua AI debug drone fetched an item.")
        ctx.mon:set_value("lua_ai_mode", "follow")
        ctx.mon:set_value("lua_ai_drop_target", runtime_ctx.serialize_tripoint_bub_ms(ctx.avatar:get_pos_ms()))
        ctx.mon:set_value("lua_ai_fetch_target", "")
        ctx.mon:set_value("lua_ai_fetch_item", "")
        return true
      end
    end
    ctx.mon:set_value("lua_ai_mode", "follow")
    ctx.mon:set_value("lua_ai_fetch_target", "")
    ctx.mon:set_value("lua_ai_fetch_item", "")
    return true
  end

  if maybe_step_toward(ctx, fetch_pos, false) then return true end
  ctx.mon:mod_moves(-50)
  return true
end

---@param runtime_ctx LuaAiDebugRuntimeContext
---@param ctx LuaAiDebugTurnContext
---@return boolean
local function run_mine_mode(runtime_ctx, ctx)
  local center_str = ctx.mon:get_value("lua_ai_mine_center")
  if center_str == "" then center_str = runtime_ctx.storage.mine_center or "" end
  local center_abs = runtime_ctx.deserialize_tripoint_abs_ms(center_str)
  if center_abs == nil then
    ctx.mon:set_value("lua_ai_mode", "follow")
    ctx.mon:set_value("lua_ai_mine_idx", "")
    return true
  end

  local idx = tonumber(ctx.mon:get_value("lua_ai_mine_idx")) or 1
  if idx < 1 then idx = 1 end
  if idx > #mine_offsets then
    ctx.mon:set_value("lua_ai_mode", "follow")
    ctx.mon:set_value("lua_ai_mine_idx", "")
    return true
  end

  -- Persist the mine center in absolute coordinates so the pattern survives map shifts.
  local center_local = ctx.here:get_local_ms(center_abs)
  local map_extent = ctx.here:get_map_size()
  if math.abs(center_local.x - ctx.pos.x) > map_extent or math.abs(center_local.y - ctx.pos.y) > map_extent then
    center_abs = ctx.here:get_abs_ms(ctx.mon:get_pos_ms())
    ctx.mon:set_value("lua_ai_mine_center", runtime_ctx.serialize_tripoint_abs_ms(center_abs))
    center_local = ctx.mon:get_pos_ms()
    idx = 1
  end

  local offset = mine_offsets[idx]
  local target_tile = TripointBubMs.new(center_local.x + offset[1], center_local.y + offset[2], center_local.z)
  local dist = math.max(math.abs(target_tile.x - ctx.pos.x), math.abs(target_tile.y - ctx.pos.y))
  if dist > 1 then
    maybe_step_toward(ctx, target_tile, false)
    return true
  end
  if ctx.mon:bash_at(target_tile) then
    ctx.mon:set_value("lua_ai_mine_idx", tostring(idx + 1))
    return true
  end
  ctx.mon:mod_moves(-30)
  ctx.mon:set_value("lua_ai_mine_idx", tostring(idx + 1))
  return true
end

---@param runtime_ctx LuaAiDebugRuntimeContext
---@param ctx LuaAiDebugTurnContext
---@return boolean
local function run_attack_mode(runtime_ctx, ctx)
  local target = ctx.target
  local atk_str = ctx.mon:get_value("lua_ai_attack_target")
  local atk_pos = runtime_ctx.deserialize_tripoint_bub_ms(atk_str)
  if atk_pos ~= nil then
    if atk_pos.x == ctx.pos.x and atk_pos.y == ctx.pos.y and atk_pos.z == ctx.pos.z then
      ctx.mon:set_value("lua_ai_attack_target", "")
      ctx.mon:set_value("lua_ai_mode", "follow")
      return true
    end
    target = atk_pos
  end
  if math.max(math.abs(target.x - ctx.pos.x), math.abs(target.y - ctx.pos.y)) <= 1 then
    ctx.mon:move_to(target, false, true, 1.0)
    return true
  end
  maybe_step_toward(ctx, target, true)
  return true
end

---@param mon Monster
---@param serialize_tripoint_abs_ms fun(pt: TripointAbsMs): string
---@param deserialize_tripoint_abs_ms fun(str: string|nil): TripointAbsMs|nil
---@param safe_step fun(mon: Monster, dest: TripointBubMs): boolean
---@return boolean
local function run_calm_dance_turn(mon, serialize_tripoint_abs_ms, deserialize_tripoint_abs_ms, safe_step)
  local dance_points = {
    { 1, 0 },
    { 1, 1 },
    { 0, 1 },
    { -1, 1 },
    { -1, 0 },
    { -1, -1 },
    { 0, -1 },
    { 1, -1 },
  }

  local here = gapi.get_map()
  local anchor_abs = deserialize_tripoint_abs_ms(mon:get_value("lua_dance_anchor_abs"))
  if anchor_abs == nil then
    anchor_abs = here:get_abs_ms(mon:get_pos_ms())
    mon:set_value("lua_dance_anchor_abs", serialize_tripoint_abs_ms(anchor_abs))
  end
  if anchor_abs.z ~= mon:get_pos_ms().z then
    anchor_abs = TripointAbsMs.new(anchor_abs.x, anchor_abs.y, mon:get_pos_ms().z)
    mon:set_value("lua_dance_anchor_abs", serialize_tripoint_abs_ms(anchor_abs))
  end

  local idx = tonumber(mon:get_value("lua_dance_index")) or 1
  if idx < 1 or idx > #dance_points then idx = 1 end

  -- The anchor is stored as absolute MS, then projected back into bubble coordinates each turn.
  local anchor_local = here:get_local_ms(anchor_abs)
  local map_extent = here:get_map_size()
  if
    math.abs(anchor_local.x - mon:get_pos_ms().x) > map_extent
    or math.abs(anchor_local.y - mon:get_pos_ms().y) > map_extent
  then
    anchor_abs = here:get_abs_ms(mon:get_pos_ms())
    mon:set_value("lua_dance_anchor_abs", serialize_tripoint_abs_ms(anchor_abs))
    idx = 1
    mon:set_value("lua_dance_index", "1")
    anchor_local = mon:get_pos_ms()
  end

  local offset = dance_points[idx]
  local dest = TripointBubMs.new(anchor_local.x + offset[1], anchor_local.y + offset[2], anchor_local.z)

  if not safe_step(mon, dest) then
    local block = gapi.get_creature_at(dest, true)
    if block ~= nil and block:is_avatar() then
      mon:mod_moves(-50)
      return true
    end
    mon:mod_moves(-30)
    mon:set_value("lua_dance_index", tostring((idx % #dance_points) + 1))
    return true
  end

  mon:set_value("lua_dance_index", tostring((idx % #dance_points) + 1))
  mon:set_value("lua_dance_anchor_abs", serialize_tripoint_abs_ms(anchor_abs))
  if gapi.rng(1, 4) == 1 then
    gapi.play_variant_sound("music", "dancer", 30)
  else
    gapi.add_msg(MsgType.info, "♪ beep boop dance ♪")
  end
  return true
end

---@param mon Monster
---@return boolean
local function run_angry_dance_turn(mon)
  -- Once angered, the dance bot deliberately delegates the whole turn to stock AI.
  mon:set_value("lua_dance_angry", "1")
  mon:run_normal_ai_turn()
  return true
end

---@type table<string, LuaAiModeRoute>
local mode_routes = {
  ignore = {
    label = "Spend the turn idling in place.",
    run = run_ignore_mode,
  },
  follow = {
    label = "Stay close to the avatar without attacking.",
    run = run_follow_mode,
  },
  fetch = {
    label = "Retrieve the requested item and bring it back.",
    run = run_fetch_mode,
  },
  mine = {
    label = "Walk a fixed pattern and bash tiles around the stored center.",
    run = run_mine_mode,
  },
  attack = {
    label = "Approach the current target and attack if adjacent.",
    run = run_attack_mode,
  },
}

---@param ctx LuaAiDebugRuntimeContext
---@param available_mode_routes table<string, LuaAiModeRoute>
---@return boolean
local function run_debug_seek_turn(ctx, available_mode_routes)
  local avatar = gapi.get_avatar()
  if avatar == nil then
    ctx.mon:mod_moves(-100)
    return true
  end

  ---@type LuaAiDebugTurnContext
  local turn_ctx = {
    mon = ctx.mon,
    avatar = avatar,
    here = gapi.get_map(),
    pos = ctx.mon:get_pos_ms(),
    target = avatar:get_pos_ms(),
    sign = ctx.sign,
  }

  if maybe_deliver_carried_items(turn_ctx, ctx.serialize_tripoint_bub_ms, ctx.deserialize_tripoint_bub_ms) then
    return true
  end

  local mode = get_debug_mode(ctx.mon, ctx.storage)
  -- Unknown modes fall back to attack so broken saved state still produces visible behavior.
  local route = available_mode_routes[mode] or available_mode_routes.attack
  return route.run(ctx, turn_ctx)
end

---@param ctx LuaAiDebugContext
---@return nil
function ai_behaviors.register(ctx)
  local effect_hit_by_player = EffectTypeId.new("hit_by_player")

  -- Register named attitude callbacks by string id. Monster JSON can point at these
  -- ids directly through `lua_attitude`.
  game.monster_attitude_functions["lua_attitude_debug_mode"] =
    function(mon, target) return lua_attitude_debug_mode(ctx, mon, target) end
  game.monster_attitude_functions["lua_attitude_dance"] = lua_attitude_dance

  -- Register the named Lua AI callback used by the debug bot's monster JSON via
  -- `lua_ai`. The per-turn handler then selects a mode entry from `mode_routes`.
  game.monster_ai_functions["lua_ai_debug_seek"] = function(mon)
    ---@type LuaAiDebugRuntimeContext
    local runtime_ctx = {
      storage = ctx.storage,
      serialize_tripoint_bub_ms = ctx.serialize_tripoint_bub_ms,
      serialize_tripoint_abs_ms = ctx.serialize_tripoint_abs_ms,
      deserialize_tripoint_bub_ms = ctx.deserialize_tripoint_bub_ms,
      deserialize_tripoint_abs_ms = ctx.deserialize_tripoint_abs_ms,
      safe_step = ctx.safe_step,
      sign = ctx.sign,
      mon = mon,
    }
    return run_debug_seek_turn(runtime_ctx, mode_routes)
  end

  -- Register the dancer's named Lua AI entry point. This is separate from the
  -- debug bot so the monster JSON can opt into a completely different turn script.
  game.monster_ai_functions["lua_ai_dance"] = function(mon)
    if
      mon:get_value("lua_dance_angry") == "1"
      or mon:has_effect(effect_hit_by_player)
      or mon:get_hp() < mon:get_hp_max()
    then
      return run_angry_dance_turn(mon)
    end
    return run_calm_dance_turn(mon, ctx.serialize_tripoint_abs_ms, ctx.deserialize_tripoint_abs_ms, ctx.safe_step)
  end
end

return ai_behaviors

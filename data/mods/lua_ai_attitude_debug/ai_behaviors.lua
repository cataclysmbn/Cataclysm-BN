local ai_behaviors = {}

---@class LuaAiDebugContext
---@field storage table
---@field serialize_tripoint fun(pt: Tripoint): string
---@field deserialize_tripoint fun(str: string|nil): Tripoint|nil
---@field safe_step fun(mon: Monster, dest: Tripoint): boolean
---@field sign fun(val: integer): integer

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

---@param ctx LuaAiDebugContext
---@return nil
function ai_behaviors.register(ctx)
  local storage = ctx.storage
  local serialize_tripoint = ctx.serialize_tripoint
  local deserialize_tripoint = ctx.deserialize_tripoint
  local safe_step = ctx.safe_step
  local sign = ctx.sign
  local effect_hit_by_player = EffectTypeId.new("hit_by_player")

  game.monster_attitude_functions["lua_attitude_debug_mode"] = function(_mon, target)
    local mode = _mon:get_value("lua_ai_mode")
    if mode == "" then mode = storage.mode or "attack" end
    if target == nil then return MonsterAttitude.MATT_IGNORE end
    if mode == "ignore" then return MonsterAttitude.MATT_IGNORE end
    if mode == "follow" or mode == "fetch" then return MonsterAttitude.MATT_FOLLOW end
    return MonsterAttitude.MATT_ATTACK
  end

  game.monster_attitude_functions["lua_attitude_dance"] = function(_mon, target)
    if _mon:get_value("lua_dance_angry") == "1" or _mon:has_effect(effect_hit_by_player) then
      _mon:set_value("lua_dance_angry", "1")
      if target == nil then return MonsterAttitude.MATT_ATTACK end
      if target:is_avatar() then return MonsterAttitude.MATT_ATTACK end
      return MonsterAttitude.MATT_IGNORE
    end
    return MonsterAttitude.MATT_IGNORE
  end

  game.monster_attitude_router = function(_mon, target)
    local type_id = _mon:get_type():str()
    if type_id == "mon_lua_ai_debug" then
      return game.monster_attitude_functions["lua_attitude_debug_mode"](_mon, target)
    end
    if type_id == "mon_lua_dancer" then return game.monster_attitude_functions["lua_attitude_dance"](_mon, target) end
    if type_id == "mon_lua_hoarder" then
      return game.monster_attitude_functions["lua_attitude_hoarder"](_mon, target)
    end
    return nil
  end

  game.monster_ai_functions["lua_ai_debug_seek"] = function(mon)
    local avatar = gapi.get_avatar()
    if avatar == nil then
      mon:mod_moves(-100)
      return true
    end
    local mode = mon:get_value("lua_ai_mode")
    if mode == "" then mode = storage.mode or "attack" end
    local pos = mon:get_pos_ms()
    local target = avatar:get_pos_ms()
    local here = gapi.get_map()
    local function try_step(dest, allow_attack)
      local occupant = gapi.get_creature_at(dest, true)
      if occupant ~= nil and occupant ~= mon then
        if allow_attack and (occupant:is_avatar() or occupant:is_npc()) then
          return mon:move_to(dest, false, true, 1.0)
        end
        return false
      end
      return mon:move_to(dest, false, false, 1.0)
    end

    local function maybe_step_toward(dest, allow_attack)
      local step = Tripoint.new(pos.x + sign(dest.x - pos.x), pos.y + sign(dest.y - pos.y), pos.z)
      mon:set_target(avatar)
      if step.x == pos.x and step.y == pos.y and step.z == pos.z then return false end
      return try_step(step, allow_attack)
    end
    local drop_target_str = mon:get_value("lua_ai_drop_target")
    local drop_target = deserialize_tripoint(drop_target_str)
    local carrying = mon:get_items()

    if #carrying > 0 and drop_target == nil then
      drop_target = avatar:get_pos_ms()
      mon:set_value("lua_ai_drop_target", serialize_tripoint(drop_target))
    end

    if #carrying > 0 and drop_target ~= nil then
      local dist = math.max(math.abs(drop_target.x - pos.x), math.abs(drop_target.y - pos.y))
      if dist <= 1 then
        for _, it in ipairs(carrying) do
          local detached = mon:remove_item(it)
          if detached ~= nil then here:add_item(drop_target, detached) end
        end
        mon:set_value("lua_ai_drop_target", "")
        mon:set_value("lua_ai_fetch_target", "")
        mon:set_value("lua_ai_fetch_item", "")
        mon:set_value("lua_ai_mode", "follow")
        gapi.add_msg(MsgType.info, "Lua AI debug drone delivered items.")
        return true
      else
        maybe_step_toward(drop_target, false)
        return true
      end
    end

    if mode == "ignore" then
      mon:mod_moves(-50)
      return true
    end

    if mode == "follow" then
      local dist = math.max(math.abs(target.x - pos.x), math.abs(target.y - pos.y))
      if dist <= 2 then
        mon:mod_moves(-50)
        return true
      end
      if maybe_step_toward(target, false) then return true end
      mon:mod_moves(-50)
      return true
    end

    if mode == "fetch" then
      local target_str = mon:get_value("lua_ai_fetch_target")
      local item_id = mon:get_value("lua_ai_fetch_item")
      local fetch_pos = deserialize_tripoint(target_str)
      if fetch_pos == nil or item_id == "" then
        mon:set_value("lua_ai_mode", "follow")
        mon:set_value("lua_ai_fetch_target", "")
        mon:set_value("lua_ai_fetch_item", "")
        return true
      end
      if fetch_pos.x == pos.x and fetch_pos.y == pos.y and fetch_pos.z == pos.z then
        local stack = here:get_items_at(fetch_pos)
        local picked = nil
        for _, it in ipairs(stack) do
          if it:get_type():str() == item_id then
            picked = it
            break
          end
        end
        if picked ~= nil then
          local taken = here:detach_item_at(fetch_pos, picked)
          if taken ~= nil then
            mon:add_detached_item(taken)
            gapi.add_msg(MsgType.info, "Lua AI debug drone fetched an item.")
            mon:set_value("lua_ai_mode", "follow")
            mon:set_value("lua_ai_drop_target", serialize_tripoint(avatar:get_pos_ms()))
            mon:set_value("lua_ai_fetch_target", "")
            mon:set_value("lua_ai_fetch_item", "")
            return true
          end
        end
        mon:set_value("lua_ai_mode", "follow")
        mon:set_value("lua_ai_fetch_target", "")
        mon:set_value("lua_ai_fetch_item", "")
        return true
      end
      if maybe_step_toward(fetch_pos, false) then return true end
      mon:mod_moves(-50)
      return true
    end

    if mode == "mine" then
      local center_str = mon:get_value("lua_ai_mine_center")
      if center_str == "" then center_str = storage.mine_center or "" end
      local center_abs = deserialize_tripoint(center_str)
      if center_abs == nil then
        mon:set_value("lua_ai_mode", "follow")
        mon:set_value("lua_ai_mine_idx", "")
        return true
      end
      local idx = tonumber(mon:get_value("lua_ai_mine_idx")) or 1
      if idx < 1 then idx = 1 end
      if idx > #mine_offsets then
        mon:set_value("lua_ai_mode", "follow")
        mon:set_value("lua_ai_mine_idx", "")
        return true
      end
      local center_local = here:get_local_ms(center_abs)
      local map_extent = here:get_map_size()
      if math.abs(center_local.x - pos.x) > map_extent or math.abs(center_local.y - pos.y) > map_extent then
        center_abs = here:get_abs_ms(mon:get_pos_ms())
        mon:set_value("lua_ai_mine_center", serialize_tripoint(center_abs))
        center_local = mon:get_pos_ms()
        idx = 1
      end
      local offset = mine_offsets[idx]
      local target_tile = Tripoint.new(center_local.x + offset[1], center_local.y + offset[2], center_abs.z)
      local dist = math.max(math.abs(target_tile.x - pos.x), math.abs(target_tile.y - pos.y))
      if dist > 1 then
        maybe_step_toward(target_tile, false)
        return true
      end
      if mon:bash_at(target_tile) then
        mon:set_value("lua_ai_mine_idx", tostring(idx + 1))
        return true
      end
      mon:mod_moves(-30)
      mon:set_value("lua_ai_mine_idx", tostring(idx + 1))
      return true
    end

    if mode == "attack" then
      local atk_str = mon:get_value("lua_ai_attack_target")
      local atk_pos = deserialize_tripoint(atk_str)
      if atk_pos ~= nil then
        if atk_pos.x == pos.x and atk_pos.y == pos.y and atk_pos.z == pos.z then
          mon:set_value("lua_ai_attack_target", "")
          mon:set_value("lua_ai_mode", "follow")
          return true
        end
        target = atk_pos
      end
    end
    if math.max(math.abs(target.x - pos.x), math.abs(target.y - pos.y)) <= 1 then
      mon:move_to(target, false, true, 1.0)
      return true
    end
    maybe_step_toward(target, true)
    return true
  end

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

  game.monster_ai_functions["lua_ai_dance"] = function(mon)
    if
      mon:get_value("lua_dance_angry") == "1"
      or mon:has_effect(effect_hit_by_player)
      or mon:get_hp() < mon:get_hp_max()
    then
      mon:set_value("lua_dance_angry", "1")
      mon:run_normal_ai_turn()
      return true
    end

    local here = gapi.get_map()
    local anchor_abs = deserialize_tripoint(mon:get_value("lua_dance_anchor_abs"))
    if anchor_abs == nil then
      anchor_abs = here:get_abs_ms(mon:get_pos_ms())
      mon:set_value("lua_dance_anchor_abs", serialize_tripoint(anchor_abs))
    end
    if anchor_abs.z ~= mon:get_pos_ms().z then
      anchor_abs = Tripoint.new(anchor_abs.x, anchor_abs.y, mon:get_pos_ms().z)
      mon:set_value("lua_dance_anchor_abs", serialize_tripoint(anchor_abs))
    end
    local idx = tonumber(mon:get_value("lua_dance_index")) or 1
    if idx < 1 or idx > #dance_points then idx = 1 end
    local anchor_local = here:get_local_ms(anchor_abs)
    local map_extent = here:get_map_size()
    if
      math.abs(anchor_local.x - mon:get_pos_ms().x) > map_extent
      or math.abs(anchor_local.y - mon:get_pos_ms().y) > map_extent
    then
      anchor_abs = here:get_abs_ms(mon:get_pos_ms())
      mon:set_value("lua_dance_anchor_abs", serialize_tripoint(anchor_abs))
      idx = 1
      mon:set_value("lua_dance_index", "1")
      anchor_local = mon:get_pos_ms()
    end
    local offset = dance_points[idx]
    local dest = Tripoint.new(anchor_local.x + offset[1], anchor_local.y + offset[2], anchor_local.z)

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
    mon:set_value("lua_dance_anchor_abs", serialize_tripoint(anchor_abs))
    if gapi.rng(1, 4) == 1 then
      gapi.play_variant_sound("music", "dancer", 30)
    else
      gapi.add_msg(MsgType.info, "♪ beep boop dance ♪")
    end
    return true
  end

  -- Remote control was removed due to instability.
end

return ai_behaviors

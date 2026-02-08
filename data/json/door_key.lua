local door_key = {}

local function get_selected_tile(prompt) return gapi.choose_adjacent(prompt, false) end

local function get_ter_str(map, pos)
  local ter = map:get_ter_at(pos)
  return ter:str_id():str()
end

local function get_locked_ter_id(closed_ter_str, prefer_interior)
  local base = nil
  local suffix = nil
  if string.sub(closed_ter_str, -2) == "_c" then
    base = string.sub(closed_ter_str, 1, -3)
    suffix = "_c"
  elseif string.sub(closed_ter_str, -7) == "_c_peep" then
    base = string.sub(closed_ter_str, 1, -8)
    suffix = "_c_peep"
  end

  if base == nil then
    return nil
  end

  local function try_locked(locked_suffix)
    local locked_ter_str = base .. locked_suffix
    local locked_ter = TerId.new(locked_ter_str)
    if locked_ter:is_valid() then
      return locked_ter:int_id(), locked_ter_str
    end
    return nil
  end

  if prefer_interior then
    local interior_suffix = suffix == "_c_peep" and "_locked_interior_peep" or "_locked_interior"
    local interior_id, interior_str = try_locked(interior_suffix)
    if interior_id ~= nil then
      return interior_id, interior_str
    end
  end

  local regular_suffix = suffix == "_c_peep" and "_locked_peep" or "_locked"
  return try_locked(regular_suffix)
end

local function get_open_ter_id(closed_ter_str)
  if string.sub(closed_ter_str, -2) == "_c" then
    local open_ter_str = string.sub(closed_ter_str, 1, -3) .. "_o"
    local open_ter = TerId.new(open_ter_str)
    if not open_ter:is_valid() then return nil end

    return open_ter:int_id(), open_ter_str
  end

  if string.sub(closed_ter_str, -7) == "_c_peep" then
    local open_ter_str = string.sub(closed_ter_str, 1, -8) .. "_o_peep"
    local open_ter = TerId.new(open_ter_str)
    if not open_ter:is_valid() then return nil end

    return open_ter:int_id(), open_ter_str
  end

  return nil
end

local function get_closed_ter_id(open_ter_str)
  if string.sub(open_ter_str, -2) == "_o" then
    local closed_ter_str = string.sub(open_ter_str, 1, -3) .. "_c"
    local closed_ter = TerId.new(closed_ter_str)
    if not closed_ter:is_valid() then return nil end

    return closed_ter:int_id(), closed_ter_str
  end

  if string.sub(open_ter_str, -7) == "_o_peep" then
    local closed_ter_str = string.sub(open_ter_str, 1, -8) .. "_c_peep"
    local closed_ter = TerId.new(closed_ter_str)
    if not closed_ter:is_valid() then return nil end

    return closed_ter:int_id(), closed_ter_str
  end

  return nil
end

local function set_key_binding(item, target_type, abs_pos, closed_ter_str, locked_ter_str, open_ter_str)
  item:set_var_tri("door_key_location", abs_pos)
  item:set_var_str("door_key_closed_ter", closed_ter_str)
  item:set_var_str("door_key_locked_ter", locked_ter_str)
  if open_ter_str ~= nil then item:set_var_str("door_key_open_ter", open_ter_str) end
  item:set_var_str("door_key_target_type", target_type)
end

local safe_variants = {
  open = "f_safe_o",
  closed = "f_safe_c",
  locked = "f_safe_l",
}

local function is_safe_furn(furn_str)
  return furn_str == safe_variants.open
      or furn_str == safe_variants.closed
      or furn_str == safe_variants.locked
end

local function get_furn_str(map, pos)
  local furn = map:get_furn_at(pos)
  if not furn:is_valid() then
    return nil
  end
  return furn:str_id():str()
end

local function get_target_str(map, pos, target_type)
  if target_type == "furn" then
    return get_furn_str(map, pos)
  end
  return get_ter_str(map, pos)
end

local function get_target_id(target_type, id_str)
  if target_type == "furn" then
    local furn = FurnId.new(id_str)
    if not furn:is_valid() then
      return nil
    end
    return furn:int_id()
  end
  local ter = TerId.new(id_str)
  if not ter:is_valid() then
    return nil
  end
  return ter:int_id()
end

local function set_target(map, pos, target_type, id_str)
  local id = get_target_id(target_type, id_str)
  if id == nil then
    return false
  end
  if target_type == "furn" then
    map:set_furn_at(pos, id)
    return get_furn_str(map, pos) == id_str
  end
  return map:set_ter_at(pos, id)
end

local function describe_lock_message(target_type, closed_from_open)
  if target_type == "furn" then
    return closed_from_open
        and locale.gettext("You close and lock the safe, then set the key.")
        or locale.gettext("You lock the safe and set the key.")
  end
  return closed_from_open
      and locale.gettext("You close and lock the door, then set the key.")
      or locale.gettext("You lock the door and set the key.")
end

local function describe_unlock_message(target_type)
  if target_type == "furn" then
    return locale.gettext("You unlock the safe.")
  end
  return locale.gettext("You unlock the door.")
end

local function describe_close_message(target_type)
  if target_type == "furn" then
    return locale.gettext("You close the safe.")
  end
  return locale.gettext("You close the door.")
end

local function describe_lock_action_message(target_type)
  if target_type == "furn" then
    return locale.gettext("You lock the safe.")
  end
  return locale.gettext("You lock the door.")
end

local function prompt_key_label()
  local input = PopupInputStr.new()
  input:title(locale.gettext("Key label (ex: 'house key'): "))
  local label = input:query_str()
  if label == "" then return nil end

  return label
end

local function get_key_binding(item)
  if not item:has_var("door_key_location") then return nil end

  local stored_pos = item:get_var_tri("door_key_location", Tripoint.new())
  local closed_ter_str = item:get_var_str("door_key_closed_ter", "")
  local locked_ter_str = item:get_var_str("door_key_locked_ter", "")
  local open_ter_str = item:get_var_str("door_key_open_ter", "")
  if closed_ter_str == "" or locked_ter_str == "" then return nil end

  return {
    pos = stored_pos,
    closed_ter = closed_ter_str,
    locked_ter = locked_ter_str,
    open_ter = open_ter_str,
    target = item:get_var_str("door_key_target_type", "terrain"),
  }
end

local function select_key_to_copy(who)
  local items = who:all_items(true)
  local key_items = {}
  local menu = UiList.new()
  menu:title(locale.gettext("Copy which key?"))
  for i = 1, #items do
    local candidate = items[i]
    if candidate:get_type():str() == "door_key" then
      local binding = get_key_binding(candidate)
      if binding ~= nil then
        table.insert(key_items, candidate)
        menu:add(#key_items, candidate:display_name(1))
      end
    end
  end

  if #key_items == 0 then return nil end

  local choice = menu:query()
  if choice <= 0 then return nil end

  return key_items[choice]
end

---@type fun(who: Character, item: Item, pos: Tripoint): integer
function door_key.copy_key(who, item, pos)
  local source_key = select_key_to_copy(who)
  if source_key == nil then
    gapi.add_msg(locale.gettext("You have no set keys to copy."))
    return 0
  end

  local binding = get_key_binding(source_key)
  if binding == nil then
    gapi.add_msg(locale.gettext("That key cannot be copied."))
    return 0
  end

  local key = who:create_item(ItypeId.new("door_key"), 0)
  set_key_binding(key, binding.target or "terrain", binding.pos, binding.closed_ter, binding.locked_ter, binding.open_ter)

  local label = prompt_key_label()
  if label ~= nil then
    key:set_var_str("item_label", label)
  elseif source_key:has_var("item_label") then
    local source_label = source_key:get_var_str("item_label", "")
    if source_label ~= "" then key:set_var_str("item_label", source_label) end
  end

  if who:is_wielding(item) then who:unwield() end
  who:remove_item(item)

  gapi.add_msg(locale.gettext("You copy the key."))
  return 0
end

---@type fun(who: Character, item: Item, pos: Tripoint): integer
function door_key.set_lock(who, item, pos)
  local map = gapi.get_map()
  local player_pos = map:get_local_ms(who:get_pos_ms())
  if not map:is_outside(player_pos) then
    gapi.add_msg(locale.gettext("You can only install locks while outside."))
    return 0
  end

  local chosen = get_selected_tile(locale.gettext("Select a door to lock."))
  if chosen == nil then return 0 end

  local abs_pos = map:get_abs_ms(chosen)
  local target_type = "terrain"
  local closed_ter_str = nil
  local locked_ter_str = nil
  local open_ter_str = nil
  local closed_from_open = false

  local furn_str = get_target_str(map, chosen, "furn")
  if furn_str ~= nil and is_safe_furn(furn_str) then
    target_type = "furn"
    closed_ter_str = safe_variants.closed
    locked_ter_str = safe_variants.locked
    open_ter_str = safe_variants.open
    if furn_str == open_ter_str then
      closed_from_open = true
      set_target(map, chosen, target_type, closed_ter_str)
    end

    if set_target(map, chosen, target_type, locked_ter_str) then
      local key = who:create_item(ItypeId.new("door_key"), 1)
      set_key_binding(key, target_type, abs_pos, closed_ter_str, locked_ter_str, open_ter_str)
      local label = prompt_key_label()
      if label ~= nil then key:set_var_str("item_label", label) end

      if who:is_wielding(item) then who:unwield() end
      who:remove_item(item)

      gapi.add_msg(describe_lock_message(target_type, closed_from_open))
    else
      gapi.add_msg(locale.gettext("You cannot lock that door."))
    end
    return 0
  end

  local ter_str = get_target_str(map, chosen, "terrain")
  closed_ter_str = ter_str
  local closed_ter_id = nil
  if string.sub(ter_str, -2) == "_o" or string.sub(ter_str, -7) == "_o_peep" then
    closed_ter_id, closed_ter_str = get_closed_ter_id(ter_str)
    closed_from_open = closed_ter_id ~= nil
  end

  local prefer_interior = not map:is_outside(chosen)
  local locked_ter_id, locked_ter_str = get_locked_ter_id(closed_ter_str, prefer_interior)
  if locked_ter_id == nil then
    gapi.add_msg(locale.gettext("That door cannot be locked with this lock."))
    return 0
  end

  open_ter_str = select(2, get_open_ter_id(closed_ter_str))

  if closed_from_open and closed_ter_id ~= nil then
    map:set_ter_at(chosen, closed_ter_id)
  end

  if map:set_ter_at(chosen, locked_ter_id) then
    local key = who:create_item(ItypeId.new("door_key"), 1)
    set_key_binding(key, target_type, abs_pos, closed_ter_str, locked_ter_str, open_ter_str)
    local label = prompt_key_label()
    if label ~= nil then key:set_var_str("item_label", label) end

    if who:is_wielding(item) then who:unwield() end
    who:remove_item(item)

    gapi.add_msg(describe_lock_message(target_type, closed_from_open))
  else
    gapi.add_msg(locale.gettext("You cannot lock that door."))
  end

  return 0
end

---@type fun(who: Character, item: Item, pos: Tripoint): integer
function door_key.menu(who, item, pos)
  local map = gapi.get_map()
  local binding = get_key_binding(item)

  if binding == nil then
    gapi.add_msg(locale.gettext("This key has not been set to a door."))
    return 0
  end

  local chosen = get_selected_tile(locale.gettext("Select the door to use the key on."))
  if chosen == nil then return 0 end

  local abs_pos = map:get_abs_ms(chosen)
  if abs_pos.x ~= binding.pos.x or abs_pos.y ~= binding.pos.y or abs_pos.z ~= binding.pos.z then
    gapi.add_msg(locale.gettext("The key does not fit that door."))
    return 0
  end

  local target_type = binding.target or "terrain"
  local current_str = get_target_str(map, chosen, target_type)
  if current_str == binding.locked_ter then
    if set_target(map, chosen, target_type, binding.closed_ter) then
      gapi.add_msg(describe_unlock_message(target_type))
    else
      gapi.add_msg(locale.gettext("The key gets stuck and nothing happens."))
    end
    return 0
  end

  if binding.open_ter ~= "" and current_str == binding.open_ter then
    if set_target(map, chosen, target_type, binding.closed_ter) then
      gapi.add_msg(describe_close_message(target_type))
    else
      gapi.add_msg(locale.gettext("The key gets stuck and nothing happens."))
    end
    return 0
  end

  if current_str == binding.closed_ter then
    if set_target(map, chosen, target_type, binding.locked_ter) then
      gapi.add_msg(describe_lock_action_message(target_type))
    else
      gapi.add_msg(locale.gettext("The key gets stuck and nothing happens."))
    end
    return 0
  end

  gapi.add_msg(locale.gettext("That door is not compatible with this key."))
  return 0
end

return door_key

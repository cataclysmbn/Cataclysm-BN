local artifact_compass = {}

artifact_compass.item_id = "artifact_compass"
artifact_compass.var_enabled = "artifact_compass_on"
artifact_compass.var_next_beep = "artifact_compass_next_beep"
artifact_compass.var_next_drain = "artifact_compass_next_drain"
artifact_compass.scan_radius = 60
artifact_compass.drain_interval_turns = 20

artifact_compass.is_compass_on = function(item)
  return item:get_var_str(artifact_compass.var_enabled, "0") == "1"
end

artifact_compass.set_compass_state = function(item, on)
  if on then
    item:set_var_str(artifact_compass.var_enabled, "1")
  else
    item:set_var_str(artifact_compass.var_enabled, "0")
  end
end

artifact_compass.use = function(who, item, pos)
  local item_name = item:tname(1, false, 0)
  if artifact_compass.is_compass_on(item) then
    artifact_compass.set_compass_state(item, false)
    item:set_var_num(artifact_compass.var_next_beep, 0)
    item:set_var_num(artifact_compass.var_next_drain, 0)
    gapi.add_msg(
      MsgType.neutral,
      string.format(locale.gettext("You switch off the %s."), item_name)
    )
  else
    local now = (gapi.current_turn() - gapi.turn_zero()):to_turns()
    artifact_compass.set_compass_state(item, true)
    item:set_var_num(artifact_compass.var_next_drain, now + artifact_compass.drain_interval_turns)
    gapi.add_msg(
      MsgType.neutral,
      string.format(locale.gettext("You switch on the %s."), item_name)
    )
  end
  return 0
end

artifact_compass.get_active_compasses = function(who)
  local active = {}
  for _, item in pairs(who:all_items(false)) do
    if item:get_type():str() == artifact_compass.item_id and artifact_compass.is_compass_on(item) then
      table.insert(active, item)
    end
  end
  return active
end

artifact_compass.find_nearest_artifact_distance = function(map, origin)
  local radius = artifact_compass.scan_radius
  local map_size = map:get_map_size()
  local min_x = math.max(0, origin.x - radius)
  local max_x = math.min(map_size - 1, origin.x + radius)
  local min_y = math.max(0, origin.y - radius)
  local max_y = math.min(map_size - 1, origin.y + radius)
  local nearest = nil

  for y = min_y, max_y do
    for x = min_x, max_x do
      local pos = Tripoint.new(x, y, origin.z)
      if map:has_items_at(pos) then
        local items = map:get_items_at(pos):as_item_stack()
        for _, item in pairs(items) do
          if item:is_artifact() then
            local dist = coords.rl_dist(origin, pos)
            if nearest == nil or dist < nearest then
              nearest = dist
              if nearest <= 1 then
                return nearest
              end
            end
          end
        end
      end
    end
  end

  return nearest
end

artifact_compass.get_beep_interval_turns = function(distance)
  if distance <= 1 then return 1 end
  if distance <= 3 then return 2 end
  if distance <= 6 then return 3 end
  if distance <= 10 then return 4 end
  if distance <= 15 then return 6 end
  if distance <= 25 then return 8 end
  if distance <= 40 then return 12 end
  return 18
end

artifact_compass.get_beep_message = function(distance)
  if distance <= 6 then
    return locale.gettext("BEEP-BEEP-BEEP")
  end
  if distance <= 15 then
    return locale.gettext("BEEP-BEEP")
  end
  return locale.gettext("BEEP")
end

artifact_compass.drain_compass = function(item, now, pos_ms)
  local next_drain = item:get_var_num(artifact_compass.var_next_drain, 0)
  if now < next_drain then
    return true
  end

  if item:has_infinite_charges() then
    item:set_var_num(artifact_compass.var_next_drain, now + artifact_compass.drain_interval_turns)
    return true
  end

  if item:ammo_remaining() <= 0 then
    local item_name = item:tname(1, false, 0)
    artifact_compass.set_compass_state(item, false)
    item:set_var_num(artifact_compass.var_next_beep, 0)
    item:set_var_num(artifact_compass.var_next_drain, 0)
    gapi.add_msg(
      MsgType.neutral,
      string.format(locale.gettext("Your %s runs out of power and switches off."), item_name)
    )
    return false
  end

  item:ammo_consume(1, pos_ms)
  item:set_var_num(artifact_compass.var_next_drain, now + artifact_compass.drain_interval_turns)
  if item:ammo_remaining() <= 0 then
    local item_name = item:tname(1, false, 0)
    artifact_compass.set_compass_state(item, false)
    item:set_var_num(artifact_compass.var_next_beep, 0)
    item:set_var_num(artifact_compass.var_next_drain, 0)
    gapi.add_msg(
      MsgType.neutral,
      string.format(locale.gettext("Your %s runs out of power and switches off."), item_name)
    )
    return false
  end

  return true
end

artifact_compass.on_every_x = function()
  local who = gapi.get_avatar()
  local active = artifact_compass.get_active_compasses(who)
  if #active == 0 then
    return
  end

  local now = (gapi.current_turn() - gapi.turn_zero()):to_turns()
  local pos_ms = who:get_pos_ms()
  local drained_active = {}
  for _, item in pairs(active) do
    if artifact_compass.drain_compass(item, now, pos_ms) then
      table.insert(drained_active, item)
    end
  end
  if #drained_active == 0 then
    return
  end

  local map = gapi.get_map()
  local origin = who:get_pos_ms()
  local nearest = artifact_compass.find_nearest_artifact_distance(map, origin)
  if nearest == nil then
    return
  end

  local interval = artifact_compass.get_beep_interval_turns(nearest)
  local message = artifact_compass.get_beep_message(nearest)
  for _, item in pairs(drained_active) do
    local next_beep = item:get_var_num(artifact_compass.var_next_beep, 0)
    if now >= next_beep then
      local item_name = item:tname(1, false, 0)
      gapi.add_msg(
        MsgType.neutral,
        string.format(locale.gettext("You %s emits a %s!"), item_name, message)
      )
      item:set_var_num(artifact_compass.var_next_beep, now + interval)
    end
  end
end

return artifact_compass

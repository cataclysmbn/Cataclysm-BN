local ui = require("lib.ui")
local survivor_radio = {}
local current_mod_id

local function color_text(text, color)
  return string.format("<color_%s>%s</color>", color, text)
end

local function add_line(lines, label, value, value_color)
  local label_clean = string.gsub(label, ":$", "")
  local value_tint = value_color or "white"
  table.insert(
    lines,
    color_text(label_clean, "green") .. color_text(":", "dark_gray") .. " " .. color_text(value, value_tint)
  )
end

local speech_baseline = 2

local function get_audience_name(entry)
  if entry.display_name then return entry.display_name end
  if entry.faction_id then return entry.faction_id end
  return locale.gettext("unknown")
end

local function now_turn()
  return gapi.current_turn():to_turn()
end

local default_stay_hours = 6
local cleanup_retry_turns = TimeDuration.from_minutes(10):to_turns()
local cleanup_departure_distance = 60
local spawn_retry_turns = TimeDuration.from_minutes(30):to_turns()

local function debug_log(msg)
  gdebug.log_info(string.format("survivor_radio: %s", msg))
end

local function pos_to_table(pos)
  return { x = pos.x, y = pos.y, z = pos.z }
end

local function pos_from_table(pos)
  if not pos then return nil end
  if pos.x == nil or pos.y == nil or pos.z == nil then return nil end
  return Tripoint.new(pos.x, pos.y, pos.z)
end

local function signal_definitions()
  return {
    {
      id = "distress",
      name = locale.gettext("Distress call"),
      desc = locale.gettext("Ask nearby survivors for help and protection."),
      audiences = {
        {
          faction_id = "wasteland_scavengers",
          display_name = locale.gettext("Wasteland scavengers"),
          npc_templates = { "radio_wandering_scavenger" },
          base_chance = 35,
          spawn_min_npcs = 1,
          spawn_max_npcs = 2,
          spawn_radius = 60,
          stay_hours = 6,
          is_primary = true,
        },
        {
          faction_id = "free_merchants",
          display_name = locale.gettext("Free Merchants"),
          npc_templates = { "radio_merchant", "radio_merchant", "radio_merchant", "radio_wandering_scavenger" },
          base_chance = 25,
          spawn_min_npcs = 2,
          spawn_max_npcs = 3,
          spawn_radius = 60,
          stay_hours = 6,
          is_primary = false,
        },
        {
          faction_id = "hells_raiders",
          display_name = locale.gettext("Hell's Raiders"),
          npc_templates = { "radio_bandit", "radio_thug" },
          base_chance = 10,
          spawn_min_npcs = 1,
          spawn_max_npcs = 2,
          spawn_radius = 60,
          stay_hours = 12,
          make_angry = true,
          is_primary = false,
        },
      },
      duration_hours = 6,
      range = 60,
      start_msg = locale.gettext("You broadcast a distress call on the survivor band."),
    },
    {
      id = "trade",
      name = locale.gettext("Trade offer"),
      desc = locale.gettext("Advertise supplies and request a meeting."),
      audiences = {
        {
          faction_id = "free_merchants",
          display_name = locale.gettext("Free Merchants"),
          npc_templates = { "radio_merchant", "radio_merchant", "radio_merchant", "radio_wandering_scavenger" },
          base_chance = 30,
          spawn_min_npcs = 2,
          spawn_max_npcs = 3,
          spawn_radius = 60,
          stay_hours = 6,
          is_primary = true,
        },
        {
          faction_id = "hells_raiders",
          display_name = locale.gettext("Hell's Raiders"),
          npc_templates = { "radio_bandit", "radio_thug" },
          base_chance = 25,
          spawn_min_npcs = 2,
          spawn_max_npcs = 3,
          spawn_radius = 60,
          stay_hours = 12,
          make_angry = true,
          is_primary = false,
        },
      },
      duration_hours = 6,
      range = 60,
      start_msg = locale.gettext("You advertise a trade offer over the survivor band."),
    },
    {
      id = "work",
      name = locale.gettext("Requesting work"),
      desc = locale.gettext("Ask nearby survivors for paid jobs."),
      audiences = {
        {
          faction_id = "no_faction",
          display_name = locale.gettext("Independent contractors"),
          npc_templates = { "radio_job_broker" },
          base_chance = 35,
          spawn_min_npcs = 1,
          spawn_max_npcs = 1,
          spawn_radius = 60,
          stay_hours = 6,
          is_primary = true,
        },
      },
      duration_hours = 6,
      range = 60,
      start_msg = locale.gettext("You broadcast a request for work on the survivor band."),
    },
    {
      id = "raider",
      name = locale.gettext("Raider challenge"),
      desc = locale.gettext("Taunt raiders into coming to you."),
      audiences = {
        {
          faction_id = "hells_raiders",
          display_name = locale.gettext("Hell's Raiders"),
          npc_templates = { "radio_bandit", "radio_thug" },
          base_chance = 50,
          spawn_min_npcs = 2,
          spawn_max_npcs = 5,
          spawn_radius = 60,
          stay_hours = 12,
          make_angry = true,
          is_primary = true,
        }
      },
      duration_hours = 6,
      range = 60,
      start_msg = locale.gettext("You taunt raiders with a reckless broadcast."),
    }
  }
end

local function get_signal_definition(signal_id)
  for _, entry in ipairs(signal_definitions()) do
    if entry.id == signal_id then return entry end
  end
  return nil
end

local function resolve_storage()
  local mod_id = current_mod_id or game.current_mod or "bn"
  local storage = game.mod_storage[mod_id]
  if not storage then
    storage = {}
    game.mod_storage[mod_id] = storage
  end
  return storage
end

local function stay_turns_for_entry(entry)
  local stay_hours = entry and entry.stay_hours or default_stay_hours
  return TimeDuration.from_hours(stay_hours):to_turns()
end

local function track_spawned_npc(npc, entry)
  if not npc then return end
  local storage = resolve_storage()
  local tracked = storage.survivor_radio_spawned or {}
  local npc_id = npc:getID():get_value()
  tracked[npc_id] = true
  storage.survivor_radio_spawned = tracked
  local cleanup_at = storage.survivor_radio_cleanup_at or {}
  cleanup_at[npc_id] = now_turn() + stay_turns_for_entry(entry)
  storage.survivor_radio_cleanup_at = cleanup_at
  gapi.add_msg("A survivor radio contact has arrived nearby.")
end

local is_active_npc
local request_departure

survivor_radio.is_tracked_npc = function(npc_id)
  local storage = resolve_storage()
  local tracked = storage.survivor_radio_spawned or {}
  return tracked[npc_id] == true
end

survivor_radio.clear_tracked_npc = function(npc_id)
  local storage = resolve_storage()
  local tracked = storage.survivor_radio_spawned or {}
  tracked[npc_id] = nil
  storage.survivor_radio_spawned = tracked
end

survivor_radio.cleanup_npc = function(npc_id)
  return cleanup_npc_id(npc_id)
end

local function queue_erase_npc_id(npc_id)
  local storage = resolve_storage()
  local pending = storage.survivor_radio_pending_erase or {}
  pending[npc_id] = true
  storage.survivor_radio_pending_erase = pending
end

local function cleanup_npc_id(npc_id)
  if not npc_id then return false end
  local was_tracked = survivor_radio.is_tracked_npc(npc_id)
  local id = CharacterId.new(npc_id)
  local npc = gapi.get_npc_by_id(id)
  local avatar = gapi.get_avatar()
  if npc and avatar and (npc:is_player_ally() or npc:is_ally(avatar) or npc:is_friendly(avatar)) then
    survivor_radio.clear_tracked_npc(npc_id)
    local storage = resolve_storage()
    local cleanup_at = storage.survivor_radio_cleanup_at or {}
    cleanup_at[npc_id] = nil
    storage.survivor_radio_cleanup_at = cleanup_at
    return true
  end
  if npc and is_active_npc(npc) then
    if avatar and coords.rl_dist(npc:get_pos_ms(), avatar:get_pos_ms()) >= cleanup_departure_distance then
      npc = nil
    else
      request_departure(npc)
      return false
    end
  end
  local removed = gapi.remove_npc_by_id(id)
  if not removed then
    queue_erase_npc_id(npc_id)
  end
  if removed then
    survivor_radio.clear_tracked_npc(npc_id)
    local storage = resolve_storage()
    local cleanup_at = storage.survivor_radio_cleanup_at or {}
    cleanup_at[npc_id] = nil
    storage.survivor_radio_cleanup_at = cleanup_at
    if was_tracked then
      gapi.add_msg(MsgType.info, locale.gettext("The survivor radio contact is cleaned up."))
    end
  end
  return removed
end

local function erase_pending_npcs()
  local storage = resolve_storage()
  local pending = storage.survivor_radio_pending_erase or {}
  for npc_id, should_erase in pairs(pending) do
    if should_erase then
      if cleanup_npc_id(npc_id) then
        pending[npc_id] = nil
      end
    end
  end
  storage.survivor_radio_pending_erase = pending
end

local function cleanup_due_npcs()
  local storage = resolve_storage()
  local cleanup_at = storage.survivor_radio_cleanup_at or {}
  local now = now_turn()
  for npc_id, due_turn in pairs(cleanup_at) do
    if due_turn and now >= due_turn then
      if cleanup_npc_id(npc_id) then
        cleanup_at[npc_id] = nil
      else
        cleanup_at[npc_id] = now + cleanup_retry_turns
      end
    end
  end
  storage.survivor_radio_cleanup_at = cleanup_at
end

local function broadcast_local_pos(broadcast, here)
  local abs_pos = pos_from_table(broadcast.pos)
  if not abs_pos then return nil end
  return here:get_local_ms(abs_pos)
end

local function is_in_bounds(here, pt)
  local size = here:get_map_size()
  return pt.x >= 0 and pt.y >= 0 and pt.x < size and pt.y < size
end

is_active_npc = function(npc)
  return gapi.get_npc_at(npc:get_pos_ms(), true) == npc
end

local function random_omt_offset(min_dist, max_dist)
  local dist = gapi.rng(min_dist, max_dist)
  local dx = gapi.rng(-dist, dist)
  local dy = gapi.rng(-dist, dist)
  if dx == 0 and dy == 0 then
    dx = dist
  end
  if math.abs(dx) < min_dist and math.abs(dy) < min_dist then
    if math.abs(dx) < min_dist then
      dx = dx < 0 and -min_dist or min_dist
    else
      dy = dy < 0 and -min_dist or min_dist
    end
  end
  return dx, dy
end

request_departure = function(npc)
  local pos_abs_omt, _ = coords.ms_to_omt(npc:get_pos_ms())
  local dx, dy = random_omt_offset(5, 9)
  npc:set_omt_destination(Tripoint.new(pos_abs_omt.x + dx, pos_abs_omt.y + dy, pos_abs_omt.z))
  npc:set_attitude(NpcAttitude.NPCATT_FLEE_TEMP)
end

local function random_offset_in_radius(radius, radius_min)
  local min_dist = radius_min or 0
  for _ = 1, 20 do
    local dx = gapi.rng(-radius, radius)
    local dy = gapi.rng(-radius, radius)
    local dist = coords.rl_dist(Tripoint.new(0, 0, 0), Tripoint.new(dx, dy, 0))
    if dist >= min_dist and dist <= radius then
      return dx, dy
    end
  end
  return 0, 0
end

local function spawn_npc_offmap(entry, template, spawn_pos_abs)
  local here = gapi.get_map()
  local avatar = gapi.get_avatar()
  local anchor_abs = avatar and avatar:get_pos_ms() or spawn_pos_abs
  local anchor_local = here:get_local_ms(anchor_abs)
  if not is_in_bounds(here, anchor_local) then return nil end

  local npc = here:place_npc(Point.new(anchor_local.x, anchor_local.y), template)
  if not npc then return nil end

  npc:set_pos_ms(spawn_pos_abs)
  npc:set_faction_id(FactionId.new(entry.faction_id))
  if entry.make_angry then npc:make_angry() end
  track_spawned_npc(npc, entry)
  return npc
end

local function count_npcs_in_radius(here, center, radius)
  local count = 0
  for _, pt in ipairs(here:points_in_radius(center, radius)) do
    local npc = gapi.get_npc_at(pt, true)
    if npc then count = count + 1 end
  end
  return count
end

local function chance_for_audience(entry, speech)
  local base = entry.base_chance or 0
  local speech_bonus = (speech - speech_baseline) * (entry.is_primary and 3 or -2)
  local chance = base + speech_bonus
  if chance < 1 then return 1 end
  if chance > 90 then return 90 end
  return chance
end

local function choose_broadcast_frequency()
  local options = {
    { hours = 0, label = locale.gettext("Broadcast once") },
    { hours = 6, label = locale.gettext("Every 6 hours") },
    { hours = 12, label = locale.gettext("Every 12 hours") },
    { hours = 24, label = locale.gettext("Every 24 hours") },
  }
  local menu = UiList.new()
  menu:title(locale.gettext("Broadcast Frequency"))
  for idx, entry in ipairs(options) do
    menu:add(idx - 1, entry.label)
  end
  local choice = menu:query()
  if choice < 0 then return nil end
  return options[choice + 1].hours
end

local function spawn_audience(entry, broadcast)
  local here = gapi.get_map()
  local abs_pos = pos_from_table(broadcast.pos)
  if not abs_pos then return end
  local local_pos = broadcast_local_pos(broadcast, here)
  local use_local = local_pos and is_in_bounds(here, local_pos)

  local max_npcs = entry.spawn_max_npcs or 0
  local spawn_radius = entry.spawn_radius or 0
  local spawn_radius_min = entry.spawn_radius_min or spawn_radius - 5
  local spawn_group_radius = entry.spawn_group_radius or 3
  if max_npcs <= 0 then return end
  if use_local and count_npcs_in_radius(here, local_pos, spawn_radius) >= max_npcs then return end

  local templates = entry.npc_templates or {}
  if #templates == 0 then return end

  if use_local then
    local points = here:points_in_radius(local_pos, spawn_radius)
    for _ = 1, 8 do
      local idx = gapi.rng(1, #points)
      local candidate = points[idx]
      if is_in_bounds(here, candidate)
        and coords.rl_dist(candidate, local_pos) >= spawn_radius_min
        and not gapi.get_creature_at(candidate, true) then
        local min_npcs = entry.spawn_min_npcs or 1
        local spawn_count = gapi.rng(min_npcs, max_npcs)
        local cluster_points = here:points_in_radius(candidate, spawn_group_radius)
        local spawned = 0
        for _ = 1, 20 do
          if spawned >= spawn_count then break end
          local cluster_idx = gapi.rng(1, #cluster_points)
          local cluster_pos = cluster_points[cluster_idx]
          if is_in_bounds(here, cluster_pos)
            and not gapi.get_creature_at(cluster_pos, true) then
            local template = templates[gapi.rng(1, #templates)]
            local npc = here:place_npc(Point.new(cluster_pos.x, cluster_pos.y), template)
            if npc then
              npc:set_faction_id(FactionId.new(entry.faction_id))
              if entry.make_angry then npc:make_angry() end
              track_spawned_npc(npc, entry)
              debug_log(string.format(
                "spawned npc=%s audience=%s",
                template,
                entry.faction_id or "unknown"
              ))
              spawned = spawned + 1
            end
          end
        end
        debug_log(string.format(
          "spawned group count=%d audience=%s",
          spawned,
          entry.faction_id or "unknown"
        ))
        return true
      end
    end
  else
    for _ = 1, 8 do
      local dx, dy = random_offset_in_radius(spawn_radius, spawn_radius_min)
      local candidate_abs = Tripoint.new(abs_pos.x + dx, abs_pos.y + dy, abs_pos.z)
      local min_npcs = entry.spawn_min_npcs or 1
      local spawn_count = gapi.rng(min_npcs, max_npcs)
      local spawned = 0
      for _ = 1, 20 do
        if spawned >= spawn_count then break end
        local cluster_dx, cluster_dy = random_offset_in_radius(spawn_group_radius, 0)
        local cluster_pos_abs = Tripoint.new(
          candidate_abs.x + cluster_dx,
          candidate_abs.y + cluster_dy,
          candidate_abs.z
        )
        local template = templates[gapi.rng(1, #templates)]
        local npc = spawn_npc_offmap(entry, template, cluster_pos_abs)
        if npc then
          debug_log(string.format(
            "spawned npc=%s audience=%s",
            template,
            entry.faction_id or "unknown"
          ))
          spawned = spawned + 1
        end
      end
      debug_log(string.format(
        "spawned group count=%d audience=%s",
        spawned,
        entry.faction_id or "unknown"
      ))
      return true
    end
  end
  debug_log(string.format("spawn failed: no valid tiles for audience=%s", entry.faction_id or "unknown"))
  return false
end

local function get_active_broadcast(storage)
  local saved = storage.survivor_radio
  if not saved then return nil end

  if not saved.pos then return nil end

  return saved
end

local function clear_pending_spawn(storage)
  storage.survivor_radio_pending_spawn = nil
end

local function schedule_spawn(storage, signal_id, audience_index, broadcast)
  local delay_hours = gapi.rng(4, 24)
  storage.survivor_radio_pending_spawn = {
    signal_id = signal_id,
    audience_index = audience_index,
    due_turn = now_turn() + TimeDuration.from_hours(delay_hours):to_turns(),
    broadcast = broadcast and {
      pos = broadcast.pos,
      range = broadcast.range,
      speech = broadcast.speech,
    },
  }
  debug_log(string.format("scheduled spawn signal=%s audience=%d delay_hours=%d", signal_id, audience_index, delay_hours))
end

survivor_radio.debug_spawn_party = function()
  local def = get_signal_definition("distress")
  if not def then
    gapi.add_msg(MsgType.warning, locale.gettext("Debug: survivor radio signal definition missing."))
    return
  end
  local entry = def.audiences and def.audiences[1]
  if not entry then
    gapi.add_msg(MsgType.warning, locale.gettext("Debug: survivor radio audience definition missing."))
    return
  end
  local avatar = gapi.get_avatar()
  local here = gapi.get_map()
  if not avatar or not here then
    gapi.add_msg(MsgType.warning, locale.gettext("Debug: unable to locate the avatar."))
    return
  end

  local center = avatar:get_pos_ms()
  local anchor_local = here:get_local_ms(center)
  if not anchor_local then
    anchor_local = Point.new(math.floor(here:get_map_size() / 2), math.floor(here:get_map_size() / 2))
  end

  local templates = entry.npc_templates or {}
  if #templates == 0 then
    gapi.add_msg(MsgType.warning, locale.gettext("Debug: audience templates missing."))
    return
  end

  local spawned = 0
  local points = here:points_in_radius(anchor_local, 3)
  for _, pt in ipairs(points) do
    if spawned >= 3 then break end
    if gapi.get_creature_at(pt, true) then goto continue end
    local template = templates[((spawned) % #templates) + 1]
    local npc = here:place_npc(Point.new(pt.x, pt.y), template)
    if not npc then goto continue end
    local world_pos = here:get_abs_ms(pt)
    npc:set_pos_ms(world_pos)
    npc:set_faction_id(FactionId.new(entry.faction_id))
    if entry.make_angry then npc:make_angry() end
    track_spawned_npc(npc, entry)
    spawned = spawned + 1
    ::continue::
  end

  if spawned > 0 then
    gapi.add_msg(MsgType.good, locale.gettext("Debug: a survivor radio party has spawned nearby."))
  else
    gapi.add_msg(MsgType.warning, locale.gettext("Debug: failed to place a survivor radio party nearby."))
  end
end

---@return number
survivor_radio.menu = function(params)
  local storage = resolve_storage()
  local pos = params.pos
  local here = gapi.get_map()
  local abs_pos = here:get_abs_ms(pos)
  local who = params.user or gapi.get_avatar()
  local speech = 0
  if who then
    speech = who:get_skill_level(SkillId.new("speech"))
  end

  local menu = UiList.new()
  menu:title(locale.gettext("Survivor Radio"))
  local menu_text = locale.gettext("Select a broadcast to send.")

  local signals = signal_definitions()
  for idx, entry in ipairs(signals) do
    menu:add_w_desc(idx - 1, entry.name, entry.desc)
  end

  local active = get_active_broadcast(storage)
  if active then
    local active_def = get_signal_definition(active.signal_id)
    local active_name = active_def and active_def.name or locale.gettext("Unknown")
    local cancel_entry = #signals
    menu:add_w_desc(cancel_entry, locale.gettext("Stop broadcasting"), locale.gettext("Clear the current signal."))
    menu_text = string.format(
      "%s\n\n%s",
      menu_text,
      string.format(locale.gettext("Current broadcast: %s"), active_name)
    )
  else
    menu_text = string.format(
      "%s\n\n%s",
      menu_text,
      locale.gettext("Current broadcast: none.")
    )
  end
  menu:text(menu_text)

  local choice = menu:query()
  if choice < 0 then return 0 end

  if active and choice == #signals then
    storage.survivor_radio = nil
    clear_pending_spawn(storage)
    gapi.add_msg(MsgType.info, locale.gettext("You stop broadcasting."))
    return 0
  end

  local selected = signals[choice + 1]
  if not selected then return 0 end

  local frequency_hours = choose_broadcast_frequency()
  if not frequency_hours then return 0 end

  local confirm_lines = {}
  local speech_delta = speech - speech_baseline
  add_line(confirm_lines, locale.gettext("Signal"), selected.name)
  table.insert(confirm_lines, color_text(selected.desc, "light_gray"))
  table.insert(confirm_lines, "")
  add_line(
    confirm_lines,
    locale.gettext("Speech modifier"),
    string.format(locale.gettext("%+d (baseline %d)"), speech_delta, speech_baseline),
    "yellow"
  )
  add_line(
    confirm_lines,
    locale.gettext("Broadcast interval"),
    string.format(locale.gettext("Every %d hours"), frequency_hours),
    "light_gray"
  )
  table.insert(confirm_lines, "")
  for _, entry in ipairs(selected.audiences or {}) do
    local base = entry.base_chance or 0
    local final = chance_for_audience(entry, speech)
    local base_text = color_text(string.format("%d%%", base), "light_gray")
    local final_text = color_text(string.format("%d%%", final), "yellow")
    local chance_text = string.format(
      locale.gettext("Base %s  \226\134\146  Final %s"),
      base_text,
      final_text
    )
    local audience_name = get_audience_name(entry)
    local audience_color = entry.is_primary and "white" or "light_gray"
    add_line(confirm_lines, locale.gettext("Audience"), audience_name, audience_color)
    table.insert(confirm_lines, "  " .. chance_text)
  end
  table.insert(confirm_lines, "")
  table.insert(confirm_lines, color_text(locale.gettext("Broadcast this signal?"), "yellow"))
  local confirm_text = table.concat(confirm_lines, "\n")
  if not ui.query_yn(confirm_text) then return 0 end

  storage.survivor_radio = {
    signal_id = selected.id,
    pos = pos_to_table(abs_pos),
    range = selected.range,
    speech = speech,
    frequency_hours = frequency_hours,
    next_tick = now_turn() + TimeDuration.from_hours(frequency_hours):to_turns(),
  }
  clear_pending_spawn(storage)
  debug_log(string.format("broadcast signal=%s speech=%d", selected.id, speech))

  gapi.add_msg(MsgType.good, selected.start_msg)
  return 0
end

---@param mod table
function survivor_radio.register(mod)
  current_mod_id = current_mod_id or game.current_mod
  mod.on_survivor_radio_tick = function()
    local storage = mod.storage or resolve_storage()
    cleanup_due_npcs()
    erase_pending_npcs()
    local broadcast = get_active_broadcast(storage)
    local pending = storage.survivor_radio_pending_spawn
    local now = now_turn()

    if pending and broadcast and pending.signal_id ~= broadcast.signal_id then
      clear_pending_spawn(storage)
      pending = nil
    end

    if pending and now >= pending.due_turn then
      local pending_def = get_signal_definition(pending.signal_id)
      local pending_entry = pending_def
        and pending_def.audiences
        and pending_def.audiences[pending.audience_index]
      local pending_broadcast = pending.broadcast or broadcast
      if pending_entry and pending_broadcast and spawn_audience(pending_entry, pending_broadcast) then
        clear_pending_spawn(storage)
        return
      end
      pending.due_turn = now + spawn_retry_turns
      storage.survivor_radio_pending_spawn = pending
      return
    elseif pending then
      return
    end

    if not broadcast then return end

    local def = get_signal_definition(broadcast.signal_id)
    if not def then
      storage.survivor_radio = nil
      return
    end

    local frequency_hours = broadcast.frequency_hours or 12
    if not broadcast.next_tick then
      broadcast.next_tick = now
    end
    if now < broadcast.next_tick then return end
    if frequency_hours <= 0 then
      storage.survivor_radio = nil
    else
      broadcast.next_tick = now + TimeDuration.from_hours(frequency_hours):to_turns()
    end

    local audiences = def.audiences or {}
    local speech = broadcast.speech or 0
    for idx, entry in ipairs(audiences) do
      local spawn_chance = chance_for_audience(entry, speech)
      local roll = gapi.rng(1, 100)
      debug_log(string.format(
        "spawn roll=%d chance=%d audience=%s",
        roll,
        spawn_chance,
        entry.faction_id or "unknown"
      ))
      if roll <= spawn_chance then
        schedule_spawn(storage, def.id, idx, broadcast)
        local audience_name = get_audience_name(entry)
        gapi.add_msg(
          MsgType.info,
          string.format(
            locale.gettext("%s responded to your broadcast and will arrive in %d hours."),
            audience_name,
            math.ceil((broadcast.next_tick - now) / TimeDuration.from_hours(1):to_turns())
          )
        )
        storage.survivor_radio = nil
        return
      end
    end
    debug_log(string.format("spawn tick: no audience scheduled for signal=%s", broadcast.signal_id or "unknown"))
  end

  mod.on_npc_unload = function(params)
      local npc = params.npc
      if npc then
          local npc_id = npc:getID():get_value()
          if mod.survivor_radio
              and mod.survivor_radio.is_tracked_npc
              and mod.survivor_radio.is_tracked_npc(npc_id) then
              if not npc:is_player_ally() then
                  cleanup_npc_id(npc_id)
              end
          end
      end
  end

  game.add_hook("on_npc_unload", function(...) return mod.on_npc_unload(...) end)

  mod.on_npc_spawn = function(params)
    local storage = mod.storage or resolve_storage()
    local broadcast = get_active_broadcast(storage)
    if not broadcast then return end

    local def = get_signal_definition(broadcast.signal_id)
    if not def then
      storage.survivor_radio = nil
      return
    end

    local here = gapi.get_map()
    local local_pos = broadcast_local_pos(broadcast, here)
    if not local_pos or not is_in_bounds(here, local_pos) then return end

    local npc = params.npc
    if not npc or npc:is_player_ally() or npc:is_stationary(true) or npc:is_guarding() then return end

    local npc_pos = npc:get_pos_ms()
    local range = broadcast.range or def.range or 0
    if range <= 0 then return end
    if coords.rl_dist(npc_pos, local_pos) > range then return end

    local audiences = def.audiences or {}
    local speech = broadcast.speech or 0
    for _, entry in ipairs(audiences) do
      local roll = gapi.rng(1, 100)
      local chance = chance_for_audience(entry, speech)
      debug_log(string.format(
        "npc_spawn roll=%d chance=%d audience=%s",
        roll,
        chance,
        entry.faction_id or "unknown"
      ))
      if roll <= chance then
        npc:set_faction_id(FactionId.new(entry.faction_id))
        if entry.make_angry then npc:make_angry() end
        debug_log(string.format("npc_spawn matched audience=%s", entry.faction_id or "unknown"))
        return
      end
    end
    debug_log("npc_spawn: no audience matched")
  end
end

return survivor_radio

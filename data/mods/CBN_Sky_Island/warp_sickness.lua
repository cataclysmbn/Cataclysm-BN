-- Sky Islands BN Port - Warp Sickness System
-- Handles warp sickness progression using JSON effects
-- Uses a SINGLE always-running hook that checks conditions each minute

local warp_sickness = {}
local util = require("util")

-- Warp sickness timing
-- Base interval is modified by difficulty setting (casual=30, normal=15, hard=10, impossible=5)
local PULSE_INTERVALS = {
  casual = 30,
  normal = 15,
  hard = 10,
  impossible = 5
}
local BASE_GRACE_PERIOD_PULSES = 8  -- 8 pulses before sickness starts
local MAX_WARP_SICKNESS_INTENSITY = 6

-- Helper: Get base pulse interval in minutes based on difficulty
local function get_base_pulse_interval(storage)
  local difficulty = storage.difficulty_pulse_interval or "normal"
  return PULSE_INTERVALS[difficulty] or 15
end

-- Effect IDs
local EFFECT_WARP_SICKNESS = EffectTypeId.new("skyisland_warpsickness")
local EFFECT_DISINTEGRATION = EffectTypeId.new("skyisland_warpdisintegration")

-- Status names for each intensity level
local STATUS_NAMES = {
  [0] = "Stable",
  [1] = "Warp Sickness",
  [2] = "Warp Nausea",
  [3] = "Warp Debilitation",
  [4] = "Warp Necrosis",
  [5] = "Warp Disintegration",
  [6] = "CRITICAL"
}

-- Get current warp status info for display
function warp_sickness.get_status_info(storage)
  local player = gapi.get_avatar()
  local pulse = storage.warp_pulse_count or 0
  local stability_bonus = storage.stability_unlocked or 0
  local grace_period = BASE_GRACE_PERIOD_PULSES + stability_bonus

  local info = {
    current_pulse = pulse,
    grace_period = grace_period,
    in_grace_period = pulse <= grace_period,
    pulses_remaining = 0,
    current_intensity = 0,
    status_name = "Stable",
    pulses_to_disintegration = 0
  }

  if info.in_grace_period then
    info.pulses_remaining = grace_period - pulse
  else
    -- Get current intensity from effect
    if player and player:has_effect(EFFECT_WARP_SICKNESS) then
      info.current_intensity = player:get_effect_int(EFFECT_WARP_SICKNESS)
    else
      info.current_intensity = pulse - grace_period
    end
    info.status_name = STATUS_NAMES[info.current_intensity] or "CRITICAL"
    info.pulses_to_disintegration = math.max(0, MAX_WARP_SICKNESS_INTENSITY - info.current_intensity)
  end

  return info
end

-- Show warp pulse dialog (blocking popup)
local function show_pulse_dialog(message, color)
  local popup = QueryPopup.new()
  popup:message(message)
  if color then
    popup:message_color(color)
  end
  popup:allow_any_key(true)
  popup:query()
end

-- Prevent re-entry during popup display
local tick_in_progress = false

-- Process a warp pulse (called when accumulated time reaches threshold)
local function process_warp_pulse(storage)
  if tick_in_progress then
    util.debug_log("Warp pulse already in progress, skipping")
    return
  end
  tick_in_progress = true

  local player = gapi.get_avatar()
  if not player then
    tick_in_progress = false
    return
  end

  -- Increment pulse counter
  storage.warp_pulse_count = (storage.warp_pulse_count or 0) + 1
  local pulse = storage.warp_pulse_count

  -- Calculate grace period based on stability upgrades
  local stability_bonus = storage.stability_unlocked or 0
  local grace_period = BASE_GRACE_PERIOD_PULSES + stability_bonus
  local pulse_multiplier = storage.current_raid_pulse_multiplier or 1
  local base_interval = get_base_pulse_interval(storage)
  local interval_minutes = base_interval * pulse_multiplier

  util.debug_log(string.format("Warp pulse %d (grace period: %d)", pulse, grace_period))

  -- Grace period: pulses during grace have no effect
  if pulse <= grace_period then
    local remaining = grace_period - pulse
    local time_remaining = remaining * interval_minutes

    if remaining > 3 then
      -- Still plenty of time - green
      gapi.add_msg(string.format(locale.gettext("[Warp Pulse] You feel a faint tingling of warp energy. Safe pulses remaining: %d (~%d min)."), remaining, time_remaining))
      show_pulse_dialog(
        string.format(locale.gettext("=== WARP PULSE ===\n\n") ..
          locale.gettext("You feel a faint tingling of warp energy.\n\n") ..
          locale.gettext("Safe pulses remaining: %d\n") ..
          locale.gettext("Time until sickness: ~%d minutes\n\n") ..
          locale.gettext("[Press any key to continue]"),
          remaining, time_remaining),
        Color.c_green)
    elseif remaining > 0 then
      -- Getting close - yellow warning
      gapi.add_msg(string.format(locale.gettext("<color_yellow>[Warp Pulse] The warp begins to tug at you... Safe pulses remaining: %d (~%d min).</color>"), remaining, time_remaining))
      show_pulse_dialog(
        string.format(locale.gettext("=== WARP PULSE ===\n\n") ..
          locale.gettext("The warp begins to tug at you...\n\n") ..
          locale.gettext("Safe pulses remaining: %d\n") ..
          locale.gettext("Time until sickness: ~%d minutes\n\n") ..
          locale.gettext("[Press any key to continue]"),
          remaining, time_remaining),
        Color.c_yellow)
    else
      -- Last safe pulse
      gapi.add_msg(locale.gettext("<color_light_red>[Warp Pulse] WARNING: This is your last safe pulse! The next pulse will begin warp sickness.</color>"))
      show_pulse_dialog(
        locale.gettext("=== WARP PULSE ===\n\n") ..
          locale.gettext("WARNING: This is your last safe pulse!\n") ..
          locale.gettext("The next pulse will begin warp sickness.\n\n") ..
          locale.gettext("[Press any key to continue]"),
        Color.c_light_red)
    end
    tick_in_progress = false
    return
  end

  -- Apply or update warp sickness effect
  player:add_effect(EFFECT_WARP_SICKNESS, TimeDuration.from_hours(999))
  local current_intensity = player:get_effect_int(EFFECT_WARP_SICKNESS)
  local status_name = STATUS_NAMES[current_intensity] or "CRITICAL"
  local pulses_to_doom = MAX_WARP_SICKNESS_INTENSITY - current_intensity

  util.debug_log(string.format("Warp sickness applied/updated, intensity now: %d", current_intensity))

  -- Apply disintegration at critical intensity
  if current_intensity >= MAX_WARP_SICKNESS_INTENSITY then
    if not player:has_effect(EFFECT_DISINTEGRATION) then
      player:add_effect(EFFECT_DISINTEGRATION, TimeDuration.from_hours(999))
    end
    -- Critical - flashing red
    gapi.add_msg(locale.gettext("<color_red>[Warp Pulse] !!! WARP DISINTEGRATION !!! You are being unmade!</color>"))
    show_pulse_dialog(
      locale.gettext("=== WARP PULSE ===\n\n") ..
        locale.gettext("!!! WARP DISINTEGRATION !!!\n\n") ..
        locale.gettext("Your skin tears like tissue paper with every movement.\n") ..
        locale.gettext("Your body is coming apart at the seams!\n") ..
        locale.gettext("GET TO THE EXIT IMMEDIATELY!\n\n") ..
        locale.gettext("[Press any key to continue]"),
      Color.i_red)
  elseif current_intensity >= 4 then
    -- Severe - red
    gapi.add_msg(string.format(locale.gettext("<color_red>[Warp Pulse] %s (%d/6). -%d stats. %d pulses to disintegration!</color>"),
      status_name, current_intensity, current_intensity * 2, pulses_to_doom))
    show_pulse_dialog(
      string.format(locale.gettext("=== WARP PULSE ===\n\n") ..
        locale.gettext("STATUS: %s (Intensity %d/6)\n\n") ..
        locale.gettext("Your body is failing!\n") ..
        locale.gettext("Stats: -%d to all\n") ..
        locale.gettext("Pulses until disintegration: %d\n\n") ..
        locale.gettext("You MUST return home soon!\n\n") ..
        locale.gettext("[Press any key to continue]"),
        status_name, current_intensity, current_intensity * 2, pulses_to_doom),
      Color.c_red)
  elseif current_intensity >= 2 then
    -- Moderate - light red/orange
    gapi.add_msg(string.format(locale.gettext("<color_light_red>[Warp Pulse] STATUS: %s (%d/6). -%d stats. %d pulses to disintegration.</color>"),
      status_name, current_intensity, current_intensity * 2, pulses_to_doom))
    show_pulse_dialog(
      string.format(locale.gettext("=== WARP PULSE ===\n\n") ..
        locale.gettext("STATUS: %s (Intensity %d/6)\n\n") ..
        locale.gettext("The warp sickness grows stronger.\n") ..
        locale.gettext("Stats: -%d to all\n") ..
        locale.gettext("Pulses until disintegration: %d\n\n") ..
        locale.gettext("Consider returning home soon.\n\n") ..
        locale.gettext("[Press any key to continue]"),
        status_name, current_intensity, current_intensity * 2, pulses_to_doom),
      Color.c_light_red)
  else
    -- Mild - yellow
    gapi.add_msg(string.format("<color_yellow>[Warp Pulse] %s (%d/6). -%d stats. %d pulses to disintegration.</color>",
      status_name, current_intensity, current_intensity * 2, pulses_to_doom))
    show_pulse_dialog(
      string.format("=== WARP PULSE ===\n\n" ..
        "STATUS: %s (Intensity %d/6)\n\n" ..
        "The warp begins to affect you.\n" ..
        "Stats: -%d to all\n" ..
        "Pulses until disintegration: %d\n\n" ..
        "[Press any key to continue]",
        status_name, current_intensity, current_intensity * 2, pulses_to_doom),
      Color.c_yellow)
  end

  tick_in_progress = false
end

-- Global hook registered flag (to prevent duplicate registration)
local global_hook_registered = false

-- Register the single global hook that runs every minute
-- This should be called ONCE during mod initialization
function warp_sickness.register_global_hook(storage)
  if global_hook_registered then
    util.debug_log("Warp sickness global hook already registered, skipping")
    return
  end
  global_hook_registered = true

  util.debug_log("Registering warp sickness global hook (1 minute interval)")

  gapi.add_on_every_x_hook(TimeDuration.from_minutes(1), function()
    -- Always run, always return true to keep running

    -- If player is home, reset accumulator and do nothing
    if not storage.is_away_from_home then
      storage.warp_pulse_accumulated = 0
      return true
    end

    -- Player is on a raid - accumulate time
    storage.warp_pulse_accumulated = (storage.warp_pulse_accumulated or 0) + 1

    -- Calculate pulse threshold based on difficulty and raid type
    local base_interval = get_base_pulse_interval(storage)
    local pulse_multiplier = storage.current_raid_pulse_multiplier or 1
    local pulse_threshold = base_interval * pulse_multiplier

    -- Check if we've accumulated enough for a pulse
    if storage.warp_pulse_accumulated >= pulse_threshold then
      util.debug_log(string.format("Warp pulse triggered: accumulated %d >= threshold %d",
        storage.warp_pulse_accumulated, pulse_threshold))
      storage.warp_pulse_accumulated = 0
      process_warp_pulse(storage)
    end

    return true  -- Always keep running
  end)
end

-- Start warp sickness for a new expedition - just reset counters
function warp_sickness.start(storage)
  storage.warp_pulse_count = 0
  storage.warp_pulse_accumulated = 0
  util.debug_log("Warp sickness: initialized for new expedition")
end

-- Stop warp sickness - remove all effects and reset counters
function warp_sickness.stop(storage)
  -- Reset counters
  if storage then
    storage.warp_pulse_accumulated = 0
    -- Note: we don't reset warp_pulse_count here so stats are preserved
  end
  tick_in_progress = false

  local player = gapi.get_avatar()
  if not player then
    return
  end

  -- Remove effects
  player:remove_effect(EFFECT_WARP_SICKNESS)
  player:remove_effect(EFFECT_DISINTEGRATION)

  -- Also remove resurrection sickness if present
  local res_sick_effect = EffectTypeId.new("skyisland_resurrection_sickness")
  player:remove_effect(res_sick_effect)

  util.debug_log("Warp sickness stopped and cleared")
end

-- Resurrection sickness tick - forcibly stabilize the player
function warp_sickness.resurrection_tick()
  local player = gapi.get_avatar()
  if not player then return true end

  -- Check if player has resurrection sickness effect
  local res_sick_effect = EffectTypeId.new("skyisland_resurrection_sickness")
  if player:has_effect(res_sick_effect) then
    -- Get remaining duration before clearing
    local remaining_dur = player:get_effect_dur(res_sick_effect)

    -- Clear ALL effects (including broken limbs, poison, etc.)
    player:clear_effects()

    -- Forcibly heal all parts to 10 HP
    player:set_all_parts_hp_cur(10)

    -- Set pain to 10 if it's greater than 10
    if player:get_pain() > 10 then
      player:set_pain(10)
    end

    -- Re-apply resurrection sickness with remaining duration
    player:add_effect(res_sick_effect, remaining_dur)

    return true  -- Keep running while effect is active
  else
    return false  -- Stop running when effect expires
  end
end

-- Apply resurrection sickness after death
function warp_sickness.apply_resurrection_sickness()
  local player = gapi.get_avatar()
  if not player then return end

  -- Set HP immediately
  player:set_all_parts_hp_cur(10)

  -- Set pain to 10 for resurrection penalty
  player:set_pain(10)

  -- Apply resurrection sickness effect to stabilize over 10 seconds
  local res_sick_effect = EffectTypeId.new("skyisland_resurrection_sickness")
  player:add_effect(res_sick_effect, TimeDuration.from_seconds(10))

  -- Start resurrection stabilization tick (runs every second)
  gapi.add_on_every_x_hook(TimeDuration.from_seconds(1), function()
    return warp_sickness.resurrection_tick()
  end)
end

return warp_sickness

local plumbing = {}

local ACT_WASH_SELF = ActivityTypeId.new("ACT_WASH_SELF")
local effect_hot = EffectTypeId.new("hot")
local item_water = ItypeId.new("water")
local item_water_clean = ItypeId.new("water_clean")
local morale_feeling_good = MoraleTypeDataId.new("morale_feeling_good")
local item_soap = ItypeId.new("soap")
local item_shampoo = ItypeId.new("shampoo")

local shower_water_charges = 24
local bath_water_charges = 180
local shower_power_cost = 500
local bath_power_cost = 1500
local warm_temperature_threshold_c = 10

local wash_mode_data = {
  shower = {
    duration_minutes = 15,
    water_charges = shower_water_charges,
    power_cost = shower_power_cost,
    morale = 6,
    morale_max = 20,
    morale_duration_hours = 2,
    morale_decay_minutes = 30,
    heat_minutes = 20,
    hygiene_bonus = 4,
  },
  bath = {
    duration_minutes = 45,
    water_charges = bath_water_charges,
    power_cost = bath_power_cost,
    morale = 14,
    morale_max = 30,
    morale_duration_hours = 5,
    morale_decay_minutes = 45,
    heat_minutes = 45,
    hygiene_bonus = 8,
  },
}

local get_fixture_mode = function(map, pos)
  local furn_id = map:get_furn_at(pos):str()
  if furn_id == "f_bathtub" then return "bath" end
  return "shower"
end

local get_mode_label = function(mode) return mode == "bath" and locale.gettext("bath") or locale.gettext("shower") end

local get_fixture_resources = function(map, pos)
  local pos_abs_ms = map:get_abs_ms(pos)
  local pos_abs_omt = coords.ms_to_omt(pos_abs_ms)
  local clean = overmapbuffer.fluid_grid_liquid_charges_at(pos_abs_omt, item_water_clean)
  local dirty = overmapbuffer.fluid_grid_liquid_charges_at(pos_abs_omt, item_water)
  local liquid = nil
  local charges = 0

  if clean > 0 then
    liquid = item_water_clean
    charges = clean
  elseif dirty > 0 then
    liquid = item_water
    charges = dirty
  end

  return {
    pos_abs_ms = pos_abs_ms,
    pos_abs_omt = pos_abs_omt,
    liquid = liquid,
    liquid_charges = charges,
  }
end

local get_hygiene_item = function(user)
  local hygiene_items = {
    { id = item_shampoo, label = locale.gettext("shampoo") },
    { id = item_soap, label = locale.gettext("soap") },
  }

  for _, candidate in ipairs(hygiene_items) do
    if candidate.id:is_valid() and user:use_charges_if_avail(candidate.id, 1) then return candidate end
  end

  return nil
end

local start_wash_activity = function(user, map, pos, mode, resources, is_warm, is_cold_wash, use_hygiene)
  local mode_data = wash_mode_data[mode]
  local water_cost = mode_data.water_charges
  if resources.liquid == nil or resources.liquid_charges < water_cost then
    local msg = mode == "bath" and locale.gettext("There is not enough water for a bath.")
      or locale.gettext("There is not enough water for a shower.")
    gapi.add_msg(MsgType.info, msg)
    return
  end

  local hygiene_item = nil
  if use_hygiene then
    hygiene_item = get_hygiene_item(user)
    if hygiene_item == nil then
      gapi.add_msg(MsgType.info, locale.gettext("You do not have any soap or shampoo to use."))
      return
    end
  end

  overmapbuffer.drain_fluid_grid_liquid_charges(resources.pos_abs_omt, resources.liquid, water_cost)
  if is_warm then
    local grid = gapi.get_distribution_grid_tracker():grid_at(resources.pos_abs_ms)
    grid:mod_resource(-mode_data.power_cost)
  end

  local duration = TimeDuration.from_minutes(mode_data.duration_minutes)
  user:assign_lua_activity({
    activity = ACT_WASH_SELF,
    duration = duration,
    callback = "PLUMBING_FINISH_WASH",
    position = pos,
    name = mode,
    values = { is_warm and 1 or 0, use_hygiene and 1 or 0, is_cold_wash and 1 or 0 },
    str_values = { hygiene_item and hygiene_item.label or "" },
  })

  local start_msg
  local mode_label = get_mode_label(mode)
  if is_warm and use_hygiene then
    start_msg = string.format(locale.gettext("You start taking a warm %s with %s."), mode_label, hygiene_item.label)
  elseif is_warm then
    start_msg = string.format(locale.gettext("You start taking a warm %s."), mode_label)
  elseif is_cold_wash and use_hygiene then
    start_msg = string.format(locale.gettext("You start taking a cold %s with %s."), mode_label, hygiene_item.label)
  elseif is_cold_wash then
    start_msg = string.format(locale.gettext("You start taking a cold %s."), mode_label)
  elseif use_hygiene then
    start_msg = string.format(locale.gettext("You start taking a %s with %s."), mode_label, hygiene_item.label)
  else
    start_msg = string.format(locale.gettext("You start taking a %s."), mode_label)
  end
  gapi.add_msg(MsgType.good, start_msg)
end

local choose_wash = function(user, map, pos, mode, resources)
  local mode_data = wash_mode_data[mode]
  local is_cold = map:get_temperature_c(pos) < warm_temperature_threshold_c
  local grid = gapi.get_distribution_grid_tracker():grid_at(resources.pos_abs_ms)
  local can_warm = is_cold and grid:is_valid() and grid:get_resource() >= mode_data.power_cost
  local has_hygiene = (item_shampoo:is_valid() and user:use_charges_if_avail(item_shampoo, 0))
    or user:use_charges_if_avail(item_soap, 0)

  local menu = UiList.new()
  menu:title(mode == "bath" and locale.gettext("Bathtub") or locale.gettext("Shower"))

  local add_wash_option = function(id, label, is_warm_option, hygiene_option)
    menu:add(id, label)
    return { id = id, is_warm = is_warm_option, use_hygiene = hygiene_option }
  end

  local options = {
    add_wash_option(
      1,
      can_warm and string.format(locale.gettext("Take a warm %s"), get_mode_label(mode))
        or string.format(locale.gettext("Take a %s"), get_mode_label(mode)),
      can_warm,
      false
    ),
  }

  if has_hygiene then
    table.insert(
      options,
      add_wash_option(
        2,
        can_warm and string.format(locale.gettext("Take a warm %s with soap or shampoo"), get_mode_label(mode))
          or string.format(locale.gettext("Take a %s with soap or shampoo"), get_mode_label(mode)),
        can_warm,
        true
      )
    )
  end

  if can_warm then
    table.insert(
      options,
      add_wash_option(3, string.format(locale.gettext("Take a cold %s"), get_mode_label(mode)), false, false)
    )
    if has_hygiene then
      table.insert(
        options,
        add_wash_option(
          4,
          string.format(locale.gettext("Take a cold %s with soap or shampoo"), get_mode_label(mode)),
          false,
          true
        )
      )
    end
  end

  if mode == "bath" then menu:add(9, locale.gettext("Manage stored contents")) end

  local choice = menu:query()
  if choice == 9 then
    gapi.call_builtin_examine("keg", gapi.get_avatar(), pos)
    return
  end

  for _, option in ipairs(options) do
    if option.id == choice then
      start_wash_activity(
        user,
        map,
        pos,
        mode,
        resources,
        option.is_warm,
        is_cold and not option.is_warm,
        option.use_hygiene
      )
      return
    end
  end
end

plumbing.examine = function(params)
  local user = params.user
  local pos = params.pos
  local map = gapi.get_map()
  local mode = get_fixture_mode(map, pos)
  local resources = get_fixture_resources(map, pos)
  choose_wash(user, map, pos, mode, resources)
end

plumbing.finish_wash = function(params)
  local user = params.user
  local activity = params.activity
  local mode = activity.name
  local mode_data = wash_mode_data[mode]
  local is_warm = activity.values[1] == 1
  local used_hygiene = activity.values[2] == 1
  local is_cold_wash = activity.values[3] == 1
  local hygiene_label = activity.str_values[2]

  user:add_morale(
    morale_feeling_good,
    mode_data.morale + (used_hygiene and mode_data.hygiene_bonus or 0),
    mode_data.morale_max,
    TimeDuration.from_hours(mode_data.morale_duration_hours),
    TimeDuration.from_minutes(mode_data.morale_decay_minutes),
    true,
    nil
  )

  if is_warm then
    local heat_duration = TimeDuration.from_minutes(mode_data.heat_minutes)
    for _, bp in ipairs(user:get_all_body_parts(true)) do
      user:add_effect(effect_hot, heat_duration, bp:str_id(), 1)
    end
  end

  local finish_msg
  local mode_label = get_mode_label(mode)
  if is_warm and used_hygiene then
    finish_msg =
      string.format(locale.gettext("You finish your warm %s feeling refreshed, clean, and cozy."), mode_label)
  elseif is_warm then
    finish_msg = string.format(locale.gettext("You finish your warm %s feeling refreshed and cozy."), mode_label)
  elseif is_cold_wash and used_hygiene then
    finish_msg = string.format(locale.gettext("You finish your cold %s feeling refreshed and clean."), mode_label)
  elseif is_cold_wash then
    finish_msg = string.format(locale.gettext("You finish your cold %s feeling refreshed."), mode_label)
  elseif used_hygiene then
    finish_msg = string.format(locale.gettext("You finish your %s feeling refreshed and clean."), mode_label)
  else
    finish_msg = string.format(locale.gettext("You finish your %s feeling refreshed."), mode_label)
  end

  if used_hygiene and hygiene_label ~= "" then
    finish_msg = string.format(locale.gettext("%s  You used %s."), finish_msg, hygiene_label)
  end

  gapi.add_msg(MsgType.good, finish_msg)
end

return plumbing

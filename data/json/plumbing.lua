local plumbing = {}

local ACT_WASH_SELF = ActivityTypeId.new("ACT_WASH_SELF")
local effect_hot = EffectTypeId.new("hot")
local item_water = ItypeId.new("water")
local item_water_clean = ItypeId.new("water_clean")
local morale_feeling_good = MoraleTypeDataId.new("morale_feeling_good")

local shower_water_charges = 24
local bath_water_charges = 80
local shower_power_cost = 500
local bath_power_cost = 1500
local warm_temperature_threshold_c = 10

local get_fixture_mode = function(map, pos)
  local furn_id = map:get_furn_at(pos):str()
  if furn_id == "f_bathtub" then return "bath" end
  return "shower"
end

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

local start_wash_activity = function(user, pos, mode, resources)
  local water_cost = mode == "bath" and bath_water_charges or shower_water_charges
  if resources.liquid == nil or resources.liquid_charges < water_cost then
    local msg = mode == "bath" and locale.gettext("There is not enough water for a bath.")
      or locale.gettext("There is not enough water for a shower.")
    gapi.add_msg(MsgType.info, msg)
    return
  end

  local power_cost = mode == "bath" and bath_power_cost or shower_power_cost
  local is_cold = map:get_temperature_c(pos) < warm_temperature_threshold_c
  local grid = gapi.get_distribution_grid_tracker():grid_at(resources.pos_abs_ms)
  local is_warm = is_cold and grid:is_valid() and grid:get_resource() >= power_cost

  overmapbuffer.drain_fluid_grid_liquid_charges(resources.pos_abs_omt, resources.liquid, water_cost)
  if is_warm then grid:mod_resource(-power_cost) end

  local duration = TimeDuration.from_minutes(mode == "bath" and 30 or 15)
  user:assign_lua_activity({
    activity = ACT_WASH_SELF,
    duration = duration,
    callback = "PLUMBING_FINISH_WASH",
    position = pos,
    name = mode,
    values = { is_warm and 1 or 0 },
  })

  local start_msg
  if mode == "bath" then
    start_msg = is_warm and locale.gettext("You start taking a warm bath.")
      or locale.gettext("You start taking a bath.")
  else
    start_msg = is_warm and locale.gettext("You start taking a warm shower.")
      or locale.gettext("You start taking a shower.")
  end
  gapi.add_msg(MsgType.good, start_msg)
end

plumbing.examine = function(params)
  local user = params.user
  local pos = params.pos
  local map = gapi.get_map()
  local mode = get_fixture_mode(map, pos)
  local resources = get_fixture_resources(map, pos)

  if mode == "bath" then
    local menu = UiList.new()
    menu:title(locale.gettext("Bathtub"))
    menu:add(1, locale.gettext("Take a bath"))
    menu:add(2, locale.gettext("Manage stored contents"))

    local choice = menu:query()
    if choice == 1 then
      start_wash_activity(user, pos, mode, resources)
    elseif choice == 2 then
      gapi.call_builtin_examine("keg", gapi.get_avatar(), pos)
    end
    return
  end

  start_wash_activity(user, pos, mode, resources)
end

plumbing.finish_wash = function(params)
  local user = params.user
  local activity = params.activity
  local mode = activity.name
  local is_warm = activity.values[1] == 1

  user:add_morale(
    morale_feeling_good,
    mode == "bath" and 8 or 5,
    20,
    TimeDuration.from_hours(2),
    TimeDuration.from_minutes(30),
    true,
    nil
  )

  if is_warm then
    local heat_duration = TimeDuration.from_minutes(mode == "bath" and 35 or 20)
    for _, bp in ipairs(user:get_all_body_parts(true)) do
      user:add_effect(effect_hot, heat_duration, bp:str_id(), 1)
    end
  end

  local finish_msg
  if mode == "bath" then
    finish_msg = is_warm and locale.gettext("You finish your warm bath feeling refreshed and cozy.")
      or locale.gettext("You finish your bath feeling refreshed.")
  else
    finish_msg = is_warm and locale.gettext("You finish your warm shower feeling refreshed and cozy.")
      or locale.gettext("You finish your shower feeling refreshed.")
  end

  gapi.add_msg(MsgType.good, finish_msg)
end

return plumbing

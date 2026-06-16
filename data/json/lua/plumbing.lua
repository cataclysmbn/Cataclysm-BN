---@class PlumbingModeData
---@field duration_minutes integer
---@field water_charges integer
---@field power_cost integer
---@field morale integer
---@field morale_duration_hours integer
---@field morale_decay_minutes integer
---@field heat_minutes integer
---@field hygiene_bonus integer

---@class PlumbingFixtureResources
---@field pos_abs_ms TripointAbsMs
---@field pos_abs_omt TripointAbsOmt
---@field clean_charges integer
---@field dirty_charges integer
---@field liquid ItypeId?
---@field liquid_charges integer
---@field source string

---@class PlumbingConsumableCandidate
---@field item Item
---@field source string
---@field pos TripointBubMs?

---@class PlumbingTowelCandidate
---@field item Item?
---@field label string

---@class PlumbingWashContext
---@field user Character
---@field map Map
---@field pos TripointBubMs
---@field mode string
---@field mode_data PlumbingModeData
---@field resources PlumbingFixtureResources
---@field is_cold_weather boolean
---@field bloody_tile_count integer

---@class PlumbingWashChoice
---@field id integer
---@field is_warm boolean
---@field use_hygiene boolean

---@class PlumbingStartOptions
---@field context PlumbingWashContext
---@field is_warm boolean
---@field use_hygiene boolean

---@class PlumbingWashActivityData
---@field mode string
---@field is_warm boolean
---@field used_hygiene boolean
---@field is_cold_wash boolean
---@field cleaner_label string

---@class PlumbingVolumeOptions
---@field charges integer
---@field liquid_type ItypeId

local plumbing = {}

local ACT_WASH_SELF = ActivityTypeId.new("ACT_WASH_SELF")
local effect_hot = EffectTypeId.new("hot")
local item_battery = ItypeId.new("battery")
local item_towel = ItypeId.new("towel")
local item_towel_wet = ItypeId.new("towel_wet")
local item_water = ItypeId.new("water")
local item_water_clean = ItypeId.new("water_clean")
local morale_shower = MoraleTypeDataId.new("morale_shower")
local morale_bath = MoraleTypeDataId.new("morale_bath")
local morale_cleansed_self = MoraleTypeDataId.new("morale_cleansed_self")
local flag_body_cleanser = JsonFlagId.new("BODY_CLEANSER")

local resource_source = {
  grid = "grid",
  vehicle = "vehicle",
}

local vehicle_shower_feature = "SHOWER"
local vehicle_towel_feature = "TOWEL"

local blood_field_ids = {
  FieldTypeId.new("fd_blood"):int_id(),
  FieldTypeId.new("fd_gibs_flesh"):int_id(),
  FieldTypeId.new("fd_blood_veggy"):int_id(),
  FieldTypeId.new("fd_gibs_veggy"):int_id(),
  FieldTypeId.new("fd_blood_insect"):int_id(),
  FieldTypeId.new("fd_gibs_insect"):int_id(),
  FieldTypeId.new("fd_blood_invertebrate"):int_id(),
  FieldTypeId.new("fd_gibs_invertebrate"):int_id(),
}

---@type table<string, PlumbingModeData>
local wash_mode_data = {
  shower = {
    duration_minutes = 15,
    water_charges = 24,
    power_cost = 500,
    morale = 6,
    morale_duration_hours = 2,
    morale_decay_minutes = 30,
    heat_minutes = 20,
    hygiene_bonus = 4,
  },
  bath = {
    duration_minutes = 45,
    water_charges = 180,
    power_cost = 1500,
    morale = 14,
    morale_duration_hours = 5,
    morale_decay_minutes = 45,
    heat_minutes = 45,
    hygiene_bonus = 8,
  },
}

local warm_temperature_threshold_c = 10
local wash_mode = {
  shower = "shower",
  bath = "bath",
}

---@param mode string
---@return string
local get_mode_label =
  function(mode) return mode == wash_mode.bath and locale.gettext("bath") or locale.gettext("shower") end

---@param mode string
---@return string
local get_fixture_title =
  function(mode) return mode == wash_mode.bath and locale.gettext("Bathtub") or locale.gettext("Shower Booth") end

---@param is_warm boolean
---@param is_cold boolean
---@return string
local get_temperature_adjective = function(is_warm, is_cold)
  if is_warm then return locale.gettext("warm ") end
  return is_cold and locale.gettext("cold ") or ""
end

---@param value number
---@return number
local round_to_tenth = function(value) return math.floor(value * 10 + 0.5) / 10 end

---@param opts PlumbingVolumeOptions
---@return number
local charges_to_liters = function(opts)
  local charges_per_liter = opts.liquid_type:obj():charges_per_volume(Volume.from_liter(1))
  if charges_per_liter <= 0 then return opts.charges end
  return round_to_tenth(opts.charges / charges_per_liter)
end

---@param opts { user: Character, pos: TripointBubMs, mode: string }
---@return boolean
local ensure_player_on_fixture = function(opts)
  local user_pos = opts.user:get_pos_ms()
  if user_pos == opts.pos then return true end

  local mode_label = get_mode_label(opts.mode)
  gapi.add_msg(MsgType.info, string.format(locale.gettext("You need to stand on the %s tile to use it."), mode_label))
  return false
end

---@param opts { map: Map, pos: TripointBubMs }
---@return PlumbingFixtureResources
local get_fixture_resources = function(opts)
  local pos_abs_ms = opts.map:bub_to_abs(opts.pos)
  local pos_abs_omt = pos_abs_ms:to_omt()
  ---@cast pos_abs_omt TripointAbsOmt
  local source = resource_source.grid
  local clean_charges = overmapbuffer.fluid_grid_liquid_charges_at(pos_abs_omt, item_water_clean)
  local dirty_charges = overmapbuffer.fluid_grid_liquid_charges_at(pos_abs_omt, item_water)
  local liquid = nil
  local liquid_charges = 0

  if opts.map:has_vehicle_part_with_feature_at(opts.pos, vehicle_shower_feature, true) then
    source = resource_source.vehicle
    clean_charges = opts.map:get_vehicle_fuel_left_at(opts.pos, vehicle_shower_feature, item_water_clean, false)
    dirty_charges = opts.map:get_vehicle_fuel_left_at(opts.pos, vehicle_shower_feature, item_water, false)
  end

  if clean_charges > 0 then
    liquid = item_water_clean
    liquid_charges = clean_charges
  elseif dirty_charges > 0 then
    liquid = item_water
    liquid_charges = dirty_charges
  end

  return {
    pos_abs_ms = pos_abs_ms,
    pos_abs_omt = pos_abs_omt,
    clean_charges = clean_charges,
    dirty_charges = dirty_charges,
    liquid = liquid,
    liquid_charges = liquid_charges,
    source = source,
  }
end

---@param opts { map: Map, center: TripointBubMs }
---@return integer
local count_bloody_tiles = function(opts)
  local bloody_tiles = 0
  for _, tile in pairs(opts.map:points_in_radius(opts.center, 1, 0)) do
    local has_blood = false
    for _, field_id in ipairs(blood_field_ids) do
      if opts.map:get_field_int_at(tile, field_id) > 0 then
        has_blood = true
        break
      end
    end
    if has_blood then bloody_tiles = bloody_tiles + 1 end
  end
  return bloody_tiles
end

---@param opts { user: Character, map: Map, center: TripointBubMs }
---@return PlumbingConsumableCandidate[]
local collect_body_cleanser_candidates = function(opts)
  local candidates = {}

  for _, item in ipairs(opts.user:all_items_with_flag(flag_body_cleanser, true)) do
    table.insert(candidates, { item = item, source = "inventory" })
  end

  for _, tile in pairs(opts.map:points_in_radius(opts.center, 1, 0)) do
    for _, item in pairs(opts.map:get_items_at(tile):items()) do
      if item:has_flag(flag_body_cleanser) then
        table.insert(candidates, { item = item, source = "map", pos = tile })
      end
    end
  end

  return candidates
end

---@param opts { user: Character, map: Map, candidate: PlumbingConsumableCandidate }
---@return string
local consume_body_cleanser_candidate = function(opts)
  local label = opts.candidate.item:display_name(1)
  if opts.candidate.source == "inventory" then
    if opts.candidate.item.charges > 1 then
      opts.user:use_charges(opts.candidate.item:get_type(), 1)
    else
      opts.user:remove_item(opts.candidate.item)
    end
  elseif opts.candidate.pos ~= nil then
    if opts.candidate.item.charges > 1 then
      opts.candidate.item.charges = opts.candidate.item.charges - 1
    else
      opts.map:remove_item_at(opts.candidate.pos, opts.candidate.item)
    end
  end
  return label
end

---@param opts { user: Character, map: Map, center: TripointBubMs }
---@return PlumbingTowelCandidate[]
local collect_dry_towel_candidates = function(opts)
  local candidates = {}

  for _, item in ipairs(opts.user:all_items(true)) do
    if item:get_type() == item_towel then table.insert(candidates, { item = item, label = item:display_name(1) }) end
  end

  for _, tile in pairs(opts.map:points_in_radius(opts.center, 1, 0)) do
    for _, item in pairs(opts.map:get_items_at(tile):items()) do
      if item:get_type() == item_towel then table.insert(candidates, { item = item, label = item:display_name(1) }) end
    end

    if opts.map:has_vehicle_part_with_feature_at(tile, vehicle_towel_feature, true) then
      table.insert(candidates, { label = locale.gettext("the vehicle towel hanger") })
    end
  end

  return candidates
end

---@param opts { user: Character, morale_type: MoraleTypeDataId, bonus: integer, duration: TimeDuration, decay_start: TimeDuration }
---@return nil
local refresh_morale = function(opts)
  local current = opts.user:get_morale(opts.morale_type)
  local delta = current < opts.bonus and opts.bonus - current or 0
  opts.user:add_morale(opts.morale_type, delta, opts.bonus, opts.duration, opts.decay_start, true, nil)
end

---@param opts { user: Character, map: Map, center: TripointBubMs }
---@return string
local wet_dry_towel = function(opts)
  local candidate = collect_dry_towel_candidates({ user = opts.user, map = opts.map, center = opts.center })[1]
  if candidate == nil then return "" end

  if candidate.item ~= nil then candidate.item:convert(item_towel_wet) end
  return candidate.label
end

---@param opts { context: PlumbingWashContext }
---@return string
local build_water_failure_message = function(opts)
  local needed_liters =
    charges_to_liters({ charges = opts.context.mode_data.water_charges, liquid_type = item_water_clean })
  local clean_liters =
    charges_to_liters({ charges = opts.context.resources.clean_charges, liquid_type = item_water_clean })
  local dirty_liters = charges_to_liters({ charges = opts.context.resources.dirty_charges, liquid_type = item_water })
  return string.format(
    locale.gettext("Need %.1f L of water for a %s, currently: %.1f L clean / %.1f L regular."),
    needed_liters,
    get_mode_label(opts.context.mode),
    clean_liters,
    dirty_liters
  )
end

---@param opts { context: PlumbingWashContext }
---@return string
local build_bloody_room_message = function(opts)
  return string.format(
    locale.gettext("This %s is too bloody to use.  Blood covers %d tile(s).  Clean it first."),
    get_mode_label(opts.context.mode),
    opts.context.bloody_tile_count
  )
end

---@return string
local build_body_cleanser_failure_message = function()
  return locale.gettext("Need 1 body cleanser nearby or in inventory to cleanse yourself.")
end

---@param opts { context: PlumbingWashContext }
---@return boolean
local clean_bloody_room = function(opts)
  if opts.context.bloody_tile_count == 0 then return true end

  for _, tile in pairs(opts.context.map:points_in_radius(opts.context.pos, 1, 0)) do
    for _, field_id in ipairs(blood_field_ids) do
      opts.context.map:remove_field_at(tile, field_id)
    end
  end

  gapi.add_msg(
    MsgType.good,
    string.format(locale.gettext("You clean the blood from the %s."), get_mode_label(opts.context.mode))
  )
  return true
end

---@param opts { context: PlumbingWashContext }
---@return PlumbingWashChoice[]
local get_wash_choices = function(opts)
  local choices = {
    { id = 1, is_warm = false, use_hygiene = false },
    { id = 2, is_warm = false, use_hygiene = true },
  }

  if opts.context.is_cold_weather then
    table.insert(choices, { id = 3, is_warm = true, use_hygiene = false })
    table.insert(choices, { id = 4, is_warm = true, use_hygiene = true })
  end

  return choices
end

---@param opts { context: PlumbingWashContext, choice: PlumbingWashChoice }
---@return string
local get_choice_label = function(opts)
  local label = string.format(
    locale.gettext("Take a %s%s"),
    get_temperature_adjective(opts.choice.is_warm, opts.context.is_cold_weather),
    get_mode_label(opts.context.mode)
  )
  if opts.choice.use_hygiene then label = string.format(locale.gettext("%s and cleanse yourself"), label) end
  return label
end

---@param opts PlumbingStartOptions
---@return boolean
local start_wash_activity = function(opts)
  if
    opts.context.resources.liquid == nil
    or opts.context.resources.liquid_charges < opts.context.mode_data.water_charges
  then
    gapi.add_msg(MsgType.info, build_water_failure_message({ context = opts.context }))
    return false
  end

  if opts.is_warm then
    if opts.context.resources.source == resource_source.vehicle then
      local available_power = opts.context.map:get_vehicle_fuel_left_at(
        opts.context.pos,
        vehicle_shower_feature,
        item_battery,
        true
      )
      if available_power < opts.context.mode_data.power_cost then
        gapi.add_msg(
          MsgType.info,
          string.format(
            locale.gettext("Need %d power for a warm %s, currently: %d."),
            opts.context.mode_data.power_cost,
            get_mode_label(opts.context.mode),
            available_power
          )
        )
        return false
      end

      opts.context.map:drain_vehicle_fuel_at(
        opts.context.pos,
        vehicle_shower_feature,
        item_battery,
        opts.context.mode_data.power_cost
      )
    else
      local grid = gapi.get_distribution_grid_tracker():grid_at(opts.context.resources.pos_abs_ms)
      if not grid:is_valid() then
        gapi.add_msg(
          MsgType.info,
          string.format(
            locale.gettext("Need an electric grid with %d power for a warm %s, currently: no grid."),
            opts.context.mode_data.power_cost,
            get_mode_label(opts.context.mode)
          )
        )
        return false
      end

      if grid:get_resource() < opts.context.mode_data.power_cost then
        gapi.add_msg(
          MsgType.info,
          string.format(
            locale.gettext("Need %d power for a warm %s, currently: %d."),
            opts.context.mode_data.power_cost,
            get_mode_label(opts.context.mode),
            grid:get_resource()
          )
        )
        return false
      end

      grid:mod_resource(-opts.context.mode_data.power_cost)
    end
  end

  local cleaner_label = ""
  if opts.use_hygiene then
    local candidate = collect_body_cleanser_candidates({
      user = opts.context.user,
      map = opts.context.map,
      center = opts.context.pos,
    })[1]
    if candidate == nil then
      gapi.add_msg(MsgType.info, build_body_cleanser_failure_message())
      return false
    end

    cleaner_label = consume_body_cleanser_candidate({
      user = opts.context.user,
      map = opts.context.map,
      candidate = candidate,
    })
  end

  if opts.context.resources.source == resource_source.vehicle then
    opts.context.map:drain_vehicle_fuel_at(
      opts.context.pos,
      vehicle_shower_feature,
      opts.context.resources.liquid,
      opts.context.mode_data.water_charges
    )
  else
    overmapbuffer.drain_fluid_grid_liquid_charges(
      opts.context.resources.pos_abs_omt,
      opts.context.resources.liquid,
      opts.context.mode_data.water_charges
    )
  end

  opts.context.user:assign_lua_activity({
    type = ACT_WASH_SELF,
    duration = TimeDuration.from_minutes(opts.context.mode_data.duration_minutes),
    on_finish = "PLUMBING_FINISH_WASH",
    pos = opts.context.pos,
    name = opts.context.mode,
    data = {
      mode = opts.context.mode,
      is_warm = opts.is_warm,
      used_hygiene = opts.use_hygiene,
      is_cold_wash = opts.context.is_cold_weather and not opts.is_warm,
      cleaner_label = cleaner_label,
    },
  })

  local adjective = get_temperature_adjective(opts.is_warm, opts.context.is_cold_weather)
  local message =
    string.format(locale.gettext("You start taking a %s%s."), adjective, get_mode_label(opts.context.mode))
  if opts.use_hygiene and cleaner_label ~= "" then
    message = string.format(
      locale.gettext("You start taking a %s%s with %s."),
      adjective,
      get_mode_label(opts.context.mode),
      cleaner_label
    )
  end
  gapi.add_msg(MsgType.good, message)
  return true
end

---@param opts { context: PlumbingWashContext }
---@return nil
local choose_wash = function(opts)
  local menu = UiList.new()
  menu:title(get_fixture_title(opts.context.mode))

  local choices = get_wash_choices({ context = opts.context })
  for _, choice in ipairs(choices) do
    menu:add(choice.id, get_choice_label({ context = opts.context, choice = choice }))
  end

  if opts.context.mode == wash_mode.bath then menu:add(9, locale.gettext("Manage stored contents")) end

  local choice_id = menu:query()
  if choice_id == 9 then
    gapi.call_builtin_examine("keg", gapi.get_avatar(), opts.context.pos)
    return
  end

  for _, choice in ipairs(choices) do
    if choice.id == choice_id then
      start_wash_activity({
        context = opts.context,
        is_warm = choice.is_warm,
        use_hygiene = choice.use_hygiene,
      })
      return
    end
  end
end

---@param opts { context: PlumbingWashContext }
---@return nil
local examine_context = function(opts)
  if not ensure_player_on_fixture({ user = opts.context.user, pos = opts.context.pos, mode = opts.context.mode }) then
    return
  end

  if opts.context.bloody_tile_count > 0 then
    gapi.add_msg(MsgType.info, build_bloody_room_message({ context = opts.context }))

    local menu = UiList.new()
    menu:title(get_fixture_title(opts.context.mode))
    menu:add(1, locale.gettext("Clean the blood away"))
    if opts.context.mode == wash_mode.bath then menu:add(9, locale.gettext("Manage stored contents")) end

    local choice_id = menu:query()
    if choice_id == 1 then
      clean_bloody_room({ context = opts.context })
    elseif choice_id == 9 then
      gapi.call_builtin_examine("keg", gapi.get_avatar(), opts.context.pos)
    end
    return
  end

  choose_wash({ context = opts.context })
end

---@param params { user: Character, pos: TripointBubMs }
---@param mode string
---@return nil
local examine = function(params, mode)
  local map = gapi.get_map()
  local context = {
    user = params.user,
    map = map,
    pos = params.pos,
    mode = mode,
    mode_data = wash_mode_data[mode],
    resources = get_fixture_resources({ map = map, pos = params.pos }),
    is_cold_weather = map:get_temperature_c(params.pos) < warm_temperature_threshold_c,
    bloody_tile_count = count_bloody_tiles({ map = map, center = params.pos }),
  }
  examine_context({ context = context })
end

---@param params { user: Character, pos: TripointBubMs }
---@return nil
plumbing.examine_shower = function(params) examine(params, wash_mode.shower) end

---@param params { user: Character, pos: TripointBubMs }
---@return nil
plumbing.examine_bathtub = function(params) examine(params, wash_mode.bath) end

---@param params LuaActivityFinishParams|{ user: Character, activity: PlayerActivity, data: PlumbingWashActivityData }
---@return nil
plumbing.finish_wash = function(params)
  local data = params.data or {}
  local mode = data.mode or params.activity.name
  local mode_data = wash_mode_data[mode]
  local is_warm = data.is_warm == true
  local used_hygiene = data.used_hygiene == true
  local is_cold_wash = data.is_cold_wash == true
  local cleaner_label = data.cleaner_label or ""
  local mode_label = get_mode_label(mode)

  local base_morale_type = mode == wash_mode.bath and morale_bath or morale_shower
  local morale_duration = TimeDuration.from_hours(mode_data.morale_duration_hours)
  local morale_decay = TimeDuration.from_minutes(mode_data.morale_decay_minutes)

  refresh_morale({
    user = params.user,
    morale_type = base_morale_type,
    bonus = mode_data.morale,
    duration = morale_duration,
    decay_start = morale_decay,
  })

  if used_hygiene then
    refresh_morale({
      user = params.user,
      morale_type = morale_cleansed_self,
      bonus = mode_data.hygiene_bonus,
      duration = morale_duration,
      decay_start = morale_decay,
    })
  end

  if is_warm then
    local heat_duration = TimeDuration.from_minutes(mode_data.heat_minutes)
    for _, bp in ipairs(params.user:get_all_body_parts(true)) do
      local current_temp = params.user:get_part_temp_btu(bp)
      local target_temp = math.min(math.max(current_temp + 1000, gapi.bodytemp_norm()), gapi.bodytemp_hot())
      params.user:set_part_temp_btu(bp, target_temp)
      params.user:add_effect(effect_hot, heat_duration, bp:str_id(), 1)
    end
  elseif is_cold_wash then
    for _, bp in ipairs(params.user:get_all_body_parts(true)) do
      local current_temp = params.user:get_part_temp_btu(bp)
      local target_temp = math.max(current_temp - 1200, gapi.bodytemp_cold())
      params.user:set_part_temp_btu(bp, target_temp)
    end
  end

  local map = gapi.get_map()
  local wash_pos = params.pos and map:abs_to_bub(params.pos) or params.user:get_pos_ms()
  local towel_label = wet_dry_towel({ user = params.user, map = map, center = wash_pos })
  if towel_label == "" then
    params.user:drench(100, params.user:get_all_body_parts(true), true)
  end

  local adjective = get_temperature_adjective(is_warm, is_cold_wash)
  local finish_msg = string.format(locale.gettext("You finish your %s%s feeling refreshed."), adjective, mode_label)
  if used_hygiene and cleaner_label ~= "" then
    finish_msg = string.format(locale.gettext("%s  You cleansed yourself with %s."), finish_msg, cleaner_label)
  end
  if towel_label ~= "" then
    finish_msg = string.format(locale.gettext("%s  You dry off with %s."), finish_msg, towel_label)
  else
    finish_msg = string.format(locale.gettext("%s  Without a dry towel, you are soaked."), finish_msg)
  end
  gapi.add_msg(MsgType.good, finish_msg)
end

return plumbing

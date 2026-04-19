---@class PlumbingModeData
---@field duration_minutes integer
---@field water_charges integer
---@field power_cost integer
---@field morale integer
---@field morale_max integer
---@field morale_duration_hours integer
---@field morale_decay_minutes integer
---@field heat_minutes integer
---@field hygiene_bonus integer

---@class PlumbingFixtureResources
---@field pos_abs_ms Tripoint
---@field pos_abs_omt Tripoint
---@field clean_charges integer
---@field dirty_charges integer
---@field liquid ItypeId?
---@field liquid_charges integer

---@class PlumbingConsumableCandidate
---@field item Item
---@field source string
---@field pos Tripoint?

---@class PlumbingWashContext
---@field user Character
---@field map Map
---@field pos Tripoint
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

---@class PlumbingVolumeOptions
---@field charges integer
---@field liquid_type ItypeId

local plumbing = {}

local ACT_WASH_SELF = ActivityTypeId.new("ACT_WASH_SELF")
local effect_hot = EffectTypeId.new("hot")
local item_water = ItypeId.new("water")
local item_water_clean = ItypeId.new("water_clean")
local morale_cold_shower = MoraleTypeDataId.new("morale_cold_shower")
local morale_shower = MoraleTypeDataId.new("morale_shower")
local morale_warm_shower = MoraleTypeDataId.new("morale_warm_shower")
local morale_cold_bath = MoraleTypeDataId.new("morale_cold_bath")
local morale_bath = MoraleTypeDataId.new("morale_bath")
local morale_warm_bath = MoraleTypeDataId.new("morale_warm_bath")
local morale_cleansed_self = MoraleTypeDataId.new("morale_cleansed_self")
local flag_washing_cleaner = JsonFlagId.new("WASHING_CLEANER")

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

local wash_mode_data = {
  shower = {
    duration_minutes = 15,
    water_charges = 24,
    power_cost = 500,
    morale = 6,
    morale_max = 20,
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
    morale_max = 30,
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

---@param opts { user: Character, pos: Tripoint, mode: string }
---@return boolean
local ensure_player_on_fixture = function(opts)
  local user_pos = opts.user:get_pos_ms()
  if user_pos == opts.pos then return true end

  local mode_label = get_mode_label(opts.mode)
  local distance = coords.rl_dist(user_pos, opts.pos)
  if distance == 1 then
    if not opts.user:can_move_to(opts.pos, true) then
      gapi.add_msg(MsgType.info, string.format(locale.gettext("You cannot step onto the %s tile."), mode_label))
      return false
    end

    opts.user:move_to(opts.pos, false, false, 0.0)
    return false
  end

  gapi.add_msg(MsgType.info, string.format(locale.gettext("You need to stand on the %s tile to use it."), mode_label))
  return false
end

---@param opts { map: Map, pos: Tripoint }
---@return PlumbingFixtureResources
local get_fixture_resources = function(opts)
  local pos_abs_ms = opts.map:get_abs_ms(opts.pos)
  local pos_abs_omt = coords.ms_to_omt(pos_abs_ms)
  local clean_charges = overmapbuffer.fluid_grid_liquid_charges_at(pos_abs_omt, item_water_clean)
  local dirty_charges = overmapbuffer.fluid_grid_liquid_charges_at(pos_abs_omt, item_water)
  local liquid = nil
  local liquid_charges = 0

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
  }
end

---@param opts { map: Map, center: Tripoint }
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

---@param opts { user: Character, map: Map, center: Tripoint }
---@return PlumbingConsumableCandidate[]
local collect_cleaner_candidates = function(opts)
  local candidates = {}

  for _, item in ipairs(opts.user:all_items_with_flag(flag_washing_cleaner, true)) do
    table.insert(candidates, { item = item, source = "inventory" })
  end

  for _, tile in pairs(opts.map:points_in_radius(opts.center, 1, 0)) do
    for _, item in pairs(opts.map:get_items_at(tile):items()) do
      if item:has_flag(flag_washing_cleaner) then
        table.insert(candidates, { item = item, source = "map", pos = tile })
      end
    end
  end

  return candidates
end

---@param opts { candidates: PlumbingConsumableCandidate[] }
---@return PlumbingConsumableCandidate?
local choose_cleaner_candidate = function(opts) return opts.candidates[1] end

---@param opts { user: Character, map: Map, candidate: PlumbingConsumableCandidate }
---@return string
local consume_cleaner_candidate = function(opts)
  local label = opts.candidate.item:display_name(1)
  if opts.candidate.source == "inventory" then
    if opts.candidate.item.charges > 1 then
      opts.user:use_charges(opts.candidate.item:get_type(), 1, false)
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
    locale.gettext("This %s is too bloody to use.  Blood covers %d tile(s).  Clean it with a washing cleaner first."),
    get_mode_label(opts.context.mode),
    opts.context.bloody_tile_count
  )
end

---@param opts { context: PlumbingWashContext, use_hygiene: boolean }
---@return string
local build_cleaner_failure_message = function(opts)
  if opts.use_hygiene then
    return locale.gettext("Need 1 washing cleaner nearby or in inventory to cleanse yourself.")
  end
  return locale.gettext("Need 1 washing cleaner nearby or in inventory to clean this washroom.")
end

---@param opts { context: PlumbingWashContext }
---@return boolean
local clean_bloody_room = function(opts)
  if opts.context.bloody_tile_count == 0 then return true end

  local candidates = collect_cleaner_candidates({
    user = opts.context.user,
    map = opts.context.map,
    center = opts.context.pos,
  })
  local candidate = choose_cleaner_candidate({ candidates = candidates })
  if candidate == nil then
    gapi.add_msg(MsgType.info, build_cleaner_failure_message({ context = opts.context, use_hygiene = false }))
    return false
  end

  local cleaner_label = consume_cleaner_candidate({
    user = opts.context.user,
    map = opts.context.map,
    candidate = candidate,
  })

  for _, tile in pairs(opts.context.map:points_in_radius(opts.context.pos, 1, 0)) do
    for _, field_id in ipairs(blood_field_ids) do
      opts.context.map:remove_field_at(tile, field_id)
    end
  end

  gapi.add_msg(
    MsgType.good,
    string.format(
      locale.gettext("You clean the blood from the %s with %s."),
      get_mode_label(opts.context.mode),
      cleaner_label
    )
  )
  return true
end

---@param opts { context: PlumbingWashContext }
---@return MoraleTypeDataId
local get_base_morale_type = function(opts)
  if opts.context.mode == wash_mode.bath then
    return opts.context.is_cold_weather and morale_cold_bath or morale_warm_bath
  end

  return opts.context.is_cold_weather and morale_cold_shower or morale_warm_shower
end

---@param opts { context: PlumbingWashContext }
---@return PlumbingWashChoice[]
local get_wash_choices = function(opts)
  local choices = {
    {
      id = 1,
      is_warm = false,
      use_hygiene = false,
    },
  }

  table.insert(choices, {
    id = 2,
    is_warm = false,
    use_hygiene = true,
  })

  if opts.context.is_cold_weather then
    table.insert(choices, {
      id = 3,
      is_warm = true,
      use_hygiene = false,
    })
    table.insert(choices, {
      id = 4,
      is_warm = true,
      use_hygiene = true,
    })
  end

  return choices
end

---@param opts { context: PlumbingWashContext, choice: PlumbingWashChoice }
---@return string
local get_choice_label = function(opts)
  local adjective = ""
  if opts.choice.is_warm then
    adjective = locale.gettext("warm ")
  elseif opts.context.is_cold_weather then
    adjective = locale.gettext("cold ")
  end

  local label = string.format(locale.gettext("Take a %s%s"), adjective, get_mode_label(opts.context.mode))
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

  local cleaner_label = ""
  if opts.use_hygiene then
    local candidates = collect_cleaner_candidates({
      user = opts.context.user,
      map = opts.context.map,
      center = opts.context.pos,
    })
    local candidate = choose_cleaner_candidate({ candidates = candidates })
    if candidate == nil then
      gapi.add_msg(MsgType.info, build_cleaner_failure_message({ context = opts.context, use_hygiene = true }))
      return false
    end

    cleaner_label = consume_cleaner_candidate({
      user = opts.context.user,
      map = opts.context.map,
      candidate = candidate,
    })
  end

  overmapbuffer.drain_fluid_grid_liquid_charges(
    opts.context.resources.pos_abs_omt,
    opts.context.resources.liquid,
    opts.context.mode_data.water_charges
  )

  opts.context.user:assign_lua_activity({
    activity = ACT_WASH_SELF,
    duration = TimeDuration.from_minutes(opts.context.mode_data.duration_minutes),
    callback = "PLUMBING_FINISH_WASH",
    position = opts.context.pos,
    name = opts.context.mode,
    values = {
      opts.is_warm and 1 or 0,
      opts.use_hygiene and 1 or 0,
      opts.context.is_cold_weather and not opts.is_warm and 1 or 0,
    },
    str_values = { cleaner_label },
  })

  local adjective = ""
  if opts.is_warm then
    adjective = locale.gettext("warm ")
  elseif opts.context.is_cold_weather then
    adjective = locale.gettext("cold ")
  end

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
  menu:title(opts.context.mode == wash_mode.bath and locale.gettext("Bathtub") or locale.gettext("Shower Booth"))

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
    menu:title(opts.context.mode == wash_mode.bath and locale.gettext("Bathtub") or locale.gettext("Shower Booth"))
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

---@param params { user: Character, pos: Tripoint }
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

---@param params { user: Character, pos: Tripoint }
---@return nil
plumbing.examine_shower = function(params) examine(params, wash_mode.shower) end

---@param params { user: Character, pos: Tripoint }
---@return nil
plumbing.examine_bathtub = function(params) examine(params, wash_mode.bath) end

---@param params { user: Character, activity: PlayerActivity }
---@return nil
plumbing.finish_wash = function(params)
  local mode = params.activity.name
  local mode_data = wash_mode_data[mode]
  local is_warm = params.activity.values[1] == 1
  local used_hygiene = params.activity.values[2] == 1
  local is_cold_wash = params.activity.values[3] == 1
  local cleaner_label = params.activity.str_values[2]
  local mode_label = get_mode_label(mode)

  local base_morale_type = mode == wash_mode.bath and morale_bath or morale_shower
  if is_warm then
    base_morale_type = mode == wash_mode.bath and morale_warm_bath or morale_warm_shower
  elseif is_cold_wash then
    base_morale_type = mode == wash_mode.bath and morale_cold_bath or morale_cold_shower
  end

  params.user:add_morale(
    base_morale_type,
    mode_data.morale,
    mode_data.morale_max,
    TimeDuration.from_hours(mode_data.morale_duration_hours),
    TimeDuration.from_minutes(mode_data.morale_decay_minutes),
    true,
    nil
  )

  if used_hygiene then
    params.user:add_morale(
      morale_cleansed_self,
      mode_data.hygiene_bonus,
      mode_data.morale_max,
      TimeDuration.from_hours(mode_data.morale_duration_hours),
      TimeDuration.from_minutes(mode_data.morale_decay_minutes),
      true,
      nil
    )
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

  local adjective = ""
  if is_warm then
    adjective = locale.gettext("warm ")
  elseif is_cold_wash then
    adjective = locale.gettext("cold ")
  end

  local finish_msg = string.format(locale.gettext("You finish your %s%s feeling refreshed."), adjective, mode_label)
  if used_hygiene and cleaner_label ~= "" then
    finish_msg = string.format(locale.gettext("%s  You cleansed yourself with %s."), finish_msg, cleaner_label)
  end
  gapi.add_msg(MsgType.good, finish_msg)
end

return plumbing

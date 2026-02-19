local lua_traits = {}

local trait_nyctophobia = MutationBranchId.new("NYCTOPHOBIA")
local trait_claustrophobia = MutationBranchId.new("CLAUSTROPHOBIA")
local trait_agoraphobia = MutationBranchId.new("AGORAPHOBIA")
local trait_clutter_intolerant = MutationBranchId.new("CLUTTER_INTOLERANT")
local seen_clutter = false

local effect_depressants = EffectTypeId.new("depressants")
local effect_downed = EffectTypeId.new("downed")
local effect_shakes = EffectTypeId.new("shakes")

local morale_indoor_misery = MoraleTypeDataId.new("morale_indoor_misery")
local morale_outdoor_misery = MoraleTypeDataId.new("morale_outdoor_misery")
local morale_clutter_intolerant = MoraleTypeDataId.new("morale_clutter_intolerant")

local clutter_radius = 4
local clutter_threshold = 12
local clutter_step = 5
local max_penalty = 30

local in_darkness_alert = false

---@return number
local function nyctophobia_threshold()
  return gapi.light_ambient_lit() - 3.0
end

---@param duration TimeDuration
---@return boolean
local function one_turn_in(duration)
  local turns = duration:to_turns()
  if turns <= 0 then return false end
  return gapi.rng(1, turns) == 1
end

---@generic T
---@param list T[]
---@return T|nil
local function random_entry(list)
  if #list == 0 then return nil end
  local idx = gapi.rng(1, #list)
  return list[idx]
end

---@param map Map
---@param pt Tripoint
---@return boolean
local function is_passable(map, pt)
  local ter = map:get_ter_at(pt):obj()
  if ter:has_flag("IMPASSABLE") or ter:get_movecost() <= 0 then return false end
  local furn = map:get_furn_at(pt):obj()
  if furn:has_flag("IMPASSABLE") then return false end
  return true
end

---@param who Character
---@param morale_id MoraleTypeDataId
---@param penalty integer
local function apply_penalty(who, morale_id, penalty)
  local magnitude = math.min(math.max(penalty, 0), max_penalty)
  if magnitude <= 0 then
    who:rem_morale(morale_id)
    return
  end

  who:add_morale(morale_id, -magnitude, -magnitude, TimeDuration.from_minutes(1), TimeDuration.from_minutes(1), true, nil)
end

---@param who Character
---@param amount integer
---@param minimum integer
local function drain_focus(who, amount, minimum)
  if not who.focus_pool then return end
  local floor = minimum or 0
  local target = who.focus_pool - amount
  if target < floor then target = floor end
  who.focus_pool = target
end

---@param here Map
---@param pt Tripoint
---@return boolean
local function is_loot_on_floor(here, pt)
  local furn = here:get_furn_at(pt):obj()
  if furn and (furn:has_flag("CONTAINER") or furn:has_flag("SEALED")) then return false end
  return true
end

---@param here Map
---@param center Tripoint
---@return integer
local function count_loose_items(here, center)
  local you = gapi.get_avatar()
  if not you then return 0 end
  local total = 0
  local visibility_radius = 15
  local seen = {}

  for _, pt in ipairs(here:points_in_radius(center, visibility_radius)) do
    if pt.z == center.z and you:sees(pt) and coords.rl_dist(center, pt) <= clutter_radius then
      local key = string.format("%d:%d:%d", pt.x, pt.y, pt.z)
      if not seen[key] and is_loot_on_floor(here, pt) then
        seen[key] = true
        for _ in pairs(here:get_items_at(pt)) do
          total = total + 1
        end
      end
    end
  end
  return total
end

---@param count integer
---@return integer
local function clutter_penalty(count)
  if count <= clutter_threshold then return 0 end
  local extra = count - clutter_threshold
  local steps = math.ceil(extra / clutter_step)
  return math.min(steps * 2, max_penalty)
end

local function tick_nyctophobia()
  ---@type Avatar
  local you = gapi.get_avatar()
  if not you:has_trait(trait_nyctophobia) then return end
  if you:get_effect_int(effect_depressants) > 3 then return end

  local here = gapi.get_map()
  local pos = you:get_pos_ms()
  local threshold = nyctophobia_threshold()
  local dark_places = {}

  for _, pt in ipairs(here:points_in_radius(pos, 5)) do
    if you:sees(pt) and here:ambient_light_at(pt) < threshold and is_passable(here, pt) then
      table.insert(dark_places, pt)
    end
  end

  local in_darkness = here:ambient_light_at(pos) < threshold
  local chance = in_darkness and 50 or 200

  if #dark_places > 0 and gapi.rng(1, chance) == 1 and one_turn_in(TimeDuration.from_hours(1)) then
    local target = random_entry(dark_places)
    if target then gapi.spawn_hallucination(target) end
  end

  if not in_darkness then
    if in_darkness_alert and you:is_avatar() then
      gapi.add_msg(MsgType.good, locale.gettext("You feel relief as you step back into the light."))
    end
    in_darkness_alert = false
    return
  end

  if you:is_avatar() and not in_darkness_alert then
    gapi.add_msg(MsgType.bad, locale.gettext("You feel a twinge of panic as darkness engulfs you."))
    in_darkness_alert = true
  end

  if gapi.rng(1, 2) == 1 and one_turn_in(TimeDuration.from_hours(1)) then you:sound_hallu() end

  if gapi.rng(1, 200) == 1 and one_turn_in(TimeDuration.from_hours(1)) and not you:is_on_ground() then
    if you:is_avatar() then
      gapi.add_msg(
        MsgType.bad,
        locale.gettext(
          "Your fear of the dark is so intense that your trembling legs fail you, and you fall to the ground."
        )
      )
    end
    you:add_effect(effect_downed, TimeDuration.from_minutes(gapi.rng(1, 2)))
  end

  if gapi.rng(1, 200) == 1 and one_turn_in(TimeDuration.from_hours(1)) and not you:has_effect(effect_shakes) then
    if you:is_avatar() then
      gapi.add_msg(
        MsgType.bad,
        locale.gettext("Your fear of the dark is so intense that your hands start shaking uncontrollably.")
      )
    end
    you:add_effect(effect_shakes, TimeDuration.from_minutes(gapi.rng(1, 2)))
  end

  if gapi.rng(1, 200) == 1 and one_turn_in(TimeDuration.from_hours(1)) then
    if you:is_avatar() then
      gapi.add_msg(
        MsgType.bad,
        locale.gettext(
          "Your fear of the dark is so intense that you start breathing rapidly, and you feel like your heart is ready to jump out of the chest."
        )
      )
    end
  end
end

local function tick_morale_traits()
  local you = gapi.get_avatar()
  if not you then return end

  if you:get_effect_int(effect_depressants) > 3 then
    you:rem_morale(morale_indoor_misery)
    you:rem_morale(morale_outdoor_misery)
    you:rem_morale(morale_clutter_intolerant)
    return
  end

  local here = gapi.get_map()
  local pos = you:get_pos_ms()
  local is_outside = here:is_outside(pos)

  if you:has_trait(trait_claustrophobia) then
    if not is_outside then
      apply_penalty(you, morale_indoor_misery, 15)
      if gapi.rng(1, 5) == 1 then drain_focus(you, 1, 20) end
    else
      you:rem_morale(morale_indoor_misery)
    end
  end

  if you:has_trait(trait_agoraphobia) then
    if is_outside then
      apply_penalty(you, morale_outdoor_misery, 15)
      if gapi.rng(1, 5) == 1 then drain_focus(you, 1, 20) end
    else
      you:rem_morale(morale_outdoor_misery)
    end
  end

  if you:has_trait(trait_clutter_intolerant) then
    local loose_items = count_loose_items(here, pos)
    local penalty = clutter_penalty(loose_items)
    apply_penalty(you, morale_clutter_intolerant, penalty)
    if penalty > 0 and not seen_clutter then
      gapi.add_msg(MsgType.bad, locale.gettext("It's so cluttered here..."))
      seen_clutter = true
    end
    if penalty == 0 then
      seen_clutter = false
    end
    if penalty > 0 then
      local chance_scale = math.max(1, math.floor(penalty / 5))
      if gapi.rng(1, 30) <= chance_scale then
        drain_focus(you, 1, 20)
      end
    end
  else
    you:rem_morale(morale_clutter_intolerant)
  end
end

---@param params table
---@return boolean
local function on_character_try_move(params)
  ---@type Character
  local ch = params.char
  if not ch then return true end
  if not ch:has_trait(trait_nyctophobia) then return true end
  if ch:get_effect_int(effect_depressants) > 3 then return true end
  if params.movement_mode == CharacterMoveMode.run then return true end

  local dest = params.to
  if not dest then return true end

  local here = gapi.get_map()
  local threshold = nyctophobia_threshold()
  if here:ambient_light_at(dest) >= threshold then return true end

  if ch:is_avatar() then
    gapi.add_msg(
      MsgType.bad,
      locale.gettext(
        "It's so dark and scary in there!  You can't force yourself to walk into this tile.  Switch to running movement mode to move there."
      )
    )
  end
  return false
end

---@param mod table
function lua_traits.register(mod)
  mod.on_character_try_move = on_character_try_move
  mod.on_nyctophobia_tick = tick_nyctophobia
  mod.on_morale_traits_tick = tick_morale_traits
end

return lua_traits

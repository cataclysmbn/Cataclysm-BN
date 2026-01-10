local nyctophobia = {}

---@param mod table
function nyctophobia.register(mod)
  local trait_nyctophobia = MutationBranchId.new("NYCTOPHOBIA")
  local effect_took_xanax = EffectTypeId.new("took_xanax")
  local effect_downed = EffectTypeId.new("downed")
  local effect_shakes = EffectTypeId.new("shakes")
  local effect_fearparalyze = EffectTypeId.new("fearparalyze")

  ---@class NyctophobiaMoveParams
  ---@field char Character
  ---@field from Tripoint
  ---@field to Tripoint
  ---@field movement_mode CharacterMoveMode
  ---@field via_ramp boolean

  ---@return number
  local function nyctophobia_threshold() return gapi.light_ambient_lit() - 3.0 end

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

  ---@param params NyctophobiaMoveParams
  ---@return boolean
  mod.on_character_try_move = function(params)
    ---@type Character
    local ch = params.char
    if not ch then return true end
    if not ch:has_trait(trait_nyctophobia) then return true end
    if ch:has_effect(effect_took_xanax) then return true end
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

  ---@return nil
  mod.on_nyctophobia_tick = function()
    ---@type Avatar
    local you = gapi.get_avatar()
    if not you:has_trait(trait_nyctophobia) then return end
    if you:has_effect(effect_took_xanax) then return end

    local here = gapi.get_map()
    local pos = you:get_pos_ms()
    local threshold = nyctophobia_threshold()
    local dark_places = {}

    for _, pt in ipairs(here:points_in_radius(pos, 5)) do
      if you:sees(pt) and here:ambient_light_at(pt) < threshold then table.insert(dark_places, pt) end
    end

    local in_darkness = here:ambient_light_at(pos) < threshold
    local chance = in_darkness and 10 or 50

    if #dark_places > 0 and gapi.rng(1, chance) == 1 then
      local target = random_entry(dark_places)
      if target then gapi.spawn_hallucination(target) end
    end

    if not in_darkness then return end

    if one_turn_in(TimeDuration.from_minutes(5)) and you:is_avatar() then
      gapi.add_msg(MsgType.bad, locale.gettext("You feel a twinge of panic as darkness engulfs you."))
    end

    if gapi.rng(1, 2) == 1 and one_turn_in(TimeDuration.from_seconds(30)) then you:sound_hallu() end

    if gapi.rng(1, 50) == 1 and not you:is_on_ground() then
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

    if gapi.rng(1, 50) == 1 and not you:has_effect(effect_shakes) then
      if you:is_avatar() then
        gapi.add_msg(
          MsgType.bad,
          locale.gettext("Your fear of the dark is so intense that your hands start shaking uncontrollably.")
        )
      end
      you:add_effect(effect_shakes, TimeDuration.from_minutes(gapi.rng(1, 2)))
    end

    if gapi.rng(1, 50) == 1 then
      if you:is_avatar() then
        gapi.add_msg(
          MsgType.bad,
          locale.gettext(
            "Your fear of the dark is so intense that you start breathing rapidly, and you feel like your heart is ready to jump out of the chest."
          )
        )
      end
      you:mod_stamina(-500 * gapi.rng(1, 3))
    end

    if gapi.rng(1, 50) == 1 and not you:has_effect(effect_fearparalyze) then
      if you:is_avatar() then
        gapi.add_msg(MsgType.bad, locale.gettext("Your fear of the dark is so intense that you stand paralyzed."))
      end
      you:add_effect(effect_fearparalyze, TimeDuration.from_turns(5))
      you:mod_moves(-4 * you:get_speed())
    end
  end
end

return nyctophobia

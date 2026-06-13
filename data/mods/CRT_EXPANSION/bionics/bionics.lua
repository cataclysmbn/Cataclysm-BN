local bionics = {}

local bionic_cold_drain = BionicDataId.new("bio_crt_cold_absorb")

bionics.on_creature_melee_attacked = function(params)
  local creature = params.char
  if not creature:is_avatar() and not creature:is_npc() then return end
  if params.success then
    local char = creature:as_character()
    if not char:is_armed() and char:has_active_bionic(bionic_cold_drain) then
      do_cold_drain(params.char, params.target)
    end
  end
end

do_cold_drain = function(char, target)
  gapi.add_msg("You draw some heat out of your hands")
  target:deal_damage(
    char,
    BodyPartTypeId.new("torso"):int_id(),
    DamageInstance.new(DamageType.DT_HEAT, 3.0, 10.0, 1.0, 1.0)
  )
  char:mod_power_level(Energy.from_joule(1500))
end

return bionics

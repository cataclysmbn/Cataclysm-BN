local M = {}

local trait_WOOLALLERGY = MutationBranchId.new("WOOLALLERGY")

local flag_WOOLED = JsonFlagId.new("wooled")

local material_wool = MaterialTypeId.new("wool")

M.on_character_try_wear = function(params)
  local res = params.results

  res.allowed = true

  local who = params.who
  local item = params.item

  if not who:has_trait(trait_WOOLALLERGY) then
    return true
  end

  if item:has_flag( flag_WOOLED ) or item:is_made_of( material_wool ) then
    res.allowed = false
    res.message = locale.gettext( "Can't wear that, it's made of wool!" )
    return res
  end
  return true
end

return M

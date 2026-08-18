local cvd = {}

local metal = MaterialTypeId.new("steel")
local cut = DamageType.DT_CUT
local stab = DamageType.DT_STAB

---@class CvdParams
---@field ench_id string
---@field item Item

---@param params CvdParams
---@return boolean
function cvd.can_use_on(params)
  return params.item:is_made_of( metal ) and
    ( params.item:is_melee(cut) or params.item:is_melee(stab) )
end

return cvd

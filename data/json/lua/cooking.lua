local M = {}

local cooking_skill = SkillId.new("cooking")

---@class ScaleCookedFoodFunParams
---@field fun integer
---@field cooking_level integer

---@param params ScaleCookedFoodFunParams
local scale_cooked_food_fun = function(params)
  local fun = params.fun
  local cooking_level = params.cooking_level
  if cooking_level <= 0 or fun == 0 then return fun end
  if fun > 0 then return math.floor(fun * (1 + cooking_level * 0.05)) end

  local scaled_magnitude = math.max(0, -fun * (1 - cooking_level * 0.05))
  return -math.floor(scaled_magnitude)
end

---@param params OnCraftResultParams
local apply_craft_enjoyment_bonus = function(params)
  local item = params.item
  if item == nil or not item:is_comestible() then return end

  local cooking_level = params.crafter:get_skill_level(cooking_skill)
  local cooked_fun = scale_cooked_food_fun({ fun = item:get_comestible_fun(), cooking_level = cooking_level })
  item:set_var_num("comestible_fun", cooked_fun)
end

---@param params OnCraftResultParams
M.on_craft_result = apply_craft_enjoyment_bonus

return M

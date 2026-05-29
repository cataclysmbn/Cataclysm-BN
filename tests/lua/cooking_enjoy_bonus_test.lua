local cooking = require("../../data/json/lua/cooking")

local function make_item(fun)
  return {
    fun = fun,
    stored_var_name = nil,
    stored_fun = nil,
    is_comestible = function() return true end,
    get_comestible_fun = function(self) return self.fun end,
    set_var_num = function(self, name, value)
      self.stored_var_name = name
      self.stored_fun = value
    end,
  }
end

local high_skill_crafter = {
  get_skill_level = function() return 10 end,
}

local unheated_food = make_item(10)
cooking.on_craft_result({
  crafter = high_skill_crafter,
  item = unheated_food,
  hot_result = false,
  dehydrated_result = false,
})

test_data.high_skill_var_name = unheated_food.stored_var_name
test_data.high_skill_fun = unheated_food.stored_fun

local unheated_bad_food = make_item(-10)
cooking.on_craft_result({
  crafter = high_skill_crafter,
  item = unheated_bad_food,
  hot_result = false,
  dehydrated_result = false,
})

test_data.high_skill_bad_fun = unheated_bad_food.stored_fun

local zero_skill_crafter = {
  get_skill_level = function() return 0 end,
}

local baseline_food = make_item(10)
cooking.on_craft_result({
  crafter = zero_skill_crafter,
  item = baseline_food,
  hot_result = false,
  dehydrated_result = false,
})

test_data.zero_skill_var_name = baseline_food.stored_var_name
test_data.zero_skill_fun = baseline_food.stored_fun

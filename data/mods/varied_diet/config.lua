---@class VariedDietFoodRule
---@field group string|nil
---@field complexity number|nil
---@field points number|nil
---@field multiplier number|nil

---@class VariedDietGroupRule
---@field patterns string[]
---@field complexity number|nil
---@field multiplier number|nil

---@class VariedDietThreshold
---@field score number
---@field morale integer
---@field health integer

---@class VariedDietRepeatStep
---@field count integer
---@field morale integer
---@field health integer

---@class VariedDietConfig
---@field enabled boolean
---@field storage_key string
---@field minimum_kcal integer
---@field history_window_hours integer
---@field item_repeat_window_hours integer
---@field group_repeat_window_hours integer
---@field kcal_reference number
---@field kcal_log_base number
---@field max_kcal_points number
---@field fun_divisor number
---@field healthy_divisor number
---@field same_item_point_penalty number
---@field same_group_point_penalty number
---@field reward_morale_type string
---@field penalty_morale_type string
---@field reward_duration_hours integer
---@field reward_decay_start_hours integer
---@field penalty_duration_hours integer
---@field penalty_decay_start_hours integer
---@field health_cooldown_hours integer
---@field reward_health_cap integer
---@field penalty_health_cap integer
---@field thresholds VariedDietThreshold[]
---@field item_repeat_penalties VariedDietRepeatStep[]
---@field group_repeat_penalties VariedDietRepeatStep[]
---@field group_order string[]
---@field groups table<string, VariedDietGroupRule>
---@field foods table<string, VariedDietFoodRule>

---@type VariedDietConfig
return {
  enabled = true,
  storage_key = "varied_diet_history",
  minimum_kcal = 75,
  history_window_hours = 72,
  item_repeat_window_hours = 48,
  group_repeat_window_hours = 24,
  kcal_reference = 250,
  kcal_log_base = 2.0,
  max_kcal_points = 5.0,
  fun_divisor = 8.0,
  healthy_divisor = 2.0,
  same_item_point_penalty = 1.75,
  same_group_point_penalty = 0.5,
  reward_morale_type = "morale_varied_diet",
  penalty_morale_type = "morale_food_repetition",
  reward_duration_hours = 12,
  reward_decay_start_hours = 6,
  penalty_duration_hours = 8,
  penalty_decay_start_hours = 3,
  health_cooldown_hours = 6,
  reward_health_cap = 100,
  penalty_health_cap = -100,
  thresholds = {
    { score = 6, morale = 2, health = 0 },
    { score = 12, morale = 5, health = 1 },
    { score = 22, morale = 9, health = 2 },
    { score = 36, morale = 14, health = 3 },
  },
  item_repeat_penalties = {
    { count = 2, morale = -1, health = 0 },
    { count = 3, morale = -3, health = -1 },
    { count = 5, morale = -6, health = -2 },
  },
  group_repeat_penalties = {
    { count = 4, morale = -1, health = 0 },
    { count = 6, morale = -3, health = -1 },
  },
  group_order = { "complex", "protein", "grain", "fruit", "vegetable", "dairy", "snack", "drink" },
  groups = {
    complex = {
      patterns = {
        "pizza",
        "sandwich",
        "soup",
        "stew",
        "curry",
        "casserole",
        "pie",
        "cake",
        "deluxe",
        "meal",
        "dinner",
        "breakfast",
        "salad",
      },
      complexity = 2.0,
      multiplier = 1.2,
    },
    protein = {
      patterns = { "meat", "fish", "egg", "sausage", "jerky", "bacon", "pemmican" },
      complexity = 0.5,
      multiplier = 1.0,
    },
    grain = {
      patterns = { "bread", "wheat", "oat", "rice", "pasta", "noodle", "flour", "cereal" },
      complexity = 0.25,
      multiplier = 1.0,
    },
    fruit = {
      patterns = { "apple", "berry", "fruit", "orange", "lemon", "melon", "grape", "pear", "peach" },
      complexity = 0.0,
      multiplier = 0.9,
    },
    vegetable = {
      patterns = { "veg", "broccoli", "cabbage", "carrot", "tomato", "potato", "onion", "garlic", "bean" },
      complexity = 0.0,
      multiplier = 0.9,
    },
    dairy = {
      patterns = { "milk", "cheese", "yogurt", "butter", "cream" },
      complexity = 0.0,
      multiplier = 0.9,
    },
    snack = {
      patterns = { "candy", "chips", "chocolate", "cookie", "cracker", "protein_bar", "junk" },
      complexity = -0.5,
      multiplier = 0.55,
    },
    drink = {
      patterns = { "juice", "smoothie", "shake", "cider", "soda" },
      complexity = 0.0,
      multiplier = 0.65,
    },
  },
  foods = {
    oatmeal_deluxe = { group = "complex", complexity = 1.5 },
    pizza_cheese = { group = "complex", complexity = 2.0 },
    pizza = { group = "complex", complexity = 2.0 },
    meat_cooked = { group = "protein", complexity = 0.5 },
    meat_smoked = { group = "protein", complexity = 0.75 },
    dehydrated_meat = { group = "protein", complexity = 0.25, multiplier = 0.75 },
    cracklins = { group = "snack", complexity = -0.25, multiplier = 0.6 },
    candy = { group = "snack", complexity = -1.0, multiplier = 0.35 },
  },
}

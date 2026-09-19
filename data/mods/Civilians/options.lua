local storage = game.mod_storage[game.current_mod]

---@class mod_option
--- @field key string
--- @field name string
--- @field type type
--- @field min_val number
--- @field max_val number
--- @field desc string
--- @field default string | number | boolean
--- @field set_value function? -> nil
--- @field get_value function? -> number | string | boolean

---@type mod_option
local OPT_SPAWN_CHANCE = {
  key = "SPAWN_CHANCE",
  name = "Spawn Chance",
  type = 'number',
  min_val = 1, max_val = 100,
  desc = "Base spawn chance",
  default = 50,
}
OPT_SPAWN_CHANCE.set_value = function(value)
  storage[OPT_SPAWN_CHANCE.key] = value
end
OPT_SPAWN_CHANCE.get_value = function()
  local stored_value = storage[OPT_SPAWN_CHANCE.key]
  if stored_value ~= nil then return stored_value end
  return OPT_SPAWN_CHANCE.default
end

---@type mod_option
local OPT_RARE_CHANCE = {
  key = "RARE_CHANCE",
  name = "Rare Chance",
  type = 'number',
  min_val = 1, max_val = 100,
  desc = "Rare spawn chance",
  default = 10
}
OPT_RARE_CHANCE.set_value = function(value)
  storage[OPT_RARE_CHANCE.key] = value
end
OPT_RARE_CHANCE.get_value = function()
  local stored_value = storage[OPT_RARE_CHANCE.key]
  if stored_value ~= nil then return stored_value end
  return OPT_RARE_CHANCE.default
end

---@type mod_option
local OPT_VANISH_PERIOD_DAYS = {
  key = "VANISH_PERIOD_DAYS",
  name = "Vanish Period Days",
  type = 'number',
  min_val = 1, max_val = 100,
  desc = "Days until minimal spawn chance",
  default = 28
}
OPT_VANISH_PERIOD_DAYS.set_value = function(value)
  storage[OPT_VANISH_PERIOD_DAYS.key] = value
end
OPT_VANISH_PERIOD_DAYS.get_value = function()
  local stored_value = storage[OPT_VANISH_PERIOD_DAYS.key]
  if stored_value ~= nil then return stored_value end
  return OPT_VANISH_PERIOD_DAYS.default
end

---@type mod_option
local OPT_VANISH_BASE_RATE = {
  key = "VANISH_BASE_RATE",
  name = "Vanish Base Rate",
  type = 'number',
  min_val = 0.01, max_val = 1.0,
  desc = "Lower bound spawn chance",
  default = 0.1
}
OPT_VANISH_BASE_RATE.set_value = function(value)
  storage[OPT_VANISH_BASE_RATE.key] = value
end
OPT_VANISH_BASE_RATE.get_value = function()
  local stored_value = storage[OPT_VANISH_BASE_RATE.key]
  if stored_value ~= nil then return stored_value end
  return OPT_VANISH_BASE_RATE.default
end

---@type mod_option
local OPT_PULPING_ENABLED = {
  key = "PULPING_ENABLED",
  name = "Pulping Enabled",
  type = 'boolean',
  desc = "Enable civilian corpse pulping",
  min_val = 0, max_val = 1,
  default = true
}
OPT_PULPING_ENABLED.set_value = function(value)
  storage[OPT_PULPING_ENABLED.key] = value
end
OPT_PULPING_ENABLED.get_value = function()
  local stored_value = storage[OPT_PULPING_ENABLED.key]
  if stored_value ~= nil then return stored_value end
  return OPT_PULPING_ENABLED.default
end


local OPT_PULPING_CIV_LIMIT = {
  key = "PULPING_CIV_LIMIT",
  name = "Pulping Civ Limit",
  type = 'number',
  min_val = 1, max_val = 200,
  desc = "Amount of cilians to evaluate during pulping.",
  default = 25
}
OPT_PULPING_CIV_LIMIT.set_value = function(value)
  storage[OPT_PULPING_CIV_LIMIT.key] = value
end
OPT_PULPING_CIV_LIMIT.get_value = function()
  local stored_value = storage[OPT_PULPING_CIV_LIMIT.key]
  if stored_value ~= nil then return stored_value end
  return OPT_PULPING_CIV_LIMIT.default
end

---@type mod_option
local OPT_PULPING_RADIUS = {
  key = "PULPING_RADIUS",
  name = "Pulping Radius",
  type = 'number',
  min_val = 1, max_val = 100,
  desc = "The distance a civilian will look for a pulpable corpse.",
  default = 4
}
OPT_PULPING_RADIUS.set_value = function(value)
  storage[OPT_PULPING_RADIUS.key] = value
end
OPT_PULPING_RADIUS.get_value = function()
  local stored_value = storage[OPT_PULPING_RADIUS.key]
  if stored_value ~= nil then return stored_value end
  return OPT_PULPING_RADIUS.default
end

---@type mod_option
local OPT_PULPING_CHANCE = {
  key = "PULPING_CHANCE",
  name = "Pulping Chance",
  type = 'number',
  min_val = 0, max_val = 100,
  desc = "Chance of pulping each turn",
  default = 50
}
OPT_PULPING_CHANCE.set_value = function(value)
  storage[OPT_PULPING_CHANCE.key] = value
end
OPT_PULPING_CHANCE.get_value = function()
  local stored_value = storage[OPT_PULPING_CHANCE.key]
  if stored_value ~= nil then return stored_value end
  return OPT_PULPING_CHANCE.default
end


---@class mod_options
--- @field spawn_chance mod_option
--- @field rare_chance mod_option
--- @field vanish_period_days mod_option
--- @field vanish_base_rate mod_option
--- @field pulping_enabled mod_option
--- @field pulping_civ_limit mod_option
--- @field pulping_radius mod_option
--- @field pulping_chance mod_option


---@type mod_options
local export_options = {
    spawn_chance = OPT_SPAWN_CHANCE,
    rare_chance = OPT_RARE_CHANCE,
    vanish_period_days = OPT_VANISH_PERIOD_DAYS,
    vanish_base_rate = OPT_VANISH_BASE_RATE,
    pulping_enabled = OPT_PULPING_ENABLED,
    pulping_civ_limit = OPT_PULPING_CIV_LIMIT,
    pulping_radius = OPT_PULPING_RADIUS,
    pulping_chance = OPT_PULPING_CHANCE
}
return export_options

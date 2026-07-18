gdebug.log_info("Varied Diet: Main")

---@class ModVariedDiet
local mod = game.mod_runtime[game.current_mod]
local gettext = locale.gettext

---@type VariedDietConfig
mod.config = require("config")

---@type VariedDietConfig
local config = mod.config

---@class VariedDietConsumeFoodParams
---@field character Character
---@field food Item
---@field nutrition integer
---@field calories integer
---@field quench integer
---@field healthy integer
---@field fun integer
---@field fun_cap integer
---@field spoiled boolean
---@field when TimePoint

---@class VariedDietEvent
---@field turn integer
---@field id string
---@field group string
---@field points number

---@param value number
---@param low number
---@param high number
---@return number
local function clamp(value, low, high) return math.max(low, math.min(high, value)) end

---@param hours integer
---@return integer
local function hours_to_turns(hours) return TimeDuration.from_hours(hours):to_turns() end

---@param value string
---@return VariedDietEvent[]
local function parse_history(value)
  local history = {}
  for raw in string.gmatch(value, "([^;]+)") do
    local turn_text, id, group, points_text = string.match(raw, "^([^,]+),([^,]+),([^,]+),([^,]+)$")
    if turn_text ~= nil and id ~= nil and group ~= nil and points_text ~= nil then
      table.insert(history, {
        turn = tonumber(turn_text) or 0,
        id = id,
        group = group,
        points = tonumber(points_text) or 0,
      })
    end
  end
  return history
end

---@param history VariedDietEvent[]
---@return string
local function serialize_history(history)
  local chunks = {}
  for _, event in ipairs(history) do
    table.insert(chunks, string.format("%d,%s,%s,%.3f", event.turn, event.id, event.group, event.points))
  end
  return table.concat(chunks, ";")
end

---@param history VariedDietEvent[]
---@param now integer
---@return VariedDietEvent[]
local function recent_history(history, now)
  local cutoff = now - hours_to_turns(config.history_window_hours)
  local recent = {}
  for _, event in ipairs(history) do
    if event.turn >= cutoff then table.insert(recent, event) end
  end
  table.sort(recent, function(left, right) return left.turn > right.turn end)
  return recent
end

---@param id string
---@param rule VariedDietFoodRule|nil
---@return VariedDietGroupRule|nil
local function group_rule_for(id, rule)
  local group = rule and rule.group or id
  return config.groups[group]
end

---@param text string
---@param patterns string[]
---@return boolean
local function matches_any(text, patterns)
  for _, pattern in ipairs(patterns) do
    if string.find(text, pattern, 1, true) then return true end
  end
  return false
end

---@param id string
---@return string, VariedDietGroupRule|nil, VariedDietFoodRule|nil
local function classify_food(id)
  local rule = config.foods[id]
  if rule and rule.group then return rule.group, group_rule_for(id, rule), rule end

  for _, group in ipairs(config.group_order) do
    local group_rule = config.groups[group]
    if group_rule ~= nil and matches_any(id, group_rule.patterns) then return group, group_rule, rule end
  end

  return id, nil, rule
end

---@param params VariedDietConsumeFoodParams
---@return integer
local function effective_calories(params)
  if params.calories ~= nil then return params.calories end
  return params.food:get_kcal()
end

---@param params VariedDietConsumeFoodParams
---@param group_rule VariedDietGroupRule|nil
---@param food_rule VariedDietFoodRule|nil
---@return number
local function food_points(params, group_rule, food_rule)
  local kcal = effective_calories(params)
  if kcal < config.minimum_kcal then return 0 end

  local kcal_points = math.log(1 + kcal / config.kcal_reference) / math.log(config.kcal_log_base)
  kcal_points = clamp(kcal_points, 0, config.max_kcal_points)

  local complexity = food_rule and food_rule.complexity or group_rule and group_rule.complexity or 0
  local multiplier = food_rule and food_rule.multiplier or group_rule and group_rule.multiplier or 1
  local fun_bonus = math.max(0, params.fun or params.food:get_comestible_fun()) / config.fun_divisor
  local health_bonus = math.max(0, params.healthy or 0) / config.healthy_divisor
  local explicit_points = food_rule and food_rule.points or 0

  return math.max(0, (kcal_points + complexity + fun_bonus + health_bonus + explicit_points) * multiplier)
end

---@param history VariedDietEvent[]
---@param now integer
---@param id string|nil
---@param group string|nil
---@param window_hours integer
---@return integer
local function count_recent(history, now, id, group, window_hours)
  local cutoff = now - hours_to_turns(window_hours)
  local count = 0
  for _, event in ipairs(history) do
    if event.turn >= cutoff and (id == nil or event.id == id) and (group == nil or event.group == group) then
      count = count + 1
    end
  end
  return count
end

---@param history VariedDietEvent[]
---@param now integer
---@return number
local function diet_score(history, now)
  local seen_items = {}
  local seen_groups = {}
  local score = 0
  local window = hours_to_turns(config.history_window_hours)
  for _, event in ipairs(history) do
    local age = math.max(0, now - event.turn)
    local age_weight = clamp(1 - age / window, 0.1, 1.0)
    local item_weight = seen_items[event.id] and 0.35 or 1.0
    local group_weight = seen_groups[event.group] and 0.75 or 1.0
    score = score + event.points * age_weight * item_weight * group_weight
    seen_items[event.id] = true
    seen_groups[event.group] = true
  end
  return score
end

---@param score number
---@return VariedDietThreshold|nil
local function reward_for_score(score)
  local best = nil
  for _, threshold in ipairs(config.thresholds) do
    if score >= threshold.score and (best == nil or threshold.score > best.score) then best = threshold end
  end
  return best
end

---@param count integer
---@param steps VariedDietRepeatStep[]
---@return VariedDietRepeatStep|nil
local function penalty_for_count(count, steps)
  local best = nil
  for _, step in ipairs(steps) do
    if count >= step.count and (best == nil or step.count > best.count) then best = step end
  end
  return best
end

---@param character Character
---@param morale_type MoraleTypeDataId
---@param target integer
---@param duration_hours integer
---@param decay_start_hours integer
---@return nil
local function refresh_morale(character, morale_type, target, duration_hours, decay_start_hours)
  if target == 0 then return end
  local current = character:get_morale(morale_type)
  local delta = target > 0 and math.max(0, target - current) or math.min(0, target - current)
  if delta == 0 then return end
  character:add_morale(
    morale_type,
    delta,
    target,
    TimeDuration.from_hours(duration_hours),
    TimeDuration.from_hours(decay_start_hours),
    true,
    nil
  )
end

---@param character Character
---@param key string
---@param now integer
---@return boolean
local function can_apply_health(character, key, now)
  local last = tonumber(character:get_value(key)) or -hours_to_turns(config.health_cooldown_hours)
  return now - last >= hours_to_turns(config.health_cooldown_hours)
end

---@param character Character
---@param key string
---@param now integer
---@param delta integer
---@param cap integer
---@return nil
local function apply_health(character, key, now, delta, cap)
  if delta == 0 or not can_apply_health(character, key, now) then return end
  character:mod_healthy_mod(delta, cap)
  character:set_value(key, tostring(now))
end

---@param params VariedDietConsumeFoodParams
---@return boolean
local function should_track(params)
  if not config.enabled then return false end
  if params.character == nil or params.food == nil then return false end
  if params.food:is_medication() then return false end
  if params.spoiled then return false end
  return params.food:is_food() or params.food:is_food_container() or effective_calories(params) >= config.minimum_kcal
end

---@param params VariedDietConsumeFoodParams
---@return nil
function mod.on_character_consumed_food(params)
  if not should_track(params) then return end

  local character = params.character
  local food = params.food
  local now = params.when ~= nil and params.when:to_turn() or gapi.current_turn():to_turn()
  local id = food:get_type():str()
  local group, group_rule, food_rule = classify_food(id)
  local points = food_points(params, group_rule, food_rule)
  if points <= 0 then return end

  local history = recent_history(parse_history(character:get_value(config.storage_key)), now)
  local item_count = count_recent(history, now, id, nil, config.item_repeat_window_hours) + 1
  local group_count = count_recent(history, now, nil, group, config.group_repeat_window_hours) + 1
  points = math.max(
    0,
    points - (item_count - 1) * config.same_item_point_penalty - (group_count - 1) * config.same_group_point_penalty
  )

  table.insert(history, 1, { turn = now, id = id, group = group, points = points })
  character:set_value(config.storage_key, serialize_history(history))

  local score = diet_score(history, now)
  local reward = reward_for_score(score)
  local reward_morale_type = MoraleTypeDataId.new(config.reward_morale_type)
  if reward ~= nil then
    refresh_morale(
      character,
      reward_morale_type,
      reward.morale,
      config.reward_duration_hours,
      config.reward_decay_start_hours
    )
    apply_health(character, config.storage_key .. "_reward_health", now, reward.health, config.reward_health_cap)
  end

  local item_penalty = penalty_for_count(item_count, config.item_repeat_penalties)
  local group_penalty = penalty_for_count(group_count, config.group_repeat_penalties)
  local penalty_morale_type = MoraleTypeDataId.new(config.penalty_morale_type)
  local penalty_morale = (item_penalty and item_penalty.morale or 0) + (group_penalty and group_penalty.morale or 0)
  local penalty_health = (item_penalty and item_penalty.health or 0) + (group_penalty and group_penalty.health or 0)

  if penalty_morale ~= 0 then
    refresh_morale(
      character,
      penalty_morale_type,
      penalty_morale,
      config.penalty_duration_hours,
      config.penalty_decay_start_hours
    )
  end
  apply_health(character, config.storage_key .. "_penalty_health", now, penalty_health, config.penalty_health_cap)

  if character:is_avatar() and reward ~= nil and reward.morale >= 9 and penalty_morale == 0 then
    gapi.add_msg(MsgType.good, string.format(gettext("Your varied diet feels satisfying. Diet score: %.1f."), score))
  elseif character:is_avatar() and penalty_morale < 0 then
    gapi.add_msg(MsgType.warning, gettext("Eating the same kind of food again is getting tiresome."))
  end
end

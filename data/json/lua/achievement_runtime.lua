local api = achievement_api

---@class achievement_runtime_time_constraint
---@field comparison string
---@field target_turn integer

---@class achievement_runtime_skill_requirement
---@field skill_id string
---@field comparison string
---@field level integer

---@class achievement_runtime_kill_requirement
---@field kind string
---@field id? string
---@field comparison string
---@field count integer

---@class achievement_runtime_requirement
---@field statistic_id string
---@field comparison string
---@field target? integer
---@field becomes_false boolean

---@class achievement_runtime_achievement
---@field id string
---@field hidden_by string[]
---@field time_constraint? achievement_runtime_time_constraint
---@field skill_requirements achievement_runtime_skill_requirement[]
---@field kill_requirements achievement_runtime_kill_requirement[]
---@field requirements achievement_runtime_requirement[]

---@param comparison string
---@param target integer?
---@param value integer
---@return boolean
local compare = function(comparison, target, value)
  if comparison == ">=" then return value >= target end
  if comparison == "<=" then return value <= target end
  return true
end

---@param requirement achievement_runtime_kill_requirement
---@return integer
local kill_count = function(requirement)
  if requirement.kind == "monster" then return api.monster_kill_count(requirement.id) end
  if requirement.kind == "species" then return api.species_kill_count(requirement.id) end
  return api.total_kill_count()
end

---@param achievement achievement_runtime_achievement
---@return string
local time_state = function(achievement)
  local constraint = achievement.time_constraint
  if not constraint then return "completed" end

  local now = api.current_turn()
  if constraint.comparison == "<=" then return now <= constraint.target_turn and "completed" or "failed" end
  if constraint.comparison == ">=" then return now >= constraint.target_turn and "completed" or "pending" end
  return "completed"
end

---@param achievement achievement_runtime_achievement
---@return string
local skill_state = function(achievement)
  local state = "completed"
  for _, requirement in ipairs(achievement.skill_requirements) do
    local satisfied = compare(requirement.comparison, requirement.level, api.skill_level(requirement.skill_id))
    if not satisfied then
      if requirement.comparison == ">=" then
        state = "pending"
      elseif requirement.comparison == "<=" then
        return "failed"
      end
    end
  end
  return state
end

---@param achievement achievement_runtime_achievement
---@return string
local kill_state = function(achievement)
  local state = "completed"
  for _, requirement in ipairs(achievement.kill_requirements) do
    local satisfied = compare(requirement.comparison, requirement.count, kill_count(requirement))
    if not satisfied then
      if requirement.comparison == ">=" then
        state = "pending"
      elseif requirement.comparison == "<=" then
        return "failed"
      end
    end
  end
  return state
end

---@param achievement achievement_runtime_achievement
---@return boolean
local requirements_complete = function(achievement)
  for _, requirement in ipairs(achievement.requirements) do
    if not compare(requirement.comparison, requirement.target, api.stat_value(requirement.statistic_id)) then
      return false
    end
  end
  return true
end

---@param achievement achievement_runtime_achievement
---@return boolean
local requirements_failed = function(achievement)
  for _, requirement in ipairs(achievement.requirements) do
    if
      requirement.becomes_false
      and not compare(requirement.comparison, requirement.target, api.stat_value(requirement.statistic_id))
    then
      return true
    end
  end
  return false
end

---@param achievement achievement_runtime_achievement
local evaluate = function(achievement)
  if api.state(achievement.id) ~= "pending" then return end

  local time = time_state(achievement)
  local skill = skill_state(achievement)
  local kill = kill_state(achievement)

  if requirements_complete(achievement) and time == "completed" and skill == "completed" and kill == "completed" then
    api.report(achievement.id, "completed")
    return
  end

  if time == "failed" or kill == "failed" or requirements_failed(achievement) then
    api.report(achievement.id, "failed")
  end
end

local M = {}
---@type achievement_runtime_achievement[]
local definitions = api.definitions()
---@type table<string, achievement_runtime_achievement[]>
local by_statistic = {}

for _, achievement in ipairs(definitions) do
  local seen_statistics = {}
  for _, requirement in ipairs(achievement.requirements) do
    if not seen_statistics[requirement.statistic_id] then
      seen_statistics[requirement.statistic_id] = true
      by_statistic[requirement.statistic_id] = by_statistic[requirement.statistic_id] or {}
      by_statistic[requirement.statistic_id][#by_statistic[requirement.statistic_id] + 1] = achievement
    end
  end
end

---@param params { statistic_id: string }
M.on_stat_changed = function(params)
  local achievements = by_statistic[params.statistic_id]
  if not achievements then return end

  for _, achievement in ipairs(achievements) do
    evaluate(achievement)
  end
end

return M

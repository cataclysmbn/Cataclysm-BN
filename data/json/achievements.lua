local achievements = {}

local completion_pending = "pending"
local completion_completed = "completed"
local completion_failed = "failed"

local function compare_value(comparison, target, value)
  if comparison == ">=" then return value >= target end
  if comparison == "<=" then return value <= target end
  return true
end

local function get_state_root(storage)
  storage.lua_achievements = storage.lua_achievements or {}
  storage.lua_achievements.states = storage.lua_achievements.states or {}
  storage.lua_achievements.initial_ids = storage.lua_achievements.initial_ids or {}
  return storage.lua_achievements
end

local function load_definitions(mod)
  if mod.achievement_defs then return mod.achievement_defs end

  local defs = gapi.get_achievement_definitions()
  mod.achievement_defs = defs or {}
  mod.achievement_defs_by_id = {}

  for _, ach in ipairs(mod.achievement_defs) do
    mod.achievement_defs_by_id[ach.id] = ach
  end

  return mod.achievement_defs
end

local function now_turn() return gapi.current_turn():to_turn() end

local function evaluate_time_constraint(ach)
  local time_constraint = ach.time_constraint
  if not time_constraint then return completion_completed end

  local comparison = time_constraint.comparison
  local target_turn = time_constraint.target_turn or 0
  local now = now_turn()

  if comparison == "<=" then
    if now <= target_turn then return completion_completed end
    return completion_failed
  end

  if comparison == ">=" then
    if now >= target_turn then return completion_completed end
    return completion_pending
  end

  return completion_completed
end

local function evaluate_kill_constraints(ach)
  local kill_requirements = ach.kill_requirements or {}
  if #kill_requirements == 0 then return completion_completed end

  local status = completion_completed
  for _, req in ipairs(kill_requirements) do
    local comparison = req.comparison
    local target = req.count or 0
    local kills = 0

    if req.monster ~= nil and req.monster ~= "" then
      kills = gapi.get_monster_kill_count(req.monster)
    elseif req.species ~= nil and req.species ~= "" then
      kills = gapi.get_species_kill_count(req.species)
    else
      kills = gapi.get_total_monster_kill_count()
    end

    local ok = compare_value(comparison, target, kills)
    if not ok then
      if comparison == ">=" then
        status = completion_pending
      elseif comparison == "<=" then
        return completion_failed
      end
    end
  end

  return status
end

local function evaluate_skill_constraints(ach)
  local skill_requirements = ach.skill_requirements or {}
  if #skill_requirements == 0 then return completion_completed end

  local status = completion_completed
  for _, req in ipairs(skill_requirements) do
    local comparison = req.comparison
    local target = req.level or 0
    local level = gapi.get_avatar_skill_level(req.skill or "")
    local ok = compare_value(comparison, target, level)
    if not ok then
      if comparison == ">=" then
        status = completion_pending
      elseif comparison == "<=" then
        return completion_failed
      end
    end
  end

  return status
end

local function evaluate_requirements(ach)
  local requirements = ach.requirements or {}
  local all_satisfied = true
  local became_false = false

  for _, req in ipairs(requirements) do
    local current = gapi.get_event_statistic(req.event_statistic or "")
    local comparison = req.comparison
    local target = req.target or 0
    local ok = compare_value(comparison, target, current)
    if not ok then
      all_satisfied = false
      if req.becomes_false then became_false = true end
    end
  end

  return all_satisfied, became_false
end

local function ensure_achievement_state(root, id)
  local state = root.states[id]
  if state then return state end

  state = {
    completion = completion_pending,
    last_state_change_turn = now_turn(),
  }
  root.states[id] = state
  return state
end

local function mark_state(state, completion)
  state.completion = completion
  state.last_state_change_turn = now_turn()
end

local function evaluate_single(mod, root, ach)
  local state = ensure_achievement_state(root, ach.id)
  if state.completion ~= completion_pending then return end

  local all_requirements_satisfied, requirement_failed = evaluate_requirements(ach)
  local time_state = evaluate_time_constraint(ach)
  local kill_state = evaluate_kill_constraints(ach)
  local skill_state = evaluate_skill_constraints(ach)

  local all_clear = all_requirements_satisfied
    and time_state == completion_completed
    and kill_state == completion_completed
    and skill_state == completion_completed

  if all_clear then
    mark_state(state, completion_completed)
    gapi.add_msg(MsgType.good, string.format(locale.gettext('You completed the achievement "%s".'), ach.name))
    return
  end

  if
    time_state == completion_failed
    or kill_state == completion_failed
    or skill_state == completion_failed
    or requirement_failed
  then
    mark_state(state, completion_failed)
  end
end

local function snapshot_initial_ids(mod, root)
  if next(root.initial_ids) ~= nil then return end

  load_definitions(mod)
  for _, ach in ipairs(mod.achievement_defs) do
    root.initial_ids[ach.id] = true
  end
end

local function run_evaluation(mod)
  local storage = mod.storage
  if not storage then return end

  local root = get_state_root(storage)
  load_definitions(mod)
  snapshot_initial_ids(mod, root)

  for _, ach in ipairs(mod.achievement_defs) do
    if root.initial_ids[ach.id] then evaluate_single(mod, root, ach) end
  end
end

local function completion_rank(completion)
  if completion == completion_pending then return 0 end
  if completion == completion_completed then return 1 end
  return 2
end

local function get_completion(mod, root, id)
  local state = root.states[id]
  if not state then return completion_pending end
  return state.completion or completion_pending
end

local function is_hidden(mod, root, ach)
  if get_completion(mod, root, ach.id) == completion_completed then return false end

  local hidden_by = ach.hidden_by or {}
  for _, blocker in ipairs(hidden_by) do
    if get_completion(mod, root, blocker) ~= completion_completed then return true end
  end
  return false
end

local function status_prefix(completion)
  if completion == completion_completed then return "[completed]" end
  if completion == completion_failed then return "[failed]" end
  return "[pending]"
end

local function sorted_visible_achievements(mod, root)
  local out = {}
  for _, ach in ipairs(mod.achievement_defs or {}) do
    if root.initial_ids[ach.id] and not is_hidden(mod, root, ach) then table.insert(out, ach) end
  end

  table.sort(out, function(a, b)
    local ac = get_completion(mod, root, a.id)
    local bc = get_completion(mod, root, b.id)
    local ar = completion_rank(ac)
    local br = completion_rank(bc)
    if ar ~= br then return ar < br end
    return (a.name or "") < (b.name or "")
  end)

  return out
end

function achievements.get_ui_text(mod)
  local storage = mod.storage
  if not storage then return locale.gettext("This game has no valid achievements.\n") end

  local root = get_state_root(storage)
  load_definitions(mod)
  snapshot_initial_ids(mod, root)

  local lines = {}
  local visible = sorted_visible_achievements(mod, root)
  if #visible == 0 then
    table.insert(lines, locale.gettext("This game has no valid achievements."))
  else
    for _, ach in ipairs(visible) do
      local completion = get_completion(mod, root, ach.id)
      local header = string.format("%s %s", status_prefix(completion), ach.name or ach.id)
      table.insert(lines, header)
      if ach.description and ach.description ~= "" then table.insert(lines, string.format("  %s", ach.description)) end
    end
  end

  table.insert(
    lines,
    locale.gettext(
      "Note that only achievements that existed when you started this game and still exist now will appear here."
    )
  )
  return table.concat(lines, "\n")
end

function achievements.register(mod)
  mod.on_achievements_game_started = function() run_evaluation(mod) end

  mod.on_achievements_game_load = function() run_evaluation(mod) end

  mod.on_achievements_mon_death = function() run_evaluation(mod) end

  mod.on_achievements_tick = function() run_evaluation(mod) end
end

return achievements

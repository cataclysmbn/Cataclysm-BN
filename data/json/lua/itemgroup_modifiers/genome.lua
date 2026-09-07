local genome = {}

---@param item Item
---@param name String
---@param monid String
---@param progress String
apply_vars = function(item, name, monid, progress)
  item:set_var_str("specimen_name", name)
  item:set_var_str("specimen_sample", monid)
  item:set_var_str("specimen_sample_progress", progress)
end

---@param item Item
dire_wolf = function(item)
  apply_vars(item, "dire wolf", "mon_wolf_mutant_huge", "5")
end

---@param item Item
black_rat = function(item)
  apply_vars(item, "black rat", "mon_black_rat", "2")
end

genome.options = { dire_wolf, black_rat }
genome.weights = { 100, 200 }

---@class ItemgroupModifierParams
---@field item Item

---@param params ItemgroupModifierParams
function genome.postprocess(params)
  local total_weight = 0
  for _, weight in ipairs(genome.weights) do
    total_weight = total_weight + weight
  end

  local weight = gapi.rng(0, total_weight)
  local option
  local count = 0
  for i, add in ipairs(genome.weights) do
    count = count + add
    if count >= weight then
      option = genome.options[i]
      break
    end
  end

  option(params.item)
end

return genome


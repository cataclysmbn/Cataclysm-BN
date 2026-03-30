---@type table<string, integer>
local material_rank = {
  steel = 5,
  iron = 4,
  copper = 3,
  bronze = 3,
  stone = 2,
  bone = 2,
  wood = 1,
  leather = 1,
  cotton = 1,
}

---@type table<string, string>
local material_label = {
  steel = "steel",
  iron = "iron",
  copper = "copper",
  bronze = "bronze",
  stone = "stone",
  bone = "bone",
  wood = "wooden",
  leather = "leather",
  cotton = "cloth",
}

---@param fact ProcPartFact
---@param wanted string
---@return boolean
local function fact_has_material(fact, wanted)
  for _, mat in ipairs(fact.mat or {}) do
    if mat == wanted then return true end
  end
  return false
end

---@param facts ProcPartFact[]?
---@param pred fun(fact: ProcPartFact): boolean
---@return boolean
local function any_fact(facts, pred)
  for _, fact in ipairs(facts or {}) do
    if pred(fact) then return true end
  end
  return false
end

---@param params ProcParams
---@param wanted string
---@return ProcPartFact[]
local function facts_for_role(params, wanted)
  local out = {}
  local facts = params.facts or {}
  for index, pick in ipairs(params.picks or {}) do
    if pick.role == wanted and facts[index] ~= nil then
      out[#out + 1] = facts[index]
    end
  end
  return out
end

---@param fact ProcPartFact
---@return integer
local function best_material_rank(fact)
  local best = 0
  for _, mat in ipairs(fact.mat or {}) do
    best = math.max(best, material_rank[mat] or 0)
  end
  return best
end

---@param facts ProcPartFact[]?
---@return string
local function primary_material(facts)
  local best_rank = -1
  local best_name = "simple"
  for _, fact in ipairs(facts or {}) do
    for _, mat in ipairs(fact.mat or {}) do
      local rank = material_rank[mat] or 0
      if rank > best_rank then
        best_rank = rank
        best_name = material_label[mat] or mat
      end
    end
  end
  return best_name
end

---@param facts ProcPartFact[]?
---@return integer
local function total_mass(facts)
  local total = 0
  for _, fact in ipairs(facts or {}) do
    total = total + math.max(fact.mass_g or 0, 0)
  end
  return total
end

---@param facts ProcPartFact[]?
---@return integer
local function total_rank(facts)
  local total = 0
  for _, fact in ipairs(facts or {}) do
    total = total + best_material_rank(fact)
  end
  return total
end

---@param name string
---@param role_facts table<string, ProcPartFact[]>
---@return string
local function gear_description(name, role_facts)
  local parts = {}
  for _, role in ipairs({ "blade", "head", "tip", "shaft", "handle", "guard", "grip", "binding", "reinforcement" }) do
    local facts = role_facts[role] or {}
    if #facts > 0 then
      parts[#parts + 1] = string.format("%s %s", primary_material(facts), role)
    end
  end
  if #parts == 0 then
    return string.format("A procedural %s.", name)
  end
  return string.format("A %s assembled from %s.", name, table.concat(parts, ", "))
end

---@param params ProcParams
---@return table<string, ProcPartFact[]>
local function gear_role_facts(params)
  local ret = {}
  for _, role in ipairs({ "blade", "guard", "handle", "grip", "reinforcement", "head", "shaft", "tip", "binding" }) do
    ret[role] = facts_for_role(params, role)
  end
  return ret
end

---@param params ProcParams
---@return ProcBlobMelee
local function weapon_melee(params)
  local role_facts = gear_role_facts(params)
  local blade = role_facts.blade
  local head = role_facts.head
  local tip = role_facts.tip
  local handle = role_facts.handle
  local shaft = role_facts.shaft
  local guard = role_facts.guard
  local grip = role_facts.grip
  local binding = role_facts.binding
  local reinforcement = role_facts.reinforcement
  local total = total_mass(params.facts)
  local striking_mass = total_mass(blade) + total_mass(head) + total_mass(tip) + total_mass(reinforcement)
  local striking_rank = total_rank(blade) + total_rank(head) + total_rank(tip) + total_rank(reinforcement)
  local hand_rank = total_rank(handle) + total_rank(shaft) + total_rank(grip) + total_rank(binding)

  local bash = 2
  local cut = 0
  local stab = 0
  local to_hit = 0
  local dur = math.max(1, math.floor((striking_rank + hand_rank) / 2))
  local moves = 90

  if params.schema_id == "sword" then
    bash = math.max(4, math.floor(total / 260) + math.floor(hand_rank / 2) + math.floor(total_rank(reinforcement) / 2))
    cut = math.max(8, math.floor(total_mass(blade) / 55) + striking_rank * 2)
    stab = math.max(4, math.floor(total_mass(blade) / 90) + striking_rank + total_rank(reinforcement))
    to_hit = math.max(-1, math.min(3, math.floor((hand_rank + total_rank(grip) + total_rank(guard)) / 2) - math.floor(total / 1400)))
    dur = math.max(6, striking_rank * 3 + hand_rank + math.floor(total / 180))
    moves = math.max(78, math.min(150, 78 + math.floor(total / 28) - hand_rank * 2))
  elseif params.schema_id == "axe" then
    bash = math.max(6, math.floor(total / 180) + striking_rank * 2)
    cut = math.max(10, math.floor(total_mass(head) / 45) + striking_rank * 3)
    stab = any_fact(head, function(fact)
      return fact_has_material(fact, "stone") or fact_has_material(fact, "bone") or fact_has_material(fact, "steel") or fact_has_material(fact, "iron")
    end) and math.max(0, math.floor(striking_rank / 2)) or 0
    to_hit = math.max(-2, math.min(2, hand_rank - math.floor(striking_mass / 900)))
    dur = math.max(6, striking_rank * 3 + hand_rank + math.floor(total / 160))
    moves = math.max(88, math.min(180, 88 + math.floor(total / 22) - hand_rank))
  elseif params.schema_id == "spear" then
    bash = math.max(3, math.floor(total / 260) + math.floor(total_rank(shaft) / 2))
    cut = math.max(0, math.floor(total_mass(tip) / 95) + total_rank(tip))
    stab = math.max(12, math.floor(total_mass(tip) / 40) + striking_rank * 3)
    to_hit = math.max(0, math.min(3, hand_rank + total_rank(binding) - math.floor(total / 1700)))
    dur = math.max(6, striking_rank * 2 + hand_rank * 2 + math.floor(total / 190))
    moves = math.max(92, math.min(165, 92 + math.floor(total / 24) - hand_rank * 2))
  elseif params.schema_id == "knife" then
    bash = math.max(1, math.floor(total / 220) + math.floor(hand_rank / 2))
    cut = math.max(6, math.floor(total_mass(blade) / 30) + striking_rank * 2)
    stab = math.max(6, math.floor(total_mass(blade) / 35) + striking_rank * 2)
    to_hit = math.max(0, math.min(3, hand_rank + total_rank(binding) - math.floor(total / 700)))
    dur = math.max(4, striking_rank * 2 + hand_rank + math.floor(total / 220))
    moves = math.max(65, math.min(110, 65 + math.floor(total / 18) - hand_rank * 2))
  end

  return {
    bash = bash,
    cut = cut,
    stab = stab,
    to_hit = to_hit,
    dur = dur,
    moves = moves,
  }
end

---@param params ProcParams
---@return string
local function gear_name(params)
  local role_facts = gear_role_facts(params)
  if params.schema_id == "sword" then
    return string.format("%s sword", primary_material(role_facts.blade))
  end
  if params.schema_id == "axe" then
    return string.format("%s axe", primary_material(role_facts.head))
  end
  if params.schema_id == "spear" then
    return string.format("%s spear", primary_material(role_facts.tip))
  end
  if params.schema_id == "knife" then
    return string.format("%s knife", primary_material(role_facts.blade))
  end
  return string.format("%s tool", params.schema_id)
end

---@type table<string, string>
local default_result = {
  sword = "proc_sword_generic",
  axe = "proc_axe_generic",
  spear = "proc_spear_generic",
  knife = "proc_knife_generic",
}

---@type table
local gear = {}

---@param params ProcParams
---@return ProcBlob
function gear.full(params)
  local blob = params.blob or {}
  local name = gear_name(params)
  local role_facts = gear_role_facts(params)
  local melee = weapon_melee(params)
  return {
    mass_g = blob.mass_g,
    volume_ml = blob.volume_ml,
    name = name,
    description = gear_description(name, role_facts),
    melee = melee,
  }
end

---@param params ProcParams
---@return ProcMakeResult
function gear.make(params)
  local blob = params.blob or {}
  return {
    result = params.schema_res or default_result[params.schema_id] or "proc_sword_generic",
    name = blob.name,
    description = blob.description,
    mass_g = blob.mass_g,
    volume_ml = blob.volume_ml,
    melee = blob.melee,
    mode = "full",
  }
end

return gear

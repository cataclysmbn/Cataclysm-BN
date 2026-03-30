---@type table<string, string>
local fallback_tags_by_role = {
  bread = "bread",
  meat = "meat",
  cheese = "cheese",
  veg = "veg",
  cond = "cond",
  nut = "trail_nut",
  dried = "trail_dried",
  sweet = "trail_sweet",
}

---@param fact ProcPartFact
---@param wanted string
---@return boolean
local function fact_has_tag(fact, wanted)
  local tags = fact.tag or {}
  for _, tag in ipairs(tags) do
    if tag == wanted then return true end
  end
  return false
end

---@param fact ProcPartFact
---@param wanted string
---@return boolean
local function fact_has_material(fact, wanted)
  local mats = fact.mat or {}
  for _, mat in ipairs(mats) do
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

---@param facts ProcPartFact[]?
---@param wanted string
---@return ProcPartFact[]
local function filter_facts_by_tag(facts, wanted)
  ---@type ProcPartFact[]
  local out = {}
  for _, fact in ipairs(facts or {}) do
    if fact_has_tag(fact, wanted) then
      out[#out + 1] = fact
    end
  end
  return out
end

---@param fact ProcPartFact
---@param wanted string
---@return boolean
local function fact_has_id(fact, wanted)
  return (fact.id or "") == wanted
end

---@param fact ProcPartFact
---@param wanted string
---@return boolean
local function fact_id_contains(fact, wanted)
  return string.find(fact.id or "", wanted, 1, true) ~= nil
end

---@param params ProcParams
---@param wanted string
---@return ProcPartFact[]
local function facts_for_role(params, wanted)
  local picks = params.picks or {}
  local facts = params.facts or {}
  ---@type ProcPartFact[]
  local out = {}
  if #picks > 0 then
    for index, pick in ipairs(picks) do
      if pick.role == wanted and facts[index] ~= nil then
        out[#out + 1] = facts[index]
      end
    end
    return out
  end

  local fallback = fallback_tags_by_role[wanted]
  if fallback == nil then
    return out
  end
  return filter_facts_by_tag(facts, fallback)
end

---@param id string
---@return boolean
local function is_chocolate_id(id)
  return id == "candy" or id == "candy2" or id == "maltballs" or string.find(id, "chocolate", 1, true) ~= nil
end

---@param params ProcParams
---@return ProcValidateResult
local function trail_mix_validate(params)
  local facts = params.facts or {}
  local has_non_chocolate = false
  for _, fact in ipairs(facts) do
    local id = fact.id or ""
    if not is_chocolate_id(id) then
      has_non_chocolate = true
      break
    end
  end
  if not has_non_chocolate then
    return { err = "Trail mix can not be made from only chocolate." }
  end

  local has_nut = false
  local has_dried = false
  for _, fact in ipairs(facts) do
    if fact_has_tag(fact, "trail_nut") then has_nut = true end
    if fact_has_tag(fact, "trail_dried") then has_dried = true end
  end
  if not has_nut then
    return { err = "Trail mix needs at least one nut ingredient." }
  end
  if not has_dried then
    return { err = "Trail mix needs at least one dried fruit ingredient." }
  end
  return { ok = true }
end

---@param facts ProcPartFact[]?
---@return string
local function sandwich_condiment_name(facts)
  local has_peanut_butter = any_fact(facts, function(fact)
    return fact_has_id(fact, "peanutbutter") or fact_has_id(fact, "peanutbutter_imitation")
  end)
  local has_jam = any_fact(facts, function(fact)
    return fact_has_id(fact, "jam_fruit")
  end)
  local has_honey = any_fact(facts, function(fact)
    return fact_has_material(fact, "honey")
  end)
  local has_syrup = any_fact(facts, function(fact)
    return fact_has_id(fact, "syrup")
  end)

  if has_peanut_butter and has_jam then return "PB&J sandwich" end
  if has_peanut_butter and has_honey then return "PB&H sandwich" end
  if has_peanut_butter and has_syrup then return "PB&M sandwich" end
  if has_peanut_butter then return "peanut butter sandwich" end
  if has_jam then return "jam sandwich" end
  if has_honey then return "honey sandwich" end
  if has_syrup then return "syrup sandwich" end

  local named_condiments = {
    mustard = "mustard sandwich",
    ketchup = "ketchup sandwich",
    mayonnaise = "mayonnaise sandwich",
    horseradish = "horseradish sandwich",
    butter = "butter sandwich",
    sauerkraut = "sauerkraut sandwich",
    soysauce = "soy sauce sandwich",
    sauce_pesto = "pesto sandwich",
    sauce_red = "red sauce sandwich",
  }
  for _, fact in ipairs(facts or {}) do
    local named = named_condiments[fact.id or ""]
    if named ~= nil then
      return named
    end
  end
  return "sauce sandwich"
end

---@param params ProcParams
---@return string
local function sandwich_name(params)
  local bread_facts = facts_for_role(params, "bread")
  local meat_facts = facts_for_role(params, "meat")
  local cheese_facts = facts_for_role(params, "cheese")
  local veg_facts = facts_for_role(params, "veg")
  local cond_facts = facts_for_role(params, "cond")

  local has_fish = any_fact(meat_facts, function(fact)
    return fact_has_material(fact, "fish") or fact_id_contains(fact, "fish")
  end)
  local has_blt = any_fact(meat_facts, function(fact)
    return fact_has_id(fact, "bacon")
  end) and any_fact(veg_facts, function(fact)
    return fact_has_id(fact, "lettuce") or fact_has_id(fact, "irradiated_lettuce")
  end) and any_fact(veg_facts, function(fact)
    return fact_has_id(fact, "tomato") or fact_has_id(fact, "irradiated_tomato")
  end)

  if has_fish then return "fish sandwich" end
  if #bread_facts >= 3 and #meat_facts > 0 and #veg_facts > 0 and #cond_facts > 0 then
    return "club sandwich"
  end
  if #meat_facts > 0 and #cheese_facts > 0 and #veg_facts > 0 and #cond_facts > 0 then
    return "deluxe sandwich"
  end
  if has_blt then return "BLT" end
  if #meat_facts > 0 then return "meat sandwich" end
  if #cheese_facts > 0 then return "cheese sandwich" end
  if #veg_facts > 0 then
    if any_fact(veg_facts, function(fact)
      return fact_has_id(fact, "cucumber")
    end) then
      return "cucumber sandwich"
    end
    return "vegetable sandwich"
  end
  if #cond_facts > 0 then
    return sandwich_condiment_name(cond_facts)
  end
  return "sandwich"
end

---@param facts ProcPartFact[]?
---@return string
local function stew_name(facts)
  if any_fact(facts, function(fact)
    return fact_has_material(fact, "fish") or fact_id_contains(fact, "fish")
  end) then
    return "fish stew"
  end
  if any_fact(facts, function(fact)
    return fact_has_tag(fact, "meat")
  end) then
    return "meat stew"
  end
  return "vegetable stew"
end

---@param params ProcParams
---@return string
local function trail_mix_name(params)
  local nut_facts = facts_for_role(params, "nut")
  local dried_facts = facts_for_role(params, "dried")
  local sweet_facts = facts_for_role(params, "sweet")
  local has_nut = #nut_facts > 0
  local has_dried = #dried_facts > 0
  local has_chocolate = any_fact(sweet_facts, function(fact)
    return is_chocolate_id(fact.id or "")
  end)

  if has_nut and has_dried and has_chocolate then return "deluxe trail mix" end
  if has_nut and has_dried then return "trail mix" end
  if has_nut then return "nut mix" end
  if has_dried then return "dried fruit mix" end
  return "snack mix"
end

---@param facts ProcPartFact[]?
---@return string
local function trail_mix_description(facts)
  ---@type table<string, integer>
  local mass_by_name = {}
  for _, fact in ipairs(facts or {}) do
    local id = fact.id or ""
    local mass = math.max(fact.mass_g or 0, 0)
    mass_by_name[id] = (mass_by_name[id] or 0) + mass
  end

  local ids = {}
  for id, _ in pairs(mass_by_name) do
    ids[#ids + 1] = id
  end
  table.sort(ids)

  if #ids == 0 then
    return "A hand-mixed trail snack."
  end

  local lines = {}
  for _, id in ipairs(ids) do
    lines[#lines + 1] = string.format("%s (%d g)", id, mass_by_name[id])
  end
  return string.format("A hand-mixed trail snack made with %s.", table.concat(lines, ", "))
end

---@type table
local food = {}

---@param params ProcParams
---@return ProcBlob
function food.full(params)
  return params.blob or {}
end

---@param params ProcParams
---@return ProcBlob
function food.name(params)
  if params.schema_id == "sandwich" then
    return { name = sandwich_name(params) }
  end
  if params.schema_id == "stew" then
    return { name = stew_name(params.facts or {}) }
  end
  if params.schema_id == "trail_mix" then
    return {
      name = trail_mix_name(params),
      description = trail_mix_description(params.facts or {}),
    }
  end
  return {}
end

---@param params ProcParams
---@return ProcMakeResult
function food.make(params)
  local blob = params.blob or {}
  local result = params.result_override or params.schema_res or "sandwich_generic"
  return {
    result = result,
    name = blob.name,
    description = blob.description,
    kcal = blob.kcal,
    mass_g = blob.mass_g,
    volume_ml = blob.volume_ml,
    vit = blob.vit,
    mode = "full",
  }
end

---@param params ProcParams
---@return ProcValidateResult
function food.validate(params)
  if params.schema_id == "trail_mix" then return trail_mix_validate(params) end
  return { ok = true }
end

return food

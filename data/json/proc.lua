local proc = {}
proc.food = {}

local function sum_parts(parts, key)
  local total = 0
  for _, part in ipairs(parts) do
    total = total + (part[key] or 0)
  end
  return total
end

local function sum_vitamins(parts)
  local total = {}
  for _, part in ipairs(parts) do
    for id, value in pairs(part.vit or {}) do
      total[id] = (total[id] or 0) + value
    end
  end
  return total
end

local function has_tag(parts, needle)
  for _, part in ipairs(parts) do
    for _, tag in ipairs(part.tag or {}) do
      if tag == needle then return true end
    end
  end
  return false
end

function proc.food.full(params)
  local facts = params.facts or {}
  local mass_g = sum_parts(facts, "mass_g")
  local volume_ml = sum_parts(facts, "volume_ml")
  local kcal = sum_parts(facts, "kcal")
  local vit = sum_vitamins(facts)
  local name = "sandwich"
  if has_tag(facts, "meat") then
    name = "meat sandwich"
  elseif has_tag(facts, "cheese") then
    name = "cheese sandwich"
  elseif has_tag(facts, "veg") then
    name = "veggy sandwich"
  end
  return {
    mass_g = mass_g,
    volume_ml = volume_ml,
    kcal = kcal,
    vit = vit,
    name = name,
  }
end

function proc.food.make(params)
  local blob = params.blob or {}
  return {
    name = blob.name,
    kcal = blob.kcal,
    mass_g = blob.mass_g,
    volume_ml = blob.volume_ml,
    mode = "none",
  }
end

return proc

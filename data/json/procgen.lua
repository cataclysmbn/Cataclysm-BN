local procgen = {}
procgen.food = {}
procgen.gear = {}

local function sword_result_from_name(name)
  if name == "nail sword" then return "sword_nail" end
  if name == "crude sword" then return "sword_crude" end
  if name == "hand-forged sword" then return "sword_metal" end
  if name == "bone sword" then return "sword_bone" end
  if name == "2-by-sword" then return "sword_wood" end
  return "proc_sword_generic"
end

function procgen.food.full(params) return params.blob or {} end

function procgen.food.make(params)
  local blob = params.blob or {}
  local result = "sandwich_generic"
  if params.schema_id == "stew" then result = "stew_generic" end
  return {
    result = result,
    name = blob.name,
    kcal = blob.kcal,
    mass_g = blob.mass_g,
    volume_ml = blob.volume_ml,
    vit = blob.vit,
    mode = "full",
  }
end

function procgen.gear.make(params)
  local blob = params.blob or {}
  local result = params.schema_res or "proc_sword_generic"
  if params.schema_id == "sword" then result = sword_result_from_name(blob.name) end
  return {
    result = result,
    name = blob.name,
    mass_g = blob.mass_g,
    volume_ml = blob.volume_ml,
    melee = blob.melee,
    mode = "compact",
  }
end

return procgen

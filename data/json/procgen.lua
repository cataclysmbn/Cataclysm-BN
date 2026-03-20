local procgen = {}
procgen.food = {}
procgen.gear = {}

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
  return {
    result = "proc_sword_generic",
    name = blob.name,
    mass_g = blob.mass_g,
    volume_ml = blob.volume_ml,
    melee = blob.melee,
    mode = "compact",
  }
end

return procgen

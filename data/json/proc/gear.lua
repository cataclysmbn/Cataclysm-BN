---@param name string?
---@return string
local function sword_result_from_name(name)
  if name == "nail sword" then return "sword_nail" end
  if name == "crude sword" then return "sword_crude" end
  if name == "hand-forged sword" then return "sword_metal" end
  if name == "bone sword" then return "sword_bone" end
  if name == "2-by-sword" then return "sword_wood" end
  return "proc_sword_generic"
end

---@type table
local gear = {}

---@param params ProcParams
---@return ProcMakeResult
function gear.make(params)
  local blob = params.blob or {}
  local result = params.result_override or params.schema_res or "proc_sword_generic"
  local melee = blob.melee or {}
  if params.schema_id == "sword" and not params.result_override then result = sword_result_from_name(blob.name) end
  return {
    result = result,
    name = blob.name,
    mass_g = blob.mass_g,
    volume_ml = blob.volume_ml,
    melee = {
      bash = melee.bash,
      cut = melee.cut,
      stab = melee.stab,
      to_hit = melee.to_hit,
      dur = melee.dur,
    },
    mode = "compact",
  }
end

return gear

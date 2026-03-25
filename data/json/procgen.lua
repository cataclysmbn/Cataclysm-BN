---@class ProcPartFact
---@field ix integer
---@field id string
---@field tag string[]?
---@field flag string[]?
---@field mat string[]?
---@field vit table<string, integer>?
---@field qual table<string, integer>?
---@field mass_g integer?
---@field volume_ml integer?
---@field kcal integer?
---@field hp number?
---@field chg integer?
---@field proc string?

---@class ProcPick
---@field ix integer
---@field slot string
---@field role string

---@class ProcBlobMelee
---@field bash integer?
---@field cut integer?
---@field stab integer?
---@field to_hit integer?
---@field dur integer?

---@class ProcBlob
---@field mass_g integer?
---@field volume_ml integer?
---@field kcal integer?
---@field name string?
---@field description string?
---@field vit table<string, integer>?
---@field melee ProcBlobMelee?

---@class ProcParams
---@field schema_id string
---@field schema_res string
---@field result_override string?
---@field blob ProcBlob?
---@field facts ProcPartFact[]?
---@field picks ProcPick[]?

---@class ProcValidateOk
---@field ok true

---@class ProcValidateErr
---@field err string

---@alias ProcValidateResult ProcValidateOk|ProcValidateErr

---@class ProcMakeResult: ProcBlob
---@field result string?
---@field mode string?

---@class ProcgenModule
---@field food table
---@field gear table

---@type ProcgenModule
local procgen = {
  food = require("proc.food"),
  gear = require("proc.gear"),
}

return procgen

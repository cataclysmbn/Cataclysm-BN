gdebug.log_info("SpeedyDex: main")
local mod = game.mod_runtime[game.current_mod]

---@class SpeedyDexStorage
---@field additional_moves_per_dex number
---@field min_required_dex number
---@field allow_negative_bonus boolean
local storage = game.mod_storage[game.current_mod]

mod.storage = storage

---CONFIGURATION
---Default: for every level above 10 dexterity, the player gains 2 moves. Negative bonuses are not allowed.
storage.min_required_dex = 10 -- The minimum dex required for bonus to kick in
storage.additional_moves_per_dex = 2 -- The amount of moves gained per dex above the minimum
storage.allow_negative_bonus = false

---Applies SpeedyDex speed bonus to the player every turn.
---@param params OnCharacterResetStatsParams
mod.speedydex = function(params)
  local character = params.character

  local dex = character:get_dex()
  local bonus = mod.calc_speedydex_bonus(dex)

  --   gdebug.log_info(character:get_name() .. "{dex: " .. dex .. " bonus: " .. bonus .. "}")
  character:mod_speed_bonus(bonus)
end

---Calculate speed bonus from dexterity value.
---@param dex number
---@return number
mod.calc_speedydex_bonus = function(dex)
  ---@diagnostic disable-next-line: unnecessary-if
  -- do negative bonus if allowed
  if storage.allow_negative_bonus and dex < storage.min_required_dex then
    return (storage.min_required_dex - dex) * storage.additional_moves_per_dex
  else
    local modified_dex = math.max(dex - storage.min_required_dex, 0)
    return modified_dex * storage.additional_moves_per_dex
  end
end

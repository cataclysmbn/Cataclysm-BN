gdebug.log_info("LUA HOOKS TEST: MAIN ONLINE")

local mod = game.mod_runtime[game.current_mod]

---@param params OnMonDeathParams
mod.chicken_death = function(params)
  local killed = params.mon
  local killer = params.killer
  if killer == nil then return end
  if killer:is_avatar() then
    if killed:get_type():str() == "mon_chicken" then
      gdebug.log_info("Good job. You killed it.")
      gapi.add_msg(MsgType.mixed, string.format("At somewhere, killing a chicken might be illegal."))
    end
  end
end

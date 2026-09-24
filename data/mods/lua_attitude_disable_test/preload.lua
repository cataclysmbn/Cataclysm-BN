gdebug.log_info("lua_attitude_disable_test: preload online.")

-- Must never run while lua_attitude is disabled (#10367).
-- If it does, it reports an error and makes the hostile test bot friendly.
---@param _mon Monster
---@param _target Character|nil
---@return MonsterAttitude
game.monster_attitude_functions["lua_attitude_disable_test_friendly"] = function(_mon, _target)
  gdebug.log_error("lua_attitude_disable_test: lua_attitude was called although it is disabled.")
  return MonsterAttitude.MATT_FRIEND
end

-- lua_ai stays enabled: the hostile test bot holds position instead of approaching.
---@param mon Monster
---@return boolean
game.monster_ai_functions["lua_ai_disable_test_hold"] = function(mon)
  local turns = (tonumber(mon:get_value("lua_ai_disable_test_turns")) or 0) + 1
  mon:set_value("lua_ai_disable_test_turns", tostring(turns))
  if turns % 10 == 1 then gapi.add_msg(MsgType.info, string.format("lua_ai test bot holds (turn %d).", turns)) end
  return true
end

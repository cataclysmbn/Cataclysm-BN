gdebug.log_info("ebook_lua: preload online.")

local mod = game.mod_runtime[game.current_mod]
---@class storage
---@field hook_assuarance table<integer, string>
local storage = game.mod_storage[game.current_mod]

mod.stor = storage

mod.cache_static = function(category, name, binding_id)
  storage[category] = storage[category] or {}
  if not storage[category][name] then storage[category][name] = binding_id.new(name) end
  return storage[category][name]
end

-- mod.cache_static( "flags", "", JsonFlagId.new())

-- function should return false after all.
---@type fun(turn: integer, func: function): boolean
mod.turntimer_hook = function(turn, func)
  gdebug.log_info(string.format("Turn %d, Timer is on for %d turn", gapi.current_turn():to_turn(), turn))
  ---@type TimeDuration
  local the_when = gapi.current_turn() - gapi.turn_zero() + TimeDuration.from_turns(turn)
  gapi.add_on_every_x_hook(the_when, func)
  --Assurance on game closing
  --storage.hook_assuarance[the_when:to_turns()] = func
  --Oops. lua table can't store function.
  storage.hook_assuarance = storage.hook_assuarance or {}
  storage.hook_assuarance[the_when:to_turns()] = "Requiem"
end

game.add_hook("on_game_load", function(...) return mod.assure_timer_hook(...) end)

game.iuse_functions["LUA_EBOOK"] = function(...) return mod.ebook_ui(...) end

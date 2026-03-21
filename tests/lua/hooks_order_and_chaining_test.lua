local function mk_hook(label, ret)
  return function(params)
    params.results.log = params.results.log or {}
    table.insert(params.results.log, label)
    if params.prev ~= nil then params.results.prev_seen = tostring(params.prev) end
    return ret
  end
end

local function mk_hook_set_prev(label, ret)
  return function(params)
    params.results.log = params.results.log or {}
    table.insert(params.results.log, label)
    if params.prev ~= nil then params.results.prev_seen = tostring(params.prev) end
    params.prev = ret
    return ret
  end
end

-- legacy hook (priority 0)
game.add_hook("on_game_load", mk_hook("legacy", "legacy_ret"))

-- priority 5 (this one overrides params.prev)
game.add_hook("on_game_load", { mod_id = "m5", priority = 5, fn = mk_hook_set_prev("p5", "p5_ret") })

-- priority 10
game.add_hook("on_game_load", { mod_id = "m10", priority = 10, fn = mk_hook("p10", "p10_ret") })

-- write results for C++ test to assert
local results = cata.run_hooks("on_game_load")
game.cata_internal.hook_test_results = results

local function mk(label, ret)
  return function(params)
    params.results.log = params.results.log or {}
    table.insert(params.results.log, label)
    return ret
  end
end

-- highest priority runs first and returns false
game.add_hook("on_game_save", { mod_id = "m10", priority = 10, fn = mk("p10", false) })

-- should not run when exit_early is enabled
game.add_hook("on_game_save", { mod_id = "m5", priority = 5, fn = mk("p5", true) })

local results = cata.run_hooks_exit_early("on_game_save")
game.cata_internal.hook_test_results = results

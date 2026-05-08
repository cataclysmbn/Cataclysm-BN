# Lua hooks

Hooks live in `game.hooks` and are lists of callbacks.

## Registering hooks

Use `game.add_hook` to register a hook:

```lua
game.add_hook("on_game_save", function(params)
  -- ...
end)
```

Automatically captures the current mod ID during registration.

You can also pass a table to set priority:

```lua
game.add_hook("on_game_save", {
  priority = 10,        -- optional (higher runs first, default 0)
  fn = function(params)
    -- ...
  end
})
```

## Execution order

Hooks are executed in descending `priority` order. For equal priorities, insertion order is preserved (stable).

## Chaining and results

When a hook runs, it receives a `params` table.

- `params.results`: shared results table for this `cata.run_hooks` call. Hooks may freely read and modify it.
- `params.prev`: the previous hook's return value (or `nil` for the first hook).

`cata.run_hooks( name )` returns the same `params.results` table.

If any hook returns boolean `false`, the results table will contain:

```lua
results.allowed = false
```

Hooks may return boolean `true` or `false` to indicate success or failure of an operation. Call sites may also request early exit using `{ .exit_early = true }` on the first `false` for performance.

## Example usage

```lua
function table_to_string(table)
  local result = "{ "
  for k, v in pairs(table) do
    if type(v) == "table" then
      result = result .. tostring(k) .. " = " .. table_to_string(v) .. ", "
    else
      result = result .. tostring(k) .. " = " .. tostring(v) .. ", "
    end
  end
  result = result .. " }"
  return result
end

game.add_hook("on_character_try_move", {
  priority = 50,
  fn = function(params)
    gapi.add_msg("50 priority hook: " .. table_to_string(params))
    local map = gapi.get_map()
    local to = params.to
    local ter_id = map:get_ter_at(to):str_id()
    if ter_id:str() == "t_grass" then
      gapi.add_msg("The floor is lava!")
      return false
    end
    return true
  end,
})
game.add_hook("on_character_try_move", {
  priority = 100,
  fn = function(params)
    params.results.extra_info = "Checked by highest priority hook"
    gapi.add_msg("100 priority hook: " .. table_to_string(params))
  end,
})
game.add_hook("on_character_try_move", {
  priority = 0,
  fn = function(params)
    gapi.add_msg("0 priority hook: " .. table_to_string(params))
  end,
})
```

will produce log output like:

```
turn=1335125   time="  1  second" type=neutral  message="100 priority hook: { to = (66,60,0), from = (65,60,0), results = { extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x7effcc33ea58, via_ramp = false, movement_mode = 1,  }"
turn=1335125   time="  1  second" type=neutral  message="50 priority hook: { to = (66,60,0), from = (65,60,0), results = { 1 = { mod_id = <unknown>, priority = 100,  }, extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x7effcc33ea58, via_ramp = false, movement_mode = 1,  }"
turn=1335125   time="  1  second" type=neutral  message="0 priority hook: { to = (66,60,0), from = (65,60,0), results = { 1 = { mod_id = <unknown>, priority = 100,  }, 2 = { mod_id = <unknown>, priority = 50, result = true,  }, 3 = { mod_id = bn, priority = 0, result = true,  }, extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x7effcc33ea58, prev = true, via_ramp = false, movement_mode = 1,  }"
turn=1335125   time="  1  second" type=warning  message="Moving onto this mound of dirt is slow!"
turn=1335126   time="  0 seconds" type=neutral  message="100 priority hook: { to = (67,60,0), from = (66,60,0), results = { extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x3fe77c38, via_ramp = false, movement_mode = 1,  }"
turn=1335126   time="  0 seconds" type=neutral  message="50 priority hook: { to = (67,60,0), from = (66,60,0), results = { 1 = { mod_id = <unknown>, priority = 100,  }, extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x3fe77c38, via_ramp = false, movement_mode = 1,  }"
turn=1335126   time="  0 seconds" type=neutral  message="The floor is lava!"
```

you can see when 'the floor is lava', 0 priority hook gets cancelled, due to `on_character_try_move` having `.exit_early` set to true at the call site.

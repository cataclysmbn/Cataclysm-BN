# Lua フック

フックは `game.hooks` にあり、コールバックのリストです。

## フックの登録

`game.add_hook` を使ってフックを登録します:

```lua
game.add_hook("on_game_save", function(params)
  -- ...
end)
```

登録時には現在の mod ID が自動的に取得されます。

優先度を設定するにはテーブルを渡すこともできます:

```lua
game.add_hook("on_game_save", {
  priority = 10,        -- optional (higher runs first, default 0)
  fn = function(params)
    -- ...
  end
})
```

## 実行順

フックは `priority` の降順で実行されます。同じ優先度では、挿入順が維持されます（安定ソート）。

## 連鎖と結果

フックが実行されると、`params` テーブルを受け取ります。

- `params.results`: この `cata.run_hooks` 呼び出しで共有される結果テーブル。フックは自由に読み取り、変更できます。
- `params.prev`: 直前のフックの戻り値（最初のフックでは `nil`）。

`cata.run_hooks( name )` は同じ `params.results` テーブルを返します。

フックがブール値 `false` を返すと、結果テーブルには次が含まれます:

```lua
results.allowed = false
```

フックは操作の成功または失敗を示すためにブール値 `true` または `false` を返せます。呼び出し側は、パフォーマンスのため最初の `false` で `{ .exit_early = true }` を指定して早期終了を要求することもできます。

## 使用例

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

次のようなログ出力になります:

```
turn=1335125   time="  1  second" type=neutral  message="100 priority hook: { to = (66,60,0), from = (65,60,0), results = { extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x7effcc33ea58, via_ramp = false, movement_mode = 1,  }"
turn=1335125   time="  1  second" type=neutral  message="50 priority hook: { to = (66,60,0), from = (65,60,0), results = { 1 = { mod_id = <unknown>, priority = 100,  }, extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x7effcc33ea58, via_ramp = false, movement_mode = 1,  }"
turn=1335125   time="  1  second" type=neutral  message="0 priority hook: { to = (66,60,0), from = (65,60,0), results = { 1 = { mod_id = <unknown>, priority = 100,  }, 2 = { mod_id = <unknown>, priority = 50, result = true,  }, 3 = { mod_id = bn, priority = 0, result = true,  }, extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x7effcc33ea58, prev = true, via_ramp = false, movement_mode = 1,  }"
turn=1335125   time="  1  second" type=warning  message="Moving onto this mound of dirt is slow!"
turn=1335126   time="  0 seconds" type=neutral  message="100 priority hook: { to = (67,60,0), from = (66,60,0), results = { extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x3fe77c38, via_ramp = false, movement_mode = 1,  }"
turn=1335126   time="  0 seconds" type=neutral  message="50 priority hook: { to = (67,60,0), from = (66,60,0), results = { 1 = { mod_id = <unknown>, priority = 100,  }, extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x3fe77c38, via_ramp = false, movement_mode = 1,  }"
turn=1335126   time="  0 seconds" type=neutral  message="The floor is lava!"
```

「the floor is lava」と表示された場合は、呼び出し側で `on_character_try_move` の `.exit_early` が true に設定されているため、優先度0のフックがキャンセルされたことが分かります。

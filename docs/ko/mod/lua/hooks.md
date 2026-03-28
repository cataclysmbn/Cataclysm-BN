# Lua 훅

훅은 `game.hooks`에 있으며 콜백 목록입니다.

## 훅 등록하기

`game.add_hook`을 사용하여 훅을 등록합니다:

```lua
game.add_hook("on_game_save", function(params)
  -- ...
end)
```

등록 중에 현재 모드 ID를 자동으로 캡처합니다.

우선순위를 설정하기 위해 테이블을 전달할 수도 있습니다:

```lua
game.add_hook("on_game_save", {
  priority = 10,        -- 선택사항 (높을수록 먼저 실행, 기본값 0)
  fn = function(params)
    -- ...
  end
})
```

## 실행 순서

훅은 `priority`가 내림차순으로 실행됩니다. 우선순위가 같으면 삽입 순서가 유지됩니다(안정적).

## 체이닝과 결과

훅이 실행될 때 `params` 테이블을 받습니다.

- `params.results`: 이 `cata.run_hooks` 호출에 대한 공유 결과 테이블. 훅은 자유롭게 읽고 수정할 수 있습니다.
- `params.prev`: 이전 훅의 반환값 (첫 번째 훅의 경우 `nil`).

`cata.run_hooks( name )`은 동일한 `params.results` 테이블을 반환합니다.

어떤 훅이 불리언 `false`를 반환하면, 결과 테이블에 다음이 포함됩니다:

```lua
results.allowed = false
```

훅은 작업의 성공 또는 실패를 나타내기 위해 불리언 `true` 또는 `false`를 반환할 수 있습니다. 호출 지점은 또한 성능을 위해 첫 번째 `false`에서 `{ .exit_early = true }`를 사용하여 조기 종료를 요청할 수 있습니다.

## 사용 예시

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

다음과 같은 로그 출력을 생성합니다:

```
turn=1335125   time="  1  second" type=neutral  message="100 priority hook: { to = (66,60,0), from = (65,60,0), results = { extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x7effcc33ea58, via_ramp = false, movement_mode = 1,  }"
turn=1335125   time="  1  second" type=neutral  message="50 priority hook: { to = (66,60,0), from = (65,60,0), results = { 1 = { mod_id = <unknown>, priority = 100,  }, extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x7effcc33ea58, via_ramp = false, movement_mode = 1,  }"
turn=1335125   time="  1  second" type=neutral  message="0 priority hook: { to = (66,60,0), from = (65,60,0), results = { 1 = { mod_id = <unknown>, priority = 100,  }, 2 = { mod_id = <unknown>, priority = 50, result = true,  }, 3 = { mod_id = bn, priority = 0, result = true,  }, extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x7effcc33ea58, prev = true, via_ramp = false, movement_mode = 1,  }"
turn=1335125   time="  1  second" type=warning  message="Moving onto this mound of dirt is slow!"
turn=1335126   time="  0 seconds" type=neutral  message="100 priority hook: { to = (67,60,0), from = (66,60,0), results = { extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x3fe77c38, via_ramp = false, movement_mode = 1,  }"
turn=1335126   time="  0 seconds" type=neutral  message="50 priority hook: { to = (67,60,0), from = (66,60,0), results = { 1 = { mod_id = <unknown>, priority = 100,  }, extra_info = Checked by highest priority hook, allowed = true,  }, char = sol.avatar *: 0x3fe77c38, via_ramp = false, movement_mode = 1,  }"
turn=1335126   time="  0 seconds" type=neutral  message="The floor is lava!"
```

'the floor is lava'일 때, 호출 지점에서 `on_character_try_move`에 `.exit_early`가 true로 설정되어 있어 0 우선순위 훅이 취소되는 것을 볼 수 있습니다.

# Lua 스크립팅 쿡북

Lua API에 익숙해지고 사용 방법을 배우는 데 도움이 되는 코드 조각들입니다.
이 예제들을 테스트하려면 `` ` `` (백틱) 키를 눌러 게임 내 Lua 콘솔에 코드를 붙여넣으세요.

## 아이템

### 인벤토리에서 장착 및 착용한 모든 아이템 목록 가져오기

```lua
local you = gapi.get_avatar()
local items = you:all_items(false)

for _, item in pairs(items) do
  local status = ""
  if you:is_wielding(item) then
    status = "wielded: "
  elseif you:is_wearing(item) then
    status = "worn: "
  end
  print(status .. item:tname(1, false, 0))
end
```

<details>
<summary>예제 출력</summary>

```
wielded: smartphone
worn: bra
worn: panties
worn: pair of socks
worn: jeans
worn: long-sleeved shirt
worn: pair of sneakers
worn: messenger bag
worn: wrist watch
pocket knife
matchbook
clean water (plastic bottle)
clean water
```

</details>

## 몬스터

### 플레이어 근처에 개 소환하기

```lua
local avatar = gapi.get_avatar()
local coords = avatar:get_pos_ms()
local dog_mtype = MtypeId.new("mon_dog_bcollie")
local doggy = gapi.place_monster_around(dog_mtype, coords, 5)
if doggy == nil then
    gdebug.log_info("Could not spawn doggy :(")
else
    gdebug.log_info(string.format("Spawned Doggy at %s", doggy:get_pos_ms()))
end
```

## 전투

### 전투 기술 사용 시 세부 정보 출력하기

먼저 함수를 정의합니다.

```lua
on_creature_performed_technique = function(params)
  local char = params.char
  local technique = params.technique
  local target = params.target
  local damage_instance = params.damage_instance
  local move_cost = params.move_cost
  gdebug.log_info(
          string.format(
                  "%s performed %s on %s (DI: %s , MC: %s)",
                  char:get_name(),
                  technique.name,
                  target:get_name(),
                  damage_instance:total_damage(),
                  move_cost
          )
  )
end
```

그런 다음 훅을 함수에 한 번만 연결합니다.

```lua
table.insert(game.hooks.on_creature_performed_technique, function(...) return on_creature_performed_technique(...) end)
```

<details>
<summary>예제 출력</summary>

```
Ramiro Waters performed Power Hit on zombie (DI: 27.96 , MC: 58)
```

</details>

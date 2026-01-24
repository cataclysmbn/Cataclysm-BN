# Lua 스크립팅 요리책

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
    status = "들고 있음: "
  elseif you:is_wearing(item) then
    status = "착용 중: "
  end
  print(status .. item:tname(1, false, 0))
end
```

<details>
<summary>예제 출력</summary>

```
들고 있음: 스마트폰
착용 중: 브래지어
착용 중: 팬티
착용 중: 양말
착용 중: 청바지
착용 중: 긴팔 셔츠
착용 중: 운동화
착용 중: 메신저백
착용 중: 손목시계
포켓칼
성냥갑
깨끗한 물 (플라스틱 병)
깨끗한 물
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
    gdebug.log_info("개를 스폰할 수 없습니다 :(")
else
    gdebug.log_info(string.format("개를 %s 위치에 스폰했습니다", doggy:get_pos_ms()))
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
                  "%s가(이) %s을(를) %s에게 사용했습니다 (DI: %s , MC: %s)",
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
Ramiro Waters가(이) zombie에게 Power Hit을(를) 사용했습니다 (DI: 27.96 , MC: 58)
```

</details>

## 아이템 내구도

### 아이템 손상도 확인 및 수정하기

```lua
local you = gapi.get_avatar()
local wielding = you:all_items(false)[1]
print(wielding:get_damage())
print(wielding:get_damage_level(4))
wielding:mod_damage(2000)
print(wielding:get_damage_level(4))
```

## 몬스터

### 몬스터 인벤토리에 아이템 추가하기

```lua
local target_monster = -- [[ 당신의 몬스터 참조 ]]
local scraps = gapi.create_item(ItypeId.new("scrap"), 3)
target_monster:as_monster():add_item(scraps)
```

## NPC

### NPC 생성 및 삭제

```lua
local player = gapi.get_avatar()
local map = gapi.get_map()
local player_pos = player:get_pos_ms()
local place_point = player_pos:xy() + Point.new(0, 2)
local new_npc = map:place_npc(place_point, "thug")

-- 나중에 NPC를 조용히 삭제할 수 있습니다
new_npc:erase()
```

## 날씨 훅

### 날씨 변화에 반응하기

먼저 preload.lua에서 훅을 설정합니다:

```lua
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_weather_changed, function(...) mod.weather_changed_alert(...) end)
table.insert(game.hooks.on_weather_updated, function(...) mod.weather_report(...) end)
```

그 다음 main.lua에서 핸들러를 정의합니다:

```lua
local mod = game.mod_runtime[game.current_mod]

-- 날씨가 변할 때 호출됨 (예: 맑음 -> 비)
mod.weather_changed_alert = function(params)
    local msg = string.format(
        "날씨가 %s에서 %s로 변경되었습니다!",
        params.old_weather_id,
        params.weather_id
    )
    gdebug.log_info(msg)
end

-- 5분마다 현재 날씨 데이터와 함께 호출됨
mod.weather_report = function(params)
    local msg = string.format(
        "현재 날씨: %s, 온도: %.1f°C, 바람: %d, 습도: %d%%",
        params.weather_id,
        params.temperature,
        params.windspeed,
        params.humidity
    )
    gdebug.log_info(msg)
end
```

## 원거리 전투

### 발사된 총과 던져진 아이템에 반응하기

먼저 preload.lua에서 훅을 설정합니다:

```lua
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_shoot, function(...) return mod.on_shoot_fun(...) end)
table.insert(game.hooks.on_throw, function(...) return mod.on_throw_fun(...) end)
```

그 다음 main.lua에서 핸들러를 정의합니다:

```lua
local mod = game.mod_runtime[game.current_mod]

mod.on_shoot_fun = function(params)
    ---@type Item
    local gun = params.gun
    ---@type Item
    local ammo_item = params.ammo
    local ammo = ItypeId.NULL_ID()
    if not ammo_item then
        ammo = gun:ammo_current()
    else
        ammo = ammo_item:get_type()
    end
    local shoot_noise = ammo:obj():slot_ammo().loudness
    gdebug.log_info(string.format("총소리: %d.", shoot_noise))
end

mod.on_throw_fun = function(params)
    ---@type Character
    local thrower = params.thrower
    ---@type Item
    local thrown = params.item
    if thrown:is_gun() then
        gdebug.log_info("어라! 총은 던지는 것이 아닙니다!")
    end
end
```

## 오버맵 쿼리

### 오버맵에서 아이템 찾기 및 조작하기

```lua
-- 특정 위치의 오버맵에서 모든 아이템 찾기
local om_pos = OmPos.new(0, 0, 0)
local items = gapi.overmap_find_items_around(om_pos, 0)

-- 맵에서 아이템을 가져와서 맵이 언로드되어도 Lua에서 유지하기
local map_pos = MapPos.new(100, 100, 0)
local item = gapi.get_map():find_item_at(map_pos)
if item then
    local detached = gapi.create_detached_item(item)
    -- 나중에 다시 위치에 부착할 수 있습니다
    local reattached = gapi.reattach_item(detached, map_pos)
end

-- 같은 맵 내에서 아이템 이동하기
local source_pos = MapPos.new(100, 100, 0)
local dest_pos = MapPos.new(110, 110, 0)
gapi.get_map():move_item_at(source_pos, dest_pos)
```

## 사망 훅

### 몬스터 사망 추적하기

```lua
-- preload.lua에서
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_mon_death, function(...) return mod.on_mon_death(...) end)
```

```lua
-- main.lua에서
local mod = game.mod_runtime[game.current_mod]

mod.on_mon_death = function(params)
    ---@type Creature
    local monster = params.creature
    ---@type Character|nil
    local killer = params.killer

    local killer_name = killer and killer:get_name() or "알 수 없음"
    gdebug.log_info(string.format("%s가(이) %s에게 죽었습니다", monster:get_name(), killer_name))
end
```

### 캐릭터 사망 추적하기

```lua
-- preload.lua에서
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_char_death, function(...) return mod.on_char_death(...) end)
```

```lua
-- main.lua에서
local mod = game.mod_runtime[game.current_mod]

mod.on_char_death = function(params)
    ---@type Character
    local char = params.char
    ---@type Character|nil
    local killer = params.killer

    if char == gapi.get_avatar() then
        gdebug.log_info("당신이 죽었습니다!")
    end
end
```

## 캐릭터 전투 스탯

### 공격 및 스태미나 비용 확인하기

```lua
local you = gapi.get_avatar()
local items = you:all_items(false)

for _, item in pairs(items) do
    print(
        item:tname(1, false, 0) 
        .. " { 공격 비용: " .. item:attack_cost() 
        .. ", 스태미나 비용: " .. item:stamina_cost()
        .. ", 근접 스태미나 비용: " .. you:get_melee_stamina_cost(item)
        .. " }"
    )
end

-- 특수 능력 확인
print("Uncanny dodge: " .. (you:uncanny_dodge() and "네" or "아니오"))
```

## 동적 아이템 액션

### Lua에서 커스텀 아이템 사용 함수 만들기

```lua
-- tick과 can_use 함수로 아이템의 사용 동작 정의
game.iuse_functions["my_custom_item"] = {
    use = function(params)
        local user = params.user
        local item = params.item
        gdebug.log_info("사용 중: " .. item:tname(1))
        return 0  -- 이동 단위로 시간 비용 반환
    end,

    can_use = function(params)
        local user = params.user
        local item = params.item
        -- 사용을 허용하려면 true, 방지하려면 false 반환
        return true
    end,

    tick = function(params)
        local user = params.user
        local item = params.item
        -- 아이템이 활성화되어 있는 동안 주기적으로 호출됨
        if item:get_countdown() == 0 then
            gdebug.log_info("아이템 카운트다운이 완료되었습니다!")
        end
    end
}

-- 주기적 틱을 트리거하기 위해 아이템에 카운트다운 설정
local item = gapi.create_item(ItypeId.new("some_item"), 1)
item:set_countdown(100)  -- 100턴 동안 틱
```

## 더 많은 전투 훅

### 회피, 방어 및 기술 이벤트에 반응하기

```lua
-- preload.lua에서
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_creature_dodged, function(...) return mod.on_creature_dodged(...) end)
table.insert(game.hooks.on_creature_blocked, function(...) return mod.on_creature_blocked(...) end)
table.insert(game.hooks.on_creature_performed_technique, function(...) return mod.on_creature_performed_technique(...) end)
table.insert(game.hooks.on_creature_melee_attacked, function(...) return mod.on_creature_melee_attacked(...) end)
```

```lua
-- main.lua에서
local mod = game.mod_runtime[game.current_mod]

mod.on_creature_dodged = function(params)
    ---@type Character
    local char = params.char
    ---@type Creature
    local source = params.source
    local difficulty = params.difficulty
    gdebug.log_info(string.format("%s가(이) %s를(을) 회피했습니다 (DC: %d)", char:get_name(), source:get_name(), difficulty))
end

mod.on_creature_blocked = function(params)
    ---@type Character
    local char = params.char
    ---@type Creature
    local source = params.source
    local bodypart_id = params.bodypart_id
    local damage_blocked = params.damage_blocked
    gdebug.log_info(string.format(
        "%s가(이) %s를(을) %s에서 방어했습니다 (방어함: %.1f 데미지)",
        char:get_name(),
        source:get_name(),
        bodypart_id,
        damage_blocked
    ))
end

mod.on_creature_melee_attacked = function(params)
    ---@type Character
    local char = params.char
    ---@type Creature
    local target = params.target
    if params.success then
        gdebug.log_info(string.format("%s가(이) %s를(을) 맞췄습니다", char:get_name(), target:get_name()))
    else
        gdebug.log_info(string.format("%s가(이) %s를(을) 빗맞혔습니다", char:get_name(), target:get_name()))
    end
end
```

## 캐릭터 함정 인식

### 함정 확인 및 기억하기

먼저 함정을 설치합니다:

```lua
local u = gapi.get_avatar()
local m = gapi.get_map()
local pos = u:get_pos_ms()
local pos4x = pos + Tripoint.new(4, 0, 0)
-- tr_landmine_buried는 가시성이 20입니다. 찾기 매우 어렵습니다.
local mine = TrapId.new("tr_landmine_buried"):int_id()
m:set_trap_at(pos4x, mine)
print(tostring(u:knows_trap(pos4x)))
```

그 다음 캐릭터가 함정을 인식하도록 합니다:

```lua
local u = gapi.get_avatar()
local m = gapi.get_map()
local pos = u:get_pos_ms()
local pos4x = pos + Tripoint.new(4, 0, 0)
u:add_known_trap(pos4x, m:get_trap_at(pos4x))
print(tostring(u:knows_trap(pos4x)))
```

두 번째 스크립트를 실행한 후에는 함정을 밟지 않고도 함정이 설치된 위치를 볼 수 있습니다.

## 아이템 타입 정보

### ItypeId를 통한 아이템 타입 속성 쿼리

```lua
local item_type = ItypeId.new("9mm")

-- 아이템 타입 객체(ItypeRaw) 가져오기
local itype_raw = item_type:obj()

-- 아이템 타입별 데이터 접근 (예: 탄약)
if itype_raw:slot_ammo() then
    local ammo_data = itype_raw:slot_ammo()
    print("탄약 데미지: " .. ammo_data.damage)
    print("탄약 범위: " .. ammo_data.range)
end

-- 컨테이너의 경우
if itype_raw:slot_container() then
    local container_data = itype_raw:slot_container()
    print("수용량: " .. container_data.capacity)
end

-- 도구의 경우
if itype_raw:slot_tool() then
    local tool_data = itype_raw:slot_tool()
    print("도구 품질: " .. tool_data.quality)
end
```

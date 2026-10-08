# Lua의 타입이 지정된 좌표

게임은 위치 연산을 안전하고 자기 설명적으로 만들기 위해 타입이 지정된 좌표 시스템을 사용합니다.
모든 좌표에는 **원점**과 **스케일**이라는 두 가지 메타데이터가 있습니다. 호환되지 않는 좌표(예: 오버맵 위치와 맵 타일 위치)를 섞으면 조용히 잘못된 결과가 나오는 대신 런타임 오류가 발생합니다.

Lua에서도 마찬가지입니다. **모든 곳에서 타입이 지정된 좌표를 우선 사용하세요. 원시 `Tripoint`와
`Point` 타입은 순수한 오프셋 연산에만 사용하세요**. 즉, 무차원 2D 또는 3D 벡터(예: 방향이나 고정 변위)를 계산하고 그 결과를 타입이 지정된 좌표에 즉시 더하거나 빼는 경우입니다.

---

## 원점

좌표의 **원점**은 측정 기준이 된 참조 프레임을 나타냅니다.

| 원점 문자열 | 의미                                                                |
| ----------- | ------------------------------------------------------------------- |
| `"rel"`     | 무차원 오프셋. 같은 스케일의 모든 좌표에 더할 수 있습니다.          |
| `"abs"`     | 절대 게임 세계 위치. 전역적으로 안정된 유일한 원점입니다.           |
| `"bub"`     | 현재 현실 버블(로드된 맵 영역)의 모서리를 기준으로 한 상대 위치.    |
| `"mnt"`     | 회전을 포함한 로컬 차량(탈것) 공간. `veh` 스케일과 함께 사용합니다. |
| `"sm"`      | 특정 서브맵의 모서리를 기준으로 한 상대 위치.                       |
| `"omt"`     | 특정 오버맵 지형 타일의 모서리를 기준으로 한 상대 위치.             |
| `"mmr"`     | 메모리 맵 영역의 모서리를 기준으로 한 상대 위치.                    |
| `"seg"`     | 세그먼트의 모서리를 기준으로 한 상대 위치.                          |
| `"om"`      | 특정 오버맵의 모서리를 기준으로 한 상대 위치.                       |

`"rel"` 원점은 특별합니다. 타입이 지정된 변위 값으로 동작합니다. 일치하는 두 절대 좌표를 빼면
`"rel"` 결과가 생성되고, `"rel"` 좌표를 상대 좌표가 아닌 좌표에 더하면 같은 비상대 원점이 유지됩니다.

---

## 스케일

좌표의 **스케일**은 각 단계의 단위 크기를 나타냅니다.

| 스케일 문자열       | 약어  | 크기                          |
| ------------------- | ----- | ----------------------------- |
| `"map_square"`      | `ms`  | 게임 타일 1개                 |
| `"vehicle"`         | `veh` | 차량 로컬 타일                |
| `"submap"`          | `sm`  | 맵 타일 12 × 12               |
| `"overmap_terrain"` | `omt` | 서브맵 2개(맵 타일 24 × 24)   |
| `"mem_map_region"`  | `mmr` | `MM_REG_SIZE` 서브맵          |
| `"segment"`         | `seg` | 오버맵 지형 타일 `SEG_SIZE`개 |
| `"overmap"`         | `om`  | 오버맵 지형 타일 `OMAPX`개    |

약어 형식(`ms`, `sm`, `omt` 등)은 모든 팩토리 함수 이름과 프로젝션 함수가 받는 문자열 인수에 사용됩니다.

---

## 타입 이름

타입이 지정된 좌표의 Lua 타입 이름은 원점과 스케일을 PascalCase로 조합하여 만듭니다:

```
Tripoint<Origin><Scale>   →   TripointAbsMs, TripointBubSm, TripointRelOmt, …
Point<Origin><Scale>      →   PointAbsMs, PointRelSm, PointBubMs, …
```

유효한 원점-스케일 조합은 31개입니다. 지원되지 않는 조합(예: `TripointAbsVeh`)을 생성하려고 하면 런타임 오류가 발생합니다.

---

## 타입이 지정된 좌표 만들기

### 팩토리 함수(함수형 스타일)

`coords` 라이브러리는 지원되는 모든 조합에 대해 `coords.tripoint_<origin>_<scale>(x, y, z)` 및
`coords.point_<origin>_<scale>(x, y)`를 제공합니다:

```lua
local player_pos  = gapi.get_avatar():get_pos_ms()   -- TripointBubMs from the API
local spawn_point = coords.tripoint_abs_ms(100, 200, 0)
local delta       = coords.tripoint_rel_ms(5, 0, 0)   -- typed offset
local omt_pos     = coords.tripoint_abs_omt(12, 8, 0)
```

원점과 스케일을 문자열로 받는 일반 생성자도 있습니다:

```lua
local p = coords.tripoint("abs", "ms", 100, 200, 0)
local q = coords.point("rel", "sm", 3, 3)
```

### 이름 있는 생성자(OOP 스타일)

모든 타입이 지정된 좌표에는 전역 범위에 이름 있는 생성자 테이블도 있습니다:

```lua
local p = TripointAbsMs.new(100, 200, 0)      -- from x, y, z
local q = TripointAbsMs.new(raw_tripoint)      -- from raw Tripoint
local r = TripointAbsMs.new(point_coord, z)    -- from matching PointAbsMs + z
local s = TripointAbsMs.new()                  -- zero
```

두 스타일은 동일한 객체를 만듭니다. 일반적으로 팩토리 스타일이 더 간결합니다.

---

## 좌표 구성요소 읽기

```lua
local p = coords.tripoint_abs_ms(10, 20, 1)

print(p:x(), p:y(), p:z())   -- 10  20  1
print(p:origin())             -- "abs"
print(p:scale())              -- "ms"
print(p:type())               -- "TripointAbsMs"

local xy = p:xy()             -- PointAbsMs(10, 20); drops z, preserves origin/scale
local raw = p:raw()           -- raw Tripoint(10, 20, 1); strips all tags
```

구성요소는 `set_x`, `set_y`, `set_z`로 변경할 수 있습니다.

---

## 산술 연산

### 덧셈

타입이 지정된 좌표에는 다음을 더할 수 있습니다:

- 원시 `Point` 또는 원시 `Tripoint`; 결과는 원래 원점과 스케일을 유지합니다.
- 같은 스케일의 `"rel"` 타입 좌표; 결과 타입은 같습니다.
- 다른 `"rel"` 타입 좌표에 더할 피연산자; 비상대 원점이 우선됩니다.

```lua
local pos     = coords.tripoint_abs_ms(10, 20, 0)
local offset  = coords.tripoint_rel_ms(3, 0, 0)  -- typed relative offset

local new_pos = pos + offset          -- TripointAbsMs(13, 20, 0)
local also    = pos + Tripoint.new(0, 5, 0)  -- TripointAbsMs(10, 25, 0)  (raw as vector)
```

### 뺄셈

`"rel"` 좌표나 원시 값을 빼는 것은 덧셈의 역방향처럼 동작합니다. 같은 원점과 스케일을 가진 서로 일치하는 비상대 좌표 두 개를 빼면 `"rel"` 결과가 생성됩니다:

```lua
local a   = coords.tripoint_abs_ms(15, 20, 0)
local b   = coords.tripoint_abs_ms(10, 20, 0)
local rel = a - b                              -- TripointRelMs(5, 0, 0)
```

### 곱셈

`"rel"` 좌표만 정수 스칼라를 곱할 수 있습니다:

```lua
local step  = coords.tripoint_rel_ms(1, 0, 0)
local five  = step * 5                          -- TripointRelMs(5, 0, 0)
```

### 동등성과 순서

타입이 지정된 두 좌표는 원점, 스케일 및 원시 값이 모두 같을 때만 동일합니다.
`<` 연산자는 `(origin, scale, raw)`를 사전식으로 비교하므로 타입이 지정된 좌표를 테이블 키와 정렬된 컨테이너에서 안전하게 사용할 수 있습니다.

---

## 프로젝션

프로젝션은 **원점을 유지한 채** 한 스케일의 좌표를 다른 스케일로 변환합니다.
게임의 스케일 계층은 다음과 같습니다:

```
ms  <  sm  <  omt  <  mmr  <  seg  <  om
```

더 거친 스케일로 프로젝션할 때는 음의 무한대 방향으로 반올림합니다(바닥 나눗셈).

```lua
local abs_ms  = coords.tripoint_abs_ms(25, 26, 2)
local abs_omt = abs_ms:to_omt()   -- TripointAbsOmt(1, 1, 2)
local abs_sm  = abs_ms:to_sm()    -- TripointAbsSm(2, 2, 2)
```

모든 대상 스케일에 대해 편의상 사용할 수 있는 메서드가 있습니다: `:to_ms()`, `:to_sm()`, `:to_omt()`,
`:to_mmr()`, `:to_seg()`, `:to_om()`. 일반적인 `:to(scale_string)` 형식도 사용할 수 있습니다.

### project_remain: 몫과 나머지로 나누기

세밀한 좌표가 어느 더 거친 타일에 속하는지와 그 타일 안의 위치를 모두 알아야 한다면
`project_remain`을 사용하세요. 두 값을 반환합니다:

```lua
local abs_ms = coords.tripoint_abs_ms(25, 26, 2)
local quotient, remainder = abs_ms:project_remain_omt()
-- quotient  → TripointAbsOmt(1, 1, 2):   which overmap terrain tile
-- remainder → PointOmtMs(1, 2):          offset within that tile (fine scale, omt origin)
```

약식 메서드: `:project_remain_sm()`, `:project_remain_omt()`, `:project_remain_mmr()`,
`:project_remain_seg()`, `:project_remain_om()`. 일반 형식은 `:project_remain("omt")`입니다.

`coords` 라이브러리는 자유 함수로도 제공합니다:
`coords.project_remain(coord, scale_string)`, `coords.project_remain_omt(coord)` 등.

### project_combine: 몫과 나머지로 재구성하기

`project_combine`은 `project_remain`의 역연산입니다. 거친 좌표와 세밀한 오프셋을 받아 세밀한 스케일의 절대 좌표를 만듭니다:

```lua
local quotient, remainder = abs_ms:project_remain_omt()
local restored = coords.project_combine(quotient, remainder)
-- restored → TripointAbsMs(25, 26, 2)
```

인스턴스 메서드 `quotient:project_combine(remainder)`로도 사용할 수 있습니다.

---

## 거리 함수

두 타입이 지정된 좌표 사이의 거리를 계산하려면 원점과 스케일이 같아야 합니다:

```lua
local a = coords.tripoint_abs_ms(10, 10, 0)
local b = coords.tripoint_abs_ms(13, 14, 0)

local rl  = coords.rl_dist(a, b)      -- rectilinear (Manhattan / Chebyshev) distance
local trig = coords.trig_dist(a, b)   -- Euclidean distance
local sq  = coords.square_dist(a, b)  -- square (Chebyshev) distance
```

인스턴스 메서드도 제공됩니다: `a:rl_dist(b)`, `a:trig_dist(b)`, `a:square_dist(b)`.

---

## 타일 열거 도우미

`coords` 라이브러리는 표준 타일 영역을 덮는 타입이 지정된 포인트 좌표 배열을 반환하는 유틸리티를 제공합니다. 영역 순회에 유용합니다:

```lua
local sm_tiles  = coords.submap_tiles()           -- all PointSmMs in one submap
local bub_tiles = coords.tinymap_tiles()          -- all PointBubMs in the tinymap
local omt_tiles = coords.overmap_terrain_tiles()  -- all PointOmtMs in one overmap terrain tile
local om_tiles  = coords.overmap_tiles()          -- all PointOmMs in one overmap
```

---

## 원시 Point와 Tripoint를 사용할 때

`Point`와 `Tripoint`는 태그가 없는 2D/3D 정수 벡터입니다. 다음 경우에만 사용하세요:

- 고유한 게임 세계 의미가 없는 순수 오프셋을 계산할 때(예: 방향 상수, 이웃 변위, 회전 결과).
- 값을 저장하지 않고 타입이 지정된 좌표에 즉시 더하거나 뺄 때.

```lua
-- Acceptable: raw Tripoint as a throwaway displacement vector
local neighbour = player_pos + Tripoint.new(1, 0, 0)

-- Preferred: named typed offset when the displacement has a scale context
local step = coords.tripoint_rel_ms(1, 0, 0)
local next  = player_pos + step
```

타입이 지정된 대응물이 있을 때 위치를 원시 `Tripoint` 또는 `Point` 값으로 저장하지 마세요. 타입이 지정된 형식은 연산 시점에 스케일 불일치 버그를 잡아내지만, 원시 형식은 잘못된 맵 좌표를 조용히 만들 수 있습니다.

---

## 유효한 원점-스케일 조합

모든 원점이 모든 스케일에서 의미가 있는 것은 아닙니다. 지원되는 조합은 다음과 같습니다:

| 원점  | 유효한 스케일                         |
| ----- | ------------------------------------- |
| `rel` | 모든 스케일                           |
| `abs` | `ms`, `sm`, `omt`, `mmr`, `seg`, `om` |
| `bub` | `ms`, `sm`                            |
| `mnt` | `veh`만                               |
| `sm`  | `ms`만                                |
| `omt` | `ms`, `sm`                            |
| `mmr` | `ms`, `sm`, `omt`                     |
| `seg` | `ms`, `sm`, `omt`, `mmr`              |
| `om`  | `ms`, `sm`, `omt`, `mmr`, `seg`       |

지원되지 않는 조합을 생성하면 잘못된 타입 이름을 식별하는 메시지와 함께 런타임 오류가 발생합니다.

---

## 빠른 참조: coords 라이브러리 API

| 함수                                             | 설명                                                    |
| ------------------------------------------------ | ------------------------------------------------------- |
| `coords.tripoint(origin, scale, x, y, z)`        | 일반 타입 지정 tripoint 생성자                          |
| `coords.point(origin, scale, x, y)`              | 일반 타입 지정 point 생성자                             |
| `coords.tripoint_<o>_<s>(x, y, z)`               | 타입 지정 tripoint 팩토리(예: `coords.tripoint_abs_ms`) |
| `coords.point_<o>_<s>(x, y)`                     | 타입 지정 point 팩토리                                  |
| `coords.project_remain(coord, scale)`            | 좌표를 몫과 나머지로 분할                               |
| `coords.project_remain_sm/omt/mmr/seg/om(coord)` | project_remain 약식 변형                                |
| `coords.project_combine(coarse, fine)`           | 분할된 쌍에서 세밀한 좌표 재구성                        |
| `coords.rl_dist(a, b)`                           | 직선(맨해튼/체비셰프) 거리                              |
| `coords.trig_dist(a, b)`                         | 유클리드 거리                                           |
| `coords.square_dist(a, b)`                       | 정사각형(체비셰프) 거리                                 |
| `coords.submap_tiles()`                          | 한 서브맵 내 모든 `PointSmMs` 오프셋 배열               |
| `coords.tinymap_tiles()`                         | 틴이맵 내 모든 `PointBubMs` 오프셋 배열                 |
| `coords.overmap_terrain_tiles()`                 | 한 OMT 내 모든 `PointOmtMs` 오프셋 배열                 |
| `coords.overmap_tiles()`                         | 한 오버맵 내 모든 `PointOmMs` 오프셋 배열               |

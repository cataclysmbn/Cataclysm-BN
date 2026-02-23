# Mapgen

# 건물과 지형이 생성되는 방식

Cataclysm은 탐색 시 `mapgen`으로 건물과 지형을 생성합니다. 이는 오버맵 지형별 함수로 동작하며,
`[m]` 지도에서 보이는 타일도 오버맵 지형에 의해 결정됩니다. 오버맵 지형("oter")은
`overmap_terrain.json`에 정의됩니다.

기본적으로 oter는 JSON 항목의 `"id"`와 일치하는 내장 mapgen 함수 하나를 가집니다
(예: "house", "bank"). 여러 함수도 가능합니다. 플레이어가 지도에서 집으로 표시된 영역의 범위에
들어오면, 게임은 "house"용 함수 목록에서 반무작위로 하나를 선택해 실행하고, 벽을 배치하고 아이템,
몬스터, 고무 닭 같은 것들을 추가합니다. 이 모든 과정은 매우 짧은 시간에 수행됩니다(나중에 중요).

모든 mapgen 함수는 24x24 타일 영역에서 생성됩니다. 큰 건물도 마찬가지이며, 다소 난해하지만 꽤
효과적인 방식으로 3x3 호텔 같은 대형 구조를 조립합니다.

무작위이면서도(어느 정도) 그럴듯한 세계를 만들기 위해 많은 규칙과 예외가 있으며, 아래에서 설명합니다.

방법은 세 가지입니다:

- mapgen 항목 추가
- JSON 객체 정의
- update_mapgen 사용

# mapgen 항목 추가

건물 변형을 만들기 위해 반드시 새 `overmap_terrain`을 만들 필요는 없습니다. 커스텀 주유소를 만들 때
mapgen 항목을 정의하고 이를 `"s_gas"` mapgen 목록에 추가하면, 세계의 주유소 무작위 변형에 포함됩니다.

기존 `overmap_terrain`을 사용하고 해당 파일에 지붕이나 다른 z-level이 연결되어 있다면, 다른 층도
지상층과 함께 생성됩니다. 이를 피하거나 자체 다중 z-level을 추가하려면 유사한 이름의
`overmap_terrain`(`s_gas_1`)을 만드세요.

## 방법

mapgen을 C++ 함수로 추가하는 것은 즉석 절차 지형 생성에서 가장 빠르고(또한 가장 유연한) 방법 중
하나지만, 게임 재컴파일이 필요합니다.

기존 C++ 건물 대부분은 JSON으로 이전되었고, 현재는 콘텐츠와 모드를 추가할 때 JSON 매핑이 권장됩니다.

- JSON: 사물과 요소를 정의하는 JSON 배열/객체 집합입니다. 장점: 적용이 가장 빠르고, 기능이 대부분
  갖춰져 있습니다. 단점: 프로그래밍 언어가 아니므로 if 문/변수가 없어 같은 JSON mapgen 정의의
  인스턴스는 비슷해집니다. 서드파티 맵 에디터는 현재 구버전입니다.

- JSON 지원에는 중첩 mapgen이 포함됩니다. 이는 연결된 mapgen 일부를 덮어쓰는 더 작은 mapgen 청크로,
  단일 mapgen 파일 안에서도 가구/지형/스폰 다양성을 높일 수 있습니다. 또한 다중 z-level 건물과
  다중 타일 건물을 위해 mapgen 파일을 연결할 수 있습니다.

- Lua: Lua 지원은 `luamethod` 필드로 정의된 lua 메서드를 사용합니다. 모드 로딩 중에는 해당 메서드를
  `game.mapgen_functions[id]`에 넣어야 하며, 여기서 `id`는 `luamethod`의 문자열입니다.
  이를 통해 하드코딩된 실험실과 유사한 절차형 오버맵 타일을 만들 수 있습니다.

## Mapgen 정의 위치

Mapgen 정의는 3곳에 추가할 수 있습니다:

### Lua mapgen

다른 곳에서 Lua로 정의한 맵을 이렇게 정의합니다.

```json
[
  {
    "type": "mapgen",
    "method": "lua",
    "om_terrain": ["slimepit", "slimepit_down"],
    "luamethod": "slimepit"
  }
]
```

### 임베디드 mapgen

`"mapgen": { ... }` 형태는 `builtin` 메서드와 함께만 사용됩니다:

```json
"mapgen": [ { "method": "builtin", "name": "parking_lot" } ]
```

이 방식은 사용하지 말고 standalone을 사용하세요.

### Standalone mapgen

data/json 내 .json 파일에 standalone `{ "type": "mapgen", ... }` 객체로 작성합니다. 아래는
패스트푸드 레스토랑 예시입니다.

```json
[
  {
    "type": "mapgen",
    "om_terrain": "s_restaurant_fast",
    "weight": 250,
    "method": "json",
    "object": {
      "//": "(see below)"
    }
  }
]
```

`"om_terrain"`이 overmap `"id"`와 일치하는 점에 주의하세요. standalone mapgen 항목에는
`om_terrain`이 **필수**입니다.

## 형식과 변수

위 예시는 mapgen 항목만 보여주며, 실제 건물 구성 형식은 보여주지 않습니다. 다만 아래 변수들은 적용
위치와 빈도에 영향을 줍니다:

- method
- om_terrain
- weight

### mapgen `method` 정의

**필수** 값: _json_ - required

```json
"object": { (more json here) }
```

### `om_terrain` 값/배열/중첩 배열로 오버맵 지형 정의

**standalone에서는 필수**

`om_terrain` 값은 세 형태 중 하나로 선언할 수 있습니다. 단일 오버맵 지형 ID, ID 목록,
또는 (목록의 목록인) 중첩 목록입니다.

첫 번째 형태는 `overmap_terrain.json`의 오버맵 지형 ID 하나를 지정합니다:

```json
"om_terrain": "oter_id"
```

두 번째 형태는 ID 목록입니다:

```json
"om_terrain": [ "house", "house_base" ]
```

이렇게 하면 나열한 모든 오버맵 지형 ID에 동일한 JSON mapgen을 적용하여 중복 오버맵 지형을 만듭니다.

세 번째 옵션은 중첩 목록입니다:

```
"om_terrain": [ [ "oter_id_1a", "oter_id_1b", ... ], [ "oter_id_2a", "oter_id_2b", ... ], ... ]
```

이 형태는 단일 JSON 객체로 여러 오버맵 지형을 정의할 수 있게 해줍니다. `rows` 속성은 여기에 나열된
오버맵 지형 수만큼 24x24 블록 단위로 확장됩니다. 지형 ID는 문자열의 중첩 배열로 지정하며,
이는 본 문서 2.1절의 `rows` 속성과 연결된 오버맵 지형 ID(`overmap_terrain.json`)의
행/열을 나타냅니다.

`"terrain"`, `"furniture"`, 또는 특수 매핑(`"items"`, `"monsters"` 등)으로 매핑된 문자는
나열된 모든 오버맵 지형에 공통 적용됩니다.

x/y 좌표(`"place_monsters"`, `"place_loot"`, `"place_item"` 등)를 사용하는 배치는 24x24를 넘어선
전체 확장 좌표를 사용합니다. 중요한 제한은 범위 난수 좌표(예: `"x": [ 10, 18 ]`)가 24x24 지형
경계를 넘으면 안 된다는 점입니다. `[ 0, 23 ]`, `[ 50, 70 ]`는 유효하지만, `[ 0, 47 ]`,
`[ 15, 35 ]`는 단일 24x24 블록을 넘으므로 유효하지 않습니다.

예시:

```json
"om_terrain": [
  [ "apartments_mod_tower_NW", "apartments_mod_tower_NE" ],
  [ "apartments_mod_tower_SW", "apartments_mod_tower_SE" ]
]
```

이 예시에서 `rows` 속성은 48x48이어야 하며, 24x24 각 사분면이 지정된 네 개의
apartments_mod_tower 오버맵 지형 ID와 각각 연결됩니다.

### 선형 지형에서의 `om_terrain`

일부 오버맵 지형은 _linear_입니다. 도로, 터널처럼 다양한 방식으로 연결되는 선형 구조에 사용됩니다.
이런 지형은 `overmap_terrain` 정의에 `LINEAR` 플래그로 표시됩니다
( [OVERMAP docs](OVERMAP.md) 참고 ).

이런 지형의 JSON mapgen을 정의할 때는 가능한 연결 형태마다 인스턴스를 정의해야 합니다. 각 인스턴스는
`overmap_terrain` ID에 접미사가 붙습니다. 접미사는 `_end`, `_straight`, `_curved`, `_tee`,
`_four_way`입니다. 예시는 [`ants.json`](../data/json/mapgen/bugs/ants.json)의 `ants` 정의를 보세요.

### mapgen `weight` 정의

(선택) 게임이 mapgen 함수를 무작위로 선택할 때, 각 함수의 weight 값이 희귀도를 결정합니다.
기본값은 1000이므로, weight 500을 추가하면(다른 함수 수에 따라 달라지지만) 대체로 1/3 정도의
비율로 등장합니다. (10000000처럼 매우 큰 값은 테스트에 유용)

값: number - _0 disables_

기본값: 1000

## `overmap_terrain` 변수가 mapgen에 주는 영향

standalone에서는 `"id"`가 필수 `"om_terrain"` id를 결정합니다. 단, `overmap_terrain`에 다음
변수가 설정된 경우는 예외입니다:

- `"extras"` - mapgen 이후 희귀 랜덤 장면(헬기 추락 등)을 적용
- `"mondensity"` - `"place_groups": [ { "monster": ...` (json)의 기본 `density`를 결정
  이 값이 없으면 `place_monsters`는 density 인자를 명시하지 않는 한 동작하지 않습니다.

## 제한 / TODO

- JSON: 특정 몬스터 스폰 추가는 아직 진행 중(WIP)
- 이전 mapgen.cpp 시스템은 매우 거대한 if/else if 체인이었고, 현재 `builtin` mapgen 클래스로
  절반 정도만 전환되었습니다. 즉, 커스텀 mapgen 함수는 허용되지만, 기본 함수는 새 함수가 추가되면
  무시될 수 있습니다.
- TODO: 이 목록에 추가

# JSON 객체 정의

mapgen 항목의 JSON 객체는 `"fill_ter"` 또는 `"rows"`와 `"terrain"`을 반드시 포함해야 합니다.
다른 필드는 모두 선택입니다.

## `fill_ter`로 지형 채우기

_`rows`가 없을 때 필수_ 지정한 지형으로 채웁니다.

값: `"string"`: data/json/terrain.json의 유효한 terrain id

예시: `"fill_ter": "t_grass"`

## `rows` 배열로 ASCII 맵 작성

_`fill_ter`가 없을 때 필수_

24(또는 48)개의 문자열 중첩 배열이며, 각 문자열은 24(또는 48)자 길이입니다. 각 문자는
`terrain` 및 선택적으로 `furniture` 또는 아래 기타 항목으로 정의됩니다.

사용법:

```json
"rows": [ "row1...", "row2...", ..., "row24..." ]
```

이 맵에 다른 요소를 연결할 수 있습니다. 예를 들어 특정 문자 위치에 주유기(가솔린 포함), 변기(물 포함),
아이템 그룹 아이템, 필드 등을 배치할 수 있습니다.

여기서 사용한 모든 문자는 어딘가에 용도를 나타내는 정의가 있어야 합니다. 정의가 없으면 오류이며,
테스트 실행 시 감지됩니다. 게임에 새 맵을 추가하는 PR을 만들면 테스트가 자동 실행됩니다.
`fill_ter`를 정의했거나 중첩 mapgen을 작성하는 경우에는 예외가 있습니다. 공백 문자와 마침표(`` 및 `.`)
는 정의 없이 `rows`의 배경으로 사용할 수 있습니다.

키로는 double-width가 아닌 모든 유니코드 문자를 사용할 수 있습니다. 예를 들어 대부분의 유럽 문자들은
가능하지만 중국어 문자는 불가능합니다. 이를 활용한다면 에디터가 UTF-8로 저장하는지 확인하세요.
강세 문자도 가능하며 [결합 문자](https://en.wikipedia.org/wiki/Combining_character)도 허용됩니다.
정규화는 수행되지 않으며, 비교는 raw bytes(code unit) 수준에서 이루어집니다. 따라서 사용 가능한
mapgen 키 문자는 사실상 무한합니다. 단, 시각적으로 구분이 어려운 문자를 남용하거나 다른 개발자 환경에서
렌더링이 어려운 희귀 문자를 쓰는 것은 피하세요.

예시:

```json
"rows": [
  ",_____ssssssssssss_____,",
  ",__,__#### ss ####__,__,",
  ",_,,,_#ssssssssss#__,__,",
  ",__,__#hthsssshth#__,__,",
  ",__,__#ssssssssss#_,,,_,",
  ",__,__|-555++555-|__,__,",
  ",_____|.hh....hh.%_____,",
  ",_____%.tt....tt.%_____,",
  ",_____%.tt....tt.%_____,",
  ",_____%.hh....hh.|_____,",
  ",_____|..........%_____,",
  ",,,,,,|..........+_____,",
  ",_____|ccccxcc|..%_____,",
  ",_____w.......+..|_____,",
  ",_____|e.ccl.c|+-|_____,",
  ",_____|O.....S|.S|_____,",
  ",_____||erle.x|T||_____,",
  ",_____#|----w-|-|#_____,",
  ",________,_____________,",
  ",________,_____________,",
  ",________,_____________,",
  " ,_______,____________, ",
  "  ,,,,,,,,,,,,,,,,,,,,  ",
  "   dd                   "
],
```

### `terrain`의 행 지형

**`rows` 사용 시 필수**

`rows`의 지형 ID를 정의합니다. 각 키는 단일 문자이며 값은 terrain id 문자열입니다.

값: `{object}: { "a", "t_identifier", ... }`

예시:

```json
"terrain": {
  " ": "t_grass",
  "d": "t_floor",
  "5": "t_wall_glass_h",
  "%": "t_wall_glass_v",
  "O": "t_floor",
  ",": "t_pavement_y",
  "_": "t_pavement",
  "r": "t_floor",
  "6": "t_console",
  "x": "t_console_broken",
  "$": "t_shrub",
  "^": "t_floor",
  ".": "t_floor",
  "-": "t_wall_h",
  "|": "t_wall_v",
  "#": "t_shrub",
  "t": "t_floor",
  "+": "t_door_glass_c",
  "=": "t_door_locked_alarm",
  "D": "t_door_locked",
  "w": "t_window_domestic",
  "T": "t_floor",
  "S": "t_floor",
  "e": "t_floor",
  "h": "t_floor",
  "c": "t_floor",
  "l": "t_floor",
  "s": "t_sidewalk"
},
```

### `furniture` 배열의 가구 심볼

**선택**

`rows`용 가구 ID를 정의합니다(각 문자는 지형 또는 지형/가구 조합). `f_null`은 가구 없음이며,
항목을 생략해도 됩니다.

예시:

```json
"furniture": {
  "d": "f_dumpster",
  "5": "f_null",
  "%": "f_null",
  "O": "f_oven",
  "r": "f_rack",
  "^": "f_indoor_plant",
  "t": "f_table",
  "T": "f_toilet",
  "S": "f_sink",
  "e": "f_fridge",
  "h": "f_chair",
  "c": "f_counter",
  "l": "f_locker"
},
```

## `set` 배열로 지형/가구/함정 설정

**선택** 지형, 가구, 함정, 방사능 등을 설정하는 명령입니다. 배열 순서대로 처리됩니다.

값: `[ array of {objects} ]: [ { "point": .. }, { "line": .. }, { "square": .. }, ... ]`

예시:

```json
"set": [
  { "point": "furniture", "id": "f_chair", "x": 5, "y": 10 },
  { "point": "radiation", "id": "f_chair", "x": 12, "y": 12, "amount": 20 },
  { "point": "trap", "id": "tr_beartrap", "x": [ 0, 23 ], "y": [ 5, 18 ], "chance": 10, "repeat": [ 2, 5 ] }
]
```

모든 X/Y 값은 `0` ~~`23`의 단일 정수 또는 `[ n1, n2 ]`(각각 `0`~~ `23`) 배열일 수 있습니다.
배열이면 해당 범위(포함)에서 난수가 선택됩니다. 위 예시에서 `"f_chair"`는 항상
`"x": 5, "y": 10`에 배치되지만, `"tr_beartrap"`은 `"x": [ 0, 23 ], "y": [ 5, 18 ]` 영역에
무작위 반복 배치됩니다.

`id` 문자열은 terrain.json, furniture.json, trap.json을 참고하세요.

### `point`에 배치

- `point` 타입과 좌표 `x`, `y` 필요
- `point` 타입이 `radiation`이면 `amount` 필요
- 그 외 타입은 terrain/furniture/trap의 `id` 필요

| Field  | Description                                                                                                          |
| ------ | -------------------------------------------------------------------------------------------------------------------- |
| point  | 허용 값: `"terrain"`, `"furniture"`, `"trap"`, `"radiation"`                                                         |
| id     | 지형/가구/함정 ID. 예: `"id": "f_counter"`, `"id": "tr_beartrap"`. "radiation"에서는 생략.                           |
| x, y   | X, Y 좌표. 값: `0-23`, 또는 범위 내 난수를 위한 `[ 0-23, 0-23 ]`. 예: `"x": 12, "y": [ 5, 15 ]`                      |
| amount | 방사능 양. 값: `0-100`.                                                                                              |
| chance | (선택) 적용될 1/N 확률                                                                                               |
| repeat | (선택) 값: `[ n1, n2 ]`. `n1`~`n2`회 무작위 반복 스폰. 좌표가 랜덤일 때 주로 의미 있음. 예: `[ 1, 3 ]` - 1~3회 반복. |

### `line`에 배치

- `line` 타입과 시작/끝점 `x`,`y`,`x2`,`y2` 필요
- `line` 타입이 `radiation`이면 `amount` 필요
- 그 외 타입은 terrain/furniture/trap의 `id` 필요

예시:

```json
{ "line": "terrain", "id": "t_lava", "x": 5, "y": 5, "x2": 20, "y2": 20 }
```

| Field  | Description                                                                                                          |
| ------ | -------------------------------------------------------------------------------------------------------------------- |
| line   | 허용 값: `"terrain"`, `"furniture"`, `"trap"`, `"radiation"`                                                         |
| id     | 지형/가구/함정 ID. 예: `"id": "f_counter"`, `"id": "tr_beartrap"`. "radiation"에서는 생략.                           |
| x, y   | 시작 X, Y 좌표. 값: `0-23`, 또는 범위 내 난수를 위한 `[ 0-23, 0-23 ]`. 예: `"x": 12, "y": [ 5, 15 ]`                 |
| x2, y2 | 끝 X, Y 좌표. 값: `0-23`, 또는 범위 내 난수를 위한 `[ 0-23, 0-23 ]`. 예: `"x": 22, "y": [ 15, 20 ]`                  |
| amount | 방사능 양. 값: `0-100`.                                                                                              |
| chance | (선택) 적용될 1/N 확률                                                                                               |
| repeat | (선택) 값: `[ n1, n2 ]`. `n1`~`n2`회 무작위 반복 스폰. 좌표가 랜덤일 때 주로 의미 있음. 예: `[ 1, 3 ]` - 1~3회 반복. |

### `square`에 배치

- `square` 타입과 대각 꼭짓점 `x`,`y`,`x2`,`y2` 필요
- `square` 타입이 `radiation`이면 `amount` 필요
- 그 외 타입은 terrain/furniture/trap의 `id` 필요

`square` 인자는 `line`과 동일하지만, `x`,`y`와 `x2`,`y2`는 대각 꼭짓점을 의미합니다.

예시:

```json
{ "square": "radiation", "amount": 10, "x": [0, 5], "y": [0, 5], "x2": [18, 23], "y2": [18, 23] }
```

| Field  | Description                                                                                |
| ------ | ------------------------------------------------------------------------------------------ |
| square | 허용 값: `"terrain"`, `"furniture"`, `"trap"`, `"radiation"`                               |
| id     | 지형/가구/함정 ID. 예: `"id": "f_counter"`, `"id": "tr_beartrap"`. "radiation"에서는 생략. |
| x, y   | 사각형 좌상단 좌표.                                                                        |
| x2, y2 | 사각형 우하단 좌표.                                                                        |

## `place_groups`로 아이템/몬스터 그룹 스폰

**선택** item_groups.json, monster_groups.json에서 아이템/몬스터를 스폰합니다.

값: `[ array of {objects} ]: [ { "monster": ... }, { "item": ... }, ... ]`

### `monster`로 그룹 몬스터 스폰

| Field   | Description                                                              |
| ------- | ------------------------------------------------------------------------ |
| monster | (필수) 값: `"MONSTER_GROUP"`. 목록에서 무작위 개체를 뽑는 몬스터 그룹 id |
| x, y    | (필수) 스폰 좌표. 값: `0-23`, 또는 범위 내 난수를 위한 `[ 0-23, 0-23 ]`  |
| density | (선택) `chance`에 곱해지는 부동소수 배율                                 |
| chance  | (선택) 스폰될 1/N 확률                                                   |

예시:

```json
{ "monster": "GROUP_ZOMBIE", "x": [13, 15], "y": 15, "chance": 10 }
```

`"x"`/`"y"`에 범위를 쓰면 최소/최대값으로 `map::place_spawns`용 사각형 좌표를 만듭니다. 그룹에서 생성된
각 몬스터는 해당 사각형 안의 서로 다른 무작위 위치에 배치됩니다. 위 예시는 (13,15)~(15,15) 범위를
만듭니다(포함).

선택 항목 `density`는 `chance` 값에 곱해지는 부동소수 배율입니다. 결과가 100%를 넘으면 100%마다 1개의
스폰 포인트를 보장하고, 나머지는 확률로 판정됩니다. 이후 몬스터는 그룹의 `cost_multiplier`에 따라
스폰됩니다. 추가로 반경 3(플레이어 주변 7x7, mapgen.cpp의 MON_RADIUS) 오버맵 밀도도 합산됩니다.
monstergroups의 `pack_size`는 굴려진 스폰 포인트 수에 대한 무작위 배율입니다.

### `item`으로 그룹 아이템 스폰

| Field  | Description                                                             |
| ------ | ----------------------------------------------------------------------- |
| item   | (필수) 값: "ITEM_GROUP". 목록에서 무작위 아이템을 고르는 아이템 그룹 id |
| x, y   | (필수) 스폰 좌표. 값: `0-23`, 또는 범위 내 난수를 위한 `[ 0-23, 0-23 ]` |
| chance | (필수) 스폰 % 확률                                                      |

예시:

```json
{ "item": "livingroom", "x": 12, "y": [5, 15], "chance": 50 }
```

`"x"`/`"y"`에 범위를 쓰면 최소/최대값으로 `map::place_items`용 사각형 좌표를 만듭니다. 그룹의 각 아이템은
사각형 내 서로 다른 무작위 위치에 배치됩니다. 위 예시는 (12,5)~(12,15) 범위를 만듭니다(포함).

## `place_monster`로 단일 몬스터 스폰

**선택** 단일 몬스터를 스폰합니다. 특정 몬스터 또는 몬스터 그룹에서 무작위 몬스터를 선택할 수 있습니다.
스폰 밀도 게임 설정의 영향을 받습니다.

값: `[ array of {objects} ]: [ { "monster": ... } ]`

| Field       | Description                                                                                                                                |
| ----------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| monster     | 스폰할 몬스터 ID                                                                                                                           |
| group       | 스폰할 몬스터를 선택할 몬스터 그룹 ID. `monster`와 `group`은 함께 사용하지 않습니다. `group`이 `monster`보다 우선합니다.                   |
| x, y        | 스폰 좌표(단일 또는 영역 사각형). 값: 0-23 또는 `[ 0-23, 0-23 ]` - `[ a, b ]` 사이 난수                                                    |
| chance      | 스폰 수행 % 확률. repeat 사용 시 각 반복마다 독립 판정                                                                                     |
| repeat      | 스폰 반복 횟수. 숫자 또는 범위 가능                                                                                                        |
| pack_size   | 스폰할 몬스터 수. 단일 숫자 또는 `[1-4]` 같은 범위. chance 및 스폰 밀도의 영향 받음. 그룹 스폰 시 무시됨                                   |
| one_or_none | 높은 스폰 밀도로 인해 1마리 초과 스폰되지 않게 함. repeat 미정의이거나 pack size 정의 시 기본 true true, 아니면 false. 그룹 스폰 시 무시됨 |
| friendly    | true면 우호적 몬스터. 기본 false                                                                                                           |
| name        | 몬스터에 표시할 추가 이름                                                                                                                  |
| target      | true면 미션 타겟으로 설정. 미션에서 스폰된 몬스터에만 동작                                                                                 |

높은 스폰 밀도 설정에서는 `monster` 사용 시 추가 몬스터가 생길 수 있습니다. `group` 사용 시에는
항상 한 마리만 스폰됩니다.

`"x"`/`"y"`에 범위를 쓰면 최소/최대값으로 `map::place_spawns`용 사각형 좌표를 만듭니다. 그룹에서 생성된
각 몬스터는 사각형 내 서로 다른 무작위 위치에 배치됩니다. 예:
`"x": 12, "y": [ 5, 15 ]`는 (12,5)~(12,15) 범위를 만듭니다(포함).

예시:

```json
"place_monster": [
    { "group": "GROUP_REFUGEE_BOSS_ZOMBIE", "name": "Sean McLaughlin", "x": 10, "y": 10, "target": true }
]
```

이 예시는 그룹 `GROUP_REFUGEE_BOSS_ZOMBIE`에서 무작위 몬스터 1마리를 배치하고,
이름을 `Sean McLaughlin`으로 지정하며, 좌표 (10,10)에 스폰하고, 미션 타겟으로 설정합니다.

예시:

```json
"place_monster": [
    { "monster": "mon_secubot", "x": [ 7, 18 ], "y": [ 7, 18 ], "chance": 30, "repeat": [1, 3] }
]
```

이 예시는 `mon_secubot`을 (7-18, 7-18) 무작위 좌표에 배치합니다. 30% 확률로 배치되며,
배치 횟수는 `[1-3]` 범위에서 무작위입니다.

## `place_monsters`로 몬스터 그룹 전체 스폰

`place_monsters`는 `place_monster`와 유사하지만, 핵심 차이는 그룹의 유효 항목을 모두 스폰한다는 점입니다.
대형 몬스터 그룹과 함께 이 플래그를 쓰는 것은 권장하지 않습니다. 총 스폰 수 제어가 어렵기 때문입니다.

| Field   | Description                                                                                                                                                                              |
| ------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| monster | 스폰하려는 몬스터 그룹 ID                                                                                                                                                                |
| x, y    | 스폰 좌표(단일 또는 영역 사각형). 값: 0-23 또는 `[ 0-23, 0-23 ]` - `[ a, b ]` 사이 난수                                                                                                  |
| chance  | 그룹 전체가 스폰될 1/N 확률. repeat마다 한 번 판정되며, 실패하면 지정한 그룹 전체가 스폰되지 않음. 비워두면 항상 스폰                                                                    |
| repeat  | 스폰 반복 횟수. 숫자 또는 범위 가능. 그룹 자체를 몇 번 스폰할지 의미                                                                                                                     |
| density | 플레이어가 있는 세계의 스폰 밀도와 곱해진 후 확률 반올림되어 그룹 스폰 횟수를 결정. repeat의 각 반복마다 적용. 예를 들어 최종 배율이 `2`, repeat이 `6`이면 그룹은 `2 * 6` 즉 12회 스폰됨 |

## `place_item` 배열로 특정 아이템 스폰

**선택** 추가할 _특정_ 항목 목록입니다. WIP: 몬스터와 차량도 여기에 포함 예정

값: `[ array of {objects} ]: [ { "item", ... }, ... ]`

예시:

```json
"place_item": [
    { "item": "weed", "x": 14, "y": 15, "amount": [ 10, 20 ], "repeat": [1, 3], "chance": 20 }
]
```

| Field  | Description                                                                                                          |
| ------ | -------------------------------------------------------------------------------------------------------------------- |
| item   | (필수) 스폰할 아이템 ID                                                                                              |
| x, y   | (필수) 스폰 좌표. 값: `0-23`, 또는 범위 내 난수를 위한 `[ 0-23, 0-23 ]`                                              |
| amount | (필수) 스폰 수량. 단일 정수 또는 범위 `[ a, b ]`                                                                     |
| chance | (선택) 아이템 스폰 1/N 확률                                                                                          |
| repeat | (선택) 값: `[ n1, n2 ]`. `n1`~`n2`회 무작위 반복 스폰. 좌표가 랜덤일 때 주로 의미 있음. 예: `[ 1, 3 ]` - 1~3회 반복. |

## `place_artifact` 배열로 아티팩트 스폰

**선택** 아티팩트 스폰 목록입니다. `natural: true`(선택적으로 `property`)를 쓰면 자연 아티팩트를,
그렇지 않으면 무작위 아티팩트를 생성합니다. 매핑 방식에서는 특수 타입이 `artifact`(또는 `artifacts`)입니다.

예시:

```json
"place_artifact": [
    { "x": 12, "y": 10 },
    { "x": [ 4, 18 ], "y": [ 4, 18 ], "repeat": [ 1, 2 ], "natural": true, "property": "ARTPROP_GLOWING" }
]
```

| Field    | Description                                                                                                    |
| -------- | -------------------------------------------------------------------------------------------------------------- |
| x, y     | (필수) 스폰 좌표. 값: `0-23`, 또는 범위 내 난수를 위한 `[ 0-23, 0-23 ]`                                        |
| repeat   | (선택) 값: `[ n1, n2 ]`. `n1`~`n2`회 무작위 스폰. 좌표가 랜덤일 때 주로 의미 있음. 예: `[ 1, 3 ]` - 1~3회 반복 |
| chance   | (선택) 아티팩트 스폰 % 확률                                                                                    |
| natural  | (선택) true면 자연 아티팩트 스폰                                                                               |
| property | (선택) 자연 아티팩트 속성용 `ARTPROP_*` 값. 생략 시 무작위 속성 선택                                           |

## specials로 추가 맵 피처

**선택** 단순 가구/지형 배치 이상을 수행하는 특수 맵 피처입니다.

Specials는 위 `rows` 항목을 사용하는 terrain/furniture 매핑과 같은 방식으로 정의하거나,
좌표로 정확한 위치를 지정해 정의할 수 있습니다.

매핑 정의 형식:

```
"<type-of-special>" : {
    "A" : { <data-of-special> },
    "B" : { <data-of-special> },
    "C" : { <data-of-special> },
    ...
}
```

`"<type-of-special>"`은 아래 나열된 타입 중 하나입니다. `<data-of-special>`은 해당 타입 전용 JSON 객체입니다.
일부 타입은 데이터가 필요 없거나 전부 선택이라 빈 객체만으로 충분합니다. 매핑은 원하는 만큼 정의할 수 있습니다.

각 매핑은 배열이 될 수 있습니다. 한 타일에 여러 번 나타날 수 있는 항목(예: items, fields)은 배열 각 항목을
순서대로 적용합니다. traps/furniture/terrain은 항목 하나를 무작위(동일 확률)로 선택해 적용합니다.

예시(`.` 칸의 2/3에는 grass, 1/3에는 dirt 배치):

```json
"terrain" : {
    ".": [ "t_grass", "t_grass", "t_dirt" ]
}
```

인스턴스 수(즉 확률)를 직접 지정할 수도 있습니다. 희귀 발생에 특히 유용합니다
(흔한 값을 반복 입력하는 대신):

```json
"terrain" : {
    ".": [ [ "t_grass", 2 ], "t_dirt" ]
}
```

예시(`.` 칸마다 blood와 bile 필드 배치):

```json
"fields" : {
    ".": [ { "field": "fd_blood" }, { "field": "fd_bile" } ]
}
```

또는 문자 하나의 매핑을 한 번에 정의:

```json
"mapping" : {
    ".": {
        "traps": "tr_beartrap",
        "field": { "field": "fd_blood" },
        "item": { "item": "corpse" },
        "terrain": { "t_dirt" }
    }
}
```

한 위치에 다양한 타입을 많이 배치하고 싶을 때 유용합니다.

좌표 기반 special 정의:

```
"place_<type-of-special>" : {
    { "x": <x>, "y": <y>, <data-of-special> },
    ...
}
```

`<x>`, `<y>`는 special 배치 위치입니다(x는 가로, y는 세로). 유효 범위는 0..23이며,
최소-최대 값도 지원합니다: `"x": [ 0, 23 ], "y": [ 0, 23 ]`는 맵 내 임의 위치에 배치합니다.

매핑 예시(`"O"`, `";"` 문자는 rows 배열에 있어야 함):

```json
"gaspumps": {
    "O": { "amount": 1000 }
},
"toilets": {
    ";": { }
}
```

변기에 배치할 물 양은 선택 항목이므로 빈 항목도 유효합니다.

좌표 예시:

```json
"place_gaspumps": [
    { "x": 14, "y": 15, "amount": [ 1000, 2000 ] }
],
"place_toilets": [
    { "x": 19, "y": 22 }
]
```

Terrain/furniture/traps는 JSON 객체 대신 단일 문자열로도 지정할 수 있습니다:

```json
"traps" : {
    ".": "tr_beartrap"
}
```

동일 의미:

```json
"traps" : {
    ".": { "trap": "tr_beartrap" }
}
```

### `fields`로 연기/가스/피 배치

| Field     | Description                                                           |
| --------- | --------------------------------------------------------------------- |
| field     | (필수, string) 필드 타입(예: `"fd_blood"`, `"fd_smoke"`)              |
| density   | (선택, integer) 필드 밀도. 기본 1. 가능 값 1, 2, 3                    |
| intensity | (선택, integer) 필드 농도. 1~3 이상. `data/json/field_type.json` 참고 |
| age       | (선택, integer) 필드 나이. 기본 0                                     |

### `npcs`로 NPC 배치

예시:

```json
"npcs": { "A": { "class": "NC_REFUGEE", "target": true, "add_trait": "ASTHMA" } }
```

| Field     | Description                                                                |
| --------- | -------------------------------------------------------------------------- |
| class     | (필수, string) npc class id. `data/json/npcs/npc.json` 참고 또는 자체 정의 |
| target    | (선택, bool) 이 NPC를 미션 타겟으로 지정. `update_mapgen`에서만 유효       |
| add_trait | (선택, string 또는 string 배열) 클래스 정의 외에 이 NPC에 부여할 특성      |

### `signs`로 표지판 배치

메시지가 적힌 표지판(가구 `f_sign`)을 배치합니다. `signage` 또는 `snippet` 중 하나는 필수입니다.
메시지에는 `<full_name>`, `<given_name>`, `<family_name>` 태그를 넣어 무작위 이름을 삽입할 수 있고,
`<city>`로 가장 가까운 도시 이름을 넣을 수 있습니다.

예시:

```json
"signs": { "P": { "signage": "Subway map: <city> stop" } }
```

| Field   | Description                                         |
| ------- | --------------------------------------------------- |
| signage | (선택, string) 표지판에 표시할 메시지               |
| snippet | (선택, string) 표지판에 표시 가능한 스니펫 카테고리 |

### `vendingmachines`로 자판기와 아이템 배치

자판기(가구)를 배치하고 아이템 그룹에서 아이템을 채웁니다. 가끔 고장난 자판기로 생성될 수 있습니다.

| Field      | Description                                                                                                               |
| ---------- | ------------------------------------------------------------------------------------------------------------------------- |
| item_group | (선택, string) 자판기 내부 아이템 생성에 사용할 아이템 그룹. 생략 시 `"vending_food"` 또는 `"vending_drink"`(무작위) 사용 |

### `toilets`로 물이 든 변기 배치

변기(가구)를 배치하고 물을 넣습니다.

| Field  | Description                                           |
| ------ | ----------------------------------------------------- |
| amount | (선택, integer 또는 min/max 배열) 변기에 넣을 물의 양 |

### `gaspumps`로 연료가 든 주유기 배치

주유기를 배치하고 가솔린(때때로 디젤)을 채웁니다.

| Field  | Description                                               |
| ------ | --------------------------------------------------------- |
| amount | (선택, integer 또는 min/max 배열) 주유기에 넣을 연료의 양 |
| fuel   | (선택, string: "gasoline" 또는 "diesel") 넣을 연료 종류   |

### `items`로 아이템 그룹 배치

| Field  | Description                                                                                                                                                                                                                     |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| item   | (필수, string 또는 itemgroup object) 사용할 item group / itype_id                                                                                                                                                               |
| chance | (선택, integer 또는 min/max 배열) 100분의 x 확률로 루프가 그룹에서 계속 아이템을 스폰(그룹 타입에 따라 다수/미스폰 가능, `ITEM_SPAWN.md` 참고). 단, chance가 100이면 item group 스폰을 정확히 1회 실행(`map::place_items` 참고) |
| repeat | (선택, integer 또는 min/max 배열) 배치 반복 횟수, 기본 1                                                                                                                                                                        |

### `monsters`로 몬스터 그룹 배치

실제 몬스터는 맵 로드 시 스폰됩니다. 필드:

| Field   | Description                                                                                                  |
| ------- | ------------------------------------------------------------------------------------------------------------ |
| monster | (필수, string) 몬스터 그룹 id. 맵 로드 시 해당 그룹에서 무작위 몬스터 스폰                                   |
| density | (선택, float) 정의 시 해당 위치의 기본 몬스터 밀도 override(도심으로 갈수록 증가) (`map::place_spawns` 참고) |
| chance  | (선택, integer 또는 min/max 배열) 스폰 포인트 생성 1/x 확률 (`map::place_spawns` 참고)                       |

### `vehicles`로 차량 타입/그룹 배치

| Field    | Description                                                                                                      |
| -------- | ---------------------------------------------------------------------------------------------------------------- |
| vehicle  | (필수, string) 차량 타입 또는 차량 그룹 id                                                                       |
| chance   | (선택, integer 또는 min/max 배열) 차량이 생성될 100분의 x 확률. 기본 1(즉 1% 확률이므로 보통 더 큰 값을 원할 것) |
| rotation | (선택, integer) 차량 방향                                                                                        |
| fuel     | (선택, integer) 연료 상태. 기본 -1은 탱크 1~7% 상태. 양수는 탱크 채움 비율(예: 100은 가득 참)                    |
| status   | (선택, integer) 기본 -1(경미 손상), 0은 완전 상태, 1은 심하게 손상                                               |

### `item`으로 특정 아이템 배치

| Field  | Description                                                               |
| ------ | ------------------------------------------------------------------------- |
| item   | (필수, string) 새 아이템의 type id                                        |
| chance | (선택, integer 또는 min/max 배열) 아이템 스폰 1/x 확률. 기본 1(항상 스폰) |
| amount | (선택, integer 또는 min/max 배열) 스폰 수량. 기본 1                       |
| repeat | (선택, integer 또는 min/max 배열) 배치 반복 횟수. 기본 1                  |

명시 좌표로 이 타입을 쓰려면(`backwards compatibility`) 이름 `place_item`을 사용:

```json
"item": {
    "x": { "item": "rock" }
},
"place_item": [
    { "x": 10, "y": 1, "item": "rock" }
]
```

### `monster`로 특정 몬스터 배치

| Field    | Description                                                             |
| -------- | ----------------------------------------------------------------------- |
| monster  | (필수, string) 몬스터 type id (예: `mon_zombie`)                        |
| friendly | (선택, bool) 우호 여부. 기본 false                                      |
| name     | (선택, string) 몬스터 이름. 기본은 무명                                 |
| target   | (선택, bool) 이 몬스터를 미션 타겟으로 지정. `update_mapgen`에서만 유효 |

### `traps`로 함정 배치

| Field | Description                                     |
| ----- | ----------------------------------------------- |
| trap  | (필수, string) 함정 type id (예: `tr_beartrap`) |

### `furniture`로 가구 배치

| Field | Description                                 |
| ----- | ------------------------------------------- |
| furn  | (필수, string) 가구 type id (예: `f_chair`) |

### `terrain`으로 지형 배치

지형에 `roof` 값이 있고 폐쇄 공간에 있으면 실내로 간주됩니다.

| Field | Description                                 |
| ----- | ------------------------------------------- |
| ter   | (필수, string) 지형 type id (예: `t_floor`) |

### `rubble`로 잔해 배치 및 기존 지형 파괴

잔해를 만들고 기존 지형을 파괴합니다(이 단계는 다른 배치 이후 마지막에 적용). 잔해 생성은 구조물을
파괴하거나 붕괴시킬 수 있는 bashing 함수를 호출합니다.

| Field       | Description                                                                                     |
| ----------- | ----------------------------------------------------------------------------------------------- |
| rubble_type | (선택, furniture id, 기본: `f_rubble`) 생성할 잔해 타입                                         |
| items       | (선택, bool, 기본: false) 구조물 파괴 결과 아이템 배치                                          |
| floor_type  | (선택, terrain id, 기본: `t_dirt`) 해당 위치에 파괴 불가 벽이 있거나 overwrite=true일 때만 사용 |
| overwrite   | (선택, bool, 기본: false) true면 현재 내용을 무시하고 그대로 덮어씀                             |

명시 좌표로 이 타입을 쓰려면 이름 `place_rubble`(복수 아님) 사용:

```json
"place_rubble": [
    { "x": 10, "y": 1 }
]
```

### `place_liquids`로 유출 액체 배치

지정 위치에 액체 아이템을 생성합니다. 액체는 현재 직접 줍기 어렵지만(탱크/주유기 가솔린 제외),
mapgen에 분위기를 더하는 용도로 쓸 수 있습니다.

| Field  | Description                                                                 |
| ------ | --------------------------------------------------------------------------- |
| liquid | (필수, item id) 아이템(액체)                                                |
| amount | (선택, integer/min-max 배열) 배치할 액체 양(0이면 아이템 기본 charges 사용) |
| chance | (선택, integer/min-max 배열) 액체 스폰 1/x 확률, 기본 1(100%)               |

기본량 가솔린(200 단위)을 바닥에 떨어뜨리는 예시(`rows` 문자 또는 명시 좌표):

```json
"liquids": {
    "g": { "liquid": "gasoline" }
},
"place_liquids": [
    { "liquid": "gasoline", "x": 3, "y": 5 }
],
```

### `loot`로 특정 아이템/그룹 아이템 배치

아이템 그룹 또는 개별 아이템을 배치합니다. `place_item`, `item`/`items`와의 중요한 차이는,
`loot`는 distribution 그룹에서 루프 없이 단일 아이템을 스폰할 수 있다는 점입니다.
총기에는 맞는 탄창/탄약도 함께 스폰할 수 있습니다.

**참고**: `group` 또는 `item` 중 하나만 지정해야 합니다(동시 사용 불가).

| Field    | Description                                                                     |
| -------- | ------------------------------------------------------------------------------- |
| group    | (string) 사용할 아이템 그룹 (`ITEM_SPAWN.md`의 collection vs distribution 참고) |
| item     | (string) 스폰할 아이템 type id                                                  |
| chance   | (선택, integer) 아이템 스폰 100분의 x 확률. 기본 100                            |
| ammo     | (선택, integer) 기본 탄약과 함께 스폰될 100분의 x 확률. 기본 0                  |
| magazine | (선택, integer) 기본 탄창과 함께 스폰될 100분의 x 확률. 기본 0                  |

### `sealed_item`으로 화분에 씨앗 심기

아이템 전용 처리 가구 내부에 아이템 또는 아이템 그룹을 배치합니다.

이 기능은 `PLANT` 플래그가 있는 `f_plant_harvest` 같은 가구를 위한 것입니다. 다른 방식으로는
`NOITEM` 플래그 때문에 해당 가구에 아이템을 배치할 수 없습니다.

이런 가구에는 식물 종을 결정하는 단일(숨겨진) 씨앗 아이템이 있어야 합니다. `sealed_item`을 사용하면
가구와 씨앗 아이템을 지정해 이런 식물을 만들 수 있습니다.

**참고**: "item" 또는 "items" 중 정확히 하나만 지정해야 합니다.

| Field     | Description                        |
| --------- | ---------------------------------- |
| furniture | (string) 선택한 가구 id            |
| item      | "item" special로 아이템 스폰       |
| items     | "items" special로 아이템 그룹 스폰 |

예시:

```json
"sealed_item": {
  "p": {
    "items": { "item": "farming_seeds", "chance": 100 },
    "furniture": "f_plant_harvest"
  }
},
```

### `graffiti`로 메시지 배치

해당 위치에 낙서 메시지를 배치합니다. `text` 또는 `snippet` 중 하나는 필수입니다. 메시지에는
`<full_name>`, `<given_name>`, `<family_name>` 태그를 넣어 무작위 이름을 삽입할 수 있고,
`<city>`로 가장 가까운 도시 이름을 삽입할 수 있습니다.

| Field   | Description                                    |
| ------- | ---------------------------------------------- |
| text    | (선택, string) 배치할 메시지                   |
| snippet | (선택, string) 메시지를 가져올 스니펫 카테고리 |

### `zones`로 NPC 팩션용 구역 배치

해당 팩션의 NPC는 이 구역을 사용해 AI 행동에 영향을 받습니다.

| Field   | Description                                                                          |
| ------- | ------------------------------------------------------------------------------------ |
| type    | (필수, string) 값: `"NPC_RETREAT"`, `"NPC_NO_INVESTIGATE"`, `"NPC_INVESTIGATE_ONLY"` |
| faction | (필수, string) 구역을 사용할 NPC 팩션 id                                             |
| name    | (선택, string) 구역 이름                                                             |

`type` 값은 NPC 행동에 영향을 줍니다. NPC는:

- `NPC_RETREAT` 구역으로 후퇴하려는 경향이 있습니다.
- `NPC_NO_INVESTIGATE` 구역에서 발생한 미확인 소리의 원인을 확인하러 이동하지 않습니다.
- `NPC_INVESTIGATE_ONLY` 구역 밖에서 발생한 미확인 소리의 원인을 확인하러 이동하지 않습니다.

### `translate_ter`로 지형 타입 변환

한 지형 타입을 다른 지형 타입으로 변환합니다. 일반 mapgen에서 쓸 이유는 거의 없지만,
`update_mapgen`으로 기준 상태를 잡을 때 유용합니다.

| Field | Description                                    |
| ----- | ---------------------------------------------- |
| from  | (필수, string) 변환 대상 지형 id               |
| to    | (필수, string) from 지형이 변환될 목적 지형 id |

### `ter_furn_transforms`로 mapgen 변환 적용

지정 위치에서 `ter_furn_transform`을 실행합니다. 일반 mapgen은 지형을 직접 지정하면 되므로,
주로 `update_mapgen` 일부로 변환을 적용할 때 유용합니다.

- `"transform"`: (필수, string) 실행할 `ter_furn_transform` id

### `place_nested`로 오버맵 이웃 기반 중첩 청크 스폰

Place_nested는 이웃 오버맵의 `"id"`와 mutable overmap special 배치에 사용된 join을 기준으로,
제한적인 조건부 청크 스폰을 허용합니다. 바이옴 사이 전환을 부드럽게 하거나 mutable 구조 가장자리에
벽을 동적으로 만드는 데 유용합니다.

| Field              | Description                                                                                                |
| ------------------ | ---------------------------------------------------------------------------------------------------------- |
| chunks/else_chunks | (필수, string) 조건부 배치할 청크의 nested_mapgen_id. 지정 이웃과 일치하면 chunks, 아니면 else_chunks 배치 |
| x and y            | (필수, int) 청크를 배치할 방위 위치                                                                        |
| neighbors          | (선택) 청크 배치 전 검사할 이웃 오버맵. 각 방향은 오버맵 `"id"` 부분문자열 목록과 연결됨                   |
| joins              | (선택) 청크 배치 전 검사할 mutable overmap special join. 각 방향은 join `"id"` 문자열 목록과 연결됨        |
| connections        | (선택) 청크 배치 전 해당 오버맵으로 향해야 하는 연결. 각 방향은 connection `"id"` 문자열 목록과 연결됨     |
|                    |                                                                                                            |

이 방식으로 검사 가능한 인접 오버맵은 다음과 같습니다:

- 직교 이웃( `"north"`, `"east"`, `"south"`, `"west"` )
- 대각 이웃( `"north_east"`, `"north_west"`, `"south_east"`, `"south_west"` )
- 수직 이웃( `"above"`, `"below"` )

Join은 직교 방향과 `"above"`, `"below"`만 검사할 수 있습니다.

예시:

```json
"place_nested": [
  { "chunks": [ "concrete_wall_ew" ], "x": 0, "y": 0, "neighbors": { "north": [ "empty_rock", "field" ] } },
  { "chunks": [ "gate_north" ], "x": 0, "y": 0, "joins": { "north": [ "interior_to_exterior" ] } },
  { "else_chunks": [ "concrete_wall_ns" ], "x": 0, "y": 0, "neighbors": { "north_west": [ "field", "microlab" ] } }
],
```

위 코드는 다음처럼 청크를 배치합니다:

- 북쪽 이웃이 field 또는 solid rock이면 `"concrete_wall_ew"`
- mutable overmap 배치 시 북쪽에 `"interior_to_exterior"` join이 사용되었으면 `"gate_north"`
- 북서 이웃이 field도 아니고 microlab 계열 오버맵도 아니면 `"concrete_wall_ns"`

## Mapgen 값

_mapgen value_는 특정 id가 필요한 다양한 위치에서 사용할 수 있습니다. 예를 들어 매개변수 기본값이나
`"terrain"` 객체의 지형 id입니다. mapgen value는 다음 세 형태 중 하나입니다:

- 단순 문자열: 리터럴 id. 예: `"t_flat_roof"`
- `"distribution"` 키를 가진 JSON 객체: 값은 문자열 id와 정수 가중치 쌍의 목록 목록. 예:

```
{ "distribution": [ [ "t_flat_roof", 2 ], [ "t_tar_flat_roof", 1 ], [ "t_shingle_flat_roof", 1 ] ] }
```

- `"param"` 키를 가진 JSON 객체: 값은 [Mapgen parameters](#mapgen-parameters)에서 설명한 매개변수
  이름 문자열. 예: `{ "param": "roof_type" }`

  경우에 따라 fallback 값도 필요합니다. 예:
  `{ "param": "roof_type", "fallback": "t_flat_roof" }`.
  fallback은 진행 중인 게임을 깨지 않고 mapgen 정의를 변경하기 위해 필요합니다. 같은 overmap special의
  서로 다른 부분이 다른 시점에 생성될 수 있으며, 생성 도중 정의에 새 매개변수가 추가되면 해당 값이
  비어 있게 되어 fallback이 사용됩니다.
- switch 문: 다른 mapgen value의 값에 따라 값을 선택합니다. 주로 mapgen 매개변수 값을 기준으로
  스위칭해 mapgen의 두 부분을 일관되게 만들 때 씁니다. 예를 들어 아래 switch는 mapgen 매개변수
  `fence_type`으로 고른 울타리 타입에 맞는 게이트 타입을 고릅니다:

```json
{
  "switch": { "param": "fence_type", "fallback": "t_splitrail_fence" },
  "cases": {
    "t_splitrail_fence": "t_splitrail_fencegate_c",
    "t_chainfence": "t_chaingate_c",
    "t_fence_barbed": "t_gate_metal_c",
    "t_privacy_fence": "t_privacy_fencegate_c"
  }
}
```

## Mapgen 매개변수

(이 기능은 개발 중이므로 동작이 문서와 정확히 일치하지 않을 수 있습니다.)

mapgen 정의 또는 팔레트에 또 다른 항목으로 `"parameters"` 키를 넣을 수 있습니다. 예:

```
"parameters": {
  "roof_type": {
    "type": "ter_str_id",
    "default": { "distribution": [ [ "t_flat_roof", 2 ], [ "t_tar_flat_roof", 1 ], [ "t_shingle_flat_roof", 1 ] ] }
  }
},
```

`"parameters"` JSON 객체의 각 항목은 하나의 매개변수를 정의합니다. 키는 매개변수 이름이며,
각 키 값은 JSON 객체여야 합니다. 이 객체는 타입(`cata_variant`의 type 문자열과 동일)을 제공해야 하며,
선택적으로 기본값을 제공할 수 있습니다. 기본값은 위에서 정의한 [mapgen value](#mapgen-values)여야 합니다.

작성 시점 기준으로 매개변수 값은 `"default"`를 통해서만 들어오므로, 보통은 항상 default를 두는 것이 좋습니다.

매개변수의 주요 용도는 `"distribution"` mapgen value로 무작위 값을 고른 뒤, 해당 매개변수를 사용하는
모든 위치에 동일한 값을 적용하는 것입니다. 위 예시에서는 무작위 지붕 지형이 선택됩니다. `"terrain"` 키에서
`"param"` mapgen value로 이 매개변수를 참조하면, 맵 전체에서 무작위이면서 일관된 지붕 지형을 사용할 수
있습니다. 반대로 `"terrain"` 객체에 `"distribution"`을 직접 넣으면 타일마다 별도로 랜덤을 뽑아
지붕이 뒤섞일 수 있습니다.

기본적으로 매개변수 범위(scope)는 생성 중인 `overmap_special`입니다. 즉 special 전체에서 같은 값을
공유합니다. default가 필요하면 special의 첫 청크 생성 시 선택되고, 이후 청크에서 재사용되도록 저장됩니다.

원한다면 `"scope": "omt"`로 범위를 단일 오버맵 타일로 제한할 수 있습니다. 그러면 OMT마다 독립적으로
default가 선택됩니다. 이 경우 해당 매개변수를 mapgen에서 사용할 때 `"fallback"`을 강제하지 않아도 됩니다.

세 번째 범위 옵션은 `"scope": "nest"`입니다. 이는 중첩 mapgen에서만 실질적 의미가 있습니다
(다른 곳에서 써도 오류는 아님). 범위가 `nest`이면 매개변수 값은 특정 중첩 청크 단위로 선택됩니다.
예를 들어 nest가 여러 타일에 카펫을 정의한다면, 매개변수로 해당 nest 내부 타일의 카펫 색을 일치시킬 수
있고, 같은 OMT의 다른 위치에 있는 동일 `nested_mapgen_id` 인스턴스는 다른 색을 선택할 수 있습니다.

mapgen 매개변수와 그 영향 디버깅을 위해, overmap editor(디버그 메뉴 접근)에서
`overmap_special` 범위 매개변수의 선택값을 확인할 수 있습니다.

## `rotation`으로 맵 회전

다른 mapgen 처리가 끝난 후 생성된 맵을 회전합니다. 값은 단일 정수 또는 범위(범위 내 무작위 선택)입니다.
예: `"rotation": [ 0, 3 ]`

값은 90° 단위입니다.

## `predecessor_mapgen`으로 베이스 mapgen 선적용

여기에 오버맵 지형 id를 지정하면, 현재 정의의 나머지를 적용하기 전에 해당 오버맵 지형 타입의 mapgen
전체를 먼저 실행합니다. 대표적인 용도는 숲/습지/호숫가 같은 자연 지형 위에 위치를 생성할 때입니다.
기존 JSON mapgen 중 상당수는 자신이 놓이는 타입의 mapgen을 흉내 내려고(예: 숲 속 오두막에 나무/풀/잡동사니
직접 배치) 작성되어, 해당 타입 생성 로직이 바뀌면 동기화가 어긋납니다. `predecessor_mapgen`을 지정하면
기존 지형에 추가되는 요소에만 집중할 수 있습니다.

예시: `"predecessor_mapgen": "forest"`

# 팔레트

**palette**는 서로 다른 mapgen 조각에서 동일한 심볼 정의를 재사용하는 방법입니다. 예를 들어 CDDA의
대부분 집은 `standard_domestic_palette`를 사용합니다. 이 팔레트는 `h`를 `f_chair`로 정의하므로,
각 집 mapgen은 매번 정의를 반복하지 않고 `"rows"`에서 `h`를 사용할 수 있습니다. 팔레트 참조는
다음을 추가해 수행합니다:

```json
"palettes": [ "standard_domestic_palette" ]
```

각 집 정의에 이를 넣으면 됩니다.

각 mapgen 조각은 여러 팔레트를 참조할 수 있습니다. 두 팔레트가 같은 심볼 의미를 모두 정의하면 둘 다
적용됩니다. 일부 경우(아이템 스폰 등) 최종 결과에 둘 다 반영되고, 다른 경우(지형/가구 설정 등)에는
하나가 다른 것을 덮어써야 합니다. 규칙은 목록에서 **나중에 적은 팔레트가 앞선 것을 덮어쓴다**는 것이며,
바깥 mapgen 정의는 내부 팔레트 정의보다 우선합니다.

팔레트 정의는 위 [JSON object definition](#json-object-definition)에서 설명한 항목 중
심볼 의미를 정의하는 부분은 포함할 수 있습니다. 하지만 특정 좌표(`"x"`, `"y"`)를 지정하는 항목은
포함할 수 없습니다.

팔레트는 `"palettes"` 키를 통해 다른 팔레트를 포함할 수도 있습니다. 여러 팔레트가 동일 심볼과 의미를
많이 공유한다면, 공통 부분을 새 팔레트로 분리해 각 팔레트가 포함하도록 하여 중복 정의를 줄일 수 있습니다.

## mapgen 값으로서의 팔레트 id

`"palettes"` 목록의 값은 단순 문자열일 필요가 없습니다. 위에서 설명한 어떤
[mapgen value](#mapgen-values)도 사용할 수 있습니다. 특히 `"distribution"`으로 팔레트 집합에서
무작위 선택할 수 있습니다.

이 선택은 overmap special 범위 [mapgen parameter](#mapgen-parameters)처럼 동작합니다. 따라서 special
내 모든 OMT는 같은 팔레트를 사용합니다. 또한 overmap editor(디버그 메뉴 접근)에서 표시되는
overmap special 인자를 보면 어떤 팔레트가 선택되었는지 확인할 수 있습니다.

예를 들어 오두막 mapgen 정의에서 아래 JSON을 사용하면

```json
"palettes": [ { "distribution": [ [ "cabin_palette", 1 ], [ "cabin_palette_abandoned", 1 ] ] } ],
```

생성된 오두막의 절반은 일반 `cabin_palette`, 나머지 절반은 `cabin_palette_abandoned`를 사용합니다.

# `update_mapgen` 사용

**update_mapgen**은 일반 JSON mapgen의 변형입니다. 새 오버맵 타일을 만드는 대신, 기존 오버맵 타일에
지정한 변경만 적용합니다. 현재는 NPC 미션 인터페이스에서만 동작하지만, 향후 기존 맵을 수정하는
범용 도구로 확장될 예정입니다.

update_mapgen은 일부 예외를 제외하고 JSON mapgen과 같은 필드를 사용합니다. 미션 지원을 위한 새 필드와,
어떤 오버맵 타일을 업데이트할지 지정하는 방법이 추가됩니다.

## 오버맵 타일 지정

update_mapgen은 기존 오버맵 타일을 업데이트합니다. 아래 필드는 업데이트 대상 타일을 지정하는 방법입니다.

### `assign_mission_target`

assign_mission_target은 오버맵 타일을 미션 타겟으로 지정합니다. 같은 스코프의 update_mapgen은 그 타일을
업데이트합니다. 요구되는 terrain ID를 가진 가장 가까운 오버맵 지형이 사용되며, 일치 지형이 없으면
om_special 타입의 overmap special을 생성한 뒤 그 special의 om_terrain을 사용합니다.

| Field      | Description                                  |
| ---------- | -------------------------------------------- |
| om_terrain | (필수, string) 미션 타겟의 오버맵 지형 ID    |
| om_special | (필수, string) 미션 타겟의 오버맵 special ID |

### `om_terrain`

가장 가까운 om_special 타입 overmap special 내에서, 타입이 om_terrain인 가장 가까운 오버맵 타일을
사용합니다. 이 타일은 업데이트되지만 미션 타겟으로는 지정되지 않습니다.

| Field      | Description                                  |
| ---------- | -------------------------------------------- |
| om_terrain | (필수, string) 미션 타겟의 오버맵 지형 ID    |
| om_special | (필수, string) 미션 타겟의 오버맵 special ID |

# 미션 special

update_mapgen은 일부 mapgen JSON 항목에 새로운 선택 키워드를 추가합니다.

### `target`

place_npc, place_monster, place_computer는 선택적 boolean `target`을 받을 수 있습니다.
`"target": true`이고 유효한 미션과 함께 update_mapgen으로 호출되면, 해당 NPC/몬스터/컴퓨터는
미션 타겟으로 표시됩니다.

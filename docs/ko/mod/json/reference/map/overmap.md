# 오버맵 생성

## 개요

**오버맵**은 게임에서 _지도 보기(m)_ 명령을 사용할 때 표시되는 화면입니다.

다음과 같이 표시됩니다:

```
...>│<......................│..............P.FFFF├─FF...
..┌─┼─┐.....................│..............FFFFF┌┘FFFF..
..│>│^│.....................│..............FF┌─┬┘FFFFF.F
──┘...└──┐..............>│<.│..............F┌┘F│FFFFFFF─
.........└──┐........vvO>│v.│............FF┌┘FF│FFFFFFFF
............└┐.......─┬──┼─>│<........┌─T──┘FFF└┐FFFFFFF
.............│.......>│<$│S.│<.......┌┘..FFFFFFF│FFF┌─┐F
..O.........┌┴──┐....v│gF│O.│<v......│...FFFFFF┌┘F.F│F└─
.OOOOO.....─┘...└─────┼──┼──┼────────┤....FFFFF│FF..│FFF
.O.O.....dddd.......^p│^>│OO│<^......└─┐FFFFFFF├────┴─FF
.........dddd........>│tt│<>│..........│FFF..FF│FFFFF.FF
.........dddd......┌──┘tT├──...........│FFF..F┌┘FFFFFFFF
.........dddd......│....>│^^..H........└┐...FF│FFFFFFFFF
..................┌┘.....│<.............└─┐FF┌┘FFFFFFFFF
..................│......│................│FF│FFFFFFFFFF
.................┌┘......│................│F┌┘FFFFFFFFFF
...FFF...........│......┌┘...............┌┘.│FFFFFFFFFFF
FFFFFF...........│.....┌┘................│FF│FFFFFFFFFFF
FFFFFF..FFFF.....│.....B..........─┐.....│┌─┘F..FFFFFFFF
```

**오버맵 생성** 맥락에서 **오버맵**은 플레이어가 직접 상호작용하는 로컬 맵보다 더 추상적인 수준에서 게임 월드의 위치를 정의하는 데이터와 특징의 집합으로, 로컬 맵 생성에 필요한 컨텍스트를 제공합니다.

예를 들어, 오버맵은 게임에 다음을 알려줍니다:

- 도시가 어디에 있는지
- 도로가 어디에 있는지
- 건물이 어디에 있는지
- 건물의 유형이 무엇인지

하지만 다음은 알려주지 않습니다:

- 실제 도로 지형이 어떻게 생겼는지
- 실제 건물 배치가 어떻게 생겼는지
- 건물 안에 어떤 아이템이 있는지

오버맵을 플레이어가 탐험하면서 채워질 게임 월드의 윤곽으로 생각하면 유용할 수 있습니다. 이 문서의 나머지 부분은 그 윤곽을 어떻게 만들 수 있는지에 대한 논의입니다.

## 용어와 타입

먼저 오버맵에서 사용하는 일부 데이터 타입에 대해 간단히 논의해야 합니다.

### overmap_terrain

오버맵의 기본 단위는 **overmap_terrain**으로, 오버맵의 단일 위치를 나타내는 데 사용되는 id, name, symbol, color(그리고 더 많지만 나중에 다룹니다)를 정의합니다. 오버맵은 가로 180개, 세로 180개, 깊이 21개(z-레벨)의 오버맵 지형으로 구성됩니다.

이 오버맵 스니펫 예제에서 각 문자는 주어진 오버맵 지형을 참조하는 오버맵의 한 항목에 해당합니다:

```
.v>│......FFF│FF
──>│<....FFFF│FF
<.>│<...FFFFF│FF
O.v│vv.vvv┌──┘FF
───┼──────┘.FFFF
F^^|^^^.^..F.FFF
```

예를 들어, `F`는 다음과 같은 정의를 가진 숲입니다:

```json
{
  "type": "overmap_terrain",
  "id": "forest",
  "name": "forest",
  "sym": "F",
  "color": "green"
}
```

그리고 `^`는 다음과 같은 정의를 가진 집입니다:

```json
{
  "type": "overmap_terrain",
  "id": "house",
  "name": "house",
  "sym": "^",
  "color": "light_green"
}
```

단일 오버맵 지형 정의가 오버맵에서 해당 오버맵 지형의 모든 사용에 대해 참조된다는 점이 중요합니다. 숲의 색상을 `green`에서 `red`로 변경하면 오버맵의 모든 숲이 빨간색이 됩니다.

### overmap_special / city_building

오버맵의 다음 중요한 개념은 **overmap_special**과 **city_building**입니다. 구조가 유사한 이 타입들은 여러 오버맵 지형을 하나의 개념적 개체로 수집하여 오버맵에 배치하는 메커니즘입니다. 넓은 저택, 2층 집, 큰 연못 또는 평범한 서점 아래 숨겨진 비밀 기지를 배치하려면 **overmap_special** 또는 **city_building**을 사용해야 합니다.

이 타입들은 본질적으로 구성하는 오버맵 지형 목록, 서로에 대한 상대적 배치, 그리고 오버맵 스페셜/도시 건물의 배치를 유도하는 데 사용되는 일부 데이터(예: 도시로부터의 거리, 도로에 연결되어야 하는지, 숲/들판/강에 배치할 수 있는지 등)입니다.

### overmap_connection

도로에 대해 말하자면, 도로, 하수도, 지하철, 철도, 숲길과 같은 선형 특징의 개념은 오버맵 지형 속성과 **overmap_connection**이라는 다른 타입의 조합으로 관리됩니다.

오버맵 연결은 본질적으로 주어진 연결을 만들 수 있는 오버맵 지형의 타입, 해당 연결을 만드는 "비용", 연결을 만들 때 배치할 지형의 타입을 정의합니다. 예를 들어, 이것은 다음을 말할 수 있게 해줍니다:

- 도로는 들판, 숲, 늪, 강에 배치될 수 있다
- 들판이 숲보다 선호되고, 숲이 늪보다 선호되며, 늪이 강보다 선호된다
- 강을 가로지르는 도로는 다리가 된다

**overmap_connection** 데이터는 이전에 정의된 규칙을 적용하여 두 지점을 연결하는 선형 도로 특징을 만드는 데 사용됩니다.

### overmap_location

오버맵 스페셜, 도시 건물, 오버맵 연결의 배치를 위해 유효한 오버맵 지형 타입을 정의하는 것에 대해 이전에 언급했지만, 명확히 할 점은 실제로 **overmap_terrain** 값을 직접 참조하는 대신 **overmap_location**이라는 다른 타입을 활용한다는 것입니다.

간단히 말해, **overmap_location**은 **overmap_terrain** 값의 이름이 지정된 집합일 뿐입니다.

예를 들어, 다음은 두 개의 간단한 정의입니다.

```json
{
    "type": "overmap_location",
    "id": "forest",
    "terrains": ["forest"]
},
{
    "type": "overmap_location",
    "id": "wilderness",
    "terrains": ["forest", "field"]
}
```

이것들의 가치는 주어진 오버맵 지형이 여러 다른 위치에 속할 수 있도록 하고, 새로운 오버맵 지형이 추가되거나 기존 지형이 제거될 때 해당 그룹이 필요한 모든 **overmap_connection**, **overmap_special**, **city_building** 대신 관련 **overmap_location** 항목만 변경하면 되도록 간접 수준을 제공한다는 것입니다.

예를 들어, 새로운 숲 오버맵 지형 `forest_thick`를 추가하면 다음과 같이 이 정의만 업데이트하면 됩니다:

```json
{
    "type": "overmap_location",
    "id": "forest",
    "terrains": ["forest", "forest_thick"]
},
{
    "type": "overmap_location",
    "id": "wilderness",
    "terrains": ["forest", "forest_thick", "field"]
}
```

## 오버맵 지형

### 회전

오버맵 지형이 회전할 수 있는 경우(즉, `NO_ROTATE` 플래그가 없는 경우), 게임이 JSON에서 정의를 로드할 때 회전된 정의를 자동으로 생성하고 여기에 정의된 `id`에 `_north`, `_east`, `_south` 또는 `_west`를 접미사로 붙입니다. 오버맵 지형이 **overmap_special** 또는 **city_building** 정의에 사용되는 경우 특히 관련이 있는데, 이것들이 회전을 허용하는 경우 참조된 오버맵 지형에 대해 특정 회전(예: 모두 `_north` 버전)을 지정하는 것이 바람직하기 때문입니다.

### 필드

| 식별자            | 설명                                                                                                                                                    |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `type`            | `overmap_terrain`이어야 합니다.                                                                                                                         |
| `id`              | 고유 ID.                                                                                                                                                |
| `name`            | 게임에 표시되는 위치 이름.                                                                                                                              |
| `sym`             | 위치를 그릴 때 사용하는 기호, `"F"` 같은 것 (또는 `70`과 같은 ASCII 값 사용 가능).                                                                      |
| `color`           | 기호를 그릴 색상. [COLOR.md](../graphics/COLOR) 참조.                                                                                                   |
| `looks_like`      | 이것에 대한 그래픽 타일이 없는 경우 사용할 다른 오버맵 지형의 ID.                                                                                       |
| `connect_group`   | 이 오버맵 지형이 인접 지형과 그래픽적으로 연결될 수 있도록 지정(타일셋이 원하는 경우). 같은 `connect_group`을 가진 다른 `overmap_terrain`과 연결됩니다. |
| `see_cost`        | 오버맵에서 플레이어 시야에 영향을 줍니다. 값이 높을수록 시야를 더 가립니다.                                                                             |
| `travel_cost`     | 경로 찾기 비용에 영향을 줍니다. 값이 높을수록 통과하기 어렵습니다 (참고: 숲 = 10).                                                                      |
| `extras`          | region_settings의 이름이 지정된 `map_extras`에 대한 참조, 적용할 수 있는 맵 추가 요소를 정의합니다.                                                     |
| `mondensity`      | 인접한 오버맵 지형의 값과 합산되어 여기서 생성되는 몬스터 밀도에 영향을 줍니다.                                                                         |
| `spawns`          | 맵젠 시 한 번 추가되는 스폰. 몬스터 그룹, % 확률, 개체수 범위 (최소/최대).                                                                              |
| `flags`           | [json_flags.md](../json_flags)의 `Overmap terrains` 참조.                                                                                               |
| `mapgen`          | C++ 맵젠 함수 지정. 하지 마세요--JSON을 사용하세요.                                                                                                     |
| `mapgen_straight` | LINEAR 특징 변형을 위한 C++ 맵젠 함수 지정. JSON 선호.                                                                                                  |
| `mapgen_curved`   | LINEAR 특징 변형을 위한 C++ 맵젠 함수 지정. JSON 선호.                                                                                                  |
| `mapgen_end`      | LINEAR 특징 변형을 위한 C++ 맵젠 함수 지정. JSON 선호.                                                                                                  |
| `mapgen_tee`      | LINEAR 특징 변형을 위한 C++ 맵젠 함수 지정. JSON 선호.                                                                                                  |
| `mapgen_four_way` | LINEAR 특징 변형을 위한 C++ 맵젠 함수 지정. JSON 선호.                                                                                                  |

### 예제

실제 `overmap_terrain`은 이 모든 것을 동시에 정의하지 않지만, 포괄적인 예제를 위해...

```json
{
  "type": "overmap_terrain",
  "id": "field",
  "name": "field",
  "sym": ".",
  "color": "brown",
  "looks_like": "forest",
  "see_cost": 2,
  "extras": "field",
  "mondensity": 2,
  "spawns": { "group": "GROUP_FOREST", "population": [0, 1], "chance": 13 },
  "flags": ["NO_ROTATE"],
  "mapgen": [{ "method": "builtin", "name": "bridge" }],
  "mapgen_straight": [{ "method": "builtin", "name": "road_straight" }],
  "mapgen_curved": [{ "method": "builtin", "name": "road_curved" }],
  "mapgen_end": [{ "method": "builtin", "name": "road_end" }],
  "mapgen_tee": [{ "method": "builtin", "name": "road_tee" }],
  "mapgen_four_way": [{ "method": "builtin", "name": "road_four_way" }]
}
```

## 오버맵 스페셜

오버맵 스페셜은 도시 생성 프로세스가 완료된 후 오버맵에 배치되는 개체로, **city_building** 타입의 "비도시" 대응물입니다. 일반적으로 여러 오버맵 지형으로 구성되며(항상 그런 것은 아니지만), 오버맵 연결(예: 도로, 하수도, 지하철)을 가질 수 있으며, JSON으로 정의된 규칙이 배치를 안내합니다.

## 배치 규칙

각 스페셜에 대해 맵젠은 먼저 발생 횟수의 `min`과 `max` 사이의 난수를 굴려 배치하려는 인스턴스 수를 결정합니다. 이 수는 몇 가지 승수로 조정됩니다: 설정된 스페셜 밀도, 지형 비율, 혼잡도 비율.

"지형 비율"은 오버맵의 육지와 호수 타일 간의 비율을 나타내는 숫자입니다. 30% 침수된 오버맵은 육지 스페셜에 대해 0.7 승수를, 호수 스페셜에 대해 0.3 승수를 갖습니다.

"혼잡도 비율"은 전체 오버맵 영역과 현재 범위 및 밀도 설정으로 모든 스페셜을 배치하는 데 필요한 예상 평균 영역 간의 비율을 나타내는 숫자입니다. 제한되고 일반적으로 x1로 유지됩니다. 그러나 물리적으로 가능한 것보다 더 많은 스페셜을 생성하려고 하면 감소합니다.

이 모든 요소를 고려한 후 2.4와 같은 숫자가 나올 수 있으며, 이는 오버맵이 2개의 인스턴스를 배치할 60% 확률과 해당 스페셜의 3개 인스턴스를 배치할 40% 확률을 갖는다는 것을 의미합니다. 최종 수는 원시 `min` 발생 값보다 낮지 않습니다.

정확한 수량이 선택되면 맵젠은 스페셜을 생성할 장소를 검색합니다. 스페셜이 도시에 의존하는 경우 해당 인스턴스는 다른 일치하는 도시 근처에 분산됩니다. 예를 들어, 맵젠이 3개의 인스턴스를 배치하려고 하고 오버맵에 필요한 크기의 도시가 2개 있는 경우, 같은 도시 근처에 2개 이상을 배치하지 않습니다.

### 고정 vs 가변 스페셜

오버맵 스페셜에는 두 가지 하위 유형이 있습니다: 고정 및 가변. 고정 오버맵 스페셜은 많은 OMT에 걸쳐 있을 수 있는 고정된 정의된 레이아웃을 가지며 회전할 수 있지만(아래 참조) 항상 본질적으로 동일하게 보입니다.

가변 오버맵 스페셜은 더 유연한 레이아웃을 가집니다. 조각이 여러 방식으로 맞출 수 있는 퍼즐처럼 오버맵 지형 모음과 함께 맞추는 방법으로 정의됩니다. 이것은 거대한 개미가 지하에 판 터널이나 맞지 않는 날개와 확장이 있는 더 큰 확장 건물과 같은 더 유기적인 모양을 형성하는 데 사용할 수 있습니다.

가변 스페셜은 오류 없이 안정적으로 배치할 수 있도록 설계하는 데 훨씬 더 많은 주의가 필요합니다.

### 회전

일반적으로 오버맵 스페셜을 회전 허용으로 정의하는 것이 바람직합니다. 이는 게임 월드에 다양성을 제공하고 오버맵 스페셜을 배치하려고 할 때 더 많은 가능한 유효 위치를 허용합니다. 오버맵 스페셜 회전과 스페셜을 구성하는 기본 오버맵 지형 회전 간의 관계의 결과로, 오버맵 스페셜은 연관된 오버맵 지형의 특정 회전된 버전을 참조해야 합니다. 일반적으로 이것은 맵젠을 위한 JSON이 정의된 방식에 해당하는 `_north` 회전입니다.

### 위치

오버맵 스페셜은 배치될 수 있는 유효한 위치(`overmap_location`)를 지정하는 두 가지 메커니즘을 가지고 있습니다. `overmaps`의 각 개별 항목에는 유효한 위치가 지정될 수 있으며, 이는 스페셜의 다른 부분이 다른 유형의 지형에 허용되는 경우 유용합니다(예: 한 부분은 해안에, 한 부분은 물에 있어야 하는 부두). 모든 값이 동일한 경우 최상위 `locations` 키를 사용하여 대신 전체 스페셜에 대해 위치를 지정할 수 있습니다. 개별 항목의 값이 최상위 값보다 우선하므로 최상위 값을 정의한 다음 다른 개별 항목에 대해서만 지정할 수 있습니다.

### 필드

| 식별자          | 설명                                                                                     |
| --------------- | ---------------------------------------------------------------------------------------- |
| `type`          | `"overmap_special"`이어야 합니다.                                                        |
| `id`            | 고유 ID.                                                                                 |
| `connections`   | 오버맵 연결과 스페셜 내의 상대적 `[ x, y, z ]` 위치 목록.                                |
| `place_nested`  | 이 스페셜에 상대적인 `{ "point": [x, y, z], "special": id }`를 가진 중첩 스페셜 배열.    |
| `subtype`       | `"fixed"` 또는 `"mutable"`. 지정되지 않으면 기본값은 `"fixed"`.                          |
| `locations`     | 스페셜이 배치될 수 있는 `overmap_location` ID 목록.                                      |
| `city_distance` | 스페셜이 배치될 수 있는 도시로부터의 최소/최대 거리. 무제한의 경우 -1 사용.              |
| `city_sizes`    | 스페셜이 근처에 배치될 수 있는 도시의 최소/최대 크기. 무제한의 경우 -1 사용.             |
| `occurrences`   | 스페셜을 배치할 때의 최소/최대 발생 수. UNIQUE 플래그가 설정되면 Y 중 X의 확률이 됩니다. |
| `flags`         | [json_flags.md](../json_flags)의 `Overmap specials` 참조.                                |
| `rotate`        | 스페셜이 회전할 수 있는지 여부. 지정되지 않으면 참.                                      |

하위 유형에 따라 추가 관련 필드가 있습니다:

#### 고정 오버맵 스페셜을 위한 추가 필드

| 식별자     | 설명                                                      |
| ---------- | --------------------------------------------------------- |
| `overmaps` | 오버맵 지형과 스페셜 내의 상대적 `[ x, y, z ]` 위치 목록. |

#### 가변 오버맵 스페셜을 위한 추가 필드

| 식별자                     | 설명                                                                                          |
| -------------------------- | --------------------------------------------------------------------------------------------- |
| `check_for_locations`      | 초기 배치를 위해 존재해야 하는 위치를 정의하는 `[ [ x, y, z ], [ locations, ... ] ]` 쌍 목록. |
| `check_for_locations_area` | 명시적 `check_for_locations` 쌍 외에 고려할 check_for_locations 영역 객체 목록.               |
| `overmaps`                 | 다양한 오버맵과 서로 결합하는 방법의 정의.                                                    |
| `root`                     | 가변 스페셜이 성장할 초기 오버맵.                                                             |
| `shared`                   | 스페셜의 일부를 확장하는 데 사용할 수 있는 `"id": value`로 정의된 승수 목록.                  |
| `phases`                   | 루트 OMT에서 오버맵 스페셜을 성장시키는 방법의 사양.                                          |

### 고정 스페셜 예제

```json
[
  {
    "type": "overmap_special",
    "id": "campground",
    "overmaps": [
      { "point": [0, 0, 0], "overmap": "campground_1a_north", "locations": ["forest_edge"] },
      { "point": [1, 0, 0], "overmap": "campground_1b_north" },
      { "point": [0, 1, 0], "overmap": "campground_2a_north" },
      { "point": [1, 1, 0], "overmap": "campground_2b_north" }
    ],
    "connections": [{ "point": [1, -1, 0], "connection": "local_road", "from": [1, 0, 0] }],
    "locations": ["forest"],
    "city_distance": [10, -1],
    "city_sizes": [3, 12],
    "occurrences": [0, 5],
    "flags": ["CLASSIC"],
    "rotate": true
  }
]
```

### 고정 스페셜 오버맵

| 식별자      | 설명                                                        |
| ----------- | ----------------------------------------------------------- |
| `point`     | 스페셜 내 오버맵 지형의 `[ x, y, z]`.                       |
| `overmap`   | 해당 위치에 배치할 `overmap_terrain`의 ID.                  |
| `locations` | 이 오버맵 지형이 배치될 수 있는 `overmap_location` ID 목록. |

### 연결

| 식별자       | 설명                                                                         |
| ------------ | ---------------------------------------------------------------------------- |
| `point`      | 연결 끝점의 `[ x, y, z]`. 스페셜에 대한 오버맵 지형 항목과 겹칠 수 없습니다. |
| `connection` | 빌드할 `overmap_connection`의 ID.                                            |
| `from`       | 연결의 원점으로 처리할 스페셜 내의 선택적 포인트 `[ x, y, z]`.               |

### 가변 스페셜 예제

```json
[
  {
    "type": "overmap_special",
    "id": "anthill",
    "subtype": "mutable",
    "locations": ["subterranean_empty"],
    "city_distance": [25, -1],
    "city_sizes": [0, 20],
    "occurrences": [0, 1],
    "flags": ["CLASSIC", "WILDERNESS"],
    "check_for_locations": [
      [[0, 0, 0], ["land"]],
      [[0, 0, -1], ["subterranean_empty"]],
      [[1, 0, -1], ["subterranean_empty"]],
      [[0, 1, -1], ["subterranean_empty"]],
      [[-1, 0, -1], ["subterranean_empty"]],
      [[0, -1, -1], ["subterranean_empty"]]
    ],
    "//1": "Same as writing out 'check_for_locations' 9 times with different points.",
    "check_for_locations_area": [
        [ { "type": [ "subterranean_empty" ], "from": [ 1, 1, -2 ], "to": [ -1, -1, -2 ] } ]
    ],
    "//2": "The anthill will have 3 possible sizes",
    "shared": { "size": [ 1, 3 ] },
    "joins": ["surface_to_tunnel", "tunnel_to_tunnel"],
    "overmaps": {
      "surface": { "overmap": "anthill", "below": "surface_to_tunnel", "locations": ["land"] },
      "below_entrance": {
        "overmap": "ants_nesw",
        "above": "surface_to_tunnel",
        "north": "tunnel_to_tunnel",
        "east": "tunnel_to_tunnel",
        "south": "tunnel_to_tunnel",
        "west": "tunnel_to_tunnel"
      },
      "crossroads": {
        "overmap": "ants_nesw",
        "north": "tunnel_to_tunnel",
        "east": "tunnel_to_tunnel",
        "south": "tunnel_to_tunnel",
        "west": "tunnel_to_tunnel"
      },
      "tee": {
        "overmap": "ants_nes",
        "north": "tunnel_to_tunnel",
        "east": "tunnel_to_tunnel",
        "south": "tunnel_to_tunnel"
      },
      "straight_tunnel": {
        "overmap": "ants_ns",
        "north": "tunnel_to_tunnel",
        "south": "tunnel_to_tunnel"
      },
      "corner": { "overmap": "ants_ne", "north": "tunnel_to_tunnel", "east": "tunnel_to_tunnel" },
      "dead_end": { "overmap": "ants_end_south", "north": "tunnel_to_tunnel" },
      "queen": { "overmap": "ants_queen", "north": "tunnel_to_tunnel" },
      "larvae": { "overmap": "ants_larvae", "north": "tunnel_to_tunnel" },
      "food": { "overmap": "ants_food", "north": "tunnel_to_tunnel" }
    },
    "root": "surface",
    "phases": [
      [{ "overmap": "below_entrance", "max": 1 }],
      [
        "//1": "Shared multiplier 'size' will affect the size",
        "//2": "of the tunnel system generated in this phase.",
        { "overmap": "straight_tunnel", "max": 10, "scale": "size" },
        { "overmap": "corner", "max": { "poisson": 2.5 }, "scale": "size" },
        { "overmap": "tee", "max": 5, "scale": "size" }
      ],
      [{ "overmap": "queen", "max": 1 }],
      [{ "overmap": "food", "max": 5 }, { "overmap": "larvae", "max": 5 }],
      [
        { "overmap": "dead_end", "weight": 2000 },
        { "overmap": "straight_tunnel", "weight": 100 },
        { "overmap": "corner", "weight": 100 },
        { "overmap": "tee", "weight": 10 },
        { "overmap": "crossroads", "weight": 1 }
      ]
    ]
  }
]
```

### 가변 스페셜 배치 방법

#### 오버맵과 조인

참고: 다음 맥락에서 "오버맵"은 "오버맵 레벨" 또는 근무한 세계 지도가 나뉘는 180x180 OMT 청크로서의 "오버맵"과 혼동되어서는 안 됩니다.

가변 스페셜은 이를 빌드하는 데 사용되는 OMT를 정의하는 _오버맵_ 모음과 서로 연결할 수 있는 방법을 정의하는 _조인_을 가집니다. 각 오버맵은 각 가장자리(4개의 기본 방향, 위, 아래)에 대한 조인을 지정할 수 있습니다. 이 조인은 해당 방향의 인접한 오버맵에 대한 반대 조인과 일치해야 합니다.

위의 예에서 `surface` 오버맵이 `"below": "surface_to_tunnel"`을 지정하는 것을 볼 수 있으며, 이는 그 아래의 조인이 `surface_to_tunnel`이어야 함을 의미합니다. 따라서 아래의 오버맵은 `"above": "surface_to_tunnel"`을 지정해야 합니다. `below_entrance` 오버맵만 그렇게 하므로 해당 오버맵이 `surface` 오버맵 아래에 배치되어야 함을 알 수 있습니다.

오버맵은 항상 회전할 수 있으므로 `north` 제약 조건이 다른 방향에 해당할 수 있습니다. 따라서 위의 `dead_end` 오버맵은 어떤 방향으로든 막다른 터널을 나타낼 수 있지만, 생성된 맵이 의미가 있으려면 선택된 OMT `ants_end_south`가 `north` 조인과 일치해야 한다는 것이 중요합니다.

오버맵은 연결도 지정할 수 있습니다. 예를 들어, 오버맵은 다음과 같이 정의될 수 있습니다:

```json
"where_road_connects": {
  "overmap": "road_end_north",
  "west": "parking_lot_to_road",
  "connections": { "north": { "connection": "local_road" } }
}
```

가변 스페셜 배치가 완료되면 이 오버맵의 북쪽 가장자리에서(다시 말하지만, 'north'는 상대적 용어이며 오버맵이 회전함에 따라 회전됩니다) 고정 스페셜에 대한 연결이 구축되는 것과 같은 방식으로 `local_road` 연결이 구축됩니다. '기존' 연결은 가변 스페셜에 대해 지원되지 않습니다.

#### 레이아웃 단계

모든 조인과 오버맵이 정의된 후, 스페셜이 배치되는 방식은 `root`와 `phases`에 의해 제공됩니다.

`root`는 이 스페셜의 원점에서 먼저 배치되는 오버맵을 지정합니다.

그런 다음 `phases`는 더 많은 오버맵을 배치하는 데 사용되는 성장 단계 목록을 제공합니다. 이 단계들은 엄격하게 순서대로 처리됩니다.

각 _단계_는 규칙 목록입니다. 각 _규칙_은 오버맵과 정수 `max` 및/또는 `weight`를 지정합니다.

Weight는 항상 단순 정수여야 하지만 `max`는 정수에 대한 확률 분포를 정의하는 객체일 수도 있습니다. 스페셜이 생성될 때마다 해당 분포에서 값이 샘플링됩니다. Poisson 분포는 `{ "poisson": 5 }`와 같은 객체를 통해 지원되며, 여기서 5는 분포의 평균(λ)이 됩니다. 플랫 분포는 `[min, max]` 쌍을 통해 지원됩니다. `max`는 또한 스페셜 내의 다른 승수 `shared`에 의해 `scale`될 수 있으며, 여러 다른 오버맵의 양을 서로 비례하여 확장할 수 있습니다. 각 `shared` 승수는 `max` 값과 같은 방식으로 확률 분포로 정의할 수 있으며, 스페셜 배치 시 한 번 샘플링됩니다.

각 단계 내에서 게임은 기존 오버맵에서 만족되지 않은 조인을 찾고 규칙에서 사용 가능한 오버맵 중에서 해당 조인을 만족시킬 오버맵을 찾으려고 시도합니다. 이 스페셜에 대한 조인을 정의하는 목록에서 먼저 나열된 조인에 우선순위가 부여되지만, 동일한(가장 높은 우선순위) ID의 여러 조인이 있는 경우 하나가 무작위로 선택됩니다.

먼저 규칙은 특정 위치에 대한 조인을 만족시킬 수 있는 것만 포함하도록 필터링되고, 그런 다음 필터링된 목록에서 가중치 선택이 이루어집니다. 가중치는 규칙에 지정된 `max`와 `weight`의 최소값으로 제공됩니다. `max`와 `weight`의 차이점은 규칙이 사용될 때마다 `max`가 1씩 감소한다는 것입니다. 따라서 해당 규칙을 선택할 수 있는 횟수를 제한합니다. `weight`만 지정하는 규칙은 임의의 횟수만큼 선택될 수 있습니다.

현재 단계의 어떤 규칙도 특정 위치에 대한 조인을 만족시킬 수 없는 경우, 해당 위치는 나중 단계에서 다시 시도하도록 제쳐둡니다.

모든 조인이 만족되거나 제쳐지면 단계가 종료되고 생성이 다음 단계로 진행됩니다.

모든 단계가 완료되고 만족되지 않은 조인이 남아 있으면 이는 오류로 간주되고 더 많은 세부 정보와 함께 디버그 메시지가 표시됩니다.

#### 청크

단계의 배치 규칙은 특정 구성에 배치될 여러 오버맵을 지정할 수 있습니다. 이것은 단일 OMT보다 큰 일부 기능을 배치하려는 경우 유용합니다. 다음은 마이크로랩의 예입니다:

```json
{
  "name": "subway_chunk_at_-2",
  "chunk": [
    { "overmap": "microlab_sub_entry", "pos": [0, 0, 0], "rot": "north" },
    { "overmap": "microlab_sub_station", "pos": [0, -1, 0] },
    { "overmap": "microlab_subway", "pos": [0, -2, 0] }
  ],
  "max": 1
}
```

청크의 `"name"`은 문제가 발생했을 때 디버깅 메시지에만 사용됩니다. `"max"` 및 `"weight"`는 위와 같이 처리됩니다.

새로운 기능은 오버맵 목록과 상대 위치 및 회전을 지정하는 `"chunk"`입니다. 오버맵은 이 스페셜에 대해 정의된 것에서 가져옵니다. `"north"`의 회전이 기본값이므로 이를 지정해도 효과가 없지만 구문을 보여주기 위해 여기에 포함되어 있습니다.

위치와 회전은 상대적입니다. 청크는 모든 오버맵이 강체처럼 함께 이동하고 회전하는 한 모든 오프셋과 회전에 배치될 수 있습니다.

#### 배치 오류를 피하는 기술

이러한 오류를 피하는 데 도움이 되도록 가변 스페셜 배치의 몇 가지 추가 기능이 도움이 될 수 있습니다.

##### `check_for_locations`

`check_for_locations`는 스페셜이 배치를 시도하기 전에 확인되는 추가 제약 조건 목록을 정의합니다. 각 제약 조건은 위치(루트에 상대적) 쌍과 위치 집합입니다. 각 위치의 기존 OMT는 주어진 위치 중 하나에 속해야 하며, 그렇지 않으면 시도된 배치가 중단됩니다.

`check_for_locations` 제약 조건은 `below_entrance` 오버맵이 루트 아래에 배치될 수 있고 4개의 기본 인접 OMT가 모두 `subterranean_empty`인지 확인하며, 이는 `below_entrance`의 4개의 다른 조인을 만족시키는 추가 오버맵을 추가하는 데 필요합니다.

`check_for_locations_area`를 사용하면 개별 포인트에 대해 `check_for_locations`를 반복하는 대신 확인할 영역을 정의할 수 있습니다.

##### `into_locations`

각 조인에는 관련 위치 목록도 있습니다. 이것은 스페셜의 위치로 기본 설정되지만 다음과 같이 특정 조인에 대해 재정의될 수 있습니다:

```json
"joins": [
  { "id": "surface_to_surface", "into_locations": [ "land" ] },
  "tunnel_to_tunnel"
]
```

해결되지 않은 조인을 만족시킬 때 오버맵이 배치되려면 특정 위치에 인접한 기존 조인을 만족시키는 것만으로는 충분하지 않습니다. 이미 일치하는 것 이상으로 소유한 잔여 조인은 해당 조인의 위치와 일치하는 지형을 가진 OMT를 가리켜야 합니다.

위의 개미집 예제의 특정 경우, 이 두 가지 추가 사항이 배치가 항상 성공하고 만족되지 않은 조인이 남아 있지 않도록 보장하는 방법을 볼 수 있습니다.

다음 몇 단계의 배치는 다양한 터널을 배치하려고 시도합니다. 조인 제약 조건은 만족되지 않은 조인(터널의 열린 끝)이 항상 `subterranean_empty` OMT를 가리키도록 보장합니다.

##### 최종 단계에서 완전한 커버리지 보장

최종 단계에서는 안테힐을 더 이상 성장시키지 않고 만족되지 않은 조인을 마무리하기 위한 5가지 다른 규칙이 있습니다. 더 적은 조인을 가진 오버맵의 규칙이 더 높은 가중치를 얻는 것이 중요합니다. 일반적인 경우 모든 만족되지 않은 조인은 `dead_end`를 사용하여 단순히 닫힙니다. 그러나 두 개의 만족되지 않은 조인이 동일한 OMT를 가리키는 가능성도 고려해야 하며, 이 경우 `dead_end`는 맞지 않고(필터링됩니다) `straight_tunnel` 또는 `corner`가 맞으며, 가장 높은 가중치를 가지고 있으므로 그 중 하나가 선택될 가능성이 높습니다. `tee`가 선택되는 것을 원하지 않습니다(맞을 수 있더라도). 왜냐하면 새로운 만족되지 않은 조인으로 이어지고 터널을 더 성장시킬 것이기 때문입니다. 하지만 이 상황에서 `tee`가 가끔 선택되는 것은 큰 문제가 아닙니다. 새로운 조인은 단순히 `dead_end`를 사용하여 만족될 것입니다.

자신만의 가변 오버맵 스페셜을 설계할 때 마지막 단계가 끝날 때까지 모든 조인이 만족되도록 이러한 순열을 생각해야 합니다.

#### 선택적 조인

최종 단계에서 모든 가능한 상황을 만족시키도록 설계된 많은 규칙을 갖는 대신, 일부 상황에서는 선택적 조인을 사용하여 이를 더 쉽게 만들 수 있습니다. 이 기능은 다른 단계에서도 사용할 수 있습니다.

가변 스페셜에서 오버맵과 연관된 조인을 지정할 때 `Crater` 오버맵 스페셜의 이 예제와 같이 타입으로 자세히 설명할 수 있습니다:

```json
"overmaps": {
  "crater_core": {
    "overmap": "crater_core",
    "north": "crater_to_crater",
    "east": "crater_to_crater",
    "south": "crater_to_crater",
    "west": "crater_to_crater"
  },
  "crater_edge": {
    "overmap": "crater",
    "north": "crater_to_crater",
    "east": { "id": "crater_to_crater", "type": "available" },
    "south": { "id": "crater_to_crater", "type": "available" },
    "west": { "id": "crater_to_crater", "type": "available" }
  }
},
```

`crater_edge`의 정의는 북쪽에 하나의 필수 조인과 다른 기본 방향에 3개의 '사용 가능한' 조인을 가지고 있습니다. '사용 가능한' 조인의 의미는 해결되지 않은 조인으로 간주되지 않으므로 더 많은 오버맵이 배치되도록 하지 않지만, 기존 해결되지 않은 조인을 만족시키기 위해 필요할 때 특정 타일에 대한 다른 조인을 만족시킬 수 있다는 것입니다.

오버맵은 항상 가능한 한 많은 필수 조인이 만족되고 사용 가능한 조인은 현재 조인이 필요하지 않은 다른 방향을 가리키도록 회전됩니다.

따라서 이 `crater_edge` 오버맵은 자체적으로 새로운 해결되지 않은 조인을 생성하지 않고 `Crater` 스페셜에 대한 해결되지 않은 조인을 만족시킬 수 있습니다. 이것은 최종 단계에서 스페셜을 마무리하는 데 훌륭합니다.

세 번째 유형의 조인 - 'optional'은 위의 두 가지를 혼합한 것으로, 실제로 새로운 해결되지 않은 조인을 생성하여 빌드하지만 그러한 조인은 필수가 아니며 해결되지 않은 상태로 남겨질 수 있습니다.

'optional' 및 'available' 조인은 어떤 종류의 해결도 요구하지 않기 때문에 원치 않는 장소에서 끝날 수 있으며, 이는 동일한 ID의 다른 조인이 연결되는 것을 금지하는 'reject' 타입의 의사 조인을 추가하여 방지할 수 있습니다.

#### 비대칭 조인

때때로 두 개의 다른 OMT를 연결하고 싶지만 둘 다 자신과 연결되는 것을 원하지 않을 수 있습니다. 이 경우 두 개 모두에 동일한 조인을 사용하고 싶지 않을 것입니다. 대신 하나를 다른 것의 반대로 지정하여 쌍을 형성하는 두 개의 조인을 정의할 수 있습니다.

이것이 발생할 수 있는 또 다른 상황은 조인의 양쪽이 다른 위치 제약 조건을 필요로 하는 경우입니다. 예를 들어, 개미집에서 표면과 지하 구성 요소는 다른 위치가 필요합니다. 다음과 같이 표면과 터널 사이의 조인을 비대칭으로 만들어 조인 정의를 개선할 수 있습니다:

```json
"joins": [
  { "id": "surface_to_tunnel", "opposite": "tunnel_to_surface" },
  { "id": "tunnel_to_surface", "opposite": "surface_to_tunnel", "into_locations": [ "land" ] },
  "tunnel_to_tunnel"
],
```

보시다시피 쌍의 `tunnel_to_surface` 부분은 표면을 향하기 때문에 `into_locations`의 기본값을 재정의해야 합니다.

#### 대체 조인

때때로 가변 스페셜의 다음 단계가 기존 해결되지 않은 조인에 연결할 수 있기를 원하지만 자체적으로 해당 타입의 해결되지 않은 조인을 생성하지 않기를 원할 수 있습니다. 이것은 구식과 새식 사이에 깨끗한 구분을 만드는 데 도움이 됩니다.

예를 들어, 이것은 `microlab_mutable` 스페셜에서 발생합니다. 이 스페셜은 `microlab` OMT의 덩어리로 둘러싸인 일부 구조화된 `hallway` OMT를 가지고 있습니다. 복도는 측면을 가리키는 `hallway_to_microlab` 조인을 가지고 있으므로 `microlab` OMT가 이를 일치시키기 위해 `microlab_to_hallway` 조인(`hallway_to_microlab`의 반대)을 가져야 합니다.

그러나 `microlab` OMT의 해결되지 않은 가장자리가 주변에 더 많은 복도를 요구하는 것을 원하지 않으므로 대부분 `microlab_to_microlab` 조인을 사용하기를 원합니다. 각 타입의 조인 수가 다른 많은 다른 `microlab` 변형을 만들지 않고 이러한 명백히 충돌하는 요구 사항을 어떻게 만족시킬 수 있을까요? 대체 조인이 여기서 도움이 될 수 있습니다.

`microlab` 오버맵의 정의는 다음과 같을 수 있습니다:

```json
"microlab": {
  "overmap": "microlab_generic",
  "north": { "id": "microlab_to_microlab", "alternatives": [ "microlab_to_hallway" ] },
  "east": { "id": "microlab_to_microlab", "alternatives": [ "microlab_to_hallway" ] },
  "south": { "id": "microlab_to_microlab", "alternatives": [ "microlab_to_hallway" ] },
  "west": { "id": "microlab_to_microlab", "alternatives": [ "microlab_to_hallway" ] }
},
```

이것은 오버맵에 이미 배치된 복도와 결합할 수 있게 하지만 새로운 해결되지 않은 조인은 더 많은 `microlab`만 일치시킵니다.

#### 새 가변 스페셜 테스트

배치 오류에 대해 가변 스페셜을 철저히 테스트하려고 하고 게임을 컴파일할 수 있는 위치에 있다면 `tests/overmap_test.cpp`의 기존 테스트를 사용하는 쉬운 방법이 있습니다.

해당 파일에서 `TEST_CASE( "mutable_overmap_placement"`를 찾으세요. 해당 함수의 시작 부분에 테스트가 생성을 시도하는 가변 스페셜 ID 목록이 있습니다. 그 중 하나를 새 스페셜의 ID로 바꾸고 다시 컴파일하고 테스트를 실행하세요.

테스트는 스페셜을 수천 번 배치하려고 시도하며 배치가 실패할 수 있는 대부분의 방법을 찾아야 합니다.

### 조인

조인 정의는 ID가 될 간단한 문자열일 수 있습니다. 또는 다음 키 중 일부를 가진 딕셔너리일 수 있습니다:

| 식별자           | 설명                                                 |
| ---------------- | ---------------------------------------------------- |
| `id`             | 정의되는 조인의 ID.                                  |
| `opposite`       | 인접한 지형에서 이것과 일치해야 하는 조인의 ID.      |
| `into_locations` | 이 조인이 가리킬 수 있는 `overmap_location` ID 목록. |

### 가변 스페셜 오버맵

오버맵은 JSON 딕셔너리입니다. 각 오버맵에는 JSON 딕셔너리 키인 ID(이 스페셜에 로컬)가 있어야 하며, 값 내의 필드는 다음과 같을 수 있습니다:

| 식별자      | 설명                                                                                                                          |
| ----------- | ----------------------------------------------------------------------------------------------------------------------------- |
| `overmap`   | 해당 위치에 배치할 `overmap_terrain`의 ID.                                                                                    |
| `locations` | 이 오버맵 지형이 배치될 수 있는 `overmap_location` ID 목록. 지정되지 않으면 스페셜 정의의 `locations` 값으로 기본 설정됩니다. |
| `north`     | 이 OMT의 북쪽 가장자리와 정렬되어야 하는 조인                                                                                 |
| `east`      | 이 OMT의 동쪽 가장자리와 정렬되어야 하는 조인                                                                                 |
| `south`     | 이 OMT의 남쪽 가장자리와 정렬되어야 하는 조인                                                                                 |
| `west`      | 이 OMT의 서쪽 가장자리와 정렬되어야 하는 조인                                                                                 |
| `above`     | 위의 OMT에 이것을 연결해야 하는 조인                                                                                          |
| `below`     | 아래의 OMT에 이것을 연결해야 하는 조인                                                                                        |

방향과 연관된 각 조인은 조인 ID로 해석되는 간단한 문자열일 수 있습니다. 또는 다음 키를 가진 JSON 객체일 수 있습니다:

| 식별자         | 설명                                                                                                                                        |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| `id`           | 여기서 사용되는 조인의 ID.                                                                                                                  |
| `type`         | `"mandatory"` 또는 `"available"`. 기본값: `"mandatory"`.                                                                                    |
| `alternatives` | 이 오버맵을 배치할 때만 `id` 아래에 나열된 것 대신 사용할 수 있는 조인 ID 목록. 배치로 생성된 해결되지 않은 조인은 기본 조인 `id`만 됩니다. |

### 생성 규칙

| 식별자                 | 설명                                                                                       |
| ---------------------- | ------------------------------------------------------------------------------------------ |
| `overmap` 또는 `chunk` | 배치할 `overmap`의 ID, 청크 구성.                                                          |
| `join`                 | 현재 단계 동안 해결되어야 하는 `join`의 ID.                                                |
| `z`                    | 이 단계에 대한 Z 레벨 제한.                                                                |
| `om_pos`               | 이 단계가 실행될 수 있는 오버맵(세계의 180x180 청크)의 절대 좌표 `[ x, y ]`.               |
| `rotate`               | 참 또는 거짓, 현재 조각이 회전할 수 있는지 여부. 스페셜의 회전 가능한 속성보다 우선합니다. |
| `max`                  | 이 규칙을 사용할 수 있는 최대 횟수.                                                        |
| `scale`                | `max`를 확장할 공유 승수의 ID.                                                             |
| `weight`               | 이 규칙을 선택할 가중치.                                                                   |

Z 레벨 제한은 숫자, 절대 좌표 제한이 있는 `["min", "mix"]` 범위, 지금까지 배치된 스페셜의 다른 타일의 경계를 참조하는 "top"\"bottom" 문자열, 위 경계의 오프셋이 있는 "top"\"bottom" 속성을 가진 객체를 지원합니다.

`max`와 `weight` 중 하나는 지정되어야 합니다. `weight`가 지정되지 않으면 `max`가 가중치로 사용됩니다.

## 도시 건물

도시 건물은 도시 생성 프로세스 중에 오버맵에 배치되는 개체로, **overmap_special** 타입의 "도시" 대응물입니다. 도시 건물의 정의는 오버맵 스페셜의 하위 집합이므로 여기서 자세히 반복하지 않습니다.

### 필수 오버맵 스페셜 / 지역 설정

도시 건물은 오버맵 스페셜과 동일한 수량 제한의 대상이 아니며, 실제로 발생 속성은 전혀 적용되지 않습니다. 대신 도시 건물의 배치는 `region_settings` 내에서 도시 건물에 할당된 빈도에 의해 구동됩니다. 자세한 내용은 [REGION_SETTINGS.md](./REGION_SETTINGS)를 참조하세요.

### 필드

| 식별자      | 설명                                                                                         |
| ----------- | -------------------------------------------------------------------------------------------- |
| `type`      | "city_building"이어야 합니다.                                                                |
| `id`        | 고유 ID.                                                                                     |
| `overmaps`  | `overmap_special`에서와 같지만 한 가지 주의 사항: 모든 포인트 x 및 y 값은 >= 0이어야 합니다. |
| `locations` | `overmap_special`에서와 같음.                                                                |
| `flags`     | `overmap_special`에서와 같음.                                                                |
| `rotate`    | `overmap_special`에서와 같음.                                                                |

### 예제

```json
[
  {
    "type": "city_building",
    "id": "zoo",
    "locations": ["land"],
    "overmaps": [
      { "point": [0, 0, 0], "overmap": "zoo_0_0_north" },
      { "point": [1, 0, 0], "overmap": "zoo_1_0_north" },
      { "point": [2, 0, 0], "overmap": "zoo_2_0_north" },
      { "point": [0, 1, 0], "overmap": "zoo_0_1_north" },
      { "point": [1, 1, 0], "overmap": "zoo_1_1_north" },
      { "point": [2, 1, 0], "overmap": "zoo_2_1_north" },
      { "point": [0, 2, 0], "overmap": "zoo_0_2_north" },
      { "point": [1, 2, 0], "overmap": "zoo_1_2_north" },
      { "point": [2, 2, 0], "overmap": "zoo_2_2_north" }
    ],
    "flags": ["CLASSIC"],
    "rotate": true
  }
]
```

## 오버맵 연결

### 필드

| 식별자            | 설명                                                                       |
| ----------------- | -------------------------------------------------------------------------- |
| `type`            | "overmap_connection"이어야 합니다.                                         |
| `id`              | 고유 ID.                                                                   |
| `default_terrain` | 무방향 연결 및 존재 확인에 사용할 기본 `overmap_terrain`.                  |
| `subtypes`        | 유효한 위치, 지형 비용, 결과 오버맵 지형을 결정하는 데 사용되는 항목 목록. |
| `layout`          | (선택적) 연결 레이아웃, 기본값은 `city`.                                   |

`city` 레이아웃을 사용하면 각 연결 포인트가 가장 가까운 도시의 중심에 연결됩니다. `p2p` 레이아웃을 사용하면 각 연결 포인트가 동일한 타입의 가장 가까운 연결에 연결됩니다.

### 예제

```json
[
  {
    "type": "overmap_connection",
    "id": "local_road",
    "subtypes": [
      { "terrain": "road", "locations": ["field", "road"] },
      { "terrain": "road", "locations": ["forest_without_trail"], "basic_cost": 20 },
      { "terrain": "road", "locations": ["forest_trail"], "basic_cost": 25 },
      { "terrain": "road", "locations": ["swamp"], "basic_cost": 40 },
      { "terrain": "road_nesw_manhole", "locations": [] },
      { "terrain": "bridge", "locations": ["water"], "basic_cost": 120 }
    ]
  },
  {
    "type": "overmap_connection",
    "id": "subway_tunnel",
    "subtypes": [
      { "terrain": "subway", "locations": ["subterranean_subway"], "flags": ["ORTHOGONAL"] }
    ]
  }
]
```

### 하위 유형

| 식별자       | 설명                                                                                                    |
| ------------ | ------------------------------------------------------------------------------------------------------- |
| `terrain`    | 배치 위치가 `locations`와 일치할 때 배치될 `overmap_terrain`.                                           |
| `locations`  | 이 하위 유형이 적용되는 `overmap_location` 목록. 비어 있을 수 있음; `terrain`이 그대로 유효함을 나타냄. |
| `basic_cost` | 경로를 찾을 때 이 하위 유형의 비용. 기본값 0.                                                           |
| `weight`     | 다른 항목보다 항목이 나타날 가중치. 기본값 0.                                                           |
| `flags`      | [json_flags.md](../json_flags)의 `Overmap connections` 참조.                                            |

## 오버맵 위치

### 필드

| 식별자     | 설명                                                    |
| ---------- | ------------------------------------------------------- |
| `type`     | "overmap_location"이어야 합니다.                        |
| `id`       | 고유 ID.                                                |
| `terrains` | 이 위치의 일부로 간주될 수 있는 `overmap_terrain` 목록. |

### 예제

```json
[
  {
    "type": "overmap_location",
    "id": "wilderness",
    "terrains": ["forest", "forest_thick", "field", "forest_trail"]
  }
]
```

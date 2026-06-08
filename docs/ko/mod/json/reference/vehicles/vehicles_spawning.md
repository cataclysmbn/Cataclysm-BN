# 차량 생성

차량 프로토타입은 기본 차량을 생성하는 데 사용됩니다. 차량이 생성된 후에는 다른 형식으로 저장됩니다.

차량 프로토타입은 현재 copy-from을 허용하지 않습니다

## 차량 프로토타입

```json
"type": "vehicle",
"id": "sample_vehicle",                    // 고유 ID. 하나의 연속된 단어여야 하며,
                                           // 필요한 경우 밑줄을 사용하세요.
"name": "Sample Vehicle",                  // 게임 내 표시 이름
"blueprint": [                             // 차량의 미리보기; 문서로 사용 가능
    "o#o",                                 // 또는 팔레트와 함께 부품을 정의하는 방법
    "o#o"                                  // 팔레트를 사용한 부분 정의도 가능
],
"blueprint_origin": { "x": 10, "y": 3 }    // 팔레트가 오프셋을 찾는 데 사용
"palette": {                               // 블루프린트가 타일에 부품을 설정하는 데 사용하는 팔레트
  "O": [ "airship_balloon_external" ]      // 문자열 배열만 가능, 부품 객체는 불가
}
"parts": [                                 // 부품 목록
    { "x": 0, "y": 0, "part": "frame" },   // 부품 정의, 양의 x 방향은 위,
    { "x": 0, "y": 0, "part": "seat" },    // 양의 y는 오른쪽
    { "x": 0, "y": 0, "part": "controls"}, // 부품 id는 vehicle_parts.json 참조
    { "x": 0, "y": 1, "parts: [ "frame", "seat" ] }, // 같은 공간의 부품 배열
    { "x": 0, "y": 1, "parts: [ { "part": "tank", "fuel": "gasoline" }, "battery_car" },
    { "x": 0, "y": 1, "part": "stereo" },  // parts 배열과 part는 같은 공간에서 혼합 가능
    { "x": 1, "y": 0, "parts: [ "frame, "wheel" ] },
    { "x": 1, "y": 1, "parts: [ "frame, "wheel" ] },
    { "x": -1, "y": 0, "parts: [ "frame, "wheel" ] },
    { "x": -1, "y": 1, "parts: [ "frame, "wheel" ] }
],
"items": [                                 // 아이템 생성 목록
    { "x": 0, "y": 0, "items": "helmet_army" },   // 개별 아이템
    { "x": 0, "y": 0, "item_groups": "army_uniform" }, // item_group의 아이템
    { "x": 0, "y": 1, "items": [ "matchbook", "two_by_four" ] }, // 목록의 모든 아이템 생성
    { "x": 0, "y": 0, "item_groups": [ "army_uniform", "rare_guns" ] } // 모든 item_groups 처리됨
]
```

.* 중요! *. 차량 부품은 게임에서 설치하는 것과 같은 순서로 정의되어야 합니다
(즉, 프레임과 마운트 포인트가 먼저). 일반적인 설치 규칙을 위반할 수도 없습니다
(스택 불가능한 부품 플래그를 스택할 수 없습니다).

### 부품 목록

부품 목록은 임의의 수의 줄을 포함합니다. 각 줄은 다음 형식입니다:
`{ "x": X, "y": Y, "part": PARTID, ... }` 또는 `{ "x": X, "y": Y, "parts": [ PARTID1, ... ] }`

첫 번째 형식에서, 줄은 차량의 X,Y 위치에 PARTID 타입의 단일 부품을 정의합니다.
선택적인 "ammo", "ammo_types", "ammo_qty", 또는 "fuel" 키를 적절한 값과 함께 가질 수 있습니다.

두 번째 형식에서, 줄은 X, Y 위치에 여러 부품을 정의합니다. 각 부품은 PARTID 문자열로 정의되거나,
위에서와 같이 선택적 키 "ammo", "ammo_types", "ammo_qty", 또는 "fuel"을 가진 { "part": PARTID, ... } 형식의 객체일 수 있습니다.

여러 다른 줄이 같은 X, Y 좌표를 가질 수 있으며 각각은 해당 위치에 추가 부품을 추가합니다.
부품은 올바른 순서로 추가되어야 합니다. 즉: 휠 허브는 휠보다 먼저 추가되어야 하지만, 프레임 이후에 추가되어야 합니다.

### 아이템 목록

아이템 목록은 임의의 수의 줄을 포함합니다. 각 줄은 { "x": X, "y": Y,
TYPE: DATA } 형식이며, 해당 위치에 생성될 수 있는 아이템을 설명합니다. TYPE과 DATA는 다음 중 하나일 수 있습니다:

```json
"items": "itemid"                              // 해당 타입의 단일 아이템
"items": [ "itemid1", "itemid2", ... ]         // 배열의 모든 아이템
"item_groups": "groupid"                       // 그룹의 하나 이상의 아이템, 그룹이 컬렉션인지
                                               // 배포인지에 따라
"item_groups": [ "groupid1", "groupid2" ... ]  // 각 그룹의 하나 이상의 아이템
```

선택적 키워드 "chance"는 특정 아이템 정의가 생성될 100 중 X 확률을 제공합니다.

여러 줄의 아이템이 같은 X와 Y 값을 공유할 수 있습니다.

### 차량 그룹

```json
"id":"city_parked",            // 고유 ID. 하나의 연속된 단어여야 하며, 필요한 경우 밑줄 사용
"vehicles":[                 // 잠재적인 차량 ID 목록. 차량이 생성될 확률은 X/T이며, 여기서
  ["suv", 600],           //    X는 특정 차량에 연결된 값이고 T는 그룹의 모든
  ["pickup", 400],          //    차량 값의 합계입니다
  ["car", 4700],
  ["road_roller", 300]
]
```

### 차량 배치

```json
"id":"road_straight_wrecks",  // 고유 ID. 하나의 연속된 단어여야 하며, 필요한 경우 밑줄 사용
"locations":[ {               // 잠재적인 차량 위치 목록. 이 배치가 사용되면 이러한 위치 중 하나가 무작위로 선택됩니다.
  "x" : [0,19],               // x 배치. 단일 값 또는 가능성의 범위일 수 있습니다.
  "y" : 8,                    // y 배치. 단일 값 또는 가능성의 범위일 수 있습니다.
  "facing" : [90,270]         // 차량의 방향. 단일 값 또는 가능한 값의 배열일 수 있습니다.
} ]
```

### 차량 생성

```json
"id":"default_city",            // 고유 ID. 하나의 연속된 단어여야 하며, 필요한 경우 밑줄 사용
"spawn_types":[ {       // 생성 타입 목록. 이 vehicle_spawn이 적용되면 가중치에 따라 생성 타입 중 하나를 무작위로 선택합니다.
  "description" : "Clear section of road",           //    이 생성 타입의 설명
  "weight" : 33,          //    이 생성 타입이 사용될 확률.
  "vehicle_function" : "jack-knifed_semi", // 생성 타입이 내장 json 함수를 사용하는 경우에만 필요합니다.
  "vehicle_json" : {      // json 지정 생성 타입에만 필요합니다.
  "vehicle" : "car",      // 생성할 차량 또는 vehicle_group.
  "placement" : "%t_parked",  // 차량을 생성할 때 사용할 vehicle_placement. x, y, facing이 지정된 경우 필요하지 않습니다.
  "x" : [0,19],     // x 배치. 단일 값 또는 가능성의 범위일 수 있습니다. placement가 지정된 경우 필요하지 않습니다.
  "y" : 8,   // y 배치. 단일 값 또는 가능성의 범위일 수 있습니다. placement가 지정된 경우 필요하지 않습니다.
  "facing" : [90,270], // 차량의 방향. 단일 값 또는 가능한 값의 배열일 수 있습니다. placement가 지정된 경우 필요하지 않습니다.
  "number" : 1, // 생성할 차량의 수.
  "fuel" : -1, // 새 차량의 연료.
  "status" : 1  // 새 차량의 상태.
} } ]
```

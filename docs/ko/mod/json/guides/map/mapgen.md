# 기본 맵젠 가이드

이 가이드는 맵젠의 기본 사항, 편집해야 할 파일, 각 파일의 태그, 그리고 스페셜이나 일반 도시 건물을 생성하는 차이점을 다룹니다.

[맵젠 항목에 대한 전체 기술 정보는 여기를 참조하세요](../../reference/map/mapgen)

먼저 몇 가지 기본 개념과 추가하거나 편집할 파일을 다루겠습니다.

## 일반 주석:

CDDA 맵젠은 익숙해지면 놀라울 정도로 강력합니다. 많은 트릭을 사용하여 맵에 다양성과 흥미를 더할 수 있습니다. 대부분의 고급 맵젠 기술은 다른 튜토리얼에서 다룰 것입니다. 이 튜토리얼은 기본 개념과 기본 단일 OMT(오버맵 지형 타일) 크기 건물을 만드는 방법을 다룹니다. 팔레트 사용법과 지붕 추가 방법도 다룰 것입니다.

## 스페셜 vs. 도시 건물:

스페셜은 도시 외곽에 생성되는 건물로, 도시로부터의 거리와 유효한 OMT 지형 타입과 같은 추가 정보가 필요합니다. 또한 최근 변경으로 스페셜 타입 건물이 도시 내부에 생성될 수 있게 되기 전까지는 게임 내 유일한 다중 타일 건물이었습니다. 스페셜의 예: 농장, 오두막, LMOE 등.

도시 건물은 단일 또는 다중 타일 크기일 수 있으며 도시 경계 내에서만 생성됩니다. 건물은 도시 건물이자 스페셜일 수 있지만 두 타입 모두에 생성되려면 두 세트의 항목이 필요합니다. 일부 모텔이 이의 예입니다(참조: 2fmotel_city 및 2fmotel).

중요한 정책: 지붕 프로젝트 이후 모든 건물은 이제 z 레벨을 가로질러 다중 타일입니다. 모든 새 건물은 항상 JSON 지붕을 추가해야 합니다. 곧 모든 지하실도 1층 맵젠에 맞춤 제작될 것이므로, 지하실을 원한다면 전용 아래층을 포함하는 것이 좋은 관행입니다.

## 파일 및 목적:

1. [data/json/mapgen](https://github.com/cataclysmbn/Cataclysm-BN/tree/main/data/json/mapgen) 또는 하위 폴더 중 하나에 새 맵젠 파일을 추가합니다. 건물에 기존 기초 형태를 사용하는 경우 해당 건물의 파일에 추가할 수 있습니다.
   - 이것은 건물의 청사진입니다. 가구와 전리품을 추가하기 위한 모든 건물 데이터를 포함할 수도 있습니다(대안으로 팔레트 참조).

2. 적절한 overmap_terrain 파일([data/json/overmap/overmap_terrain](https://github.com/cataclysmbn/Cataclysm-BN/tree/main/data/json/overmap/overmap_terrain))에서 생성하는 각 z 레벨에 대한 항목을 추가합니다.
   - 이러한 항목은 오버맵에서 건물이 어떻게 보이는지, 심볼, 색상, 인도 추가와 같은 생성 요구사항을 정의하며, 일부 맵젠 기능에 대한 플래그도 제어합니다.

3. 스페셜인지 도시 건물인지에 따라 specials.json 또는 multitile_city_buildings.json에 항목을 추가합니다.
   - multitile_city_buildings의 경우 건물의 다양한 z 레벨 및/또는 여러 OMT를 연결합니다.
   - 스페셜의 경우 건물의 다양한 z 레벨 또는 여러 OMT를 연결하고, 필요한 도로/지하철/등의 연결을 정의하며, 생성 매개변수를 정의합니다.

4. 도시 건물의 경우 regional_settings.json에 항목을 추가합니다. 이렇게 하면 세계에 생성될 수 있습니다.

5. 선택 사항이지만 대규모 프로젝트에 권장: mapgen_palettes 폴더에 새 팔레트 파일 추가(기존 팔레트도 사용 가능).
   - 이것은 맵젠 파일 데이터의 일부를 보유하는 여러 맵 간에 공유할 수 있는 파일입니다. 일반적으로 지형과 가구를 정의하는 데 사용됩니다. 전리품, 차량, 몬스터 생성 및 맵젠 파일의 `"object"` 태그에 일반적으로 들어가는 기타 데이터도 넣을 수 있습니다.
   - 기존 맵젠 팔레트를 편집하면 팔레트와 맵젠 파일의 조합을 사용하는 기존 맵에 영향을 줄 수 있으므로 피하세요.

## 맵젠 항목 시작:

새 맵 프로젝트를 시작할 때 일반적으로 처음부터 게임에 생성되는 데 필요한 모든 항목을 추가합니다. 이렇게 하면 작업하면서 테스트하고 필요에 따라 조정할 수 있습니다. 따라서 프로젝트를 시작할 때 필요한 모든 파일을 설정하는 것이 좋습니다.

시작하기 전에 몇 가지 결정을 내려야 합니다:

1. 전체 크기는 얼마나 될까요(OMT가 몇 개인가요)?
2. 어디에 생성될까요?
3. 팔레트를 사용할까요, 아니면 맵젠 파일에 모든 것을 넣을까요?
   - 팔레트를 사용하는 경우 처음부터 가능한 한 많이 정의하세요.

4. 고급 질문:
   - 중첩 맵을 사용할까요?
   - 지하철이나 도로에 연결하고 싶나요?
   - 팔레트와 결합하여 맵젠 객체 데이터를 사용할까요(둘 다의 결합 사용에 대한 마스터 클래스를 원하면 쇼핑몰 2층을 참조)?

## 맵젠 맵:

이것은 맵젠 파일 맵 플래그와 그것이 하는 일을 평이한 용어로 다룹니다. [MAPGEN](../../reference/map/mapgen)에서 더 광범위한 정보를 얻을 수 있습니다.

맵젠 파일에는 일부 메타데이터 태그와 맵을 만드는 모든 것을 정의하는 `"object"` 데이터가 있습니다.

### 메타데이터:

샘플:

```json
"type": "mapgen",
"method": "json",
"om_terrain": "s_restaurant_coffee",
"weight": 250,
```

1. `"type"`은 항상 mapgen이고(나중 튜토리얼에서 다른 맵 타입을 다룰 것), `"method"`는 항상 JSON입니다. 이 데이터는 프로그램에 이 파일을 처리하는 방법을 알려줍니다.

2. `"om_terrain"`: 이것은 기본적으로 맵의 내부 이름입니다(오버맵에 표시되는 이름이 아님). **동일한 건물 기초 형태를 공유하는** 동일한 맵의 여러 변형을 계획하지 않는 한 일반적으로 고유해야 합니다(참고: 실제 건물 기초의 형태를 의미함).

3. `"weight"`: 이 항목은 **동일한 om_terrain 이름을 공유하는** 다른 맵과 비교한 이 특정 맵의 가중치입니다. 집의 한 버전이 있고 다른 생성물이 있는 동일한 집을 만든다고 가정합시다(완전히 가구가 갖춰진 집과 버려진 버전처럼). 이 가중치는 각각이 다른 것과 관련하여 얼마나 자주 생성되는지 결정합니다. 가구가 있는 집이 100이고 버려진 집이 20이라고 가정합시다. 그러면 가구가 있는 집보다 5배 적게 생성됩니다.

### 객체 데이터:

이것은 맵, 지형, 가구 및 다양한 생성 타입을 정의하는 태그 섹션입니다. 아이템(및 중첩 맵)을 배치하는 여러 방법이 있습니다. 이들은 자체 튜토리얼이 필요합니다. 이 문서에서는 가장 사용하기 쉬운 전리품 생성을 위한 "명시적 심볼" 배치를 사용할 것입니다. 객체 섹션의 모든 것은 rows와 fill_ter을 제외하고 mapgen_palette에 배치할 수 있습니다.

샘플 객체 세그먼트: 객체의 모든 것은 객체의 {} 중괄호 내에 있어야 하며 그렇지 않으면 포함되지 않습니다. 끝 괄호를 잘못 배치하면 로딩 오류가 발생하지 않을 수 있습니다.

샘플:

```json
"object": {
  "fill_ter": "t_floor",
  "rows": [
    "S___________SSTzzzzzzzTS",
    "S_____,_____SSzMbMbMbMzS",
    "S_____,_____SSSSSSSS/MzS",
    "S_____,_____SSSSSSSS/MzS",
    "S_____,_____SSzzzSSS/MzS",
    "S_____,_____SS||V{{{V||S",
    "S_____,_____SS|D     <|S",
    "SSSSSSSSSSSSSS|r      OS",
    "SSSSSSSSSSSSSS|r      |S",
    "SVVVVVVVz./Mzz|   #W##|S",
    "SVD>>>>Vz./bMzV   #ww%|S",
    "SV BBB>Vz.b/..{   xwwF|S",
    "SV   B>Vz.Mb..{   flwl|S",
    "SV   B>Vz....zV   flwU|S",
    "SV   B>Vzbbbzz|X  #wwG|S",
    "S|B ^||||||||||^  ||I||S",
    "SO6  B|=;|;=|99  r|FwC|S",
    "S|B  6|=A|A=|9   r|Fwc|S",
    "S|   B|+|||+|9   D|!ww|S",
    "S|B               |Lwl|S",
    "SO6   6 6 6   B66B|Lwl|S",
    "S|B ^???????^ B66B|Lwl|S",
    "S|||||||||||||||||||3||S",
    "S4SSSSSSSSSSSSSSSSSSSSSS"
  ],
  "terrain": {
    " ": "t_floor",
    "!": "t_linoleum_white",
    "#": "t_linoleum_white",
    "%": "t_linoleum_white",
    "+": "t_door_c",
    ",": "t_pavement_y",
    ".": "t_grass",
    "/": "t_dirt",
    "3": [ "t_door_locked", "t_door_locked_alarm" ],
    ";": "t_linoleum_white",
    "=": "t_linoleum_white",
    "A": "t_linoleum_white",
    "C": "t_linoleum_white",
    "F": "t_linoleum_white",
    "G": "t_linoleum_white",
    "I": "t_door_locked_interior",
    "L": "t_linoleum_white",
    "M": "t_dirt",
    "O": "t_window",
    "S": "t_sidewalk",
    "U": "t_linoleum_white",
    "V": "t_wall_glass",
    "W": "t_fencegate_c",
    "_": "t_pavement",
    "b": "t_dirt",
    "c": "t_linoleum_white",
    "f": "t_linoleum_white",
    "l": "t_linoleum_white",
    "w": "t_linoleum_white",
    "x": "t_console_broken",
    "z": "t_shrub",
    "{": "t_door_glass_c",
    "|": "t_wall_b",
    "<": "t_stairs_up",
    "4": "t_gutter_downspout",
    "T": "t_tree_coffee"
  },
  "furniture": {
    "#": "f_counter",
    "%": "f_trashcan",
    "/": "f_bluebell",
    "6": "f_table",
    "9": "f_rack",
    ">": "f_counter",
    "?": "f_sofa",
    "A": "f_sink",
    "B": "f_chair",
    "C": "f_desk",
    "D": "f_trashcan",
    "F": "f_fridge",
    "G": "f_oven",
    "L": "f_locker",
    "M": "f_dahlia",
    "U": "f_sink",
    "X": "f_rack",
    "^": "f_indoor_plant",
    "b": "f_dandelion",
    "f": "f_glass_fridge",
    "l": "f_rack",
    "r": "f_rack"
  },
  "toilets": { ";": {  } },
  "items": {
    "#": { "item": "coffee_counter", "chance": 10 },
    "6": { "item": "coffee_table", "chance": 35 },
    "9": { "item": "coffee_display_2", "chance": 85, "repeat": [ 1, 8 ] },
    ";": { "item": "coffee_bathroom", "chance": 15 },
    "=": { "item": "coffee_bathroom", "chance": 35 },
    ">": { "item": "coffee_table", "chance": 25 },
    "A": { "item": "coffee_bathroom", "chance": 35 },
    "C": { "item": "office", "chance": 70 },
    "D": { "item": "coffee_trash", "chance": 75 },
    "F": { "item": "coffee_fridge", "chance": 80, "repeat": [ 1, 8 ] },
    "G": { "item": "oven", "chance": 35 },
    "L": { "item": "coffee_locker", "chance": 75 },
    "U": { "item": "coffee_dishes", "chance": 75 },
    "X": { "item": "coffee_newsstand", "chance": 90, "repeat": [ 1, 8 ] },
    "f": { "item": "coffee_freezer", "chance": 85, "repeat": [ 1, 8 ] },
    "l": { "item": "coffee_prep", "chance": 50 },
    "r": { "item": "coffee_condiments", "chance": 80, "repeat": [ 1, 8 ] }
  },
  "monsters": { "!": { "monster": "GROUP_COFFEE_SHOP_ZOMBIE", "chance": 1 } },
  "place_monsters": [ { "monster": "GROUP_ZOMBIE", "x": [ 3, 17 ], "y": [ 13, 18 ], "chance": 1 } ],
  "vehicles": { "c": { "vehicle": "swivel_chair", "chance": 100, "status": 1 } }
}
```

1. `"fill_ter"`: 이 태그는 가구 아래 및 rows의 정의되지 않은 심볼에 사용할 기본 지형/바닥을 정의합니다. 일반적으로 가장 많은 가구가 연결된 지형을 선택합니다.

2. `"rows"`: 이것은 건물의 실제 청사진입니다. 표준 OMT 타일(오버맵 타일)은 24x24 타일입니다. Rows는 맵의 왼쪽 상단 모서리인 0,0에서 x,y 좌표를 시작합니다.
   - 팁: 십자수를 하거나 십자수 패턴에 익숙하다면 이 모든 것이 매우 친숙해 보일 것입니다. 맵과 "범례" 영역이 있습니다.

3. `"terrain"`: 이것은 rows의 모든 문자가 지형일 때 무엇을 의미하는지 정의합니다. 심볼은 단일 지형을 반환하거나 지형 선택에서 생성될 기회를 제공할 수 있습니다. 다음은 몇 가지 빠른 예입니다:

`"*": "t_door_c",`: 이것은 rows의 모든 *를 닫힌 나무 문으로 만듭니다.

`"*": [ [ "t_door_locked_peep", 2 ], "t_door_locked_alarm", [ "t_door_locked", 10 ], "t_door_c" ],`: 이 배열은 문 선택에서 무작위로 선택합니다. 일부는 다른 것보다 생성될 확률이 높도록 가중치가 부여됩니다. 잠긴 문이 가장 일반적이고, 그 다음 엿보기 구멍 문입니다. 마지막으로 닫힌 문과 잠긴/경보 문은 같은 기본 가중치를 가지며 가장 적게 생성됩니다.

_참고: 제 예에서 fill_ter은 바닥용입니다. 따라서 다른 바닥으로 생성하려는 가구가 있는 경우, 가구에 부여한 동일한 심볼을 사용하고 새 바닥재에 대한 지형으로도 정의해야 합니다. 위의 예에서 여러 가구 심볼은 바닥재로 흰색 리놀륨을 사용하고 있습니다. 이 단계를 수행하지 않으면 가구가 잘못된 바닥재를 갖게 되며, 특히 부수면 눈에 띄게 됩니다. 저는 종종 최종 맵 체크를 하는데, 맵을 제출하기 전에 게임 내에서 가구를 부수며 돌아다니면서 지형을 확인합니다. 이것은 상당히 카타르시스가 될 수 있습니다._

4. `"furniture"` 태그: 지형과 마찬가지로 이것은 가구 ID와 맵 심볼의 목록입니다. 지형과 같은 종류의 배열을 처리할 수 있습니다.

5. `"toilets"` 및 기타 특별히 정의된 가구: 더 쉬운 배치를 허용하는 일부 특별히 정의된 일반 가구를 접하게 될 것입니다. 샘플 맵에서 항목: `"toilets": { ";": {  } },`은 심볼 항목을 정의하고 변기에 물을 자동으로 배치합니다. 몇 가지 다른 특수 가구 항목이 있습니다.

다른 가장 일반적인 것은:
`"vendingmachines": { "D": { "item_group": "vending_drink" }, "V": { "item_group": "vending_food" } }`
이것은 자동 판매기에 대한 두 심볼을 할당하고 하나는 음료용, 하나는 음식용으로 만듭니다. _참고: 총알 자판기처럼 기계에 모든 item_group을 넣을 수 있습니다_.

6. 아이템 생성: 아이템을 배치하는 방법은 많습니다. 이 튜토리얼은 가장 쉬운 명시적 심볼 배치만 다룹니다. 자세한 정보는 전리품 생성에 관한 모든 문서를 읽을 수 있습니다. 참조: [ITEM_SPAWN.md](../../reference/items/item_spawn).

샘플은 태그로 "items":를 사용합니다. 다른 것으로는: "place_item", "place_items", "place_loot"이 있습니다. 이들 중 일부는 개별 아이템 배치를 허용하고 다른 것은 그룹 또는 둘 다를 허용합니다. 이것은 다른 튜토리얼에서 다룰 것입니다.

지금은 이것을 분해해 봅시다:

```json
"items": {
      "a": { "item": "stash_wood", "chance": 30, "repeat": [ 2, 5 ] },
      "d": [
        { "item": "SUS_dresser_mens", "chance": 50, "repeat": [ 1, 2 ] },
        { "item": "SUS_dresser_womens", "chance": 50, "repeat": [ 1, 2 ] }
       ]
      }
```

이 경우 "a"는 맵에 정의된 대로 벽난로입니다. 그래서 벽난로에 stash_wood item_group을 추가하고 싶습니다. "a" 아래에 그룹을 정의하면 맵의 모든 벽난로가 이 그룹을 얻습니다. 이것은 모든 냉장고가 동일한 item_group을 얻는 집과 같은 일반 item_group에 특히 강력합니다. chance는 생성 확률이며 repeat는 해당 확률에 대한 롤을 2-5회 반복한다는 의미이므로 이 벽난로는 추가로 비축되거나 조금 있거나 확률 롤에 실패하면 아무것도 없을 수 있습니다.

"d" 항목은 dresser입니다. 저는 dresser가 두 가지 가능한 아이템 그룹에서 가져오기를 원했습니다. 하나는 남성용 선택이고 다른 하나는 여성용입니다. 따라서 이 심볼에 대한 모든 가능한 item_group을 포함하는 배열 `[ ... ]`이 있습니다.

배열에 원하는 만큼 많은 item_group을 추가할 수 있습니다. 이것은 일반 집 팔레트의 제 랙 중 하나입니다:

```json
"q": [
        { "item": "tools_home", "chance": 40 },
        { "item": "cleaning", "chance": 30, "repeat": [ 1, 2 ] },
        { "item": "mechanics", "chance": 1, "repeat": [ 1, 2 ] },
        { "item": "camping", "chance": 10 },
        { "item": "tools_survival", "chance": 5, "repeat": [ 1, 2 ] }
      ],
```

_참고: 명시적 심볼 배치를 사용할 때, 그룹이 해당 심볼을 사용하는 모든 가구에 생성될 기회가 있다는 것을 기억하세요. 따라서 상당히 후할 수 있습니다. 다른 아이템 생성이 있는 두 개의 책장을 원한다면, 각 책장에 고유한 심볼을 부여하거나 배치를 위해 x,y 좌표를 사용하는 대체 아이템 생성 형식을 사용하세요._

7. 몬스터 생성: 예제에는 두 가지 유형의 몬스터 생성이 나열되어 있습니다.

```json
"monsters": { "!": { "monster": "GROUP_COFFEE_SHOP_ZOMBIE", "chance": 1 } },
"place_monsters": [ { "monster": "GROUP_ZOMBIE", "x": [ 3, 17 ], "y": [ 13, 18 ], "chance": 1 } ],
```

첫 번째 항목은 명시적 심볼 배치 기술을 사용하고 있습니다. 끝 항목은 몬스터를 배치하기 위해 범위 x,y 좌표를 사용하고 있습니다. _참고: 우리는 overmap_terrain 항목을 선호하여 맵젠 파일에 몬스터를 넣는 것에서 멀어지고 있으므로, 특정 이유로 특정 몬스터 그룹을 생성하려는 경우에만 이것을 사용하세요. 자세한 정보는 overmap_terrain 섹션을 참조하세요._

8. 차량 생성:

```json
"vehicles": { "c": { "vehicle": "swivel_chair", "chance": 100, "status": 1 } }
```

우리의 차량은 명시적 심볼 배치를 사용하는 회전 의자입니다.

다음 예제는 vehicle_groups와 x,y 배치를 사용합니다. 회전과 상태도 포함합니다. 회전은 차량이 맵에서 생성될 방향이고, 상태는 전반적인 상태입니다. 연료는 꽤 자명합니다. 항상 게임 내에서 차량 생성을 테스트하세요. 배치에서 상당히 까다로울 수 있으며 회전은 숫자가 의미할 것으로 예상되는 것과 실제로 일치하지 않습니다. 차량의 0,0 지점은 다를 수 있으므로 특히 좁은 공간에서 올바른 위치에 생성하려면 실험해야 합니다.

```json
"place_vehicles": [
        { "chance": 75, "fuel": 0, "rotation": 270, "status": 1, "vehicle": "junkyard_vehicles", "x": 12, "y": 18 },
        { "chance": 75, "fuel": 0, "rotation": 270, "status": 1, "vehicle": "junkyard_vehicles", "x": 19, "y": 18 },
        { "chance": 90, "fuel": 0, "rotation": 0, "status": -1, "vehicle": "engine_crane", "x": 28, "y": 3 },
        { "chance": 90, "fuel": 0, "rotation": 90, "status": -1, "vehicle": "handjack", "x": 31, "y": 3 },
        { "chance": 90, "fuel": 30, "rotation": 180, "status": -1, "vehicle": "welding_cart", "x": 34, "y": 3 },
        { "chance": 75, "fuel": 0, "rotation": 180, "status": -1, "vehicle": "junkyard_vehicles", "x": 31, "y": 9 },
        { "chance": 75, "fuel": 0, "rotation": 270, "status": 1, "vehicle": "junkyard_vehicles", "x": 28, "y": 18 },
        { "chance": 75, "fuel": 0, "rotation": 270, "status": 1, "vehicle": "junkyard_vehicles", "x": 35, "y": 18 }
      ]
```

9. 가구 내 액체: 이 항목은 서 있는 탱크용입니다. 가구 항목에서 탱크를 "g"로 정의했습니다.

`"liquids": { "g": { "liquid": "water_clean", "amount": [ 0, 100 ] } },`

이것은 탱크에 깨끗한 물을 배치하고 생성할 양의 범위를 지정합니다.

10. 다른 특수 배치 기술이 있으며 더 많은 맵을 보면서 익히게 될 것입니다. 제가 좋아하는 것 중 하나는 새로운 화분용입니다:

화분은 "봉인된 아이템"이므로 해당 컨테이너에 무엇이 들어갈지 정의합니다. 이 예제는 화분에 씨앗(수확 준비)을 배치합니다. 첫 번째 것은 묘목을 배치하고, 다른 것들은 수확 준비가 되어 있습니다. 더 빠른 배치를 위해 각 화분 타입에 명시적 심볼을 부여했습니다.

```json
"sealed_item": {
      "1": { "item": { "item": "seed_rose" }, "furniture": "f_planter_seedling" },
      "2": { "item": { "item": "seed_chamomile" }, "furniture": "f_planter_harvest" },
      "3": { "item": { "item": "seed_thyme" }, "furniture": "f_planter_harvest" },
      "4": { "item": { "item": "seed_bee_balm" }, "furniture": "f_planter_harvest" },
      "5": { "item": { "item": "seed_mugwort" }, "furniture": "f_planter_harvest" },
      "6": { "item": { "item": "seed_tomato" }, "furniture": "f_planter_harvest" },
      "7": { "item": { "item": "seed_cucumber" }, "furniture": "f_planter_harvest" },
      "8": { "item": { "item": "soybean_seed" }, "furniture": "f_planter_harvest" },
      "9": { "item": { "item": "seed_zucchini" }, "furniture": "f_planter_harvest" }
    }
```

11. 모범 사례:

- 새 집을 만드는 경우 이 팔레트를 사용하세요: "standard_domestic_palette". 전리품이 이미 할당되어 있으며 광범위한 가정용 가구를 다룹니다. 이렇게 하면 전리품 생성을 위해 집이 다른 모든 집과 동기화됩니다.
- 모든 건물은 지붕 항목도 가져야 합니다.
- JSON의 항목 배치는 실제로 중요하지 않지만, 대부분의 기존 맵처럼 맵젠 파일을 정리하려고 노력하세요. 미래의 기여자에게 친절하세요. 파일 상단에 메타 데이터를 추가하세요. 객체 항목은 두 번째입니다. 객체 항목 내에서 일반적으로: 팔레트, set_points, 지형, 가구, 변기와 같은 무작위 특수 항목, 아이템 생성, 차량 생성, 몬스터 생성 순서입니다.
- 모든 건물은 지붕 접근을 위해 최소한 하나의 "t_gutter_downspout"를 가져야 합니다. 플레이어는 최소 2개를 원할 것입니다. 다중 z 레벨 건물에 배수로를 추가하는 경우 중간 층을 잊지 마세요. 플레이어가 올라갈 수 있도록 낙수관을 엇갈리게 배치해야 합니다(사다리처럼).
- 지붕에 올라가기 어려운 물건을 놓는 경우 사다리와 계단을 통해 더 나은 지붕 접근을 제공해야 합니다.
- 밖의 기본 잔디 덮개에는 맵 경계를 넘어 일관성을 유지하기 위해 `"t_region_groundcover_urban",`을 사용하세요. 다음은 잔디, 관목 및 나무에 대한 제 표준 식물 항목입니다:

```json
".": "t_region_groundcover_urban",
"A": [ "t_region_shrub", "t_region_shrub_fruit", "t_region_shrub_decorative" ],
"Z": [ [ "t_region_tree_fruit", 2 ], [ "t_region_tree_nut", 2 ], "t_region_tree_shade" ],
```

마지막으로 꽃(가구임):

```json
"p": "f_region_flower"
```

## 지붕 추가!

거의 모든 CDDA 건물이 이제 지붕이 가능하며 우리는 그것을 그렇게 유지하고 싶습니다. 건물과 함께 지붕 맵을 제출해야 합니다. 이것은 1층과 동일한 건물 형태/기초를 공유하는 다른 층과 같은 파일에 들어갈 수 있습니다.

그래서 이것은 방금 살펴본 건물에 비해 매우 쉽습니다. 모든 동일한 기본 구성 요소가 있습니다. 1층 맵의 rows를 사용하여 시작하고 `"roof_palette"` 심볼 세트로 변환하는 것이 좋습니다. 기본적으로 배수로로 윤곽을 추적하고, 아래의 t_gutter_spout 옆에 t_gutter_drop을 추가하고 거기에 일부 인프라를 던집니다. 저는 상업용 건물 지붕에 네스트를 광범위하게 사용했으며 고급 맵젠에서 다룰 것입니다.

샘플 지붕:

```json
{
  "type": "mapgen",
  "method": "json",
  "om_terrain": "house_01_roof",
  "object": {
    "fill_ter": "t_shingle_flat_roof",
    "rows": [
      "                        ",
      "                        ",
      "                        ",
      "  |222222222222222223   ",
      "  |.................3   ",
      "  |.................3   ",
      "  |.............N...3   ",
      "  5.................3   ",
      "  |.................3   ",
      "  |.................3   ",
      "  |........&........3   ",
      "  |.................3   ",
      "  |......=..........3   ",
      "  |.................3   ",
      "  |----------------53   ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        "
    ],
    "palettes": ["roof_palette"],
    "terrain": { ".": "t_shingle_flat_roof" }
  }
}
```

1. 저는 항상 건물 ID에 `_roof`를 추가합니다.
2. 팔레트가 이전 예제의 모든 데이터를 대신하는 것을 보세요. 매우 깨끗하고 쉽습니다.
3. 이것은 건물과 함께만 생성되므로(연결되면) `"weight"` 항목이 없습니다.
4. 제 팔레트는 기본 지붕으로 "t_flat_roof"를 사용합니다. 집의 경우 판자를 원했습니다. 따라서 이 맵젠에 "t_shingle_flat_roof"를 추가했으며 이것은 `".": "t_flat_roof"`에 대한 팔레트의 항목을 재정의합니다(고급 맵젠에서 더 자세히).

별도의 지붕 문서가 있습니다: [JSON_ROOF_MAPGEN](../json_roof_mapgen.md).

## multitile_city_buildings.json을 사용하여 다양한 맵젠 맵 연결

이 파일은 다음 위치에 있습니다:
[data/json/overmap/multitile_city_buildings.json](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/data/json/overmap/multitile_city_buildings.json).

_이 파일은 도시 건물 전용이며 스페셜이 아님을 기억하세요_

표준 항목:

```json
{
  "type": "city_building",
  "id": "house_dogs",
  "locations": [ "land" ],
  "overmaps": [
    { "point": [ 0, 0, 0 ], "overmap": "house_dogs_north" },
    { "point": [ 0, 0, 1 ], "overmap": "house_dogs_roof_north" },
    { "point": [ 0, 0, -1 ], "overmap": "basement" }
  ]
},
```

1. `"type"`은 변경되지 않습니다. 항상 "city_building"이어야 합니다.
2. `"id"`: 이 ID는 종종 맵젠 ID와 같지만 같을 필요는 없습니다. "house"와 같이 더 일반적인 것을 사용할 수 있습니다. 이 ID는 생성을 위해 regional settings에서 사용되므로 ID를 사용하는 건물이 얼마나 많은지 염두에 두세요. 저는 디버그 생성을 훨씬 훨씬 더 쉽게 만들기 때문에 고유한 ID를 선호합니다.
3. `"locations"`: 오버맵 지형 타입별로 이 건물이 배치될 수 있는 위치를 정의합니다. Land가 기본값입니다.
4. `"overmaps"`: 이것은 맵이 어떻게 함께 맞는지 정의하는 부분이므로 분해해 봅시다:
   `{ "point": [ 0, 0, 0 ], "overmap": "house_dogs_north" },` point: 건물의 다른 맵젠 파일과 관련된 지점입니다. 좌표는 `[ x, y, z ]`입니다. 이 예에서 x,y는 z 레벨당 하나의 맵만 있기 때문에 0입니다. y에 대해 0은 이것이 지상 레벨임을 의미합니다. 위의 지붕은 1에 있고 지하실은 -1에 있습니다.
5. ID에 `_north` 추가:
   - 건물이 회전하는 경우 층이 올바르게 일치하도록 이 나침반 지점이 필요합니다. 이것은 일반 지하실 맵젠 그룹이므로 `_north`를 얻지 못합니다(집에 전용 계단을 추가하면서 변경될 것입니다).

## regional_map_settings.json을 사용하여 오버맵 생성 설정

[data/json/regional_map_settings.json](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/data/json/regional_map_settings.json)

1. 도시 건물과 집의 경우 `"city":` 플래그로 스크롤합니다.
2. 적절한 하위 태그, 일반적으로 `"houses"` 또는 `"shops"`를 찾으세요.
3. multitile_city_buildings 항목에서 ID를 추가하세요. 이것은 맵젠 ID도 수락할 수 있으며 불평하지 않으므로 종종 같은 이름입니다.
4. 건물에 적합한 가중치를 선택하세요.

## 스페셜 연결 및 생성:

다음 위치에 항목을 넣으세요:
[data/json/overmap/overmap_special/specials.json](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/data/json/overmap/overmap_special/specials.json).

이 항목은 regional_map_settings와 multitile_city_buildings 및 기타 재미있는 오버맵 항목의 작업을 모두 수행합니다.

예제:

```json
{
  "type": "overmap_special",
  "id": "pump_station",
  "overmaps": [
    { "point": [0, 0, 0], "overmap": "pump_station_1_north" },
    { "point": [0, 0, 1], "overmap": "pump_station_1_roof_north" },
    { "point": [0, 1, 0], "overmap": "pump_station_2_north" },
    { "point": [0, 1, 1], "overmap": "pump_station_2_roof_north" },
    { "point": [0, -1, -1], "overmap": "pump_station_3_north" },
    { "point": [0, 0, -1], "overmap": "pump_station_4_north" },
    { "point": [0, 1, -1], "overmap": "pump_station_5_north" }
  ],
  "connections": [
    { "point": [0, -1, 0], "connection": "local_road", "from": [0, 0, 0] },
    { "point": [1, -1, -1], "connection": "sewer_tunnel", "from": [0, -1, -1] },
    { "point": [-1, -1, -1], "connection": "sewer_tunnel", "from": [0, -1, -1] },
    { "point": [-1, 1, -1], "connection": "sewer_tunnel", "from": [0, 1, -1] }
  ],
  "locations": ["land"],
  "city_distance": [1, 4],
  "city_sizes": [4, 12],
  "occurrences": [0, 1],
  "flags": ["CLASSIC"]
}
```

1. `"type"`: overmap_special입니다.
2. `"id"`는 오버맵의 건물 ID입니다. 게임 내 오버맵에도 표시됩니다.
3. `"overmaps"` 이것은 도시 건물 항목에서와 같은 방식으로 작동합니다. 펌프장은 지상에서 1 OMT보다 크므로 y 좌표도 변경됩니다.
4. `"connections"`: 맵에 도로, 하수도, 지하철 연결을 배치합니다.
5. `"locations"`: 이 건물이 배치될 수 있는 유효한 OMT 타입.
6. `"city_distance"`, `"city_sizes"` 둘 다 도시와 관련하여 이것이 생성되는 위치에 대한 매개변수입니다.
7. `"occurrences": [ 0, 1 ],`: 발생은 "UNIQUE" 플래그를 사용하는지 여부에 따라 두 가지를 의미할 수 있습니다. 플래그가 없으면 이것은 단순히 이 스페셜이 오버맵당 생성될 수 있는 횟수로 해석됩니다. 이 경우 0에서 1까지입니다. UNIQUE 플래그를 사용하면 이것은 백분율이 되므로 `[ 1, 10 ]`은 오버맵당 1에서 10번이 아니라 생성될 10% 확률의 1이 됩니다. 따라서 오버맵당 한 번 생성될 10% 확률입니다.
8. `"flags"`: 스페셜을 더 정의하는 데 사용할 수 있는 플래그입니다. 플래그 목록은 다음을 참조하세요:
   [json_flags](../../reference/json_flags.md).

자세한 내용은 [OVERMAP](../../reference/map/overmap.md)을 읽으세요.

## Overmap_terrain 항목:

다음 위치에서 건물 타입에 대한 파일을 선택하세요:
[data/json/overmap/overmap_terrain](https://github.com/cataclysmbn/Cataclysm-BN/tree/main/data/json/overmap/overmap_terrain).

이 항목 세트는 오버맵에서 건물이 어떻게 보일지 정의합니다. copy-from을 지원합니다.
예제:

```json
{
  "type": "overmap_terrain",
  "id": "s_music",
  "copy-from": "generic_city_building",
  "name": "music store",
  "sym": "m",
  "color": "brown",
  "mondensity": 2,
  "extend": { "flags": [ "SOURCE_LUXURY" ] }
},
{
  "type": "overmap_terrain",
  "id": "s_music_roof",
  "copy-from": "generic_city_building",
  "name": "music store roof",
  "sym": "m",
  "color": "brown"
}
```

맵젠 ID당 하나의 항목이 필요합니다:

1. `"type"`은 항상 overmap_terrain입니다.
2. `"id"`는 맵젠 파일에서 사용한 것과 동일한 ID가 됩니다.
3. `"copy-from"` 이것은 여기서 정의하는 것을 제외하고 다른 항목의 모든 데이터를 복사합니다.
4. `"name"` 오버맵에 이름이 표시되는 방법.
5. `"sym"` 오버맵에 표시되는 심볼. 생략하면 꺾쇠 표시가 사용됩니다 `v<>^`
6. `"color"` 오버맵 심볼의 색상.
7. `"mondesntiy"` 이 오버맵 타일에 대한 기본 몬스터 밀도를 설정합니다. 일반 좀비 생성에 이것을 사용하고 해당 위치에 대한 특수 생성을 위해 맵젠 몬스터 항목을 예약합니다(예: 애완동물 가게의 애완동물).
8. `"extend"` 이러한 플래그 중 많은 것이 미래에 AI를 위해 NPC가 사용할 것이므로 위치에 적합한 플래그를 추가하려고 노력하세요. 다른 것들은 인도 생성과 같은 맵젠을 더 정의합니다.

자세한 정보는 다음을 참조하세요:
[OVERMAP의 Overmap Terrain 섹션](../../reference/map/overmap#overmap-terrain).

## 팔레트:

앞서 언급했듯이 팔레트는 `rows`와 `fill_ter`을 제외하고 객체 항목에 포함된 거의 모든 정보를 보유할 수 있습니다. 주요 목적은 관련 맵에 동일한 기본 데이터를 추가할 필요성을 줄이고 심볼 일관성을 유지하는 것입니다.

맵젠 파일의 객체에 추가된 항목은 팔레트에서 사용된 동일한 심볼을 재정의합니다. 이렇게 하면 팔레트를 사용하고 필요에 따라 각 맵젠에 선택적 변경을 할 수 있습니다. 이것은 카펫, 콘크리트 등과 같은 여러 지상 지형에 특히 유용합니다.

지형은 맵젠 파일을 통해 대체할 때 매우 잘 작동합니다. 가구 재정의에는 덜 성공했지만 의도한 대로 작동하는 시기를 명확히 하려면 더 테스트해야 합니다. 최근에 팔레트 심볼이 값 배열을 사용하는 경우 맵젠 항목이 이를 재정의할 수 없다는 것을 발견했습니다.

예제: 맵젠 파일 객체에 대한 항목: `"palettes": [ "roof_palette" ],`

팔레트 메타데이터:

```json
"type": "palette",
"id": "roof_palette",
```

다른 모든 것은 일련의 객체 항목처럼 보일 것입니다. 예를 들어 roof_palette:

```json
{
  "type": "palette",
  "id": "roof_palette",
  "terrain": {
    ".": "t_flat_roof",
    " ": "t_open_air",
    "o": "t_glass_roof",
    "_": "t_floor",
    "2": "t_gutter_north",
    "-": "t_gutter_south",
    "3": "t_gutter_east",
    "4": "t_gutter_downspout",
    "|": "t_gutter_west",
    "5": "t_gutter_drop",
    "#": "t_grate",
    "&": "t_null",
    ":": "t_null",
    "X": "t_null",
    "=": "t_null",
    "A": "t_null",
    "b": "t_null",
    "c": "t_null",
    "t": "t_null",
    "r": "t_null",
    "L": "t_null",
    "C": "t_null",
    "Y": "t_null",
    "y": "t_null"
  },
  "furniture": {
    "&": "f_roof_turbine_vent",
    "N": "f_TV_antenna",
    ":": "f_cellphone_booster",
    "X": "f_small_satelitte_dish",
    "~": "f_chimney",
    "=": "f_vent_pipe",
    "A": "f_air_conditioner",
    "b": "f_bench",
    "c": "f_counter",
    "t": "f_table",
    "r": "f_rack",
    "L": "f_locker",
    "C": ["f_crate_c", "f_cardboard_box"],
    "Y": "f_stool",
    "s": "f_sofa",
    "S": "f_sink",
    "e": "f_oven",
    "F": "f_fridge",
    "y": ["f_indoor_plant_y", "f_indoor_plant"]
  },
  "toilets": { "T": {} }
}
```

더 복잡한 팔레트를 보려면 [data/json/mapgen_palettes/house_general_palette.json](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/data/json/mapgen_palettes/house_general_palette.json)의 standard_domestic_palette가 모든 CDDA 집에서 작동하도록 설계된 팔레트를 잘 보여줍니다. 전리품 생성을 포함하고 집에서 사용될 대부분의 가구를 설명합니다. 또한 특정 위치 요구 사항을 위해 맵젠 파일에서 사용할 수 있도록 열어둔 심볼 목록을 남겼습니다.

마지막으로 [data/json/mapgen_palettes/house_w_palette.json](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/data/json/mapgen_palettes/house_w_palette.json)의 house_w 팔레트 시리즈는 중첩 맵젠을 사용하는 집을 위해 함께 작동하도록 설계되었습니다. 기초에 전념하는 팔레트가 있고, 네스트용 팔레트가 있으며, 마지막으로 가정용 야외 중첩 청크용으로 설계한 또 다른 팔레트가 있습니다.

## 최종 주석:

여기의 정보는 맵젠을 돌아다니고 맵을 만들기 시작하기에 충분해야 하지만 집중적으로 다룰 많은 변형이 있습니다.

이 문서에서 다루지 않은 것:

- 중첩 맵 및 배치.
- NPC 생성.
- 복잡한 바닥 옵션을 위한 고급 지형 트릭.
- 함정, 지형 및 당신.
- update_mapgen(NPC 및 플레이어가 트리거하는 맵 업데이트).
- 필드를 방출하는 가구.

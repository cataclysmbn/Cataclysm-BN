---
title: Furniture and Terrain
---

### Furniture

```json
{
  "type": "furniture",
  "id": "f_toilet",
  "name": "toilet",
  "symbol": "&",
  "looks_like": "chair",
  "color": "white",
  "move_cost_mod": 2,
  "light_emitted": 5,
  "required_str": 18,
  "flags": ["TRANSPARENT", "BASHABLE", "FLAMMABLE_HARD"],
  "crafting_pseudo_item": "anvil",
  "examine_action": "toilet",
  "close": "f_foo_closed",
  "open": "f_foo_open",
  "lockpick_result": "f_safe_open",
  "lockpick_message": "With a click, you unlock the safe.",
  "provides_liquids": "beer",
  "bash": "TODO",
  "deconstruct": "TODO",
  "max_volume": "1000 L",
  "examine_action": "workbench",
  "workbench": { "multiplier": 1.1, "mass": 10000, "volume": "50L" },
  "boltcut": {
    "result": "f_safe_open",
    "duration": "1 seconds",
    "message": "The safe opens.",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "count": 3 }]
  },
  "hacksaw": {
    "result": "f_safe_open",
    "duration": "12 seconds",
    "message": "The safe is hacksawed open!",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "count": 13 }]
  }
}
```

#### `type`

고정 문자열이며, JSON 객체를 가구로 식별하려면 반드시 `furniture`여야 합니다.

`"id", "name", "symbol", "looks_like", "color", "bgcolor", "max_volume", "open", "close", "bash", "deconstruct", "examine_action", "flgs`

지형과 동일합니다. 아래 "가구와 지형 공통" 장을 참고하세요.

#### `move_cost_mod`

이동 비용 보정치입니다(`-10` = 통과 불가, `0` = 변화 없음). 이는 기반 지형의 이동 비용에 더해집니다.

#### `lockpick_result`

(선택) 가구를 잠금 해제(lockpick)하는 데 성공했을 때 바뀌는 가구입니다.

#### `lockpick_message`

(선택) 가구를 잠금 해제(lockpick)하는 데 성공했을 때 플레이어에게 출력되는 메시지입니다.
없으면 일반적인 `"The lock opens…"` 메시지가 대신 출력됩니다.

#### `oxytorch`

(선택) 산소 절단기(oxytorch) 사용 데이터입니다.

```cpp
oxytorch: {
    "result": "furniture_id", // (optional) furniture it will become when done, defaults to f_null
    "duration": "1 seconds", // ( optional ) time required for oxytorching, default is 1 second
    "message": "You quickly cut the metal", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `light_emitted`

가구가 내는 빛의 양입니다. 10이면 해당 타일을 밝게 비추고, 15이면 해당 타일과 주변 타일을 밝게 비추며,
광원으로부터 2타일 떨어진 타일도 약하게 비춥니다. 예시: 천장등은 120, 작업등은 240, 콘솔은 10입니다.

#### `boltcut`

(선택) 볼트 커터 사용 데이터입니다.

```cpp
"boltcut": {
    "result": "furniture_id", // (optional) furniture it will become when done, defaults to f_null
    "duration": "1 seconds", // ( optional ) time required for bolt cutting, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "sound": "Gachunk!", // ( optional ) description of the sound when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `hacksaw`

(선택) 쇠톱(hacksaw) 사용 데이터입니다.

```cpp
"hacksaw": {
    "result": "furniture_id", // (optional) furniture it will become when done, defaults to f_null
    "duration": "1 seconds", // ( optional ) time required for hacksawing, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `required_str`

가구를 밀어 옮기는 데 필요한 힘입니다. 음수 값은 이동 불가능한 가구를 의미합니다.

#### `crafting_pseudo_item`

(선택) 이 가구의 범위 내에서 제작 시 사용할 수 있는 아이템(도구)의 id입니다
(가구가 해당 타입 아이템처럼 동작). 배열로도 만들 수 있습니다. 예:
`"crafting_pseudo_item": [ "fake_gridwelder", "fake_gridsolderingiron" ],`

#### `workbench`

(선택) 여기서 제작 가능함을 의미합니다. 속도 배율, 허용 질량, 허용 부피를 지정해야 합니다.
이 한계를 넘는 질량/부피에는 속도 페널티가 적용됩니다. 동작하려면
`"workbench"` `examine_action`과 함께 사용해야 합니다.

#### `plant_data`

(선택) 이 가구는 식물입니다. 식물 변환값과 상황별 기준값(base)을 지정해야 합니다.
`GROWTH_HARVEST` 플래그가 있으면 수확/성장 배율도 추가할 수 있습니다.

#### `surgery_skill_multiplier`

(선택) 수술 시 이 가구 옆에 서 있는 생존자에게 적용되는 수술 스킬 배율(float)입니다.

#### `provides_liquids`

(선택) 상호작용 시 지정한 액체 아이템을 무한 공급합니다. 동작하려면
`"examine_action": "liquid_source"`와 함께 사용해야 합니다.

### Terrain

```json
{
  "type": "terrain",
  "id": "t_spiked_pit",
  "name": "spiked pit",
  "symbol": "0",
  "looks_like": "pit",
  "color": "ltred",
  "move_cost": 10,
  "light_emitted": 10,
  "trap": "spike_pit",
  "fill_result": "t_dirt", // Terrain result from filling this terrain in
  "fill_minutes": 10, // Number of minutes to fill terrain in
  "max_volume": "1000 L",
  "flags": ["TRANSPARENT"],
  "digging_results": {
    "digging_min": 1,
    "result_ter": "t_pit_shallow",
    "num_minutes": 60,
    "items": "digging_sand_50L"
  },
  "connects_to": "WALL",
  "close": "t_foo_closed",
  "open": "t_foo_open",
  "lockpick_result": "t_door_unlocked",
  "lockpick_message": "With a click, you unlock the door.",
  "nail_pull_result": "t_fence_post", // terrain ID to transform into when you use a hammer to pull the nails out
  "nail_pull_items": [8, 5], // nails and planks (respectively) to drop as a result of pulling nails with a hammer. Defaults to 0,0 and thus technically optional even if you add a nail_pull_result.
  "bash": "TODO",
  "deconstruct": "TODO",
  "harvestable": "blueberries",
  "transforms_into": "t_tree_harvested",
  "harvest_season": "WINTER",
  "roof": "t_roof",
  "examine_action": "pit",
  "boltcut": {
    "result": "t_door_unlocked",
    "duration": "1 seconds",
    "message": "The door opens.",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "2x4": 3 }]
  },
  "hacksaw": {
    "result": "t_door_unlocked",
    "duration": "12 seconds",
    "message": "The door is hacksawed open!",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "2x4": 13 }]
  }
}
```

#### `type`

고정 문자열이며, JSON 객체를 식별하려면 반드시 "terrain"이어야 합니다.

`"id", "name", "symbol", "looks_like", "color", "bgcolor", "max_volume", "open", "close", "bash", "deconstruct", "examine_action", "flgs`

가구와 동일합니다. 아래 "가구와 지형 공통" 장을 참고하세요.

#### `move_cost`

통과 이동 비용입니다. 값이 0이면 통과 불가(예: 벽)입니다. 음수 값을 사용하면 안 됩니다.
양수 값은 50 이동 포인트의 배수이며, 예를 들어 값 2는 지형을 통과할 때
`2 * 50 = 100` 이동 포인트를 사용함을 의미합니다.

#### `light_emitted`

지형이 내는 빛의 양입니다. 10이면 해당 타일을 밝게 비추고, 15이면 해당 타일과 주변 타일을 밝게 비추며,
광원으로부터 2타일 떨어진 타일도 약하게 비춥니다. 예시: 천장등은 120, 작업등은 240, 콘솔은 10입니다.

#### `digging_results`

(선택) 삽으로 지형을 팔 때 관련 필드입니다.

- `"digging_min"` - 이 지형을 팔기 위해 필요한 최소 digging 품질
- `"result_ter"` - 파낸 결과 지형 ID
- `"num_minutes"` - 파는 데 소요되는 분 수
- `"items"` - (선택) 아이템 배열 또는 기존 itemgroup ID. 기본값은 `digging_soil_loam_200L`

#### `lockpick_result`

(선택) 지형을 잠금 해제(lockpick)하는 데 성공했을 때 바뀌는 지형입니다.

#### `lockpick_message`

(선택) 지형을 잠금 해제(lockpick)하는 데 성공했을 때 플레이어에게 출력되는 메시지입니다.
없으면 일반적인 `"The lock opens…"` 메시지가 대신 출력됩니다.

#### `oxytorch`

(선택) 산소 절단기(oxytorch) 사용 데이터입니다.

```cpp
oxytorch: {
    "result": "terrain_id", // terrain it will become when done
    "duration": "1 seconds", // ( optional ) time required for oxytorching, default is 1 second
    "message": "You quickly cut the bars", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `trap`

(선택) 해당 지형의 내장 트랩 id입니다.

예를 들어 `t_pit` 지형은 내장 트랩 `tr_pit`을 가집니다. 게임 내에서 지형이 `t_pit`인 모든 타일에는
따라서 암묵적으로 `tr_pit` 트랩이 존재합니다. 이 둘은 분리할 수 없습니다
(플레이어는 내장 트랩을 비활성화할 수 없고, 지형을 바꾸면 내장 트랩도 비활성화됩니다).

내장 트랩이 있으면 다른 트랩을 명시적으로 추가할 수 없습니다(플레이어/맵젠 모두).

#### `harvestable`

(선택) 정의되면 해당 지형은 수확 가능합니다. 이 항목은 수확되는 과일(또는 유사물)의 아이템 타입을 정의합니다.
이 기능을 동작시키려면 `harvest_*` `examine_action` 함수 중 하나도 설정해야 합니다.

#### `boltcut`

(선택) 볼트 커터 사용 데이터입니다.

```cpp
"boltcut": {
    "result": "terrain_id", // terrain it will become when done
    "duration": "1 seconds", // ( optional ) time required for bolt cutting, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "sound": "Gachunk!", // ( optional ) description of the sound when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `hacksaw`

(선택) 쇠톱(hacksaw) 사용 데이터입니다.

```cpp
"hacksaw": {
    "result": "terrain_id", // terrain it will become when done
    "duration": "1 seconds", // ( optional ) time required for hacksawing, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `transforms_into`

(선택) 지형의 다양한 변환에 사용됩니다. 정의된 경우 유효한 지형 id여야 합니다. 예:

- 과일 수확 시(지형을 수확된 형태로 변경)
- `HARVESTED` 플래그와 `harvest_season`을 함께 사용해 수확된 지형을 다시 과일이 열리는 지형으로 변경

#### `harvest_season`

(선택) "SUMMER", "AUTUMN", "WINTER", "SPRING" 중 하나이며, "HARVESTED" 플래그와 조합해
지형을 다시 수확 가능한 지형으로 되돌리는 데 사용됩니다.

#### `roof`

(선택) 이 지형 위(지붕)의 지형입니다.

### Common To Furniture And Terrain

일부 값은 지형과 가구 모두에서 설정할 수/해야 하며, 의미는 각 경우 동일합니다.

#### `id`

객체의 id입니다. 해당 타입의 객체(모든 지형 또는 모든 가구 타입) 사이에서 고유해야 합니다.
관례상(기술적으로 필수는 아니지만) 가구 id는 `f_`, 지형 id는 `t_` 접두사를 사용합니다.
이 값은 번역되지 않습니다. 나중에 변경하면 저장 호환성이 깨지므로 변경하면 안 됩니다.

#### `name`

객체의 표시 이름입니다. 번역됩니다.

#### `flags`

(선택) 여러 추가 플래그. "doc/json_flags.md"를 참고하세요.

#### `connects_to`

(선택) 이 지형이 연결되는 지형 그룹입니다. 타일 회전과 연결, 그리고
"AUTO_WALL_SYMBOL" 플래그가 있는 지형의 ASCII 심볼 표시 방식에 영향을 줍니다.

현재 값:

- `CHAINFENCE`
- `RAILING`
- `WALL`
- `WATER`
- `WOODFENCE`
- `GUTTER`

예시: `-`, `|`, `X`, `Y`는 같은 `connects_to` 값을 공유하는 지형입니다. `O`는 그렇지 않습니다.
`X`와 `Y`는 `AUTO_WALL_SYMBOL` 플래그도 가집니다. `X`는 T자 교차점(서/남/동 연결)으로 그려지고,
`Y`는 수평선(서에서 동으로, 남쪽 연결 없음)으로 그려집니다.

```
-X-    -Y-
 |      O
```

#### `symbol`

게임에서 객체가 표시될 때의 ASCII 심볼입니다. symbol 문자열은 정확히 1글자여야 합니다.
또는 계절별 심볼을 정의하는 문자열 4개 배열일 수도 있습니다. 첫 항목은 봄 심볼입니다.
배열이 아니면 1년 내내 같은 심볼을 사용합니다.

#### `comfort`

이 지형/가구의 안락함입니다. 그 위에서 잠들 수 있는 능력에 영향을 줍니다.
uncomfortable = -999, neutral = 0, slightly_comfortable = 3,
comfortable = 5, very_comfortable = 10

#### `floor_bedding_warmth`

수면에 사용할 때 이 지형/가구가 제공하는 추가 보온값입니다.

#### `bonus_fire_warmth_feet`

근처 불로부터 발이 받는 보온값을 증가시킵니다(기본값 = 300).

#### `looks_like`

이 항목이 닮아 보이는 유사 항목의 id입니다. 타일셋 로더는 이 항목에 타일이 없으면 해당 항목 타일을
로드하려고 시도합니다. looks_like 항목은 암묵적으로 체인됩니다. 예를 들어 'throne'이
looks_like 'big_chair'를 가지고, 'big_chair'가 looks_like 'chair'를 가지면,
throne과 big_chair 타일이 없을 때 throne은 chair 타일로 표시됩니다.
looks_like 체인 어느 항목에서도 타일을 찾지 못하면 ASCII 심볼로 표시됩니다.

#### `color` or `bgcolor`

게임에서 객체가 표시되는 색상입니다. `color`는 전경색(배경 없음), `bgcolor`는 단색 배경색을 정의합니다.
`symbol` 값과 마찬가지로 계절별 색을 의미하는 4개 항목 배열을 사용할 수도 있습니다.

> **NOTE**: 반드시 "color" 또는 "bgcolor" 중 하나만 사용해야 합니다.

#### `max_volume`

(선택) 이 위치에 저장할 수 있는 최대 부피입니다. ml, L 단위를 사용할 수 있습니다
(`"50 ml"`, `"2 L"`).

#### `examine_action`

(선택) 객체를 조사할 때 호출되는 json 함수입니다. "src/iexamine.h"를 참고하세요.

#### `close" And "open`

(선택) 값은 지형 항목 내부에서는 지형 id, 가구 항목 내부에서는 가구 id여야 합니다.
둘 중 하나라도 정의되어 있으면 플레이어가 객체를 열고 닫을 수 있습니다.
열기/닫기를 수행하면 해당 타일의 객체가 지정된 것으로 변경됩니다.
예를 들어 `"safe_c"`가 `"safe_o"`로 "open"되고, `"safe_o"`가 다시 `"safe_c"`로 "close"되도록
설정할 수 있습니다. 여기서 `safe_c`, `safe_o`는 서로 다른 속성을 가진 두 개의 지형(또는 가구) 타입입니다.

#### `bash`

(선택) 객체를 부술 수 있는지와, 가능하다면 어떤 일이 일어나는지 정의합니다. "map_bash_info"를 참고하세요.

#### `deconstruct`

(선택) 객체를 해체할 수 있는지와, 가능하다면 결과가 무엇인지 정의합니다.
"map_deconstruct_info"를 참고하세요.

#### `pry`

(선택) 객체를 지렛대로 열 수 있는지와, 가능하다면 어떤 일이 일어나는지 정의합니다.
"prying_result"를 참고하세요.

#### `map_bash_info`

플레이어 또는 다른 무언가가 지형/가구를 부술 때 일어나는 다양한 일을 정의합니다.

```json
{
  "str_min": 80,
  "str_max": 180,
  "str_min_blocked": 15,
  "str_max_blocked": 100,
  "str_min_supported": 15,
  "str_max_supported": 100,
  "sound": "crunch!",
  "sound_vol": 2,
  "sound_fail": "whack!",
  "sound_fail_vol": 2,
  "ter_set": "t_dirt",
  "furn_set": "f_rubble",
  "explosive": 1,
  "collapse_radius": 2,
  "destroy_only": true,
  "bash_below": true,
  "tent_centers": ["f_groundsheet", "f_fema_groundsheet", "f_skin_groundsheet"],
  "items": "bashed_item_result_group"
}
```

#### `str_min`, `str_max`, `str_min_blocked`, `str_max_blocked`, `str_min_supported`, `str_max_supported`

TODO

#### `sound`, `sound_fail`, `sound_vol`, `sound_fail_vol`

(선택) 부수기에 성공해 객체가 파괴될 때, 또는 부수기에 실패했을 때 나는 소리와 그 음량입니다.
사운드 문자열은 번역되며(플레이어에게 표시됨).

#### `furn_set`, `ter_set`

원본이 파괴되었을 때 설정될 지형/가구입니다. 지형의 bash 항목에서는 필수이고,
가구 항목에서는 선택(기본값: 가구 없음)입니다.

#### `explosive`

(선택) 0보다 크면 객체 파괴 시 이 위력으로 폭발이 발생합니다(`game::explosion` 참고).

#### `destroy_only`

TODO

#### `bash_below`

TODO

#### `tent_centers`, `collapse_radius`

(선택) 텐트 구성 가구의 경우 중심 부품 id를 정의합니다. 텐트의 다른 부품이 부서질 때 중심도 함께
파괴됩니다. 중심은 지정된 `collapse_radius` 반경에서 탐색하며, 텐트 크기와 일치해야 합니다.

#### `items`

(선택) 아이템 그룹(인라인) 또는 아이템 그룹 id입니다. doc/ITEM_SPAWN.md를 참고하세요.
기본 subtype은 `collection`입니다. 부수기에 성공하면 해당 그룹의 아이템이 스폰됩니다.

#### `map_deconstruct_info`

```json
{
  "furn_set": "f_safe",
  "ter_set": "t_dirt",
  "items": "deconstructed_item_result_group"
}
```

#### `furn_set`, `ter_set`

원본이 해체된 뒤 설정될 지형/가구입니다. `furn_set`은 선택(기본값: 가구 없음),
`ter_set`은 지형의 `deconstruct` 항목에서만 사용되며 그 경우 필수입니다.

#### `items`

(선택) 아이템 그룹(인라인) 또는 아이템 그룹 id입니다. doc/ITEM_SPAWN.md를 참고하세요.
기본 subtype은 `collection`입니다. 객체를 해체하면 해당 그룹의 아이템이 스폰됩니다.

#### `prying_result`

```json
{
  "success_message": "You pry open the door.",
  "fail_message": "You pry, but cannot pry open the door.",
  "break_message": "You damage the door!",
  "pry_quality": 2,
  "pry_bonus_mult": 3,
  "noise": 12,
  "break_noise": 10,
  "sound": "crunch!",
  "break_sound": "crack!",
  "breakable": true,
  "difficulty": 8,
  "new_ter_type": "t_door_o",
  "new_furn_type": "f_crate_o",
  "break_ter_type": "t_door_b",
  "break_furn_type": "f_null",
  "break_items": [
    { "item": "2x4", "prob": 25 },
    { "item": "wood_panel", "prob": 10 },
    { "item": "splinter", "count": [1, 2] },
    { "item": "nail", "charges": [0, 2] }
  ]
}
```

#### `new_ter_type`, `new_furn_type`

원본을 지렛대로 여는 데 성공한 뒤 설정될 지형/가구입니다. `furn_set`은 선택(기본값: 가구 없음),
`ter_set`은 지형의 `pry` 항목에서만 사용되며 그 경우 필수입니다.

#### `success_message`, `fail_message`, `break_message`

지형/가구를 지렛대로 여는 데 성공했을 때, 실패했을 때, 또는 실패 시 지형/가구가 다른 것으로
부서졌을 때 표시되는 메시지입니다. `break_message`는 `breakable`이 true이고
`break_ter_type`이 정의된 경우에만 필요합니다.

#### `pry_quality`, `pry_bonus_mult`, `difficulty`

지형/가구를 지렛대로 열기를 시도할 수 있는 최소 pry 품질과, 성공 확률을 결정합니다.
iuse.cpp에서:

```cpp
int diff = pry->difficulty;
diff -= ( ( pry_level - pry->pry_quality ) * pry->pry_bonus_mult );
```

```cpp
if( dice( 4, diff ) < dice( 4, p->str_cur ) ) {
    p->add_msg_if_player( m_good, pry->success_message );
```

`difficulty`는 지렛대 시도 중 캐릭터 현재 힘과 비교됩니다.
도구의 pry 품질이 요구 `pry_quality`보다 높으면 `pry_bonus_mult`로 정해진 비율만큼 유효 난이도가 감소합니다.
`pry_bonus_mult`는 선택이며 기본값은 1입니다
(즉 도구의 pry 품질이 최소치보다 1 높을 때마다 `diff`가 1 감소).

#### `noise`, `break_noise`, 'sound', 'break_sound'

`noise`를 지정하면 지형/가구를 지렛대로 여는 데 성공했을 때 지정 음량으로 `sound`를 재생합니다.
`breakable`이 true이고 `break_noise`를 지정하면, 지렛대 실패로 지형/가구가 부서질 때
지정 음량으로 `break_noise`를 재생합니다.

`noise` 또는 `break_noise`를 지정하지 않으면 해당 지형/가구를 지렛대로 열거나 부숴도
아무 소리도 나지 않습니다(따라서 선택 항목). `sound`와 `break_sound`도 선택이며,
기본 메시지는 각각 "crunch!"와 "crack!"입니다.

#### `breakable`, `break_ter_type`, `break_furn_type`

`breakable`이 true면 지렛대 시도 실패 시 지형/가구가 대신 부서질 확률이 생깁니다.
지형의 경우 `breakable`이 true일 때 `break_ter_type`은 필수이며,
`break_furn_type`은 선택(기본 null)입니다.

#### `break_items`

(선택) 아이템 그룹(인라인) 또는 아이템 그룹 id입니다. doc/ITEM_SPAWN.md를 참고하세요.
기본 subtype은 `collection`입니다. `breakable`이 true면 지렛대 실패로 객체가 부서질 때
해당 그룹의 아이템이 스폰됩니다.

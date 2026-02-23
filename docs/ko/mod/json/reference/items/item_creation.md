# 아이템 생성 / 타입

> [!NOTE]
>
> 이 페이지는 현재 작성 중이며 최근 `JSON INFO`에서 분리되었습니다.

### 일반 아이템

```json
"type": "GENERIC",                // 일반 아이템으로 정의
"id": "socks",                    // 고유 ID. 공백 없이 하나의 단어로, 필요시 밑줄 사용
"name": {
    "ctxt": "clothing",           // 선택적 번역 컨텍스트. 문자열에 여러 의미가 있어 다른 언어로 번역할 때 다르게 번역해야 할 때 유용
    "str": "pair of socks",       // 조사 상자에 표시되는 이름. 공백으로 구분된 여러 단어 가능
    "str_pl": "pairs of socks"    // 선택사항. 이름이 불규칙한 복수형을 가진 경우(즉, 단수형에 "s"를 붙여 복수형을 만들 수 없는 경우) 지정해야 함. 단수와 복수가 같은 경우 "str_sp" 사용 가능
},
"conditional_names": [ {          // 지정된 조건에서 적용될 이름의 선택적 목록 (자세한 내용은 조걶� 명명 섹션 참조).
    "type": "COMPONENT_ID",       // 조건 타입.
    "condition": "leather",       // 확인할 조건.
    "name": { "str": "pair of leather socks", "str_pl": "pairs of leather socks" } // 이름 필드, 위와 같은 규칙 적용
} ],
"container": "null",             // 이 아이템이 생성되어야 하는 컨테이너(있는 경우)
"repairs_like": "scarf",          // 이 아이템에 제작법이 없는 경우, 수리할 때 어떤 아이템의 제작법을 찾을지
"color": "blue",                 // 아이템 심볼의 색상
"symbol": "[",                   // 맵에 표시되는 아이템 심볼. 정확히 1개의 콘솔 셀 너비를 가진 유니코드 문자열이어야 함
"looks_like": "rag",              // 타일셋에 힌트. 이 아이템에 타일이 없는 경우 looks_like 타일 사용
"description": "Socks. Put 'em on your feet.", // 아이템 설명
"ascii_picture": "ascii_socks", // 이 아이템에 사용되는 asci_art의 ID
"phase": "solid",                            // (선택사항, 기본값 = "solid") 상태
"weight": "350 g",                           // 무게, 그램 단위로 표현, mg과 kg 사용 가능 - "50 mg", "5 g" 또는 "5 kg". 쌓을 수 있는 아이템(탄약, 식량)의 경우 이것은 충전량당 무게
"volume": "250 ml",                          // 부피, ml과 L 단위 사용 가능 - "50 ml" 또는 "2 L". 쌓을 수 있는 아이템(탄약, 식량)의 경우 이것은 stack_size 충전량의 부피
"integral_volume": 0,                        // 아이템이 다른 아이템에 통합될 때(예: 총에 통합된 총알 개조) 기본 아이템에 추가되는 부피. ml과 L 단위 사용 가능 - "50 ml" 또는 "2 L"
"integral_weight": 0,                        // 아이템이 다른 아이템에 통합될 때(예: 총에 통합된 총알 개조) 기본 아이템에 추가되는 무게
"rigid": false,                              // 비강성 아이템의 경우 부피(및 착용 아이템의 경우 동작 제한)는 내용물에 비례하여 증가
"insulation": 1,                             // (선택사항, 기본값 = 1) 컨테이너나 차량 부품인 경우 내용물에 제공해야 하는 보온 정도
"price": 100,                                // NPC와 거래할 때 사용. 쌓을 수 있는 아이템(탄약, 식량)의 경우 이것은 stack_size 충전량당 가격. 문자열 "cent" "USD" 또는 "kUSD" 사용 가능
"price_postapoc": "1 USD",                   // price와 같지만 대재앙 이후 가치를 나타냄. 문자열 "cent" "USD" 또는 "kUSD" 사용 가능
"material": ["COTTON"],                      // 재료 타입, 원하는 만큼 많이 사용 가능. 가능한 옵션은 materials.json 참조
"weapon_category": [ "WEAPON_CAT1" ],        // (선택사항) 이 아이템이 속한 무술 무기 카테고리
"cutting": 0,                                // (선택사항, 기본값 = 0) 근접 무기로 사용할 때 가하는 절단 피해. 음수 값은 불가능
"bashing": 0,                                // (선택사항, 기본값 = 0) 근접 무기로 사용할 때 가하는 타격 피해. 음수 값은 불가능
"to_hit": 0,                                 // (선택사항, 기본값 = 0) 근접 무기로 사용할 때 명중 별림치
"attacks": [                                 // (선택사항) 새로운 공격 스탯블록, 진행 중인 기능
  { "id": "BASH",                            // 공격의 ID. ID가 "DEFAULT"인 공격은 계산된 데이터로 대첼됨("copy-from" 아이템에서 커스텀 공격을 제거하는 데 사용 가능)
    "to_hit": 1,                             // 이 공격의 명중 별림치
    "damage": { "values": [ { "damage_type": "bash", "amount": 50 } ] } }, // 이 공격의 피해, `damage_instance` 구문 사용 (아래 참조)
  { "id": "THRUST", "damage": { "values": [ { "damage_type": "stab", "amount": 45 } ] } }
],
"flags": ["VARSIZE"],                        // 특수 효과를 나타냄, json_flags.md 참조
"environmental_protection_with_filter": 6,   // 아이템(예: 방독면)에 필터가 필요하고 필터가 설치된 경우 환경 효과에 대한 저항. use_action 'GASMASK' 및 'DIVE_TANK'와 함께 사용
"magazine_well": 0,                          // 탄창이 아이템에서 튀어나와 추가 부피를 더하기 시작하는 부피
"magazines": [                               // 각 탄약 타입(있는 경우)에 사용할 수 있는 탄창 타입
    [ "9mm", [ "glockmag" ] ]                // 각 탄약 타입에 대해 지정된 첫 번째 탄창이 기본값
    [ "45", [ "m1911mag", "m1911bigmag" ] ],
],
"milling": {                                 // 선택사항. 주어진 경우 아이템을 물/풍차에서 분쇄할 수 있음
  "into": "flour",                           // 제품의 아이템 ID. 제품은 반드시 충전량을 사용하는 것이어야 함
  "conversion_rate": 4                       // 소비된 아이템당 제품 수. conversion_rate가 4인 경우 1개 아이템을 4개 제품으로 분쇄. 정수만 허용
},
"item_vars": {                               // 아이템 변수 미리 채우기; 현재는 모드와 게임 내 로직 외에는 사용되지 않음
  "test": "test"                             // 주로 lua 모드에서 유용함
}
"explode_in_fire": true,                     // 불에 태울 때 폭발해야 하는지
"explosion": {                               // 물리적 폭발 데이터
  "damage": 10,                              // 폭발 중심지에서 플레이어에게 가하는 피해. 50% 반경 이상에서는 피해가 절반으로 감소
  "radius": 8,                               // 폭발 반경. 0은 중심지만 영향받음을 의미
  "fire": true,                              // 폭발이 불을 남겨야 하는지
  "fragment": {                              // "파편"의 투사체 데이터. 이 투사체는 범위와 시야 내의 모든 대상에 정확히 한 번 명중
    "damage": {                              // 파편 투사체의 피해 데이터. `damage_instance` 구문 사용 (아래 참조)
      "damage_type": "acid",                 // 가하는 피해 타입
      "amount": 10                           // 가하는 피해량
      "armor_penetration": 4                 // 무시하는 방어력. 방어구별로 적용되며 총계가 아님
      "armor_multiplier": 2.5                // 방어력에서 남은 피해 감소에 적용되는 승수, 관통 후 적용(있는 경우). 높은 숫자는 방어구가 이 폭발의 파편을 더 효과적으로 막게 하고, 낮은 숫자는 남은 방어력의 백분율 감소로 작동
    }
  }
},
"repair_difficulty": 2                       // 수리 난이도를 덮어쓰는 제작법 난이도
```

#### damage_instance

```json
{
  "damage_type": "acid", // 가하는 피해 타입
  "amount": 10, // 가하는 피해량
  "armor_penetration": 4, // 무시하는 방어력. 방어구별로 적용되며 총계가 아님
  "armor_multiplier": 2.5 // 방어력에서 남은 피해 감소에 적용되는 승수, 관통 후 적용(있는 경우). 높은 숫자는 방어구가 더 효과적으로 보호하게 하고, 낮은 숫자는 남은 방어력의 백분율 감소로 작동
}
```

### 탄약

```json
"type" : "AMMO",            // 탄약으로 정의
...                         // 일반 아이템과 동일한 항목
                            // 추가적인 탄약 특정 항목:
"ammo_type" : "shot",       // 장전할 수 있는 것을 결정
"damage" : 18,              // 발사 시 원거리 피해
"prop_damage": 2,           // 무기 피해에 승수 적용(damage 필드 덮어씀)
"pierce" : 0,               // 발사 시 방어 관통 능력
"range" : 5,                // 발사 시 사거리
"dispersion" : 0,           // 탄약의 부정확성, 1/4도 단위로 측정
"recoil" : 18,              // 발사 시 발생하는 반동
"count" : 25,               // 함께 생성되는 탄 수
"stack_size" : 50,          // (선택사항) 위에서 정의한 부피에 몇 발이 들어있는지. 생략하면 'count'와 동일
"show_stats" : true,        // (선택사항) 전투 탄약에 대해 강제로 스탯 표시(damage와 prop_damage가 모두 없는 투사체용)
"dont_recover_one_in": 1    // (선택사항) 탄약을 회수하지 않을 1/x 확률(100은 99% 회수 확률 의미)
"drop": "nail",             // (선택사항) 투사체 위치에 100% 확률로 떨어지는 객체 정의
"drop_active": false        // (선택사항) 객체가 활성화된 상태로 시작하는지. 기본값은 true
"drop_count": 1,            // (선택사항) 떨어뜨릴 아이템 수. 도구의 경우 이것은 충전량을 설정
                             // 생략하면 해당 타입의 'count'에서 정의한 떨어뜨림 양이 기본값
"effects" : ["COOKOFF", "SHOT"]
```

### 탄창

```json
"type": "MAGAZINE",              // 탄창으로 정의
...                              // 일반 아이템과 동일한 항목
                                 // 추가적인 탄창 특정 항목:
"ammo_type": [ "40", "357sig" ], // 이 탄창에 장전할 수 있는 탄약 타입
"capacity" : 15,                 // 탄창 용량(탄약 충전량과 동등한 단위)
"count" : 0,                     // 탄창에 기본적으로 포함되는 탄약 양(탄약 벨트의 경우 설정)
"default_ammo": "556",           // 지정된 경우 기본 탄약을 덮어씀(탄약 벨트의 경우 선택적으로 설정)
"reliability" : 8,               // 이 탄창이 얼마나 신뢰할 수 있는지 0에서 10 사이의 범위(GAME_BALANCE.md 참조)
"reload_time" : 100,             // 탄창에 탄약 한 단위를 장전하는 데 걸리는 시간
"linkage" : "ammolink"           // 설정된 경우 소비된 탄약 단위마다 하나의 연결(주어진 타입)이 떨어짐(분해 탄약 벨트용)
```

### 방어구

방어구는 다음과 같이 정의할 수 있습니다:

```json
"type" : "ARMOR",     // 방어구(ARMOR)로 정의합니다.
...                   // 일반 아이템과 공통되는 항목
                      // 추가적인 방어구 특정 항목:
"covers" : ["FEET"],  // 보호하는 부위. 가능한 옵션: TORSO(몸통), HEAD(머리), EYES(눈), MOUTH(입), ARMS(팔), HANDS(손), LEGS(다리), FEET(발)
"storage" : 0,        // (선택사항, 기본값 = 0) 추가되는 수납 슬롯 수
"warmth" : 10,        // (선택사항, 기본값 = 0) 옷이 제공하는 보온성
"environmental_protection" : 0,  //  (선택사항, 기본값 = 0) 환경 보호 수준(가스, 산 등에 대한 저항)
"encumbrance" : 0,    // 기본 동작 제한(수선되지 않은 상태의 값)
"max_encumbrance" : 0,    // 비강성(부드러운 소재) 수납 용기에서 용량이 가득 찼을 때의 동작 제한 값. 캐릭터 용량이 가득 차지 않은 경우 encumbrance(최소값)와 max_encumbrance(최대값) 사이의 값을 가집니다: encumbrance + (max_encumbrance - encumbrance) * character volume.
"weight_capacity_bonus": "20 kg",    // (선택사항, 기본값 = 0) 무게 용량 병행에 대한 별림치. 음수도 가능. 문자열 사용 필요 - "5000 g" 또는 "5 kg"
"weight_capacity_modifier": 1.5, // (선택사항, 기본값 = 1) 기본 무게 용량에 대한 수정 계수
"coverage" : 80,      // 신체 부위를 덮는 면적의 백분율(%)
"material_thickness" : 1,  // 재료 두께(대략 밀리미터 단위). 일반적으로 1~5, 특수 방어구는 10 이상도 가능
"power_armor" : false, // 파워 아머 여부(특수 취급됨)
"valid_mods" : ["steel_padded"] // 적용 가능한 의류 개조 목록. 의류 개조 데이터에 "제한" 항목이 없는 경우 이 목록에 작성할 필요 없음
"resistance": { "cut": 0, "bullet": 1000 } // 설정된 경우 재료와 두께에서 계산되는 일반 방어력 계산을 덮어씁니다. 값은 무손상 상태의 아이템입니다. 두께(thickness)는 피해에 따른 성능 저하 정도에 영향을 줍니다 - 두께 1: 피해를 받아도 방어력이 감소하지 않음. 두께 2: 첫 번째 손상(1단계 피해)에서 방어력이 절반으로 감소. 두께 10: 손상 1단계마다 방어력이 10%씩 감소
```

또는 모든 아이템(책, 도구, 총, 심지어 음식도)은 armor_data를 가지면 방어구로 사용할 수 있습니다:

```json
"type" : "TOOL",      // 또는 다른 아이템 타입
...                   // 해당 타입에 맞는 공통 항목(예: 도구)
"armor_data" : {      // 추가로 위와 같은 방어구 데이터 항목
    "covers" : ["FEET"],
    "storage" : 0,
    "warmth" : 10,
    "environmental_protection" : 0,
    "encumbrance" : 0,
    "coverage" : 80,
    "material_thickness" : 1,
    "power_armor" : false
}
```

#### 방어구 부위 데이터

여러 부위를 보호하고 부위별로 다른 커버리지나 동작 제한을 설정하려면 `armor_portion_data`를 사용합니다. 이를 통해 각 부위 또는 부위 그룹에 대해 별도의 값을 정의할 수 있습니다:

```json
"armor_portion_data": [
    {
        "covers": [ "torso" ],
        "coverage": 95,
        "encumbrance": 15
    },
    {
        "covers": [ "arms", "legs" ],
        "coverage": 80,
        "encumbrance": 10,
        "max_encumbrance": 20
    }
]
```

`armor_portion_data` 배열의 각 항목 필드:

- `covers`: 이 항목이 적용되는 신체 부위 ID 배열 (예: "torso", "head", "eyes", "mouth", "arms", "hands", "legs", "feet")
- `coverage`: 신체 부위 면적을 덮는 백분율 (0-100). 값이 높을수록 보호 성능이 높음
- `encumbrance`: 해당 부위에 대한 동작 제한 값. 기본값은 0
- `max_encumbrance`: 수납 용량이 가득 찼을 때의 동작 제한 값. 기본값은 `encumbrance`와 같음

`armor_portion_data`를 사용하는 경우, 최상위 `covers`, `coverage`, `encumbrance`, `max_encumbrance` 필드는 사용하지 않으며 부위 데이터로 대체됩니다.

### 펫 방어구

펫 방어구는 다음과 같이 정의할 수 있습니다:

```json
"type" : "PET_ARMOR",     // 펫 방어구로 정의합니다.
...                   // 일반 아이템과 공통되는 항목
                      // 추가적인 펫 방어구 특정 항목:
"storage" : 0,        // (선택사항, 기본값 = 0) 추가되는 수납 슬롯 수
"environmental_protection" : 0,  // (선택사항, 기본값 = 0) 환경 보호 수준
"material_thickness" : 1,  // 재료 두께(대략 밀리미터 단위). 일반적으로 1~5, 특수 방어구는 10 이상도 가능
"pet_bodytype": "dog",       // 이 방어구가 맞는 펫의 신체 타입. MONSTERS.md 참조
"max_pet_vol": "50 ml",   // 이 방어구에 들어갈 수 있는 펫의 최대 크기. ml 또는 L 단위 사용 가능 - "50 ml" 또는 "2 L"
"min_pet_vol": "50 ml",   // 이 방어구에 들어갈 수 있는 펫의 최소 크기. ml 또는 L 단위 사용 가능 - "50 ml" 또는 "2 L"
"power_armor" : false, // 파워 아머 여부(특수 취급됨)
```

또는 모든 아이템(책, 도구, 총, 심지어 음식도)은 pet_armor_data를 가지면 펫 방어구로 사용할 수 있습니다:

```json
"type" : "TOOL",      // 또는 다른 아이템 타입
...                   // 해당 타입에 맞는 공통 항목(예: 도구)
"pet_armor_data" : {      // 추가로 위와 같은 펫 방어구 데이터 항목
    "storage" : 0,
    "environmental_protection" : 0,
    "pet_bodytype": "dog",
    "max_pet_vol": "35000 ml",
    "min_pet_vol": "25000 ml",
    "material_thickness" : 1,
    "power_armor" : false
}
```

자세한 내용은 영문 문서를 참조하세요: [Item Creation / Types](../../en/mod/json/reference/items/item_creation.md)

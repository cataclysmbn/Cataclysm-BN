# 차량

> [!NOTE]
>
> 이 페이지는 현재 작성 중이며 최근 `JSON INFO`에서 분리되었습니다

### 차량 부품

차량에 설치되는 차량 구성 요소.

```json
"id": "wheel",                // 고유 식별자
"name": "wheel",              // 표시 이름
"symbol": "0",                // 부품이 작동할 때 표시되는 ASCII 문자
"looks_like": "small_wheel",  // 이 부품에 타일이 없으면 타일셋에 대한 힌트, looks_like 타일을 사용
"color": "dark_gray",         // 부품이 작동할 때 사용되는 색상
"broken_symbol": "x",         // 부품이 고장났을 때 표시되는 ASCII 문자
"broken_color": "light_gray", // 부품이 고장났을 때 사용되는 색상
"damage_modifier": 50,        // (선택사항, 기본값 = 100) 이 부품이 무언가를 칠 때 가해지는 피해 배율, 백분율. 높을수록 = 피격 생물에게 더 많은 피해
"durability": 200,            // 부품이 고장나기 전에 받을 수 있는 피해량
"description": "A wheel."     // 설치할 때 이 차량 부품에 대한 설명
"size": 2000                  // 차량 부품에 "FLUIDTANK" 플래그가 있으면 mL 단위의 용량, 그렇지 않으면 4로 나눈 값이 공간의 부피
"wheel_width": 9,             /* (선택사항, 기본값 = 0)
                               * 특별: 부품은 다음 필드 중 최대 하나만 가질 수 있습니다:
                               *    wheel_width = 인치 단위의 기본 휠 너비
                               *    size        = 트렁크/박스 저장 부피 용량
                               *    power       = 와트 단위의 기본 엔진 출력
                               *    bonus       = 부여되는 보너스; 머플러 = 소음 감소%, 안전벨트 = 차량에서 튕겨나가지 않을 보너스
                               *    par1        = 헤드라이트의 빛 강도와 같은 고유 보너스에 사용되는 일반 값 */
"wheel_type":                 // (선택사항: standard, off-road)
"contact_area":               // (선택사항) 차량 지면 압력에 영향
"cargo_weight_modifier": 33,  // (선택사항, 기본값 = 100) 설정된 백분율로 화물 무게 수정
"weight_modifier": 33,        // (선택사항, 기본값 = 100) 설정된 백분율로 기본 부품 무게 수정
"fuel_type": "NULL",          // (선택사항, 기본값 = "NULL") 부품이 소비하는 연료/탄약의 타입, 아이템 id로

"item": "wheel",              // 이 부품을 설치하는 데 사용되는 아이템, 이 부품을 제거할 때 얻는 아이템
"difficulty": 4,              // 이 부품을 설치하려면 역학 스킬이 최소한 이 레벨이어야 합니다
"breaks_into" : [             // 차량 부품이 파괴되면, 이 아이템 그룹의 아이템들이 지면의 부품 주변에 생성됩니다 (ITEM_SPAWN.md 참조).
  {"item": "scrap", "count": [0,5]} // 배열 대신, 인라인 아이템 그룹일 수 있으며,
],
"breaks_into" : "some_item_group", // 또는 아이템 그룹의 id만 가능합니다.
	"flags": [                    // 부품과 관련된 플래그
	     "EXTERNAL", "MOUNT_OVER", "WHEEL", "MOUNT_POINT", "VARIABLE_SIZE"
	],
	"rotating_light": {           // 선택 사항. 일반 조명 플래그 대신 회전 방향 원뿔 조명을 내는 조명 부품용
	  "arc": 30,                  // 원뿔 폭(도). 기본값 30
	  "step": 90,                 // 주기마다 진행하는 각도. 음수면 반대 방향으로 회전. 기본값 90
	  "phase": 0,                 // 초기 방향 오프셋(도). 기본값 0
	  "period": "1 turns",        // 회전 단계 사이의 시간. 정수 턴 수도 가능. 기본값 1턴
	  "beams": 2                  // 동시에 표시할 균일 간격 광선 수. 기본값 2
	},
	"damage_reduction" : {        // 아래 설명된 대로 피해의 평면 감소. 지정되지 않으면 0으로 설정
    "all" : 10,
    "physical" : 5
},
                              // 다음 선택 필드는 엔진에만 해당됩니다.
"m2c": 50,                    // ENGINE 플래그를 가진 부품에 필수 필드, 순항 출력 대 최대 출력 비율을 나타냄
"backfire_threshold": 0.5,    // 선택 필드, 기본값 0. 역화를 일으키는 손상 HP 대 최대 HP의 최대 비율
"backfire_freq": 20,          // 역화 임계값 > 0이 아니면 선택 필드, 그러면 필수, 기본값 0. 역화의 X 중 1 확률.
"noise_factor": 15,           // 선택 필드, 기본값 0. 엔진 출력에 이 숫자를 곱하여 소음을 선언합니다.
"damaged_power_factor": 0.5,  // 선택 필드, 기본값 0. 0보다 크면, 손상 시 출력은 power * ( damaged_power_factor + ( 1 - damaged_power_factor ) * ( damaged HP / max HP )로 스케일됩니다
"muscle_power_factor": 0,     // 선택 필드, 기본값 0. 0보다 크면, 생존자의 힘 8 이상의 각 포인트는 엔진에 이만큼의 출력을 추가하고, 8 미만의 각 포인트는 이만큼의 출력을 제거합니다.
"exclusions": [ "souls" ]     // 선택 필드, 기본값은 비어있음. 단어 목록. 차량의 엔진이 exclusions의 단어를 공유하면 새 엔진을 차량에 설치할 수 없습니다.
"fuel_options": [ "soul", "black_soul" ] // 선택 필드, 기본값은 fuel_type. 단어 목록. 엔진은 fuel_options의 모든 연료 타입으로 연료를 공급받을 수 있습니다. 제공되면 fuel_type을 재정의하고 fuel_type의 연료를 포함해야 합니다.
"comfort": 3,                 // 선택 필드, 기본값 0. 이 지형/가구가 얼마나 편안한지. 그 위에서 잠들 수 있는 능력에 영향. (uncomfortable = -999, neutral = 0, slightly_comfortable = 3, comfortable = 5, very_comfortable = 10)
"floor_bedding_warmth": 300,  // 선택 필드, 기본값 0. 수면에 사용할 때 이 지형/가구가 제공하는 보너스 온기.
"bonus_fire_warmth_feet": 200,// 선택 필드, 기본값 300. 근처 불로부터 발에 받는 온기 증가.
"height": 5,                  // 선택 필드, 미터 단위의 풍선 높이 (즉, 양력의 배수)
"lift_coff": 0.5,             // 선택 필드, 날개 효과의 배수
"propeller_diameter": 0.5,    // 선택 필드, 프로펠러의 직경
"length": 3,                  // 선택 필드, 사다리의 z-레벨 길이
"default_color": "#622625"    // 선택 필드, 다른 색조가 정의되지 않았을 때의 기본 색조
```

### 통합 도구

```json
"integrated_tools": [ "foo" ],
```

이 차량 부품이 제작 목적으로 제공할 도구의 선택 배열, 가구의 `crafting_pseudo_item`과 비교 대조하세요. 작동하려면 차량 부품에 `CRAFTING` 플래그가 있어야 합니다.

대부분의 레거시 제작 차량 부품 플래그는 제거되었으며 동등한 도구로 교체되어야 합니다. 검사 시 특정 기능을 제공하는 `WATER_PURIFIER`, `FAUCET`, `WATER_FAUCET` 플래그는 유지되었습니다.

```json
"integrated_tools": [ "pot", "pan", "hotplate" ],  // `KITCHEN` 플래그 대체
"integrated_tools": [ "dehydrator", "vac_sealer", "food_processor", "press" ],  // `CRAFTRIG` 플래그 대체
"integrated_tools": [ "chemistry_set", "electrolysis_kit" ],  // `CHEMLAB` 플래그 대체
"integrated_tools": [ "forge" ],  // `FORGE` 플래그 대체
"integrated_tools": [ "fake_adv_butchery" ],  // `BUTCHER_EQ` 플래그와 함께 필요
"integrated_tools": [ "kiln" ],  // `KILN` 플래그 대체
"integrated_tools": [ "soldering_iron", "welder" ],  // 도구 대체
"integrated_tools": [ "water_purifier" ],  // 도구를 대체하지만, `WATER_PURIFIER` 플래그의 차량 탱크에서 물을 정화하는 능력은 대체하지 않음
```

### 변환기

`CONVERTER` 플래그가 있는 차량이 연료 탱크 안에서 한 액체를 다른 액체로 변환하도록 하는 필드입니다.
아이템이나 주변 액체 공급원에는 사용할 수 없습니다.

```json
"converter": {
  "input": "input_itype_id",   // 입력되는 itype
  "input_step": 3,               // 반복마다 소비되는 입력량
  "output": "output_itype_id",  // 출력되는 itype
  "output_step": 1,              // 반복마다 소비되는 출력량
  "max_steps": 10,               // 분당 최대 반복 횟수
  "charge_cost": 100             // 반복마다 필요한 kJ(차량에서 사용하는 최소 단위)
}
```

### 부품 저항

```json
"all" : 0.0f,        // 모든 저항의 초기 값, 더 구체적인 타입에 의해 재정의됨
"physical" : 10,     // bash, cut, stab의 초기 값
"non_physical" : 10, // acid, heat, cold, electricity, biological의 초기 값
"biological" : 0.2f, // 특정 타입에 대한 저항. 이것들은 일반적인 것들을 재정의합니다.
"bash" : 3,
"cut" : 3,
"acid" : 3,
"stab" : 3,
"heat" : 3,
"cold" : 3,
"electric" : 3
```

### 형태

서로 다른 스프라이트를 가진 차량 부품의 copy-from 버전을 자동으로 만드는 필드입니다.
추상 부품에서 올바른 객체를 자동으로 생성합니다.

참고: 이 필드는 copy-from을 지원하지 않으므로 각 객체에 정의해야 합니다.

```jsonc
"shapes": [
  {
    "direction": "left",     // ID에 추가되는 문자열. wing_metal은 wing_metal_left가 되며 looks_like에도 추가됩니다.
    "symbol": "y",           // 위의 symbol 정의와 동일
    "broken_symbol": "y",    // 위의 broken_symbol 정의와 동일
    "looks_like": "board_nw" // 위의 looks_like 정의와 동일하며 direction으로 인한 변경을 덮어씀
  },
]
```

### 차량

`vehicles_spawning.md`도 참조하세요

```json
"id": "shopping_cart",                     // 내부적으로 사용되는 이름.
"name": "Shopping Cart",                   // 표시 이름, i18n 적용 대상.
"blueprint": "#",                          // 차량의 미리보기 - 코드에서 무시되므로, 문서로만 사용
"parts": [                                 // 부품 목록
    {"x": 0, "y": 0, "part": "box"},       // 부품 정의, 양의 x 방향은 왼쪽, 양의 y는 오른쪽
    {"x": 0, "y": 0, "part": "casters"}    // 부품 id는 vehicle_parts.json 참조
]
                                           /* 중요! 차량 부품은 게임에서 설치하는 것과 같은 순서로 정의되어야 합니다
                                            * (즉, 프레임과 마운트 포인트가 먼저).
                                            * 일반적인 설치 규칙을 위반할 수도 없습니다
                                            * (스택 불가능한 부품 플래그를 스택할 수 없습니다). */
```

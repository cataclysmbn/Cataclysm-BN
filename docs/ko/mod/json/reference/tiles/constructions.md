---
title: 건설 정보
---

> [!NOTE]
>
> 이 문서는 최근 `JSON INFO`에서 분리되었으며 추가 작업이 필요할 수 있습니다

### 건설

```json
"id": "constr_pit_spiked",                                          // 건설의 식별자
"group": "spike_pit",                                               // 건설 그룹, UI에서 설명을 제공하고 관련 건설을 그룹화하는 데 사용됩니다(예: 일부 건설의 다른 단계).
"category": "DIG",                                                  // 건설 카테고리
"required_skills": [ [ "survival", 1 ] ],                           // 건설을 수행하는 데 필요한 스킬 레벨
"time": "30 m",                                                     // 건설을 완료하는 데 필요한 시간. 정수는 분으로 읽히거나 시간 문자열을 사용할 수 있습니다.
"components": [ [ [ "spear_wood", 4 ], [ "pointy_stick", 4 ] ] ],   // 건설에 사용되는 아이템
"using": [ [ "welding_standard", 5 ] ],                             // (선택적) 사용할 외부 요구 사항
"pre_note": "I am a dwarf and I'm digging a hole!",                 // (선택적) 주석
"pre_flags": [ "FLAT" ],                                // (선택적) 지형이 건설되기 위해 가져야 하는 플래그
"needs_diggable": true,                                          // (선택적) 사전 지형이 파낼 수 있어야 하는지 여부
"pre_terrain": "t_pit",                                             // (선택적) 건설을 위해 필요한 지형
"pre_furniture": "f_sandbag_half",                                  // (선택적) 건설을 위해 필요한 가구
"pre_special": "check_down_OK",                                     // (선택적) 하드코딩된 타일 유효성 검사, construction.cpp 참조
"post_terrain": "t_pit_spiked",                                     // (선택적) 건설 완료 후 지형 타입
"post_furniture": "f_sandbag_wall",                                 // (선택적) 건설 완료 후 가구 타입
"post_special": "done_dig_stair",                                   // (선택적) 하드코딩된 완료 함수, construction.cpp 참조
"post_flags": [ "keep_items" ],                                     // (선택적) 추가 하드코딩 효과, construction.cpp 참조. 2022년 9월 기준으로 사용 가능한 유일한 것은 `keep_items`입니다
"byproducts": [ { "item": "pebble", "charges": [ 3, 6 ] } ],        // (선택적) 건설 부산물
"vehicle_start": false,                                             // (선택적, 기본값 false) 차량을 생성할지 여부(하드코드 목적)
"on_display": false,                                                // (선택적, 기본값 true) 플레이어 UI에 표시할지 여부
"dark_craftable": true,                                             // (선택적, 기본값 false) 어둠 속에서 건설할 수 있는지 여부
```

### 건설 그룹

```json
"id": "build_wooden_door",            // 그룹 식별자
"name": "Build Wooden Door",          // 건설 메뉴에 표시되는 설명 문자열
```

### 건설 시퀀스

건설 시퀀스는 기지 캠프 청사진 요구 사항의 자동 계산에 필요합니다. 각 건설 시퀀스는 빈 흙 타일에 지정된 지형이나 가구를 건설하는 데 필요한 단계를 설명합니다. 언제든지 모든 지형이나 가구에 대해 하나의 건설 시퀀스만 정의될 수 있습니다. 편의상 빈 흙 타일에서 시작하는 블랙리스트에 없는 각 건설 레시피는 자동으로 1개 요소 길이의 건설 시퀀스를 생성하지만, 동일한 시퀀스를 생성하는 다른 건설 레시피가 없고 동일한 결과를 가진 명시적으로 정의된 시퀀스가 없는 경우에만 그렇습니다.

```json
"id": "f_workbench",                // 시퀀스 식별자
"blacklisted": false,               // (선택적) 이 시퀀스가 블랙리스트에 있는지 여부
"post_furniture": "f_workbench",    // (선택적) 결과 가구의 식별자
"post_terrain": "t_rootcellar",     // (선택적) 결과 지형의 식별자
"elems": [                          // 건설 식별자 배열.
  "constr_pit",                     //   첫 번째 건설은 빈 흙 타일에서 시작해야 하며,
  "constr_rootcellar"               //   마지막 건설은 post_furniture 또는
]                                   //   post_terrain 중 지정된 것으로 결과를 가져야 합니다.
                                    //   청사진 자동 계산에서 지형/가구를 제외하려면 비워 두세요.
```

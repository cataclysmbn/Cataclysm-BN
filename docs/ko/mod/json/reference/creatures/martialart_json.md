# 무술 & 기술

### 무술

```json
"type" : "martial_art",
"id" : "style_debug",       // 고유 ID. 하나의 연속된 단어여야 하며,
                            // 필요한 경우 밑줄을 사용합니다.
"name" : "Debug Mastery",   // 게임 내 표시 이름
"description": "개발자와 치터만 사용하는 비밀 무술입니다.",    // 게임 내 설명
"initiate": [ "You stand ready.", "%s stands ready." ],     // 플레이어 또는 NPC가 이 무술을 선택할 때 표시되는 메시지
"autolearn": [ [ "unarmed", "2" ] ],     // 충족되면 플레이어에게 무술을 자동으로 가르치는 기술 요구사항 목록
"learn_difficulty": 5,      // "주요 기술"을 기반으로 책에서 스타일을 배우는 난이도
                            // 책을 한 번 읽어서 스타일을 배울 총 확률은 (10 + learn_difficulty - primary_skill) 중 1입니다
"arm_block" : 99,           // 팔 블로킹이 해제되는 비무장 기술 레벨
"leg_block" : 99,           // 다리 블로킹이 해제되는 비무장 기술 레벨
"static_buffs" : [          // 매 턴마다 자동으로 적용되는 버프 목록
    "id" : "debug_elem_resist",
    "heat_arm_per" : 1.0
],
"ondodge_buffs" : [],        // 회피 성공 시 자동으로 적용되는 버프 목록
"onattack_buffs" : [],       // 공격 후 적중 여부와 관계없이 자동으로 적용되는 버프 목록
"onhit_buffs" : [],          // 적중 성공 시 자동으로 적용되는 버프 목록
"onmove_buffs" : [],         // 이동 시 자동으로 적용되는 버프 목록
"onmiss_buffs" : [],         // 빗나감 시 자동으로 적용되는 버프 목록
"oncrit_buffs" : [],         // 크리티컬 시 자동으로 적용되는 버프 목록
"onkill_buffs" : [],         // 적을 죽였을 때 자동으로 적용되는 버프 목록
"techniques" : [            // 이 무술을 사용할 때 사용 가능한 기술 목록
    "tec_debug_slow",
    "tec_debug_arpen"
],
"weapons": [ "tonfa" ],      // 이 무술과 함께 사용할 수 있는 무기 목록
"weapon_category": [ "WEAPON_CAT1" ], // 여기에 있는 카테고리 중 하나를 가진 무기는 이 무술과 함께 사용할 수 있습니다.
"mutation": [ "UNSTYLISH" ] // 이 무술을 사용하기 위해 필요한 변이 목록, 최소 1개 필요
```

### 기술

```json
"id" : "tec_debug_arpen",   // 고유 ID. 하나의 연속된 단어여야 함
"name" : "phasing strike",  // 게임 내 표시 이름
"unarmed_allowed" : true,   // 비무장 캐릭터가 이 기술을 사용할 수 있는가
"unarmed_weapons_allowed" : true,    // 이 기술은 캐릭터가 실제로 비무장이어야 하는가 아니면 비무장 무기를 허용하는가
"weapon_categories_allowed" : [ "BLADES", "KNIVES" ], // 기술을 이러한 무기 카테고리로만 제한합니다. 생략하면 모든 무기 카테고리가 허용됩니다. unarmed_allowed가 true이면 빈 손은 항상 허용됩니다.
"melee_allowed" : true,     // 무술의 무기뿐만 아니라 모든 근접 무기를 사용할 수 있음을 의미합니다
"skill_requirements": [ { "name": "melee", "level": 3 } ],     // 이 기술을 사용하기 위해 필요한 기술과 최소 레벨. 모든 기술이 가능합니다.
"weapon_damage_requirements": [ { "type": "bash", "min": 5 } ],     // 이 기술을 사용하기 위해 필요한 최소 무기 데미지. 모든 데미지 타입이 가능합니다.
"req_buffs": [ "eskrima_hit_buff" ],    // 이 기술은 명명된 버프가 활성화되어 있어야 합니다
"crit_tec" : true,          // 이 기술은 크리티컬 히트에서만 작동합니다
"crit_ok" : true,           // 이 기술은 일반 및 크리티컬 히트 모두에서 작동합니다
"downed_target": true,      // 기술은 쓰러진 대상에게만 작동합니다
"stunned_target": true,     // 기술은 기절한 대상에게만 작동합니다
"human_target": true,       // 기술은 인간형 대상에게만 작동합니다
"knockback_dist": 1,        // 대상이 밀려나는 거리
"knockback_spread": 1,      // 넉백이 대상을 곧바로 뒤로 보내지 않을 수 있습니다
"knockback_follow": 1,      // 공격자는 대상이 밀려나면 따라갑니다
"stun_dur": 2,              // 대상이 기절하는 지속 시간
"down_dur": 2,              // 대상이 쓰러져 있는 지속 시간
"side_switch": true,        // 기술은 대상을 사용자 뒤로 이동시킵니다
"disarms": true,            // 이 기술은 상대를 무장 해제할 수 있습니다
"take_weapon": true,        // 기술은 손이 비어 있으면 대상의 무기를 무장 해제하고 장착합니다
"grab_break": true,         // 이 기술은 사용자에 대한 붙잡기를 깰 수 있습니다
"aoe": "spin",              // 이 기술은 범위 효과를 가집니다; 단독 대상에게는 작동하지 않습니다
"block_counter": true,      // 이 기술은 블록 성공 시 자동으로 반격할 수 있습니다
"dodge_counter": true,      // 이 기술은 회피 성공 시 자동으로 반격할 수 있습니다
"weighting": 2,             // 많은 기술이 사용 가능할 때 이 기술이 선택될 가능성에 영향을 줍니다
"defensive": true,          // 게임은 공격할 때 이 기술을 선택하지 않습니다
"wall_adjacent": true,      // 벽에 인접해야 합니다
"miss_recovery": true,      // 공격 중 빗나가면 더 적은 이동을 사용합니다
"messages" : [              // 플레이어와 NPC가 이 기술을 사용할 때 출력되는 내용
    "You phase-strike %s",
    "<npcname> phase-strikes %s"
]
"movecost_mult" : 0.3,       // 아래 설명된 모든 보너스
"mutations_required": [ "MASOCHIST" ] // 기술을 사용하기 위해 필요한 변이 목록, 최소 1개 필요
```

### 버프

```json
"id" : "debug_elem_resist",         // 고유 ID. 하나의 연속된 단어여야 함
"name" : "Elemental resistance",    // 게임 내 표시 이름
"description" : "+Strength bash armor, +Dexterity acid armor, +Intelligence electricity armor, +Perception fire armor.",    // 게임 내 설명
"buff_duration": 2,                 // 이 버프가 지속되는 턴 단위 지속 시간
"unarmed_allowed" : true,           // 비무장 캐릭터에게 이 버프를 적용할 수 있는가
"melee_allowed" : false,          // 무장한 캐릭터에게 이 버프를 적용할 수 있는가
"weapon_categories_allowed" : [ "BLADES", "KNIVES" ], // 버프를 이러한 무기 카테고리로만 제한합니다. 생략하면 모든 무기 카테고리가 허용됩니다. unarmed_allowed가 true이면 빈 손은 항상 허용됩니다.
"unarmed_weapons_allowed" : true,          // 이 버프는 캐릭터가 실제로 비무장이어야 합니까. true이면 비무장 무기(너클, 펀치 대거)를 허용합니다
"max_stacks" : 8,                   // 버프의 최대 스택 수. 버프 보너스는 현재 버프 강도에 곱해집니다
"bonus_blocks": 1       // 턴당 추가 블록
"bonus_dodges": 1       // 턴당 추가 회피
"flat_bonuses" : [                  // 고정 보너스, 아래 참조
],
"mult_bonuses" : [                  // 곱셈 보너스, 아래 참조
]
```

### 보너스

보너스 배열에는 다음과 같은 여러 보너스 항목이 포함됩니다:

```json
{
  "stat": "damage",
  "type": "bash",
  "scaling-stat": "per",
  "scale": 0.15
}
```

- `"stat"`: 영향을 받는 통계, "hit", "dodge", "block", "speed", "movecost", "damage", "armor", "arpen", "target_armor_multiplier" 중 하나.
- `"type"`: 영향을 받는 통계의 데미지 타입("bash", "cut", "heat" 등), 영향을 받는 통계가 "damage", "armor", "arpen" 또는 "target_armor_multiplier"인 경우에만 필요합니다.
- `"scale"`: 보너스 자체의 값.
- `"scaling-stat"`: 스케일링 스탯, "str", "dex", "int", "per" 중 하나. 선택 사항. 스케일링 스탯이 지정되면 보너스 값에 해당 사용자 스탯이 곱해집니다.

보너스는 올바른 순서로 작성해야 합니다.

`useless` 타입의 토큰은 오류를 일으키지 않지만 효과가 없습니다. 예를 들어, 기술의 `speed`는 효과가 없습니다(기술에는 `movecost`를 사용해야 함).

현재 추가 원소 데미지는 적용되지 않지만 추가 원소 방어구는 적용됩니다(일반 방어구 후).

예제: 들어오는 타격 데미지는 근력 값의 30%만큼 감소합니다. 버프에서만 유용합니다:

- `flat_bonuses : [ { "stat": "armor", "type": "bash", "scaling-stat": "str", "scale": 0.3 } ]`

모든 베기 데미지는 `(민첩의 10%)*(데미지)`로 곱해집니다:

- `mult_bonuses : [ { "stat": "damage", "type": "cut", "scaling-stat": "dex", "scale": 0.1 } ]`

이동 비용은 근력 값의 100%만큼 감소합니다

- `flat_bonuses : [ { "stat": "movecost", "scaling-stat": "str", "scale": -1.0 } ]`

static_bonuses에서 사용 가능한 추가 필드

```json
"stealthy": true, // 모든 이동이 소음을 덜 냅니다 "quiet": true, // 공격이 완전히 조용합니다
"wall_adjacent": true, // 벽에 인접해야 합니다
"throw_immune": true, // 던져지는 것에 면역입니다
```

### 세계와 캐릭터 생성에 관련 아이템 배치

무술의 시작 특성 선택은 mutations.json에 있습니다. 무술을 올바른 카테고리(자기 방어, 소림 동물 형태, 근접 스타일 등)에 배치하세요.

json/itemgroups/를 사용하여 무술 책과 무술을 위해 만든 모든 무술 무기를 세계의 다양한 위치의 생성에 배치하세요. 무기를 거기에 배치하지 않으면 제작 레시피만 옵션이 됩니다.

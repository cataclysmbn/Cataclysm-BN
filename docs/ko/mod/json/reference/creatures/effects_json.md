# 효과 (Effects)

## 게임 내에서 효과를 부여하는 방법은?

### 소모품 (Comestibles)

게임 내에서 플레이어에게 효과를 부여하는 첫 번째 방법은 약물 시스템을 통하는 것입니다. 이를 위해 아이템에는 "consume_drug" 타입의 use_action이 있어야 합니다.

```json
"use_action" : {
    "type" : "consume_drug",
    "activation_message" : "You take some oxycodone.",
    "effects" : [
        {
            "id": "pkill3",
            "duration": 20
        },
        {
            "id": "pkill2",
            "duration": 200
        }
    ]
},
```

"effects" 필드에 주목하세요. 각 효과에는 네 가지 잠재적 필드가 있습니다:

```json
"id" - 필수
"duration" - 필수
"bp" - 이 효과가 특정 신체 부위를 대상으로 하도록 합니다
```

유효한 "bp" 항목은 다음과 같습니다 (항목이 없으면 효과가 대상 지정되지 않음):

```json
"torso"
"head"
"eyes"
"mouth"
"arm_l"
"arm_r"
"hand_l"
"hand_r"
"leg_l"
"leg_r"
"foot_l"
"foot_r"
```

### 생물 공격 (Creature attacks)

생물에는 아이템의 "consume_drug" 항목과 유사한 효과 필드가 있습니다. 생물에 "attack_effs" 항목을 추가하여 생물의 공격이 효과를 적용하도록 만들 수 있습니다.

```json
"attack_effs": [
    {
        "//": "applying this multiple times makes intensity go up by 3 instead of 1",
        "id": "paralyzepoison",
        "duration": 33
    },
    {
        "id": "paralyzepoison",
        "duration": 33
    },
    {
        "id": "paralyzepoison",
        "duration": 33
    }
],
```

"attack_effs"의 필드는 "consume_drug"의 필드와 동일하게 작동합니다. 그러나 생물에는 추가 필드가 있습니다:

```json
"chance" - 좋은 타격 시 효과가 적용될 백분율 확률, 기본값은 100%
```

생물이 플레이어에게 성공적으로 피해를 입히고 확률 굴림이 성공하면 나열된 모든 효과를 플레이어에게 적용합니다. 효과는 하나씩 추가됩니다.

### 변이 (Mutations)

변이는 "enchantments" 필드로 효과를 부여할 수 있으며, 다음 변이는 특수 공격을 부여합니다. 잉크샘 변이 후 제공되는 인챈트는 플레이어의 방어구 값에 영구적으로 영향을 줍니다.

```json
{
    "type": "mutation",
    "id": "INK_GLANDS",
    "name": { "str": "Ink glands" },
    "points": 1,
    "visibility": 1,
    "ugliness": 1,
    "description": "Several ink glands have grown onto your torso.  They can be used to spray defensive ink and blind an attacker in an emergency, as long as the torso isn't covered.",
    "enchantments": [ "MEP_INK_GLAND_SPRAY" ],
    "category": [ "CEPHALOPOD" ]
  },
  {
    "type": "enchantment",
    "id": "ENCH_BIO_CARBON",
    "condition": "ALWAYS",
    "values": [
      { "value": "ARMOR_BASH", "multiply": -0.05 },
      { "value": "ARMOR_CUT", "multiply": -0.1 },
      { "value": "ARMOR_STAB", "multiply": -0.08 },
      { "value": "ARMOR_BULLET", "multiply": -0.15 }
    ]
  }
```

## 필수 필드

```json
"type": "effect_type",      - 필수
"id": "xxxx"                - 고유해야 함
```

## 선택적 필드

### 최대 강도

```json
"max_intensity": 3          - 많은 이후 필드에 사용됨, 기본값은 1
"max_effective_intensity"   - 효과를 적용할 강도 레벨의 수.
                              다른 강도 레벨은 지속 시간만 증가시킵니다.
```

### 이름

```json
"name": ["XYZ"]
또는
"name": [
    "ABC",
    "XYZ",
    "123"
]
```

`"max_intensity" > 1`이고 `"name"`의 항목 수 `>= "max_intensity"`인 경우 적절한 강도 이름을 사용하려고 시도합니다. 이 경우 강도 1은 "ABC"라는 이름을, 2는 "XYZ"를, 3은 "123"을 부여합니다. `"max_intensity" == 1`이거나 "name"의 항목 수가 "max_intensity"보다 적은 경우, 현재 강도 > 1이면 첫 번째 항목 다음에 대괄호 안의 강도를 사용합니다, 즉 `"ABC", "ABC [2]", "ABC [3]"`. "name"의 원하는 항목이 빈 문자열 ("")이거나 "name"이 누락된 경우 효과는 상태 화면에서 플레이어에게 표시되지 않습니다.

"name"의 각 항목에는 선택적 컨텍스트도 있을 수 있습니다:

```json
"name": [ { "ctxt": "ECIG", "str": "Smoke" } ]
```

이 경우, 게임은 주어진 컨텍스트 "ECIG"로 이름을 번역하며, 이를 통해 동사 "Smoke"와 명사 "Smoke"를 다른 언어에서 구별할 수 있습니다.

```json
"speed_name" : "XYZ"        - 기본값은 첫 번째 이름 값
```

이것은 플레이어 속도의 수정자 목록에 사용되는 값입니다. 존재하지 않으면 "name"의 첫 번째 항목을 기본값으로 사용하며, 둘 다 존재하지 않거나 "speed_name"이 빈 문자열 ("")이면 플레이어 속도의 수정자 목록에 나타나지 않습니다(효과가 여전히 영향을 미칠 수 있음).

### 설명

```json
"desc": ["XYZ"]
또는
"desc": [
    "ABC",
    "XYZ",
    "123"
]
```

설명은 사용할 것을 선택할 때 이름 필드와 동일하게 작동합니다. 일반적으로 설명은 1줄만 되어야 합니다. 스탯과 효과는 포함할 필요가 없으며 다른 효과 데이터에서 자동으로 생성됩니다. 설명 줄이 빈 문자열 ("")인 경우 효과 설명에 스탯 변경만 표시됩니다.

설명에는 수정자 역할을 할 수 있는 두 번째 필드도 있습니다:

```json
"part_descs": true      - 존재하지 않으면 기본값은 false
```

"part_descs" == true이면 설명 앞에 "Your X"가 붙으며, 여기서 X는 신체 부위 이름입니다. 즉, 이전 설명은 "Your left arm ABC"로 표시됩니다.

설명에는 축소된 형태도 있을 수 있습니다:

```json
"reduced_desc": ["XYZ"]
또는
"reduced_desc": [
    "ABC",
    "XYZ",
    "123"
]
```

이것은 효과가 감소될 때 사용될 설명입니다. 기본적으로 존재하지 않으면 일반 설명을 사용합니다.

### 등급

```json
"rating": "good"        - 누락된 경우 기본값은 "neutral"
```

이것은 효과가 적용되고 제거될 때 메시지가 표시되는 방식에 사용됩니다. 또한 이것은 "blood_analysis_description" (아래 참조) 필드에 영향을 줍니다: "good" 등급의 효과는 녹색으로 표시되고, 다른 등급의 효과는 캐릭터가 어떤 수단으로 혈액 분석을 수행할 때 빨간색으로 표시됩니다. 유효한 항목은:

```json
"good"
"neutral"
"bad"
"mixed"
```

### Looks_like

```json
"looks_like": "drunk"
```

"looks_like" 필드가 존재하는 경우, 효과는 지정된 태그(예: "drunk")와 시각적으로 유사하게 보입니다.
이것은 NPC 또는 캐릭터 위의 효과 모양에만 영향을 주며, 기계적 동작에는 영향을 주지 않습니다.

### 메시지

```json
"apply_message": "message",
"remove_message": "message"
```

"apply_message" 또는 "remove_message" 필드가 존재하는 경우, 효과의 추가 또는 제거 시 해당 메시지가 표시됩니다. 참고: "apply_message"는 효과가 추가될 때만 표시되며, 현재 효과를 단순히 증가시키는 경우에는 표시되지 않습니다(따라서 새로운 물림만 등).

### 추모 로그

```json
"apply_memorial_log": "log",
"remove_memorial_log": "log"
```

"apply_memorial_log" 또는 "remove_memorial_log" 필드가 존재하는 경우, 게임은 효과의 추가 또는 제거 시 해당 메시지를 추모 로그에 추가합니다. 메시지 필드와 유사하게 "apply_memorial_log"는 새로운 효과 추가에 대해서만 로그에 추가됩니다.

### 저항

```json
"resist_trait": "NOPAIN",
"resist_effect": "flumed"
```

이 필드들은 효과가 저항되고 있는지 여부를 결정하는 데 사용됩니다. 플레이어가 일치하는 특성이나 효과를 가지고 있으면 효과를 "저항"하고 있으며, 이는 효과와 설명을 변경합니다. 효과는 한 번에 하나의 "resist_trait"와 하나의 "resist_effect"만 가질 수 있습니다.

### 효과 제거

```json
"removes_effects": ["bite", "flu"]
```

이 필드는 효과가 존재하는 경우 나열된 효과의 다른 복사본을 자동으로 제거하도록 합니다. 위의 예에서 배치된 효과는 플레이어가 가진 물림 상처나 독감을 자동으로 치료합니다. 여기의 모든 값은 "blocks_effects"에도 자동으로 계산되므로 거기에 중복할 필요가 없습니다.

### 효과 차단

```json
"blocks_effects": ["cold", "flu"]
```

이 필드는 효과가 나열된 효과의 배치를 방지하도록 합니다. 위의 예에서 효과는 플레이어가 감기나 독감에 걸리는 것을 방지합니다(하지만 진행 중인 감기나 독감을 치료하지는 않습니다). "removes_effects"에 있는 모든 효과는 자동으로 "blocks_effects"에 추가되므로 수동으로 중복할 필요가 없습니다.

### 효과 제한자

```json
"max_duration": 100,
"dur_add_perc": 150     - 기본값은 100%
```

이들은 현재 기존 효과에 추가할 때 사용됩니다. "max_duration"은 효과의 전체 지속 시간을 제한합니다. "dur_add_perc"는 기존 효과에 추가하기 위한 일반 지속 시간의 백분율 값입니다. 예:

1. 플레이어에게 100 틱 동안 효과 A를 추가합니다.
2. 플레이어에게 다시 100 틱 동안 효과 A를 추가합니다. 위의 예에서 "dur_add_perc" = 150이므로, 두 번째 추가는 100 * 150% = 150 틱의 총합을 추가하여, 두 번째에서 총 250 틱의 값을 얻습니다. 이것은 100% 미만일 수도 있으며, 심지어 음수일 수도 있어, 향후 적용이 남은 전체 시간을 감소시킬 수 있습니다.

### 강도

강도는 효과의 효과, 이름 및 설명을 제어하는 데 사용됩니다. 다음과 같이 정의됩니다:

```json
"int_add_val": 2        - 기본값은 0! 이것은 변경하지 않으면 향후 적용이 강도를 증가시키지 않는다는 것을 의미합니다!
그리고/또는
"int_decay_step": -2,    - 기본값은 -1
"int_decay_tick": 10
또는
"int_dur_factor": 700
```

첫 번째 값은 이미 존재하는 효과에 추가할 때 강도가 증가될 양입니다. 예:

1. 플레이어에게 효과 A를 추가합니다
2. 플레이어에게 다시 효과 A를 추가합니다 "int_add_val" = 2이므로, 두 번째 추가는 효과 강도를 1에서 1 + 2 = 3으로 변경합니다. 참고: 강도가 무언가를 하려면 3개의 강도 데이터 세트 중 적어도 하나가 있어야 합니다!

"int_decay_step"과 "int_decay_tick"은 무언가를 하려면 서로가 필요합니다. 둘 다 존재하는 경우 게임은 "int_decay_tick" 틱마다 "int_decay_step"만큼 현재 효과 강도를 자동으로 증가시키며, 결과를 `[1, "max_intensity"]`로 제한합니다. 이것은 시간이 지남에 따라 효과가 자동으로 강도를 증가하거나 감소하도록 만드는 데 사용할 수 있습니다.

"int_dur_factor"는 다른 세 개의 강도 필드를 재정의하고, 강도를 intensity = duration / "int_dur_factor" 올림으로 정의된 숫자로 강제합니다(따라서 0에서 "int_dur_factor"까지가 강도 1입니다).

### 영속성

영속적인 효과는 시간이 지나도 지속 시간을 잃지 않습니다. 즉, 지속 시간이 1 턴이더라도 제거될 때까지 지속됩니다.

```json
"permanent": true
```

### 빗나감 메시지

```json
"miss_messages": [["Your blisters distract you", 1]]
또는
"miss_messages": [
    ["Your blisters distract you", 1],
    ["Your blisters don't like you", 10],
]
```

이것은 효과가 적용되는 동안 주어진 확률로 다음 빗나감 메시지를 추가합니다.

### 감쇠 메시지

```json
"decay_messages": [["The jet injector's chemicals wear off.  You feel AWFUL!", "bad"]]
또는
"decay_messages": [
    ["The jet injector's chemicals wear off.  You feel AWFUL!", "bad"],
    ["OOGA-BOOGA.  You feel AWFUL!", "bad"],
]
```

메시지는 강도와 일치하므로 첫 번째 메시지는 강도 1용이고, 두 번째는 강도 2용 등입니다. 메시지는 감쇠 틱 또는 "int_dur_factor"를 통해 더 높은 강도에서 일치하는 강도로 강도가 감소할 때마다 출력됩니다. 따라서 3+ 에서 강도 2로 감쇠되면 플레이어에게 나쁜 메시지로 "OOGA-BOOGA. You feel AWFUL!"을 표시합니다.

### 대상 지정 수정자

```json
"main_parts_only": true     - 기본값은 false
```

이것은 비주요 부위(손, 눈, 발 등)의 모든 효과를 일치하는 주요 부위(팔, 머리, 다리 등)로 자동으로 재대상 지정합니다.

### 효과 수정자

```json
"pkill_addict_reduces": true,   - 기본값은 false
"pain_sizing": true,            - 기본값은 false
"hurt_sizing": true,            - 기본값은 false
"harmful_cough": true           - 기본값은 false
```

"pkill_addict_reduces"는 플레이어의 진통제 중독이 효과가 더 많은 pkill을 줄 확률을 감소시키도록 합니다. "pain_sizing"과 "hurt_sizing"은 큰/거대한 변이가 고통 및 상처 효과 발동 확률에 영향을 주도록 합니다. "harmful_cough"는 이 효과로 인한 기침이 플레이어에게 피해를 줄 수 있음을 의미합니다.

### 사기

```json
"morale": "morale_high"
```

제공되는 사기 효과의 유형. 사기 효과가 있는 경우 필수이며, 그렇지 않으면 지정하면 안 됩니다.

### 제거 시 다른 효과

```json
"effects_on_remove": [
    {
        "intensity_requirement": 0, - 기본값은 0
        "effect_type": "cold",      - (필수) 적용될 효과
        "allow_on_decay": false,    - 기본값은 true
        "allow_on_remove" true,     - 기본값은 false
        "intensity": 5,             - 기본값은 0
        "inherit_intensity": false, - 기본값은 false
        "duration": "10 s",         - 기본값은 0
        "inherit_duration": true,   - 기본값은 true
        "body_part": "hand_r,       - 기본값은 null
        "inherit_body_part": false  - 기본값은 true
    }
]
```

"intensity_requirement"는 현재 효과의 강도가 낮은 경우 새 효과 추가를 방지합니다.
"allow_on_decay"는 부모가 감쇠된 경우(지속 시간 0으로 인해 제거된 경우) 효과 추가를 활성화합니다.
"allow_on_remove"는 플레이어가 지속 시간 0 이전에 부모가 제거된 경우 효과 추가를 활성화합니다.
"inherit_duration", "inherit_intensity" 및 "inherit_body_part"는 관련 변수가 부모 효과에서 복사되도록 합니다.

### 효과의 효과

```json
"base_mods" : {
    arguments
},
"scaling_mods": {
    arguments
}
```

이것이 효과 JSON 정의의 진짜 핵심입니다. 각각은 다양한 인수를 받을 수 있습니다. 소수는 유효하지만 "0.X" 또는 "-0.X"로 형식화되어야 합니다. 게임은 실제 적용 값을 계산할 때 끝에서 0으로 반올림합니다.

기본 정의:

```json
"X_amount"      - 효과가 배치될 때 적용되는 X의 양. 적용 메시지와 같이 새 효과에만 트리거됩니다
"X_min"         - 굴림 트리거 시 적용되는 X의 최소 양
"X_max"         - 굴림 트리거 시 적용되는 X의 최대 양 (항목이 없으면 매번 정확히 X_min을 줍니다 대신 rng(min, max)
"X_min_val"     - 효과가 당신을 밀어낼 최소 값, 0은 무제한을 의미합니다! 일부 X에는 존재하지 않습니다!
"X_max_val"     - 효과가 당신을 밀어낼 최대 값, 0은 무제한을 의미합니다! 일부 X에는 존재하지 않습니다!
"X_chance"      - 매번 X 트리거의 기본 확률, 정확한 공식은 "X_chance_bot"에 따라 다릅니다
"X_chance_bot"  - 존재하지 않으면 트리거 확률은 (1 in "X_chance")입니다. 존재하면 확률은 ("X_chance" in "X_chance_bot")입니다
"X_tick"        - 효과가 Y 틱마다 X 트리거를 굴립니다
```

유효한 인수:

```json
"str_mod"           - 양수 값은 스탯을 높이고, 음수 값은 스탯을 낮춥니다
"dex_mod"           - 양수 값은 스탯을 높이고, 음수 값은 스탯을 낮춥니다
"per_mod"           - 양수 값은 스탯을 높이고, 음수 값은 스탯을 낮춥니다
"int_mod"           - 양수 값은 스탯을 높이고, 음수 값은 스탯을 낮춥니다
"speed_mod"         - 양수 값은 스탯을 높이고, 음수 값은 스탯을 낮춥니다

"pain_amount"       - 양수는 고통을 증가시키고, 음수는 아무것도 하지 않습니다. 너무 높게 만들지 마세요.
"pain_min"          - 특정 효과가 줄/가질 고통의 최소량
"pain_max"          - 0이거나 누락된 경우 값은 정확히 "pain_min"입니다
"pain_max_val"      - 기본값은 0이며, 이는 무제한을 의미합니다
"pain_chance"       - 더 많은 고통을 얻을 확률
"pain_chance_bot"
"pain_tick"         - 기본값은 매 틱마다입니다.

"hurt_amount"       - 양수는 피해를 주고, 음수는 대신 치유합니다. 너무 높게 만들지 마세요.
"hurt_min"          - 특정 효과가 줄/가질 피해의 최소량
"hurt_max"          - 0이거나 누락된 경우 값은 정확히 "hurt_min"입니다
"hurt_chance"       - 피해를 줄 확률
"hurt_chance_bot"
"hurt_tick"         - 기본값은 매 틱마다

"sleep_amount"      - 잠자는 데 소비한 턴 수.
"sleep_min"         - 특정 효과가 줄 수 있는 최소 수면 턴 수
"sleep_max"         - 0이거나 누락된 경우 값은 정확히 "sleep_min"입니다
"sleep_chance"      - 잠들 확률
"sleep_chance_bot"
"sleep_tick"        - 기본값은 매 틱마다

"pkill_amount"      - 진통 효과의 양. 너무 높게 만들지 마세요.
"pkill_min"         - 특정 효과가 줄 진통의 최소량
"pkill_max"         - 0이거나 누락된 경우 값은 정확히 "pkill_min"입니다
"pkill_max_val"     - 기본값은 0이며, 이는 무제한을 의미합니다
"pkill_chance"      - 진통 효과를 일으킬 확률(고통을 줄임)
"pkill_chance_bot"
"pkill_tick"        - 기본값은 매 틱마다

"stim_amount"       - 음수는 진정제 효과를, 양수는 흥분제 효과를 일으킵니다.
"stim_min"          - 특정 효과가 줄 흥분제의 최소량.
"stim_max"          - 0이거나 누락된 경우 값은 정확히 "stim_min"입니다
"stim_min_val"      - 기본값은 0이며, 이는 무제한을 의미합니다
"stim_max_val"      - 기본값은 0이며, 이는 무제한을 의미합니다
"stim_chance"       - 두 흥분제 효과 중 하나를 일으킬 확률
"stim_chance_bot"
"stim_tick"         - 기본값은 매 틱마다

"health_amount"     - 음수는 건강을 감소시키고 양수는 증가시킵니다. 이것은 반-숨겨진 스탯으로, 치유에 영향을 줍니다.
"health_min"        - 특정 효과가 줄/가질 건강의 최소량.
"health_max"        - 0이거나 누락된 경우 값은 정확히 "health_min"입니다
"health_min_val"    - 기본값은 0이며, 이는 무제한을 의미합니다
"health_max_val"    - 기본값은 0이며, 이는 무제한을 의미합니다
"health_chance"     - 건강을 변경할 확률
"health_chance_bot"
"health_tick"       - 기본값은 매 틱마다

"h_mod_amount"      - 건강 스탯 성장에 영향을 주며, 양수는 증가시키고 음수는 감소시킵니다
"h_mod_min"         - 특정 효과가 줄/가질 health_modifier의 최소량
"h_mod_max"         - 0이거나 누락된 경우 값은 정확히 "h_mod_min"입니다
"h_mod_min_val"     - 기본값은 0이며, 이는 무제한을 의미합니다
"h_mod_max_val"     - 기본값은 0이며, 이는 무제한을 의미합니다
"h_mod_chance"      - health_modifier를 변경할 확률
"h_mod_chance_bot"
"h_mod_tick"        - 기본값은 매 틱마다

"rad_amount"        - 줄/가질 수 있는 방사능의 양. [50] 이상은 치명적이라는 점에 유의하세요.
"rad_min"           - 특정 효과가 줄/가질 방사능의 최소량
"rad_max"           - 0이거나 누락된 경우 값은 정확히 "rad_min"입니다
"rad_max_val"       - 기본값은 0이며, 이는 무제한을 의미합니다
"rad_chance"        - 더 많은 방사능을 얻을 확률
"rad_chance_bot"
"rad_tick"          - 기본값은 매 틱마다

"hunger_amount"     - 줄/가질 수 있는 배고픔의 양.
"hunger_min"        - 특정 효과가 줄/가질 배고픔의 최소량
"hunger_max"        - 0이거나 누락된 경우 값은 정확히 "hunger_min"입니다
"hunger_min_val"    - 기본값은 0이며, 이는 무제한을 의미합니다
"hunger_max_val"    - 기본값은 0이며, 이는 무제한을 의미합니다
"hunger_chance"     - 더 배고파질 확률
"hunger_chance_bot"
"hunger_tick"       - 기본값은 매 틱마다

"thirst_amount"     - 줄/가질 수 있는 갈증의 양.
"thirst_min"        - 특정 효과가 줄/가질 갈증의 최소량
"thirst_max"        - 0이거나 누락된 경우 값은 정확히 "thirst_min"입니다
"thirst_min_val"    - 기본값은 0이며, 이는 무제한을 의미합니다
"thirst_max_val"    - 기본값은 0이며, 이는 무제한을 의미합니다
"thirst_chance"     - 더 목마를 확률
"thirst_chance_bot"
"thirst_tick"       - 기본값은 매 틱마다

"sleepdebt_amount"     - 줄/가질 수 있는 수면 부채의 양.
"sleepdebt_min"        - 특정 효과가 줄/가질 수면의 최소량
"sleepdebt_max"        - 0이거나 누락된 경우 값은 정확히 "sleepdebt_min"입니다
"sleepdebt_min_val"    - 기본값은 0이며, 이는 무제한을 의미합니다
"sleepdebt_max_val"    - 기본값은 0이며, 이는 무제한을 의미합니다
"sleepdebt_chance"     - 더 많은 수면을 줄 확률
"sleepdebt_chance_bot" - 최소 확률, 불확실함, 감사가 필요함
"sleepdebt_tick"       - 기본값은 매 틱마다

"fatigue_amount"    - 줄/가질 수 있는 피로의 양. 일정량 이후 캐릭터는 잠을 자야 합니다.
"fatigue_min"       - 특정 효과가 줄/가질 피로의 최소량
"fatigue_max"       - 0이거나 누락된 경우 값은 정확히 "fatigue_min"입니다
"fatigue_min_val"   - 기본값은 0이며, 이는 무제한을 의미합니다
"fatigue_max_val"   - 기본값은 0이며, 이는 무제한을 의미합니다
"fatigue_chance"    - 더 피곤해질 확률
"fatigue_chance_bot"
"fatigue_tick"      - 기본값은 매 틱마다

"stamina_amount"    - 줄/가질 수 있는 스태미나의 양.
"stamina_min"       - 특정 효과가 줄/가질 스태미나의 최소량
"stamina_max"       - 0이거나 누락된 경우 값은 정확히 "stamina_min"입니다
"stamina_min_val"   - 기본값은 0이며, 이는 무제한을 의미합니다
"stamina_max_val"   - 기본값은 0이며, 이는 무제한을 의미합니다
"stamina_chance"    - 스태미나 변경을 얻을 확률
"stamina_chance_bot"
"stamina_tick"      - 기본값은 매 틱마다

"cough_chance"      - 기침을 일으킬 확률
"cough_chance_bot"
"cough_tick"        - 기본값은 매 틱마다

"vomit_chance"      - 구토를 일으킬 확률
"vomit_chance_bot"
"vomit_tick"        - 기본값은 매 틱마다

"healing_rate"      - 하루당 치유율
"healing_head"      - 머리의 치유 값 백분율
"healing_torso"     - 몸통의 치유 값 백분율

"morale"            - 제공되는 사기의 양. 단일 숫자여야 합니다(저항 지원 안 됨).

다음 효과는 몬스터에만 적용됩니다:

"hit_mod"           - 몬스터 melee_skill을 높이거나 낮춤
"dodge_mod"         - 몬스터 회피 등급을 높이거나 낮춤
"bash_mod"          - 기본 공격 및 근접 특수 공격으로 입히는 타격 피해를 높이거나 낮춤, 0으로 낮추면 피해를 입히지 않음
"cut_mod"           - 기본 공격 및 근접 특수 공격으로 입히는 절단 피해를 높이거나 낮춤, 0으로 낮추면 피해를 입히지 않음
"size_mod"          - 몬스터의 크기를 줄이거나 늘림, 몬스터를 작은 것보다 작게 또는 거대한 것보다 크게 만들 수 없음
```

각 인수는 하나 또는 두 개의 값을 받을 수도 있습니다.

```json
"thirst_min": [1]
또는
"thirst_min": [1, 2]
```

효과가 "저항"되는 경우("resist_effect" 또는 "resist_trait"를 통해) 두 번째 값을 사용합니다. 하나의 값만 주어진 경우 항상 그 양을 사용합니다.

Base mods와 Scaling mods: 강도 = 1일 때 효과는 "base_mods"의 기본 효과만 가집니다. 그러나 얻는 각 추가 강도에 대해 각 "scaling_mods"의 값을 계산에 추가합니다. 따라서:

```json
강도 1 값 = base_mods 값
강도 2 값 = base_mods 값 + scaling_mods 값
강도 3 값 = base_mods 값 + 2 * scaling_mods 값
강도 4 값 = base_mods 값 + 3 * scaling_mods 값
```

등등.

특수한 경우: 유일한 특수한 경우는 base_mods의 "X_chance_bot" + intensity * scaling_mods의 "X_chance_bot" = 0이면 1과 같은 것으로 취급한다는 것입니다(즉, 매번 트리거).

## 효과 예제

```json
"type": "effect_type",
"id": "drunk",
"name": [
    "Tipsy",
    "Drunk",
    "Trashed",
    "Wasted"
],
"looks_like": "drunk",
"max_intensity": 4,
"apply_message": "You feel lightheaded.",
"int_dur_factor": 1000,
"miss_messages": [["You feel woozy.", 1]],
"morale": "morale_drunk",
"base_mods": {
    "str_mod": [1],
    "vomit_chance": [-43],
    "sleep_chance": [-1003],
    "sleep_min": [2500],
    "sleep_max": [3500],
    "morale": [ 5 ]
},
"scaling_mods": {
    "str_mod": [-0.67],
    "per_mod": [-1],
    "dex_mod": [-1],
    "int_mod": [-1.42],
    "vomit_chance": [21],
    "sleep_chance": [501],
    "morale": [ 10 ]
}
```

먼저 "drunk"가 플레이어에게 적용될 때 이미 취하지 않은 경우 "You feel lightheaded"라는 메시지를 출력합니다. 또한 적용되는 동안 "You feel woozy" 빗나감 메시지를 추가합니다. "int_dur_factor": 1000이 있으며, 이는 강도가 항상 duration / 1000 올림과 같다는 것을 의미하고, "max_intensity": 4가 있으며, 이는 강도가 도달할 수 있는 최고값이 3000 이상의 지속 시간에서 4라는 것을 의미합니다. 다른 강도를 거치면서 이름이 변경됩니다. 설명은 추가 설명 없이 스탯 변경만 표시합니다.

강도 레벨을 거치면서 효과는 다음과 같습니다:

```json
강도 1
    +1 STR
    +5 사기
    다른 효과 없음 (두 "X_chance"가 모두 음수이기 때문)
강도 2
    1 - .67 = .33 =         0 STR (0으로 반올림)
    0 - 1 =                 -1 PER
    0 - 1 =                 -1 DEX
    0 -1.42 =               -1 INT
    -43 + 21 =              여전히 음수, 따라서 구토 없음
    -1003 + 501 =           여전히 음수, 따라서 기절 없음
    5 + 10 =                15 사기
강도 3
    1 - 2 * .67 = -.34 =    0 STR (0으로 반올림)
    0 - 2 * 1 =             -2 PER
    0 - 2 * 1 =             -2 DEX
    0 - 2 * 1.43 =          -2 INT
    -43 + 2 * 21 = -1       여전히 음수, 구토 없음
    -1003 + 2 * 501 = -1    여전히 음수, 기절 없음
    5 + 2 * 10 =            25 사기
강도 4
    1 - 3 * .67 = - 1.01 =  -1 STR
    0 - 3 * 1 =             -3 PER
    0 - 3 * 1 =             -3 DEX
    0 - 3 * 1.43 =          -4 INT
    -43 + 3 * 21 = 20       "vomit_chance_bot"이 존재하지 않으므로 20분의 1 확률로 구토. "vomit_tick"이 존재하지 않으므로 매 턴마다 굴립니다.
    -1003 + 3 * 501 = 500   "sleep_chance_bot"이 존재하지 않으므로 500분의 1 확률로 rng(2500, 3500) 턴 동안 기절. "sleep_tick"이 존재하지 않으므로 매 턴마다 굴립니다.
    5 + 3 * 10 =            35 사기
```

### 혈액 분석 설명

```json
"blood_analysis_description": "Minor Painkiller"
```

이 설명은 캐릭터가 혈액 분석을 수행할 때(예: Blood Analysis CBM을 통해) 이 필드를 가진 모든 효과에 대해 표시됩니다.

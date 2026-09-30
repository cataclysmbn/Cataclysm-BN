# 인챈트 (`Enchantments`)

인챈트를 사용하면 아이템, 생체공학 또는 돌연변이가 제공하는 사용자 지정 효과를 정의할 수 있습니다.

### 필드 (`Fields`)

#### `id`

(문자열) 이 인챈트의 고유 식별자입니다.

#### `conditions`

(문자열 배열) 인챈트의 활성화 여부를 결정하는 조건입니다.

인챈트가 유효하려면 모든 조건을 통과해야 합니다. 조건이 없으면 자동으로 참이 됩니다.

기본 게임의 모든 값은 [여기](#Basegame-Enchantment-Condition-ID-List)를 참조하세요.

#### `emitter`

(문자열) 이 인챈트가 활성화된 동안 함께 활성화되는 방출기의 식별자입니다. 기본값은 방출기 없음입니다.

#### `ench_effects`

(배열) 이 인챈트가 활성화된 동안 지정된 강도의 효과를 부여합니다.

단일 항목의 구문:

```json
{
  // (필수) 효과 식별자
  "effect": "effect_identifier",

  // (필수) 강도. 실제 강도 단계가 없는 효과에는 1을 사용합니다.
  "intensity": 2
}
```

#### `hit_you_effect`

(배열) 인챈트가 활성화되어 있고 캐릭터가 생물을 근접 공격할 때 시전될 수 있는 주문 목록입니다.

단일 항목의 구문:

```json
{
  // (필수) 주문 식별자
  "id": "spell_identifier",

  // true이면 주문의 중심이 캐릭터의 위치가 됩니다.
  // false이면 주문의 중심이 캐릭터가 공격하는 생물의 위치가 됩니다.
  // 기본값: false
  "hit_self": false,

  // 발동 확률. X번에 한 번입니다.
  // 기본값: 1
  "once_in": 1,

  // 플레이어에게 주문이 발동했을 때 표시할 메시지입니다.
  // %1$s는 플레이어의 이름이고, %2$s는 생물의 이름입니다.
  // 기본값: 메시지 없음
  "message": "You pierce %2$s with Magic Piercing!",

  // NPC에게 주문이 발동했을 때 표시할 메시지입니다.
  // %1$s는 NPC의 이름이고, %2$s는 생물의 이름입니다.
  // 기본값: 메시지 없음
  "npc_message": "%1$s pierces %2$s with Magic Piercing!",

  // TODO: 작동하지 않음?
  "min_level": 1,

  // TODO: 작동하지 않음?
  "max_level": 2
}
```

#### `hit_me_effect`

(배열) 인챈트가 활성화되어 있고 캐릭터가 생물의 근접 공격을 받을 때 시전될 수 있는 주문 목록입니다.

구문은 `hit_you_effect`와 같습니다.

#### `mutations`

(배열) 인챈트가 활성화된 동안 일시적으로 부여되는 돌연변이 목록입니다.

#### `intermittent_activation`

(객체) 인챈트가 활성화된 동안 무작위로 발생하는 효과를 지정하는 규칙입니다.

구문:

```json
{
  // 인챈트가 활성화된 동안 매 턴 실행할 검사 목록입니다.
  "effects": [
    {
      // 평균 발동 빈도입니다.
      // 검사를 통과할 정확한 확률은 매 턴 "1 / (X를 턴으로 환산한 값)"입니다.
      "frequency": "5 minutes",

      // 검사를 통과했을 때 시전할 주문 목록입니다.
      "spell_effects": [
        {
          // (필수) 주문 식별자
          "id": "nasty_random_effect",

          // TODO: 작동하지 않음?
          "min_level": 1,

          // TODO: 작동하지 않음?
          "max_level": 5
          // TODO: 다른 필드도 불러오는 것으로 보이지만 사용하지 않습니다.
        }
      ]
    }
  ]
}
```

#### `values`

(배열) 변경할 기타 캐릭터/아이템 값 목록입니다.

단일 항목의 구문:

```json
{
  // (필수) 변경할 값 ID입니다. 아래 목록을 참조하세요.
  "value": "VALUE_ID_STRING",

  // 덧셈 보너스입니다. 선택적 정수이며 기본값은 0입니다.
  // 다음 값에는 적용되지 않습니다.
  // METABOLISM, MANA_REGEN, STAMINA_CAP, STAMINA_REGEN, THIRST, FATIGUE
  "add": 13,

  // 곱셈 보너스입니다. 선택 사항이며 기본값은 0입니다.
  "multiply": -0.3
}
```

덧셈 보너스와 곱셈 보너스는 다음과 같이 별도로 적용됩니다.

```json
bonus = add + base_value * multiply
```

따라서 `multiply` 값이 -0.8이면 -80%, 2.5이면 +250%입니다. 정수 값을 변경할 때는 최종 보너스를 0 방향으로 반올림합니다(소수 부분을 버립니다).

여러 인챈트(예: 아이템의 인챈트 하나와 생체공학의 인챈트 하나)가 같은 값을 변경하면, 각 보너스를 반올림하지 않고 모두 더한 뒤 필요할 경우 그 합을 반올림하여 기본값에 적용합니다.

캐릭터가 동시에 보유할 수 있는 인챈트 수에는 제한이 없으므로, 의도하지 않은 동작을 막기 위해 최종 계산값에는 하드코딩된 한계가 적용됩니다.

기본 게임의 모든 값은 [여기](#Basegame-Enchantment-Value-ID-List)를 참조하세요.

#### 플래그 (`Flags`)

(배열) `enchantment_flag_id` 값입니다.

기본 게임의 모든 값은 [여기](#Basegame-Enchantment-Flag-ID-List)를 참조하세요.

#### 시야 (`Vision`)

(배열) `enchantment_vision_id` 값입니다.

정의 방법은 [여기](#Enchantment-Vision)를 참조하세요.

#### 면역 효과 (`Immune Effects`)

(배열) `effect_type_id` 값입니다.

나열한 효과를 새로 받지 않게 하지만 이미 존재하는 효과는 계속 유지됩니다.

#### 면역 필드 (`Immune Fields`)

(배열) `field_type_id` 값입니다.

필드의 환경 효과가 적용되지 않게 합니다.

#### 가짜 아이템 (`Fake Items`)

(배열) `itype_id` 값입니다.

나열한 아이템을 제작 인벤토리에 추가합니다. `USES_BIONIC_POWER`와 함께 사용하면 생체공학 전력을 사용할 수 있습니다.

### 예시 (`Examples`)

```json
[
  {
    "//": "On-hit effect for ink glands mutation, implemented via enchantment.",
    "type": "enchantment",
    "id": "MEP_INK_GLAND_SPRAY",
    "hit_me_effect": [
      {
       "id": "generic_blinding_spray_1",
        "hit_self": false,
        "once_in": 15,
        "message": "Your ink glands spray some ink into %2$s's eyes.",
        "npc_message": "%1$s's ink glands spay some ink into %2$s's eyes."
      }
    ]
  },
  {
    "//": "This one would look good on a katana for an anime mod.",
    "type": "enchantment",
    "id": "ENCH_ULTIMATE_ASSKICK",
    "has": "WIELD",
    "condition": "ALWAYS",
    "ench_effects": [{ "effect": "invisibility", "intensity": 1 }],
    "hit_you_effect": [{ "id": "AEA_FIREBALL" }],
    "hit_me_effect": [{ "id": "AEA_HEAL" }],
    "mutations": ["KILLER", "PARKOUR"],
    "values": [{ "value": "STRENGTH", "multiply": 1.1, "add": -5 }],
    "intermittent_activation": {
      "effects": [
        {
          "frequency": "1 hour",
          "spell_effects": [
            { "id": "AEA_ADRENALINE" }
          ]
        }
      ]
    },
    "flags": ["FOOD_POISON_IMMUNE"],
    "immune_fields": ["fd_fire"],
    "immune_effects": ["poison"],
    "fake_items": ["fake_burrowing"]
    }
  }
]
```

## 인챈트 값 (`Enchantment Values`)

```jsonc
{
  "id": "CLIMATE_CONTROL", // 인챈트 ID
  "type": "enchantment_value", // 필수 유형
  "can_add": true, // 인챈트 값에 덧셈을 적용할 수 있는지 여부. 기본값: true
  "can_mult": true, // 인챈트 값에 곱셈을 적용할 수 있는지 여부. 기본값: true
  "can_max": false, // 이 유형의 최댓값을 구하는 연산을 적용할 수 있는지 여부. 기본값: false
  "suffixes": [ // 모든 접미사입니다. 이 부분이 말 그대로 가장 복잡합니다.
    [
      { "suffix": "COOLING", "desc_insert": [ "hot", "" ] }, // `CLIMATE_CONTROL_XXX` 형태로 나타납니다.
      { "suffix": "HEATING", "desc_insert": [ "cold", "" ] } // 이 `desc_insert`는 아래의 `desc_insert`를 덮어씁니다. `desc`도 교체할 수 있습니다.
    ],
    [ // 두 번째 집합을 정의합니다. 집합은 원하는 만큼 만들 수 있습니다.
      { "suffix": "TORSO", "replace": { "desc_insert": { "idx": 1, "val": "Torse" } } }, // `replace`는 0부터 시작하는 인덱스를 기준으로 교체하므로 ""를 "Torso"로 교체합니다.
      {
        "suffix": "ARM",
        "replace": { "desc_insert": { "idx": 1, "val": "Arm" } },
        "suffixes": [ // 이 접미사에 한정된 추가 접미사 목록입니다.
          { "suffix": "L", "replace": { "desc_insert": { "idx": 1, "val": "Left Arm" } } }, // 이 접미사에도 추가 중첩 접미사를 넣을 수 있습니다.
          { "suffix": "R", "replace": { "desc_insert": { "idx": 1, "val": "Right Arm" } } } // `CLIMATE_CONTROL_XXX_ARM_XXX` 형태로만 나타나며 다른 유형에는 적용되지 않습니다.
        ]
      },
    ] // 집합이 여러 개인 이유는 아래에서 더 자세히 설명합니다.
  ],
  "desc": "Keeps you comfortable against %1$s temperatures ( %2$s )", // 추가 문자열을 삽입해 서식을 지정할 수 있는 설명
  "desc_insert": [ "all bodyparts", "" ], // 서식에 삽입할 추가 문자열
  "unsupported_conditions": ["character", "item_and_character"] // 이 값이 호출되는 곳에서 절대 사용할 수 없는 조건 목록입니다.
},
```

### 접미사 설명 (`Suffixes Explanation`)

먼저 위의 트리를 살펴보세요. 두 그룹이 있습니다.

- `COOLING`과 `HEATING`
- `TORSO`와 `ARM`(`ARM`의 하위 항목 포함)

여기서는 순서가 중요합니다. `COOLING`이 먼저 정의되었으므로 `COOLING` 지정자가 필요할 때는 항상 앞에 와야 합니다. 예를 들어 `CLIMATE_CONTROL_COOLING_ARM`은 유효하지만 `CLIMATE_CONTROL_ARM_COOLING`은 유효하지 않습니다.

위 JSON에서 생성되는 체인은 다음과 같습니다.

- `CLIMATE_CONTROL`
  - `CLIMATE_CONTROL_COOLING`
    - `CLIMATE_CONTROL_COOLING_ARM`
      - `CLIMATE_CONTROL_COOLING_ARM_L` -> `CLIMATE_CONTROL_ARM_L` 참조
      - `CLIMATE_CONTROL_COOLING_ARM_R` -> `CLIMATE_CONTROL_ARM_R` 참조
  - `CLIMATE_CONTROL_HEATING`
    - `CLIMATE_CONTROL_HEATING_ARM`
      - `CLIMATE_CONTROL_HEATING_ARM_L` -> `CLIMATE_CONTROL_ARM_L` 참조
      - `CLIMATE_CONTROL_HEATING_ARM_R` -> `CLIMATE_CONTROL_ARM_R` 참조
  - `CLIMATE_CONTROL_ARM` -> 효과가 중첩되지 않도록 `CLIMATE_CONTROL`을 참조하지 않음
    - `CLIMATE_CONTROL_ARM_L`
    - `CLIMATE_CONTROL_ARM_R`

체인은 원하는 만큼 만들 수 있지만, 체인이 늘어날수록 점점 복잡해집니다.

인챈트 하위 값의 수가 계속 늘어나므로 이 문서에서는 인챈트 값의 전체 목록이 아니라 체인 그룹만 설명합니다.

C++이나 Lua에서 이 값들을 참조할 때는 항상 가장 구체적인 값을 사용해야 합니다. 나머지 단계의 값은 자동으로 채워집니다. 예를 들어 값을 가져올 때는 `CLIMATE_CONTROL_ARM_R`이나 `CLIMATE_CONTROL_ARM`이 아니라 `CLIMATE_CONTROL_COOLING_ARM_R`을 참조해야 합니다.

### 표준 접미사 (`Standard suffixes`)

<a id="bodyparts"></a>

#### 신체 부위 (`Bodyparts`)

모든 신체 부위에 사용하는 표준 접미사 집합입니다. 다음 항목을 포함합니다.

- `HEAD`
- `TORSO`
- `EYES`
- `MOUTH`
- `ARM`
  - `L`
  - `R`
- `LEG`
  - `L`
  - `R`
- `HAND`
  - `L`
  - `R`
- `FOOT`
  - `L`
  - `R`

<a id="skills"></a>

#### 스킬 (`Skills`)

모든 스킬 레벨에 사용합니다.

- `BARTER`
- `SPEECH`
- `COMPUTER`
- `FIRSTAID`
- `MECHANICS`
- `TRAPS`
- `DRIVING`
- `SWIMMING`
- `FABRICATION`
- `COOKING`
- `TAILOR`
- `SURVIVAL`
- `ELECTRONICS`
- `ARCHERY`
- `GUN`
- `LAUNCHER`
- `PISTOL`
- `RIFLE`
- `SHOTGUN`
- `SMG`
- `THROW`
- `MELEE`
- `BASHING`
- `CUTTING`
- `DODGE`
- `STABBING`
- `UNARMED`

<a id="damage-types"></a>

#### 피해 유형 (`Damage Types`)

<a id="Basegame-Enchantment-Value-ID-List"></a>

### 기본 게임 인챈트 값 ID 목록 (`Basegame Enchantment Value ID List`)

#### 캐릭터 값 (`Character values`)

##### `STRENGTH`

힘 능력치입니다. 여기서 `base_value`는 기본 능력치 값입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `DEXTERITY`

민첩 능력치입니다. 여기서 `base_value`는 기본 능력치 값입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `PERCEPTION`

지각 능력치입니다. 여기서 `base_value`는 기본 능력치 값입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `INTELLIGENCE`

지능 능력치입니다. 여기서 `base_value`는 기본 능력치 값입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `STRENGTH_PERMANENT`

기본 힘 능력치에 영향을 줍니다. 여기서 `base_value`는 기본 능력치 값입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `DEXTERITY_PERMANENT`

기본 민첩 능력치에 영향을 줍니다. 여기서 `base_value`는 기본 능력치 값입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `PERCEPTION_PERMANENET`

기본 지각 능력치에 영향을 줍니다. 여기서 `base_value`는 기본 능력치 값입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `INTELLIGENCE_PERMANENT`

기본 지능 능력치에 영향을 줍니다. 여기서 `base_value`는 기본 능력치 값입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `HEALTH_POINTS`

체력 값입니다. 여기서 `base_value`는 기본 체력 값입니다. 최종 값은 1보다 작아질 수 없습니다.

하위 값은 다음과 같습니다.

| 집합 (`Set`) | 값 (`Values`)                                       | 용도 (`Use`)               |
| ------------ | --------------------------------------------------- | -------------------------- |
| `Bodyparts`  | `TORSO`, `HEAD`, `ARM` (`L`, `R`), `LEG` (`L`, `R`) | 영향을 받는 특정 신체 부위 |

##### `SPEED`

캐릭터의 속도입니다. 여기서 `base_value`는 통증, 배고픔, 무게 페널티를 반영한 캐릭터 속도입니다. 최종 속도는 기본 속도의 25%보다 낮아질 수 없습니다.

##### `ATTACK_COST`

근접 공격 비용이며 낮을수록 좋습니다. 여기서 `base_value`는 능력치와 스킬 수정치를 반영한 해당 무기의 공격 비용입니다. 최종 값은 25보다 작아질 수 없습니다.

##### `MOVE_COST`

이동 비용입니다. 여기서 `base_value`는 의복과 특성의 수정치를 반영한 타일 이동 비용입니다. 최종 값은 20보다 작아질 수 없습니다.

##### `FLAT_MOVE_COST`

평지에서의 이동 비용에 영향을 줍니다. 여기서 `base_value`는 처리 과정 일부를 거친 이동 비용입니다. `MOVE_COST`와 마찬가지로 최종 값은 20보다 작아질 수 없습니다. 이 값은 `MOVE_COST`와 중첩됩니다.

##### `OBSTACLE_MOVE_COST`

장애물에서의 이동 비용에 영향을 줍니다. 여기서 `base_value`는 최초 이동 비용입니다. 최종 값은 100보다 작아질 수 없습니다. `MOVE_COST`보다 먼저 적용되며 `MOVE_COST`와 중첩됩니다.

##### `SWIM_MOVE_COST`

수영 중 이동 비용에 영향을 줍니다. 최종 값은 30보다 작아질 수 없습니다. 이 값은 `MOVE_COST`와 중첩되지 **않습니다**.

##### `READING_SPEED`

책을 읽는 속도입니다. `base_value`는 이동 포인트 단위의 최종 독서 속도입니다. 최종 값은 1초보다 짧아질 수 없습니다.

##### `CRAFTING_SPEED`

제작 속도입니다. `base_value`는 제작 속도 배율입니다. 다른 모든 배율을 적용한 뒤 계산합니다.

##### `CONSTRUCTION_SPEED`

건설 속도입니다. `base_value`는 차량 및 가구/지형 건설 속도의 배율입니다. 다른 모든 배율을 적용한 뒤 계산합니다.

| 집합 (`Set`)        | 값 (`Values`) | 용도 (`Use`)                                       |
| ------------------- | ------------- | -------------------------------------------------- |
| `Construction Type` | `CON`, `VEH`  | 일반 건설과 차량 건설 중 어느 속도에 영향을 주는지 |

##### `METABOLISM`

대사율입니다. 이 수정자는 `add` 필드를 무시합니다. 여기서 `base_value`는 특성으로 수정된 `PLAYER_HUNGER_RATE`입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `MANA_CAP`

마나 최대량입니다. 여기서 `base_value`는 특성으로 수정된 캐릭터의 기본 마나 최대량입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `MANA_REGEN`

마나 재생률입니다. 이 수정자는 `add` 필드를 무시합니다. 여기서 `base_value`는 특성으로 수정된 캐릭터의 기본 마나 획득률입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `STAMINA_CAP`

기력 최대량입니다. 이 수정자는 `add` 필드를 무시합니다. 여기서 `base_value`는 특성으로 수정된 캐릭터의 기본 기력 최대량입니다. 최종 값은 `PLAYER_MAX_STAMINA`의 10%보다 작아질 수 없습니다.

##### `STAMINA_REGEN`

기력 재생률입니다. 이 수정자는 `add` 필드를 무시합니다. 여기서 `base_value`는 입의 방해도로 수정된 캐릭터의 기본 기력 획득률입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `THIRST`

갈증 증가율입니다. 여기서 `base_value`는 캐릭터의 기본 갈증 증가율입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `FATIGUE`

피로 증가율입니다. 여기서 `base_value`는 캐릭터의 기본 피로 증가율입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `MENDING_MULT`

부러진 팔다리의 회복률 배율을 변경합니다. `base_value`는 돌연변이를 적용한 뒤의 재생 수정치입니다(기본값 0.25). 최종 값의 범위는 0.0~1.0입니다.

##### `HEARING`

청각 배율입니다. `base_value`는 청각에 적용되는 최종 배율입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `NOISE`

발걸음 소음 값입니다. `base_value`는 돌연변이를 적용한 뒤의 소음 배율입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `SCENT`

냄새 값입니다. `base_value`는 돌연변이를 적용한 뒤의 냄새 값입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `STEALTH`

은신 수정치입니다. 값이 높으면 은신이 증가하고 낮으면 감소합니다. `base_value`는 돌연변이를 적용한 뒤의 값입니다. 값은 20~160으로 제한됩니다. 160이면 60% 더 잘 보이고, 20이면 80% 덜 보입니다.

##### `MOTION_ALARM`

접근하는 생물을 얼마나 먼 거리에서 감지해 알림을 받을지를 정합니다. `base_value`는 항상 0입니다. 알림이 작동하는 타일 범위에는 이 값의 최댓값을 사용합니다.

##### `BODYTEMP_X`

쾌적하게 받아들이는 체온 범위의 수정치입니다. 알맞은 값은 다음과 같습니다.

- `BODYTEMP_MIN`: 쾌적한 최소 온도
- `BODYTEMP_MAX`: 쾌적한 최대 온도

##### `BODYTEMP_SLEEP`

수면 중에 추가로 얻는 체온입니다. `base_value`는 돌연변이와 이전 인챈트의 값입니다. 제한은 없습니다.

##### `BODYTEMP_SPEED`

`COLDBLOOD4` 캐릭터에게 추가로 적용되는 속도 변화입니다. `base_value`는 돌연변이 값 또는 0입니다. 현재 제한은 없습니다.

##### `SLEEP_PAIN_THRESHOLD`

잠에서 깨는 데 추가로 필요한 통증입니다. `base_value`는 기본 수면 통증 값입니다. 최솟값은 1입니다.

##### `SLEEP_DB_RESIST`

잠에서 깨는 데 필요한, 주변 환경 소음보다 큰 추가 소음의 양을 변경합니다. `base_value`는 20입니다. 최솟값과 최댓값은 없습니다.

##### `CLIMATE_CONTROL`

플레이어가 느끼는 온도를 특정 지점에 가까워지도록 조절합니다. `base_value`는 플레이어가 현재 느끼는 온도입니다. 정상 온도(돌연변이 포함)보다 낮은지 높은지에 따라 온도를 높이거나 낮춥니다. 하위 집합은 두 가지입니다.

| 집합 (`Set`)  | 값 (`Values`)                                   | 용도 (`Use`)                                 |
| ------------- | ----------------------------------------------- | -------------------------------------------- |
| `Temperature` | `COOLING`, `HEATING`                            | 이상적인 온도를 향해 열을 줄일지 늘릴지 지정 |
| `Bodypart`    | 일반 신체 부위 인챈트는 [여기](#bodyparts) 참조 | 적용되는 신체 부위                           |

각 값은 각각 냉각 또는 가열만 수행합니다.

##### `LIE`

거짓말 성공 확률의 수정치입니다. `base_value`는 스킬 효과를 적용한 뒤의 값입니다. 0 미만이거나 100을 초과하는 값은 추가 변화를 일으키지 않습니다.

##### `PERSUADE`

설득에 적용된다는 점을 제외하면 `LIE`와 같습니다.

##### `INTIMIDATE`

협박에 적용된다는 점을 제외하면 `LIE`와 같습니다.

##### `HEALTHY_MULT`

건강도 수정치입니다. `base_value`는 1입니다.

##### `FALL_DAMAGE_MULT`

낙하 피해 배율의 수정치입니다. `base_value`는 돌연변이와 다른 수정치를 적용한 뒤의 값입니다. 0보다 작아질 수 없습니다.

##### `CARRY_STORAGE`

휴대 가능한 수납 부피의 수정치입니다. `base_value`는 밀리리터 단위의 현재 수납 부피입니다. 0보다 작아질 수 없습니다.

##### `CARRY_WEIGHT`

휴대 가능한 무게의 수정치입니다. `base_value`는 밀리리터 단위의 현재 수납 부피입니다. 0보다 작아질 수 없습니다.

##### `WEIGHTMOD`

플레이어의 현재 무게를 변경합니다. `base_value`는 아래 범주 가운데 해당하는 범주의 무게입니다. 최솟값은 0이며 최댓값에는 제한이 없습니다.

| 집합 (`Set`) | 값 (`Values`)                                    | 용도 (`Use`)                    |
| ------------ | ------------------------------------------------ | ------------------------------- |
| `Category`   | `BIONICS`, `WEAPON`, `INVENTORY`, `BODY`, `WORN` | 플레이어 무게에서 적용되는 부분 |

##### `OVERMAP_SIGHT`

오버맵 시야 수정치입니다. `base_value`는 돌연변이 중 가장 높은 값입니다. 최댓값은 3입니다.

##### `EFFECTIVE_FOCUS`

집중력 수정치입니다. `base_value`는 현재 집중력입니다. 제한은 없습니다.

##### `MELEE_HIT`

근접 명중도의 수정치입니다. `base_value`는 모든 수정치를 적용한 뒤의 값입니다. 제한은 없습니다.

##### `OVERKILL`

좀비가 죽은 뒤 가해지는 피해의 수정치입니다. `base_value`는 현재 과잉 피해량입니다. 최솟값은 0입니다.

##### `FOOD_FUN`

음식의 사기 효과 수정치입니다. `base_value`는 현재 음식 사기 값입니다. 제한은 없습니다.

##### `VOMIT_MOD`

구토 확률의 수정치입니다. `base_value`는 항상 1입니다. 최솟값은 0입니다.

##### `PAIN`

통증 변화량의 수정치입니다. `base_value`는 통증 수정치입니다.

| 집합 (`Set`) | 값 (`Values`)  | 용도 (`Use`)                 |
| ------------ | -------------- | ---------------------------- |
| `Type`       | `GAIN`, `LOSS` | 증가 또는 감소에만 각각 적용 |

##### `PAIN_MOD`

현재 통증량을 일정하게 변경하는 수정치입니다. `base_value`는 현재 통증입니다. 최솟값은 0입니다.

##### `CHRONIC_PAIN_MOD`

`CHRONIC_PAIN` 옵션으로 인한 통증의 수정치입니다. `base_value`는 만성 통증 값입니다. `PAIN_MOD` 적용 후의 값에 더한 결과의 최솟값은 0입니다.

##### `PERCEIVED_PAIN_MOD`

체감 통증의 수정치입니다. `base_value`는 `PAIN_MOD`를 적용한 뒤의 현재 통증입니다. 최솟값은 0입니다.

| 집합 (`Set`) | 값 (`Values`)                     | 용도 (`Use`)                             |
| ------------ | --------------------------------- | ---------------------------------------- |
| `Stat`       | `SPD`, `STR`, `DEX`, `INT`, `PER` | 각각 속도, 힘, 민첩, 지능, 지각에만 적용 |

##### `PAIN_PENALTY`

통증이 심할 때 받는 능력치 페널티에 영향을 줍니다. `base_value`는 현재 페널티입니다. 최댓값은 0이며, 이때 효과가 없습니다.

##### `ADDICTION_STRENGTH`

중독 강도가 한 단계 더 증가할 가능성의 수정치입니다. `base_value`는 추가되는 중독의 강도입니다. 제한은 없습니다.

##### `ADDICTION_TIME_PER_ADDITION`

중독이 적용될 때 중독 시간이 얼마나 늘어나는지를 변경합니다. `base_value`는 중독이 적용될 때마다 추가되는 기본 시간(초)입니다. 제한은 없습니다.

##### `ADDICTION_TIME_PER_INTENSITY`

중독이 지속되는 시간을 변경합니다. `base_value`는 중독 중첩 하나가 제거되는 데 걸리는 시간입니다. 값을 줄이면 중독 시간이 _늘어나고_, 값을 더하면 중독 시간이 _줄어듭니다_. 제한은 없습니다.

##### `UNCANNY_DODGE`

총알을 회피할 0~1 범위의 확률입니다. `base_value`는 0입니다. 초인적 회피를 한 번 할 때마다 기력 100을 소모합니다. 0 미만인 값은 0과 같고 1을 초과하는 값은 1과 같은 효과를 냅니다.

##### `BONUS_DODGE`

회피 페널티가 적용되기 전에 한 턴에 할 수 있는 추가 회피 횟수입니다. 여기서 `base_value`는 페널티 적용 전 캐릭터의 기본 턴당 회피 횟수(보통 1)입니다. 최종 값이 0보다 작아질 수도 있으며, 이 경우 회피 판정에 페널티가 적용됩니다.

##### `FORCEFIELD`

특정 피해 유형의 피해를 막을 0~1 범위의 확률입니다. `base_value`는 0입니다.

| 집합 (`Set`)  | 값 (`Values`)                                      | 용도 (`Use`)       |
| ------------- | -------------------------------------------------- | ------------------ |
| `Damage Type` | 일반 피해 유형 접미사는 [여기](#damage-types) 참조 | 적용되는 피해 유형 |

##### `CROWD_CRUSH_RESIST`

군중 압사에 휘말릴 가능성의 수정치입니다. `base_value`는 항상 5입니다. 값을 높이면 군중 압사에 휘말릴 가능성이 낮아집니다. 범위는 0(저항할 가능성 없음)부터 95(저항하지 못할 가능성 5%)까지입니다.

##### `BLISTER_COUNT`

물집 효과를 얻을 때 적용되는 유효 열 방어구 수정치입니다. `base_value`는 물집 수입니다. 최종 값이 0보다 작으면 캐릭터에게 절대 물집이 생기지 않으며, 반대로 더 높아져 항상 물집이 생길 수도 있습니다.

##### `LUMINATION`

활성화된 동안 플레이어 주변을 밝히는 밝기입니다. 이 인챈트에는 덧셈이나 곱셈을 적용할 수 없고 최댓값만 사용할 수 있습니다. 최종 값은 0보다 작아지지 않으며 최댓값에는 제한이 없습니다.

##### `NIGHT_VISION`

플레이어의 야간 시야 값입니다. `EFFECT_NIGHT_VISION` 또는 `GNV_EFFECT`는 10.0이고 `GNVE_EFFECT`는 18.0입니다. `max`만 작동하며 인챈트와 다른 야간 시야 효과 가운데 가장 높은 값을 사용합니다.

##### `CLAIRVOYANCE`

플레이어의 투시 값입니다. `CLAIRVOYANCE_SUPER`는 40.0, `CLAIRVOYANCE_PLUS`는 8.0, `CLAIRVOYANCE`는 3입니다. `max`만 작동하며 인챈트와 다른 투시 효과 가운데 가장 높은 값을 사용합니다.

##### `FLASH_PROTECTION`

플레이어의 섬광 보호 값입니다. 아이템과 효과 플래그는 3을 부여합니다. `max`만 작동하며 인챈트, 아이템, 효과의 값 가운데 가장 높은 값을 사용합니다.

##### `GROUNDED_CREATURE_SIGHT`

지면에 있는 생물을 적외선 형태로 벽 너머까지 볼 수 있는 시야입니다. 이 값은 효과가 작동하는 타일 수입니다. `max`만 작동합니다.

##### `REACH_RANGE`

모든 근접 공격(비무장 또는 무장)의 도달 거리를 늘리는 값입니다. `base_value`는 항상 0입니다. `max`만 작동합니다.

| 집합 (`Set`) | 값 (`Values`)      | 용도 (`Use`)                        |
| ------------ | ------------------ | ----------------------------------- |
| `Type`       | `UNARMED`, `ARMED` | 비무장 또는 무장 공격에만 각각 적용 |

##### `ARMOR_X`

받는 피해의 수정치입니다. 능동 방어 시스템 생체공학을 적용한 뒤, 아이템이 피해를 흡수하기 전에 적용됩니다. 여기서 `base_value`는 해당 유형의 받는 피해량이므로 양수 `add`와 1보다 큰 `mul`은 캐릭터가 받는 피해를 **증가**시킵니다.

| 집합 (`Set`)  | 값 (`Values`)                                      | 용도 (`Use`)       |
| ------------- | -------------------------------------------------- | ------------------ |
| `Damage Type` | 일반 피해 유형 접미사는 [여기](#damage-types) 참조 | 적용되는 피해 유형 |

##### `SKILL_LEVEL`

캐릭터 전체에 적용되는 스킬 레벨 수정치입니다. `base_value`는 플레이어의 현재 스킬 레벨입니다.

| 집합 (`Set`) | 값 (`Values`)                           | 용도 (`Use`)  |
| ------------ | --------------------------------------- | ------------- |
| `Skill Type` | 일반 스킬 접미사는 [여기](#skills) 참조 | 적용되는 스킬 |

##### `SKILL_EXP`

캐릭터 전체에 적용되는 스킬 경험치 획득 수정치입니다. `base_value`는 현재 행동으로 얻는 경험치입니다. 주의: 이 값에는 곱셈만 적용할 수 있으며 덧셈은 적용할 수 없습니다.

| 집합 (`Set`) | 값 (`Values`)                           | 용도 (`Use`)  |
| ------------ | --------------------------------------- | ------------- |
| `Skill Type` | 일반 스킬 접미사는 [여기](#skills) 참조 | 적용되는 스킬 |

##### `Encumbrance`

캐릭터 전체에 적용되는 방해도 수정치이며, 하위 값은 특정 신체 부위만 변경합니다.

| 집합 (`Set`) | 값 (`Values`)                                   | 용도 (`Use`)       |
| ------------ | ----------------------------------------------- | ------------------ |
| `Bodypart`   | 일반 신체 부위 인챈트는 [여기](#bodyparts) 참조 | 적용되는 신체 부위 |

##### `MELEE_DAMAGE`

캐릭터 전체에 적용되는 근접 피해 수정치입니다(도달 공격 포함). 하위 값은 특정 피해 유형만 변경합니다.

| 집합 (`Set`)  | 값 (`Values`)                                      | 용도 (`Use`)       |
| ------------- | -------------------------------------------------- | ------------------ |
| `Damage Type` | 일반 피해 유형 접미사는 [여기](#damage-types) 참조 | 적용되는 피해 유형 |

##### `MELEE_ARMOR_PENETRATION`

캐릭터 전체에 적용되는 근접 방어구 관통 수정치입니다. 하위 값은 특정 피해 유형만 변경합니다.

| 집합 (`Set`)  | 값 (`Values`)                                      | 용도 (`Use`)       |
| ------------- | -------------------------------------------------- | ------------------ |
| `Damage Type` | 일반 피해 유형 접미사는 [여기](#damage-types) 참조 | 적용되는 피해 유형 |

#### 아이템 값 (`Item values`)

##### `ITEM_ATTACK_COST`

이 아이템으로 근접 또는 투척 공격을 할 때의 공격 비용입니다. 조건과 위치를 무시하며 항상 활성화됩니다. 여기서 `base_value`는 아이템의 기본 공격 비용입니다. 최종 값은 0보다 작아질 수 없습니다.

##### `ITEM_DAMAGE_X`

이 아이템의 근접 피해입니다. 조건과 위치를 무시하며 항상 활성화됩니다. 여기서 `base_value`는 해당 유형의 아이템 기본 피해입니다. 최종 값은 0보다 작아질 수 없습니다. 지원되는 피해 유형 외에 전역 피해 수정치 `ITEM_DAMAGE`도 있습니다.

| 집합 (`Set`)  | 값 (`Values`)                                      | 용도 (`Use`)       |
| ------------- | -------------------------------------------------- | ------------------ |
| `Damage Type` | 일반 피해 유형 접미사는 [여기](#damage-types) 참조 | 적용되는 피해 유형 |

##### `ITEM_ARMOR_PENETRATION_X`

이 아이템의 방어구 관통력입니다. 여기서 `base_value`는 해당 유형의 기본 방어구 관통력입니다. 최종 값은 0보다 작아질 수 없습니다. 지원되는 피해 유형 외에 전역 수정치 `ITEM_ARMOR_PENTRATION`도 있습니다.

| 집합 (`Set`)  | 값 (`Values`)                                      | 용도 (`Use`)       |
| ------------- | -------------------------------------------------- | ------------------ |
| `Damage Type` | 일반 피해 유형 접미사는 [여기](#damage-types) 참조 | 적용되는 피해 유형 |

##### `ITEM_ARMOR_X`

이 아이템에 적용되는 받는 피해 수정치이며, 아이템이 피해를 흡수하기 전에 적용됩니다. 여기서 `base_value`는 해당 유형의 받는 피해량이므로 양수 `add`와 1보다 큰 `mul`은 캐릭터가 받는 피해를 **증가**시킵니다. 전역 값 `ITEM_ARMOR` 외에 각 피해 유형마다 고유한 인챈트 값이 있습니다.

| 집합 (`Set`)  | 값 (`Values`)                                      | 용도 (`Use`)       |
| ------------- | -------------------------------------------------- | ------------------ |
| `Damage Type` | 일반 피해 유형 접미사는 [여기](#damage-types) 참조 | 적용되는 피해 유형 |

##### `ITEM_REACH_RANGE`

아이템의 도달 거리 수정치입니다. 이 값과 아이템 플래그의 값 가운데 최댓값을 사용합니다.

## 인챈트 플래그 (`Enchantment Flag`)

```jsonc
{
  "id": "NEARSIGHTED",               // 인챈트 플래그 ID
  "type": "enchantment_flag",        // 필수 유형
  "parents": [ "BLIND" ],            // 함께 부여하는 다른 enchantment_flag의 배열
  "conflicts": [ "FIX_NEARSIGHTED" ] // 이 플래그가 무효화하는 다른 enchantment_flag의 배열
  "info": "<bad>Causes nearsightedness</bad>" // 인챈트 정보에 표시되는 안내 문자열
},
```

아래에 설명된 모든 효과는 인챈트를 부여하는 대상을 소유한 캐릭터에게 적용됩니다.

<a id="Basegame-Enchantment-Flag-ID-List"></a>

### 기본 게임 인챈트 플래그 ID 목록 (`Basegame Enchantment Flag ID List`)

#### 시야 (`Sight`)

##### `UNDERWATER_SIGHT`

수중에서도 방해받지 않고 볼 수 있게 합니다.

##### `SLEEP_SIGHT`

자는 동안에도 볼 수 있게 합니다.

##### `NEARSIGHTED`

시야를 크게 제한합니다. 일부 안경으로 해결할 수 있습니다.

##### `FIX_NEARSIGHTED`

`NEARSIGHTED`와 충돌하며, 이를 치료하고 제거합니다.

##### `BLIND`

어떤 타일도 볼 수 없게 합니다. 벽에 부딪히면 해당 벽은 드러납니다.

##### `FIX_BLIND`

`BLIND`와 충돌하며, 이를 치료하고 제거합니다.

##### `INFRARED_VISION`

적외선 시야를 얻습니다.

##### `ELECTROSENSE`

벽 너머의 로봇과 전기 생물을 볼 수 있게 합니다.

##### `SONAR`

땅을 파고 이동하는 생물을 `INFRARED_VISION` 스프라이트로 볼 수 있게 합니다.

##### `ANTIGLARE`

햇빛 등에 의한 눈부심 효과를 방지합니다.

#### 섭취 (`Consumption`)

##### `EAT_ROTTEN`

썩은 음식을 안전하게 먹을 수 있게 합니다.

##### `ONLY_EAT_ROTTEN`

신선한 음식을 먹을 때 큰 페널티를 부여하지만, 신선한 액체는 계속 마실 수 있습니다.

##### `EAT_ROTTEN_MORALE`

썩은 음식을 먹어도 사기 페널티를 받지 않게 합니다.

##### `CONSUME_UNCLEAN`

불결한 액체를 마시고 불결한 음식을 먹을 수 있게 합니다.

##### `FOOD_PARASITE_IMMUNE`

음식을 섭취하여 기생충에 감염되는 것을 방지합니다.

##### `FOOD_POISON_IMMUNE`

음식을 섭취하여 독에 걸리는 것을 방지합니다.

#### 기타 (`Miscellaneous`)

##### `ALARMCLOCK`

잠잘 때 알람을 설정할 수 있게 합니다.

##### `INTENAL_ALARMCLOCK`

`ALARMCLOCK`의 효과를 제공하지만 소리를 내지 않습니다. 또한 알람을 못 듣고 계속 자는 일을 방지해야 합니다.

##### `VIEW_DRONE_CAM`

`effect_drone_marker`가 있는 모든 생물을 볼 수 있게 합니다. 이 효과는 일반적으로 `PHOTOGRAPH` 로봇이 부여합니다.

##### `RADIO`

라디오를 소지한 것과 같은 효과를 제공합니다.

##### `THERMOMETER`

온도계를 소지한 것과 같은 효과를 제공합니다.

##### `WATCH`

정확한 시간을 볼 수 있게 합니다.

##### `FIRE_FIELD_IMMUNE`

화염 필드에 대한 면역을 제공합니다.

##### `SILENT`

플레이어가 움직일 때 소리가 나지 않게 합니다.

##### `NO_THERMAL_WAKE`

극한 온도 때문에 플레이어가 잠에서 깨지 않게 합니다.

##### `NO_DAMAGE_WAKE`

피해를 받아도 플레이어가 잠에서 깨지 않게 합니다.

##### `NO_LIGHT_WAKE`

빛 때문에 플레이어가 잠에서 깨지 않게 합니다.

## 인챈트 조건 (`Enchantment Condition`)

```jsonc
{
  "id": "WORN", // 조건 ID
  "type": "enchantment_condition", // 필수 유형
  "condition_type": "item_and_character", // 조건 유형. 가능한 값은 `global`, `item`, `character`, `item_and_character`입니다.
  "condition_function": "worn", // 사용할 함수. 일반적으로 하드코딩된 함수 또는 Lua 함수를 참조합니다.
  "condition_info": "While worn", // 아이템의 인챈트 정보에 표시할 조건 설명
}
```

<a id="Basegame-Enchantment-Condition-ID-List"></a>

### 기본 게임 인챈트 조건 ID 목록 (`Basegame Enchantment Condition ID List`)

#### 아이템과 캐릭터 (`Item and Character`)

##### `HELD`

인벤토리에 있을 때 활성화됩니다.

##### `WIELD`

손에 들고 있을 때 활성화됩니다.

##### `WORN`

방어구로 착용하고 있을 때 활성화됩니다.

#### 전역 (`Global`)

##### `ALWAYS`

항상 활성화됩니다. 더 이상 권장되지 않지만 지원됩니다. 항상 참이 되므로 조건 자체가 필요하지 않습니다.

##### `NIGHT`

밤일 때 활성화됩니다.

##### `DUSK`

해질녘일 때 활성화됩니다.

##### `DAY`

낮일 때 활성화됩니다.

##### `DAWN`

새벽일 때 활성화됩니다.

##### `ACTIVE`

아이템, 돌연변이, 생체공학 등 인챈트가 붙은 대상이 활성 상태일 때 활성화됩니다.

##### `INACTIVE`

`ACTIVE`의 반대입니다.

#### 캐릭터 (`Character`)

##### `INSIDE`

아이템 소유자가 실내에 있을 때 활성화됩니다.

##### `OUTSIDE`

아이템 소유자가 실외에 있을 때 활성화됩니다.

##### `UNDERGROUND`

아이템 소유자가 Z 레벨 0보다 아래에 있을 때 활성화됩니다.

##### `ABOVEGROUND`

아이템 소유자가 Z 레벨 0 이상에 있을 때 활성화됩니다.

##### `UNDERWATER`

소유자가 수영할 수 있는 지형에 있을 때 활성화됩니다.

<a id="Enchantment-Vision"></a>

## 인챈트 시야 (`Enchantment Vision`)

다음은 인챈트 시야 정의입니다. 표시하려면 모든 조건을 통과해야 합니다.

```jsonc
{
  "id": "ELECTROSENSE",                          // 인챈트 플래그 ID
  "type": "enchantment_vision",                  // 필수 유형
  "desc": "Allows Sight Of Electric Creatures",  // 인챈트 정보에 표시되는 안내 문자열
  "distance": 5,                                 // 작동하는 최대 타일 거리
  "same_z_level": false,                         // 다른 Z 레벨까지 표시
  "require_los": true,                           // 시야선 필요
  "detect_heat": true,                           // `is_warm` 필요
  "show_with_species": [ "ROBOT" ],              // 종 목록. 하나라도 참이면 통과
  "show_with_flag": [ "ELECTRIC" ],              // 몬스터 플래그 목록. 하나라도 참이면 통과
  "show_without_any_flag": [ "FLIES" ],          // 몬스터 플래그 목록. 하나라도 참이면 실패
  "show_with_effect": [ "drone_marker" ],        // 효과 목록. 하나라도 참이면 통과
  "show_without_any_effect": [ "drone_marker" ], // 효과 목록. 하나라도 참이면 실패
  "show_normal": true,                           // 몬스터를 정상적으로 표시합니다. 이 필드나 vision_desc 중 하나를 사용해야 합니다.
  "vision_desc": {                               // 배열 또는 객체일 수 있음
    "tile_id": "infrared_creature",              // 표시할 타일
    "description": "You sense electricity."      // 크기와 관계없이 조사할 때 표시할 메시지
  },
  "vision_desc": [ // `TINY`, `SMALL`, `MEDIUM`, `LARGE`, `HUGE`마다 하나씩 정의해야 합니다.
    {
      "tile_id": "infrared_creature",                     // 타일 ID
      "description": "You sense tiny bits of electricity" // 조사할 때 표시할 메시지
      "size": "TINY"                                      // 생물의 크기 열거형
    }
  ]
},
```

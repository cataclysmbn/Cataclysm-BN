# 바이오닉

> [!WARNING]
> 많은 생체공학에는 코드에만 정의된 효과가 있으며 여기에 나열되지 않습니다.

새로운 생체 공학을 추가할 때 다른 생체 공학에 포함되어 있지 않은 경우 해당 CBM도 추가해야 합니다.
[여기](#paired-item-definition)에 설명된 항목입니다. 결함이 있는 생체 공학의 경우에도 마찬가지입니다.

### 필드

| 식별자                     | 설명                                                                                                                                                                 |
| -------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| id                         | (_필수_) 고유 문자열 ID                                                                                                                                              |
| name                       | (_필수_) 게임 내 바이오닉 이름                                                                                                                                       |
| description                | (_필수_) 생체 공학에 대한 게임 설명                                                                                                                                  |
| flags                      | (_선택 사항_) json 플래그 배열입니다. [목록](/mod/json/reference/json_flags/#bionics) 보기                                                                           |
| capacity                   | (_선택 사항_) 이 생체 공학으로 얻은 생체 공학 능력                                                                                                                   |
| act_cost                   | (_선택 사항_) 생체 공학을 활성화하는 데 드는 전력 비용입니다.                                                                                                        |
| deact_cost                 | (_선택 사항_) 생체 공학을 비활성화하는 데 드는 전력 비용입니다.                                                                                                      |
| trigger_cost               | (_선택 사항_) 생체 공학 효과를 발동하는 데 드는 전력 비용입니다.                                                                                                     |
| kcal_trigger_cost          | (_선택 사항_) 생체 공학 효과를 발동하는 데 드는 칼로리 비용                                                                                                          |
| react_cost                 | (_선택 사항_) `time` 틱마다 소비하는 전력량                                                                                                                          |
| time                       | (_선택적_) `react_cost` 전력 소비 사이의 시간                                                                                                                        |
| stat_bonus                 | (_선택적_) 배열의 배열, 내부 배열은 `["STAT", integer bonus]`                                                                                                        |
| fuel_options               | (_선택 사항_) 이 생체 공학에 연료를 공급하기 위해 허용되는 Itype ID                                                                                                  |
| fuel_capacity              | (_선택적_) 생체공학에 의해 저장된 연료 요금                                                                                                                          |
| fuel_efficiency            | (_선택 사항_) 연료 소비로 생성된 전력에 대한 승수                                                                                                                    |
| fuel_multiplier            | (_선택적_) 충전당 생체공학에 적재된 연료의 양을 곱합니다.                                                                                                            |
| passive_fuel_efficiency    | (_선택 사항_) `fuel_efficiency` 그러나 생체 공학은 비활성 상태이며 `PERPETUAL` 연료에도 사용됩니다.                                                                  |
| coverage_power_gen_penalty | (_선택 사항_) `occupied_bodyparts` 항목의 적용 범위와 이 값(50% 본문 부분 적용 범위 +)을 `fuel_efficiency`에 곱합니다.                                               |
| exothermic_power_gen       | (_선택적_) 동력으로 변환되지 않은 연료가 열로 변환되는 경우                                                                                                          |
| remote_fuel_draw           | (_선택 사항_) 연결된 외부 케이블에서 전력 소모(즉, 케이블 충전기 시스템)                                                                                             |
| power_gen_emission         | (_선택 사항_) [필드 ID](/mod/json/reference/map/field_type) 전원을 공급하면서 생성                                                                                   |
| weight_capacity_modifier   | (_선택적_) 중량 용량 승수                                                                                                                                            |
| weight_capacity_bonus      | (_선택사항_) 중량 용량 추가                                                                                                                                          |
| upgraded_bionic            | (_선택적_) 이 바이오닉을 설치하여 업그레이드할 수 있는 바이오닉 ID                                                                                                   |
| required_bionics           | (_선택 사항_) 설치에 필요한 생체 공학 ID 배열                                                                                                                        |
| included_bionics           | (_선택 사항_) 설치 시 제공되는 생체 공학 ID 배열                                                                                                                     |
| included                   | (_선택 사항_) 부울, 이 생체 공학이 다른 생체 공학에 포함되어 있습니까?                                                                                               |
| available_upgrades         | (_선택 사항_) 업그레이드할 수 있는 생체 공학 ID 배열                                                                                                                 |
| env_protec                 | (_선택 사항_) 배열 배열, 내부 배열은 `["bodypart", integer]`입니다. 여기서 정수는 본문 부분의 환경 보호입니다.                                                       |
| bash_protec                | (_선택 사항_) 배열 배열, 내부 배열은 `["bodypart", integer]`입니다. 여기서 정수는 본문 부분에 대한 bash 보호입니다.                                                  |
| cut_protec                 | (_선택 사항_) 배열 배열, 내부 배열은 `["bodypart", integer]`입니다. 여기서 정수는 본문 부분의 절단 보호입니다.                                                       |
| bullet_protec              | (_선택적_) 배열 배열, 내부 배열은 `["bodypart", integer]`입니다. 여기서 정수는 신체 부위의 방탄복입니다.                                                             |
| occupied_bodyparts         | (_선택 사항_) 배열 배열, 내부 배열은 `["bodypart", integer]`입니다. 여기서 정수는 본체 부분에서 차지하는 생체 공학 슬롯입니다.                                       |
| encumbrance                | (_선택 사항_) 배열 배열, 내부 배열은 `["bodypart", integer]`입니다. 여기서 정수는 본문 부분의 부담입니다.                                                            |
| canceled_mutations         | (_선택 사항_) 설치 시 취소되고 이후에 방지되는 특성 ID 배열                                                                                                          |
| learned_spells             | (_선택적_) 배열의 배열, 내부 배열은 `["spell", integer]`입니다. 여기서 정수는 주문을 배우는 레벨입니다.                                                              |
| enchantments               | (_선택 사항_) 생체 공학과 함께 제공할 마법 부여 ID 배열                                                                                                              |
| bio_enchantments           | (_선택 사항_) 생체 공학과 함께 제공할 인라인 마법 배열                                                                                                               |
| fake_item                  | (_선택 사항_) 생체공학이 제공하는 아이템입니다. `BIONIC_TOOLS`를 사용하면 도구로 제공되고, `BIONIC_TOGGLED`을 사용하면 휘두르며, `BIONIC_GUN`를 사용하면 발사됩니다. |
| can_uninstall              | (_선택적_) 바이오닉을 제거할 수 있는 경우                                                                                                                            |
| no_uninstall_reason        | (_선택 사항_) `can_uninstall`에 필요하며 제거할 수 없다는 메시지가 표시됩니다.                                                                                       |
| starting_bionic            | (_선택 사항_) true인 경우 생체 공학으로 시작할 수 있습니다.                                                                                                          |
| points                     | (_선택 사항_) 캐릭터 생성에 필요한 포인트 수                                                                                                                         |

```jsonc
{
  "id": "bio_chaos",
  "name": "Complete Chaos",
  "description": "This does a lot of things that makes little sense",
  "flags": ["BIONIC_TOGGLED", "BIONIC_NPC_USABLE", "COMBAT_NPC_USE"],
  "capacity": "100 kJ",
  "act_cost": "5 J",
  "deact_cost": "5 J",
  "trigger_cost": "5 J",
  "react_cost": "5 J",
  "kcal_trigger_cost": 5,
  "time": 1,
  "stat_bonus": [["STR", 2]],
  "fuel_options": ["gasoline"],
  "fuel_capacity": 500,
  "fuel_efficiency": 0.25,
  "fuel_multiplier": 2,
  "coverage_power_gen_penalty": 0.5,
  "passive_fuel_efficiency": 0.125,
  "exothermic_power_gen": true,
  "remote_fuel_draw": "1 kJ",
  "powergen_emission": "fd_fire",
  "weight_capacity_modifier": 2.0,
  "weight_capacity_bonus": "20 kg",
  "upgraded_bionic": "bio_chaos_v1",
  "required_bionics": ["bio_chaos_base"],
  "included_bionics": ["bio_chaos_stuff", "bio_chaos_morestuff"],
  "included": false,
  "available_upgrades": ["bio_chaos_v50"],
  "env_protec": [["eyes", 7]],
  "bash_protec": [["eyes", 3]],
  "cut_protec": [["eyes", 3]],
  "bullet_protec": [["eyes", 3]],
  "occupied_bodyparts": [["torso", 1]],
  "encumbrance": [["mouth", 10]],
  "canceled_mutations": ["ASTHMA"],
  "learned_spells": [["mint_breath", 2]],
  "enchantments": ["ENCH_UNCANNY_DODGE"],
  "bio_enchantments": [
    { "condition": "ALWAYS", "values": [{ "value": "SPEED", "multiply": 0.1 }] },
  ],
  "can_uninstall": false,
  "no_uninstall_reason": "The telescopic lenses are part of the patient's eyes now, remove them and the patient would become blind",
  "starting_bionic": true,
  "points": 1,
}
```

<a id="paired-item-definition"></a>

### 페어링 항목 정의

```jsonc
{
  "id": "bio_adrenaline", // 생체공학 ID와 정확히 일치해야 합니다.
  "copy-from": "bionic_general", // 이것에는 몇 가지 변형이 있지만 이것이 기본입니다.
  "type": "BIONIC_ITEM", // 필수 유형
  // 나머지는 일반 항목 필드입니다.
  "name": { "str": "Adrenaline Pump CBM" },
  "looks_like": "bio_int_enhancer",
  "description": "A stimulator system that is implanted alongside the adrenal glands, allowing the user to trigger their body's adrenaline response at the cost of some bionic power.",
  "price": "40 USD",
  "weight": "250 g",
  "difficulty": 6
},
```

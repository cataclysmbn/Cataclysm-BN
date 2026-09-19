# 직업

## 직업

### 필드

| 식별자            | 설명                                                                                                                                                                         |
| ----------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| id                | (_필수_) 직업의 고유 ID                                                                                                                                                      |
| name              | (_필수_) `male` 및 `female` 이름이 있는 개체 또는 두 성별에 대한 문자열이 설정될 개체                                                                                        |
| description       | (_필수_) 직업 설명, 남녀 모두 동일                                                                                                                                           |
| vehicle           | (_선택 사항_) 직업을 시작할 차량 프로토타입 ID                                                                                                                               |
| pets              | (_선택적_) 애완동물 시작을 위한 `name`(몬스터 ID) 및 `amount`(몬스터 ID 수) 개체 배열                                                                                        |
| spells            | (_선택적_) 주문 시작을 위한 `id`(주문 ID) 및 `level`(주문 레벨) 개체의 배열입니다.                                                                                           |
| points            | (_필수_) 직업을 얻는 데 필요한 포인트 비용                                                                                                                                   |
| items             | (_필수_) 세 가지 다른 [항목 그룹](/mod/json/reference/items/item_spawn/#format) 또는 문자열 ID 배열(기술적으로 레거시). `male` 남성 전용 `female` 여성 전용 `both` 남녀 모두 |
| no_bonus          | (_선택 사항_) 배열은 직업 아이템 대체 보너스에서 이러한 아이템을 얻는 것을 방지합니다.                                                                                       |
| starting_cash     | (_선택 사항_) 직업이 시작되는 현금 금액                                                                                                                                      |
| skills            | (_선택 사항_) `level`(기술 수준) 및 `name`(기술 ID) 개체 배열                                                                                                                |
| addictions        | (_선택적_) `type`(중독 유형) 및 `intensity`(정수 중독 강도) 객체의 배열                                                                                                      |
| CBMs              | (_선택적_) 직업이 획득하는 생체공학에 대한 생체공학 ID 배열                                                                                                                  |
| traits            | (_선택 사항_) 직업이 얻는 특성에 대한 특성 ID 배열                                                                                                                           |
| forbidden_traits  | (_선택 사항_) 시작 시 선택할 수 없는 특성 ID 배열                                                                                                                            |
| allowed_traits    | (_선택 사항_) 시작 시 선택할 수 있는 비시작 특성에 대한 특성 ID 배열                                                                                                         |
| forbidden_bionics | (_선택 사항_) 시작 시 선택할 수 없는 생체 공학 ID 배열                                                                                                                       |
| allowed_bionics   | (_선택 사항_) 시작 시 선택할 수 있는 비시작 생체 공학에 대한 생체 공학 ID 배열                                                                                               |
| forbids_bionics   | (_선택 사항_) 시작 시 생체 공학을 선택하지 못하도록 하는 부울                                                                                                                |
| flags             | (_선택적_) [직업과 호환되는](/mod/json/reference/json_flags/#scenarios) 플래그 배열                                                                                          |
| missions          | (_선택 사항_) 시작할 임무 ID(및 임무)의 배열                                                                                                                                 |
| npcs              | (_선택 사항_) 시작할 npc 클래스 ID 배열(따라서 해당 클래스의 npcs)                                                                                                           |
| age               | (_선택 사항_) 시작 연령 범위에 대한 `min` 및 `max` 개체 또는 가능한 한 연령에 대한 int                                                                                       |

### 예

```json
{
  "type": "profession",
  "id": "wolfpack_mutant",
  "name": "Mutant Pack Leader",
  "description": "They treated you like an animal in that lab.  Now that you're free, you wonder if they had the right idea.  Your new friends don't seem afraid of you like they are of humans, after all.",
  "vehicle": "covered_wagon",
  "pets": [ { "name": "mon_wolf", "amount": 2 } ],
  "spells": [ { "id": "summon_zombie", "level": 1 }, { "id": "necrotic_gaze", "level": 1 } ],
  "points": 4,
  "items": { "both": [ "subsuit_xl" ], "male": [ "briefs" ], "female": [ "bra", "panties" ] },
  "no_bonus": [ "glasses_eye" ]
  "starting_cash": 0,
  "skills": [ { "level": 1, "name": "survival" } ],
  "addictions": [ { "intensity": 10, "type": "nicotine" } ],
  "CBMs": [ "bio_batteries", "bio_lockpick", "bio_fingerhack", "bio_power_storage_mkII" ],
  "traits": [ "LUPINE_FUR", "TAIL_FLUFFY", "LUPINE_EARS", "THRESH_LUPINE" ],
  "forbidden_traits": [ "CARRY_PERMIT" ],
  "allowed_traits": [ "MUZZLE" ],
  "forbidden_bionics": [ "bio_power_storage" ],
  "allowed_bionics": [ "bio_razors" ],
  "flags": [ "SCEN_ONLY" ],
  "missions": [ "MISSION_HEIST_DRIVER" ],
  "npcs": [ "NC_PROF_ESCAPIST_LAB", "NC_PROF_ESCAPIST_LAB", "NC_PROF_ESCAPIST_LAB" ],
  "age": { "min": 17, "max": 35 }
},
```

## 기술 레벨 기준

직업을 추가할 때 시작 기술 값에 적용하는 일반적인 기준입니다. 이는 대략적인 지침일 뿐이므로 필요하거나 재미있다고 판단되면 조정할 수 있습니다. 또한 캐릭터 생성 시 기술에 포인트를 배분할 수 있으므로, 배경 이야기만이 아니라 직업 자체를 기준으로 기술을 판단하세요.

0: 경험 없음
사회생활과 일반적인 일상 활동을 통해 얻는 기본 지식 수준입니다. 훈련받지는 않았지만 해당 주제에 대한 일반적인 인식은 있습니다.
예: 취미, 심화 과정, 직업 등을 고려하지 않은 평균적인 미국 고등학교 졸업자는 모든 기술이 0입니다.

1: 취미 수준
약간의 경험이 있는 취미인입니다.
예: 총기 잡지를 읽고 사격장에 한두 번 가 본 사람은 사격술 1을 가질 수 있습니다.

2: 열성자
약간의 경험이 있거나 해당 주제의 기본 훈련을 받았습니다.
예: 사격장에 어느 정도 자주 방문하고 사격장 교관에게 기본기를 배운 사람은 사격술 2를 가질 수 있습니다. 기본 훈련을 막 마친 병사는 훈련은 받았지만 전문 분야는 아니므로 응급 처치 2를 가질 수 있으며, 전투 의무병 같은 경우는 더 높은 응급 처치를 가질 수 있습니다.

3: 이전 단계에 경험을 더한 수준

4: 훈련됨
평균적인 전문화 수준으로 해당 기술을 훈련받았습니다. 준학사 학위 정도의 수준입니다.
예: 보병 훈련을 막 마친 병사는 사격술 4를 가질 수 있습니다.

5: 이전 단계에 경험을 더한 수준

6: 고급 훈련
고급 전문화 수준으로 해당 기술을 훈련받았습니다. 학사 학위 정도의 수준입니다.
예: 지정사수 훈련을 막 마친 병사는 사격술 6을 가질 수 있습니다.

7: 이전 단계에 경험을 더한 수준

8: 전문가
전문가 수준으로 해당 기술을 훈련받았습니다. 대학원 학위 정도의 수준입니다.
예: 저격수 훈련을 막 마친 병사는 사격술 8을 가질 수 있습니다. "average" 프로 운동선수는 운동 8을 가질 수 있습니다.

9: 이전 단계에 경험을 더한 수준

10: 베테랑
전문가 수준의 훈련을 받았고, 수년간의 경험과 해당 분야에서 다른 사람을 능가하는 재능을 갖췄습니다.
예: 유명 프로 운동선수는 운동 10을 가질 수 있습니다. 최정예 특수부대원은 사격술 10을 가질 수 있습니다. 유명 화학자는 요리 10을 가질 수 있습니다.

## 직업 아이템 대체

### 대체

#### 특성 기반

```jsonc
{
  "type": "profession_item_substitutions", // 필수 유형
  "trait": "WOOLALLERGY", // 꼭 필요한 특성
  "sub": [
    {
      "item": "blazer", // 교체할 품목
      "new": ["jacket_leather_red"], // 이를 대체하는 아이템입니다. 일대일 비율
    },
    {
      "item": "hat_hunting", // 교체할 품목
      "new": [{
        "item": "hat_cotton", // 이를 대체하는 아이템
        "ratio": 2, // 모든 항목에 대해 교체를 위해 2개를 제공합니다.
      }],
    },
  ],
}
```

위의 예에서. `WOOLALLERGY` 특성이 있습니다. `blazer`은 1 `jacket_leather_red`로 대체됩니다.
그리고 `hat_hunting`은 2개의 `hat_cotton` 항목으로 대체됩니다.

#### 아이템 기반

```jsonc
{
  "type": "profession_item_substitutions", // 필수 유형
  "item": "sunglasses", // 교체할 품목
  "sub": [
    {
      "present": [ "HYPEROPIC" ], // 이 특성을 모두 갖추어야 합니다.
      "absent": [ "MYOPIC" ], // 이런 특성을 가질 수 없습니다
      "new": [ "fitover_sunglasses" ] // 이것들로 대체됨
    },
    {
      "present": [ "MYOPIC" ], // 이 특성을 모두 갖추어야 합니다.
      "new": [ { "fitover_sunglasses", "ratio": 2 } ] // 또한 비율 대체도 지원합니다.
    }
  ]
}
```

따라서 `HYPEROPIC`이 있으면 핏오버 선글라스 1개를 얻게 됩니다.
그리고 `MYOPIC`은 `HYPEROPIC`에서도 모든 경우에 2개의 핏오버 선글라스를 제공합니다.

### 보너스

#### 아이템 그룹

```jsonc
{
  "type": "profession_item_substitutions", // 필요한 유형
  "item_group": "profession_carry_bonus",  // 선물할 아이템 그룹
  "bonus": { "present": [ "CARRY_PERMIT" ] } // 위와 같이 현재 및 부재 모두에서 작동합니다.
},
```

이는 아이템 그룹 `profession_carry_bonus`에서 `CARRY_PERMIT` 특성을 가진 캐릭터에게 무료 스폰을 부여합니다.

#### 아이템

```jsonc
{
  "type": "profession_item_substitutions", // 필요한 유형
  "item": "glasses_eye",                   // 선물할 아이템
  "bonus": { "present": [ "MYOPIC" ], "absent": [ "HYPEROPIC" ] } // 위와 같이 현재 작품과 부재 작품
},
```

이는 `MYOPIC` 특성을 가진 캐릭터에게 무료 아이템 `glasses_eye`을 부여하지만 `HYPEROPIC`는 부여하지 않습니다.

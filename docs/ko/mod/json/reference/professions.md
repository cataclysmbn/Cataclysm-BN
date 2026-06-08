---
title: 직업
---

> [!CAUTION]
>
> 이 문서는 최근 `JSON INFO`에서 분리되었으며, 모드가 기본 게임의 직업을 확장하고 삭제하는 오래된 방법을 언급하고 있어 업데이트가 필요합니다.

### 직업 아이템 대체

시작 특성에 따라 시작 아이템에 적용되는 아이템 교체를 정의합니다.
예를 들어 캐릭터가 양모 알레르기 특성으로 시작할 때 양모 아이템을 비양모 아이템으로 교체할 수 있습니다.

JSON 객체에 "item" 멤버가 포함되어 있으면 다음과 같이 주어진 아이템에 대한 교체를 정의합니다:

```json
{
  "type": "profession_item_substitutions",
  "item": "sunglasses",
  "sub": [
    { "present": [ "HYPEROPIC" ], "new": [ "fitover_sunglasses" ] },
    { "present": [ "MYOPIC" ], "new": [ { "fitover_sunglasses", "ratio": 2 } ] }
  ]
}
```

이것은 "sunglasses" 타입의 각 아이템이 다음으로 교체됨을 정의합니다:

- 캐릭터가 "HYPEROPIC" 특성을 가지고 있으면 "fitover_sunglasses" 아이템 하나로,
- 캐릭터가 "MYOPIC" 특성을 가지고 있으면 "fitover_sunglasses" 아이템 두 개로.

JSON 객체에 "trait" 멤버가 포함되어 있으면 캐릭터가 주어진 특성을 가지고 있을 때 적용되는 여러 아이템에 대한 교체를 정의합니다:

````json
{
  "type": "profession_item_substitutions",
  "trait": "WOOLALLERGY",
  "sub": [
    { "item": "blazer", "new": [ "jacket_leather_red" ] },
    { "item": "hat_hunting", "new": [ { "item": "hat_cotton", "ratio": 2 } ] }
  ]
}
```json
이것은 WOOLALLERGY 특성을 가진 캐릭터가 일부 아이템을 교체받도록 정의합니다:
- "blazer"는 "jacket_leather_red"로 변환되고,
- 각 "hat_hunting"은 *두* 개의 "hat_cotton" 아이템으로 변환됩니다.

### 직업

직업은 "type" 멤버가 "profession"으로 설정된 JSON 객체로 지정됩니다:

```json
{
    "type": "profession",
    "id": "hunter",
    ...
}
````

id 멤버는 직업의 고유 id여야 합니다.

다음 속성들이 지원됩니다(명시되지 않은 한 필수):

#### `description`

(string)

게임 내 설명.

#### `name`

(string 또는 "male"과 "female" 멤버를 가진 객체)

게임 내 이름, 성 중립적인 하나의 문자열이거나 성별에 따른 이름을 가진 객체.
예시:

```json
"name": {
    "male": "Groom",
    "female": "Bride"
}
```

#### `points`

(integer)

직업의 포인트 비용. 양수 값은 포인트를 소비하고 음수 값은 포인트를 부여합니다.

#### `addictions`

(선택적, 중독 배열)

시작 중독 목록. 목록의 각 항목은 다음 멤버를 가진 객체여야 합니다:

- "type": 중독의 문자열 id (json_flags.md 참조),
- "intensity": 중독의 강도 (정수).

예시:

```json
"addictions": [
    { "type": "nicotine", "intensity": 10 }
]
```

모드는 이 목록을 수정할 수 있습니다 (`"edit-mode": "modify"` 필요, 예시 참조). "add:addictions"와
"remove:addictions"를 통해 수정하며, 제거는 중독 타입만 필요합니다. 예시:

```json
{
  "type": "profession",
  "id": "hunter",
  "edit-mode": "modify",
  "remove:addictions": ["nicotine"],
  "add:addictions": [{ "type": "alcohol", "intensity": 10 }]
}
```

#### `skills`

(선택적, 스킬 레벨 배열)

시작 스킬 목록. 목록의 각 항목은 다음 멤버를 가진 객체여야 합니다:

- "name": 스킬의 문자열 id (skills.json 참조),
- "level": 스킬의 레벨 (정수). 이것은 캐릭터 생성에서 선택할 수 있는 스킬 레벨에 추가됩니다.

예시:

```json
"skills": [
    { "name": "archery", "level": 2 }
]
```

모드는 이 목록을 수정할 수 있습니다 (`"edit-mode": "modify"` 필요, 예시 참조). "add:skills"와
"remove:skills"를 통해 수정하며, 제거는 스킬 id만 필요합니다. 예시:

```json
{
  "type": "profession",
  "id": "hunter",
  "edit-mode": "modify",
  "remove:skills": ["archery"],
  "add:skills": [{ "name": "computer", "level": 2 }]
}
```

#### `items`

(선택적, 선택적 멤버 "both", "male", "female"을 가진 객체)

이 직업을 선택할 때 플레이어가 시작하는 아이템. 캐릭터의 성별에 따라 다른 아이템을 지정할 수 있습니다. 각 아이템 목록은 아이템 id의 배열이거나 아이템 id와 스니펫 id의 쌍이어야 합니다. 아이템 id는 여러 번 나타날 수 있으며, 이 경우 아이템이 여러 번 생성됩니다. 세 목록 모두 동일한 구문을 사용합니다.

예시:

```json
"items": {
    "both": [
        "pants",
        "rock",
        "rock",
        ["tshirt_text", "allyourbase"],
        "socks"
    ],
    "male": [
        "briefs"
    ],
    "female": [
        "panties"
    ]
}
```

이것은 플레이어에게 바지, 돌 두 개, 스니펫 id "allyourbase"를 가진 티셔츠(특별한 설명을 가짐), 양말, 그리고 (성별에 따라) 팬티 또는 브리프를 제공합니다.

모드는 기존 직업의 목록을 수정할 수 있습니다. 이를 위해서는 "edit-mode" 멤버가 "modify" 값을 가져야 합니다(예시 참조). "add:both" / "add:male" / "add:female"을 통해 목록에 아이템을 추가할 수 있습니다. 동일한 내용을 허용합니다(스니펫 id가 있는 아이템 추가 가능). "remove:both" / "remove:male" / "remove:female"을 통해 아이템을 제거하며, 아이템 id만 포함할 수 있습니다.

모드 예시:

```json
{
  "type": "profession",
  "id": "hunter",
  "edit-mode": "modify",
  "items": {
    "remove:both": ["rock", "tshirt_text"],
    "add:both": ["2x4"],
    "add:female": [["tshirt_text", "allyourbase"]]
  }
}
```

이 모드는 돌 하나를 제거하고(다른 돌은 여전히 생성됨), 티셔츠를 제거하고, 2x4 아이템을 추가하며, 여성 캐릭터에게 특별한 스니펫 id를 가진 티셔츠를 제공합니다.

#### `pets`

(선택적, 문자열 mtype_id 배열)

문자열 목록, 각각은 몬스터 id와 동일하며 플레이어는 이것들을 길들여진 애완동물로 시작합니다.

#### `vehicle`

(선택적, 문자열 vproto_id)

차량(vproto_id)과 동일한 문자열, 플레이어는 이것을 근처 차량으로 시작합니다. (가장 가까운 도로를 찾아 그곳에 배치한 다음 오버맵에 "기억됨"으로 표시합니다)

#### `flags`

(선택적, 문자열 배열)

플래그 목록. TODO: 여기에 플래그들을 문서화하세요.

모드는 `add:flags`와 `remove:flags`를 통해 이것을 수정할 수 있습니다.

#### `cbms`

(선택적, 문자열 배열)

캐릭터에 이식된 CBM id 목록.

모드는 `add:CBMs`와 `remove:CBMs`를 통해 이것을 수정할 수 있습니다.

#### `traits`

(선택적, 문자열 배열)

캐릭터에 적용되는 특성/돌연변이 id 목록.

모드는 `add:traits`와 `remove:traits`를 통해 이것을 수정할 수 있습니다.

#### `starting_cash`

(선택적, int)

이 직업이 대재앙 시작 시 가지게 될 돈의 양.

#### `npcs`

(선택적, 문자열 목록)

게임 시작 시 생성될 NPC. NPC 클래스의 ID를 나타내는 문자열로 사용됩니다.

#### `forbidden_traits`

(선택적, trait_id 목록)

직업 때문에 선택할 수 없는 특성.

#### `allowed_traits`

(선택적, trait_id 목록)

시작 특성인지 여부와 관계없이 선택할 수 있는 특성.

#### `forbidden_bionics`

(선택적, bionic_id 목록)

직업 때문에 선택할 수 없는 생체공학.

#### `allowed_bionics`

(선택적, bionic_id 목록)

시작 생체공학인지 여부와 관계없이 선택할 수 있는 생체공학.

#### `forbids_bionics`

(선택적, bool)

플레이어가 시작 시 생체공학을 선택하는 것을 금지합니다.

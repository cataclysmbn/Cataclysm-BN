# 아이템 스폰 시스템

## 컬렉션 또는 분배

컬렉션에서는 각 항목을 다른 항목과 독립적으로 선택합니다. 따라서 각 항목에 연결된 확률은 절대 확률이며 0...1 범위입니다. JSON 파일에서는 0부터 100까지의 백분율로 작성합니다.

확률이 0(또는 음수)이면 항목은 선택되지 않으며, 100%이면 항상 선택됩니다. 기본값이 100인 이유는 가장 유용한 값이기 때문입니다. 기본값이 0이면 해당 항목을 어차피 제거할 수 있으므로 작성할 필요가 없게 됩니다.

분배는 기존 시스템과 같은 가중치 목록입니다. 목록에서 정확히 하나의 항목을 선택합니다. 각 항목의 확률은 다른 항목의 확률에 대한 상대적인 값입니다. 확률이 0(또는 음수)이면 해당 항목은 선택되지 않습니다.

예를 들어 아이템 A의 확률이 30이고 아이템 B의 확률이 20이라고 합시다. A와 B의 네 가지 조합 확률은 다음과 같습니다.

| 조합            | 컬렉션 | 분배 |
| --------------- | ------ | ---- |
| A도 B도 없음    | 56%    | 0%   |
| A만 있음        | 24%    | 60%  |
| B만 있음        | 14%    | 40%  |
| A와 B 모두 있음 | 6%     | 0%   |

## 형식

형식은 다음과 같습니다.

```json
{
    "type": "item_group",
    "subtype": "<subtype>",
    "id": "<some name>",
    "ammo": <some number>,
    "magazine": <some number>,
    "purge": <true/false>,
    "delete": [ ... ],
    "entries": [ ... ]
}
```

`purge`는 선택 사항입니다. 아이템 그룹에 이전에 정의된 모든 내용을 제거할지 나타내는 불리언입니다. `subtype`도 선택 사항이며 `collection` 또는 `distribution`으로 지정할 수 있습니다. 지정하지 않으면 `old`가 기본값이며, 이 아이템 그룹이 이전 형식(기술적으로는 분배)을 사용한다는 뜻입니다.

`ammo` 또는 `magazine`을 사용할 때는 [몇 가지 주의사항](#탄약과-탄창)이 있습니다.

### entries 배열

`entries` 목록의 각 항목은 다음 형식 중 하나입니다.

1. 아이템
   ```json
   { "item": "<item-id>", ... }
   ```

2. 그룹
   ```json
   { "group": "<group-id>", ... }
   ```

3. 분배
   ```json
   {
     "distribution": [
       "이 네 가지 형식 중 하나에 해당하는 항목의 배열"
     ]
   }
   ```

4. 컬렉션
   ```json
   {
     "collection": [
       "이 네 가지 형식 중 하나에 해당하는 항목의 배열"
     ]
   }
   ```

게임은 `item` 또는 `group` 값의 존재 여부에 따라 항목이 아이템인지 다른 아이템 그룹에 대한 참조인지 결정합니다.

각 항목에는 추가 값을 지정할 수 있습니다(위에서 `...`으로 표시한 부분). 이를 사용하면 생성되는 아이템에 속성을 더 지정할 수 있습니다.

```json
"damage": <number>|<array>,
"damage-min": <number>,
"damage-max": <number>,
"count": <number>|<array>,
"count-min": <number>,
"count-max": <number>,
"charges": <number>|<array>,
"charges-min": <number>,
"charges-max": <number>,
"active": "<bool>"
"contents-item": "<item-id>" (문자열 또는 문자열 배열),
"contents-group": "<group-id>" (문자열 또는 문자열 배열),
"ammo-item": "<ammo-item-id>",
"ammo-group": "<group-id>",
"container-item": "<container-item-id>",
"container-group": "<group-id>",
```

`contents`는 생성된 아이템의 내용물로 추가됩니다. 내용물이 아이템 안에 들어갈 수 있는지는 확인하지 않습니다. 따라서 책을 넣은 물, 그 안에 철제 프레임, 그 안에 시체가 들어 있는 구조도 만들 수 있습니다.

`count`는 아이템 생성을 반복하여 더 많은 아이템을 만듭니다. 반복할 때마다 새 아이템을 생성합니다.

`charges`: 최솟값만 설정하고 최댓값을 설정하지 않으면 게임이 컨테이너 또는 탄약/탄창 용량에 따라 최댓값을 계산합니다. 최댓값을 용량보다 높게 설정하면 최대 용량으로 줄어듭니다. 최솟값을 설정하지 않고 최댓값만 설정하면 최솟값은 0으로 설정됩니다.

`active`: true이면 스폰할 때 아이템을 활성화합니다. 아이템 그룹에는 작동하지 않고, 아이템 그룹 안의 개별 아이템에만 작동합니다.

#### `active` 사용 예시

```json
[
  {
    "id": "test_grenade",
    "type": "item_group",
    "subtype": "collection",
    "items": [{ "item": "grenade_act", "prob": 100, "active": true }]
  },
  {
    "type": "mapgen",
    "method": "json",
    "om_terrain": "shelter_roof",
    "weight": 100,
    "object": {
      // ...
      "place_items": [{ "item": "test_grenade", "x": 12, "y": 12, "chance": 100, "repeat": 1 }],
      "place_item": [{ "item": "grenade_act", "x": 15, "y": 15, "chance": 100, "active": true }]
    }
  }
]
```

```json
"damage-min": 0,
"damage-max": 3,
"count": 4
"charges": [10, 100]
```

이렇게 하면 아이템 네 개가 생성됩니다. 각 아이템의 피해 값은 따로 굴리므로 손상 정도가 서로 다를 수 있습니다. 각 아이템은 10에서 100(양 끝 포함) 사이의 충전량(즉 탄약)을 가집니다. 아이템에 충전량을 넣기 전에 탄창이 필요하다면 자동으로 처리합니다. `charges`/`count`/`damage`에 두 항목으로 이루어진 배열을 사용하는 것은 명시적으로 최솟값과 최댓값을 쓰는 것과 같습니다. 즉, `"count": [a,b]`는 `"count-min": a, "count-max": b`와 같습니다.

컨테이너를 확인한 뒤 아이템을 안에 넣고, 아이템의 충전량을 컨테이너 크기에 맞게 제한하거나 늘립니다.

### delete 배열

`delete` 목록의 각 항목은 다음 중 하나입니다.

1. 아이템
   ```json
   { "item": "<item-id>" }
   ```

2. 그룹
   ```json
   { "group": "<group-id>" }
   ```

이 ID를 가진 기존 아이템은 목록에서 모두 제거됩니다. 존재하지 않아도 오류를 내지 않고 건너뜁니다(다른 모드가 이미 제거했을 가능성이 높기 때문입니다).

### 탄약과 탄창

아이템을 탄약/탄창과 함께 또는 없이 스폰하는 방법은 다음과 같습니다. `entries` 배열의 총과 탄창에는 `ammo-item`을 지정하여 기본값이 아닌 탄약 유형을 사용할 수 있습니다.

- 아이템 그룹 전체에 탄약/탄창 확률을 지정합니다. `ammo`는 항목이 완전히 장전된 상태로 스폰될 확률(%)이며, 탄창이 필요하면 자동으로 추가합니다. `magazine`은 탄창과 함께 스폰될 확률(%)입니다. 지정하지 않으면 둘 다 기본값은 0입니다.

  `ammo`와 `magazine`은 도구, 총, 탄창에만 적용됩니다. 항목에서 생성 시 탄약량(충전량)을 명시한 도구나, JSON 아이템 정의에 무작위 또는 0이 아닌 고정 초기 충전량이 있는 도구에는 적용되지 않습니다.

  아이템 그룹에서 다른 아이템 그룹을 참조하면 참조된 그룹의 탄약/탄창 확률은 무시되고 현재 그룹의 값이 사용됩니다.

- `entries` 배열에서 `charges`, `charges-min` 또는 `charges-max`를 사용합니다. 필요하면 기본 탄창이 자동으로 추가됩니다.

## 단축 형식

다음:

```json
"items": [ "<id-1>", [ "<id-2>", 10 ] ]
```

은 다음과 같습니다.

```json
"entries": [ { "item": "<id-1>" }, { "item": "<id-2>", "prob": 10 } ]
```

즉, 문자열 하나는 아이템 ID이고, 문자열과 숫자를 포함하는 배열은 아이템 ID와 확률입니다.

그룹도 같습니다.

```json
"groups": [ "<id-1>", [ "<id-2>", 10 ] ]
```

이 형식에서는 생성 아이템의 추가 속성을 지정할 수 없습니다. 확률은 컬렉션 항목에서만 생략할 수 있습니다.

`entries`, `items`, `groups`가 존재하면 그 내용은 모두 추가됩니다. 다음 예에서는 `<id-1>`이 아이템 그룹에 두 번 나타납니다.

```json
{
  "items": ["<id-1>"],
  "entries": [{ "item": "<id-1>" }]
}
```

다른 예로, `milk` 그룹은 `milk_containers`에서 컨테이너를 스폰하고 그 안에 우유를 넣습니다. `charges`를 지정하지 않았으므로 컨테이너에 들어갈 수 있는 최대량이 들어갑니다.

```json
{
    "type" : "item_group",
    "id": "milk_containers",
    "subtype": "distribution",
    "items": [
    "bottle_plastic", "bottle_glass", "flask_glass",
    "jar_glass", "jar_3l_glass", "flask_hip", "55gal_drum"
    ]
},
{
    "type" : "item_group",
    "id": "milk",
    "subtype": "distribution",
    "entries": [
        { "item": "milk", "container-group": "milk_containers" }
    ]
},
```

## 인라인 아이템 그룹

일부 위치에서는 그룹 ID를 지정하는 대신 아이템 그룹을 직접 정의할 수 있습니다. 표시되는 ID가 없으므로(내부적으로는 불특정 또는 무작위 ID를 사용) 다른 곳에서 참조할 수 없습니다. 해당 장소에만 특수하고 다른 곳에서는 사용되지 않을 그룹에 유용합니다.

예를 들어 몬스터 사망 드롭(`MONSTER` 객체의 `death_drops` 항목, [여기](../../json_info) 참조)에 사용할 수 있습니다. 특수 로봇이나 고유한 엔드게임 몬스터처럼 몬스터가 특정한 경우, 죽을 때 생성되는 아이템은 다른 그룹에 같은 형태로 나타나지 않습니다.

따라서 다음:

```json
{
    "type": "item_group",
    "id": "specific_group_id",
    "subtype": "distribution",
    "items": [ "a", "b" ]
},
{
    "death_drops": "specific_group_id"
}
```

은 다음과 같습니다.

```json
{
  "death_drops": {
    "subtype": "distribution",
    "items": ["a", "b"]
  }
}
```

인라인 그룹은 다른 그룹과 같은 방식으로 읽히며 위에서 설명한 모든 속성을 사용할 수 있습니다. `type`과 `id` 멤버는 항상 무시됩니다.

전체 JSON 객체 대신 JSON 배열을 쓸 수도 있습니다. 이 경우 기본 subtype을 사용하고 배열을 `entries` 배열처럼 읽습니다. 배열의 각 항목은 JSON 객체여야 합니다.

```json
{
  "death_drops": [
    { "item": "rag", "damage": 2 },
    { "item": "bowling_ball" }
  ]
}
```

---

### 참고

#### 테스트

게임에서 아이템 그룹을 테스트할 수 있습니다.

1. 게임을 불러오고 디버그 메뉴를 엽니다.
   > 팁: 디버그 메뉴에 키가 지정되지 않았거나 잊었다면 <kbd>ESC</kbd>를 누른 뒤 <kbd>1</kbd>을 누르세요.

2. `Test Item Group`을 선택합니다.

3. 디버깅할 아이템 그룹을 선택합니다.

   게임은 해당 그룹의 아이템을 100번 스폰하고 생성된 아이템을 셉니다. 결과는 빈도순으로 정렬되어 표시됩니다.

   > 팁: 디버그 메뉴에서 <kbd>/</kbd>를 사용해 무엇이든 필터링할 수 있습니다.

아이템을 `EMPTY_GROUP`에 추가하지 마세요. 이 그룹은 그룹 ID가 필요하지만 아이템을 스폰하고 싶지 않을 때 사용하며, 이 그룹에서는 아이템이 스폰되지 않습니다.

#### SUS

아이템 그룹에 아이템을 추가할 때는 **SUS 아이템 그룹**을 찾거나 만들어 보세요. SUS 아이템 그룹은 특정 수납 가구에서 스폰될 법한 아이템을 현실적인 비율로 모은 컬렉션입니다. SUS는 "specific use storage"(특정 용도 수납)의 약자입니다. 아이템 그룹을 특정 용도 수납별로 구성하는 목적 중 하나는 유지·확장하기 쉬운 재사용 가능한 표를 장려하는 것입니다.

SUS 아이템 그룹은 `/data/json/itemgroups/SUS`에서 찾을 수 있습니다.

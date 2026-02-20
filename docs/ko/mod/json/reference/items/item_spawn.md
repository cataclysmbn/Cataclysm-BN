# 아이템 스폰

아이템 스폰은 게임에서 아이템이 나타나는 위치와 방법을 정의합니다. 이는 아이템 그룹, 스폰 확률, 맵 배치를 통해 제어됩니다.

## 아이템 그룹

아이템 그룹은 함께 스폰할 수 있는 아이템의 모음입니다. 전리품 테이블, 스폰 풀 및 무작위 생성에 사용됩니다.

### 기본 아이템 그룹 구조

```json
{
  "type": "item_group",
  "id": "guns_pistol_common",
  "items": [
    ["glock_19", 50],
    ["sig_mosquito", 30],
    ["walther_ppq", 20]
  ]
}
```

### 필드

| 필드     | 타입   | 설명                                                              |
| -------- | ------ | ----------------------------------------------------------------- |
| `type`   | string | 반드시 `"item_group"`이어야 합니다.                               |
| `id`     | string | 이 아이템 그룹의 고유 식별자.                                     |
| `items`  | array  | 아이템과 가중치의 배열. 각 항목은 `[item_id, weight]` 형식입니다. |
| `groups` | array  | 선택 사항. 포함할 다른 아이템 그룹.                               |

### 예시: 의료용품 그룹

```json
{
  "type": "item_group",
  "id": "medical_supplies",
  "items": [
    ["bandages", 40],
    ["disinfectant", 30],
    ["aspirin", 20],
    ["first_aid", 10]
  ]
}
```

## 컬렉션 vs 분배

아이템 그룹은 두 가지 모드로 작동할 수 있습니다:

### 컬렉션 (Collection)

컬렉션에서 각 아이템은 가중치에 따라 독립적으로 스폰됩니다.

```json
{
  "type": "item_group",
  "subtype": "collection",
  "id": "zombie_drops",
  "entries": [
    { "item": "cash_card", "prob": 5 },
    { "item": "id_military", "prob": 1 },
    { "item": "glass_shard", "prob": 80 }
  ]
}
```

### 분배 (Distribution)

분배에서 하나의 아이템만 가중 무작위 선택에 따라 스폰됩니다.

```json
{
  "type": "item_group",
  "subtype": "distribution",
  "id": "tools_common",
  "entries": [
    { "item": "hammer", "prob": 30 },
    { "item": "screwdriver", "prob": 40 },
    { "item": "wrench", "prob": 30 }
  ]
}
```

## 확률

확률은 `prob` 필드를 사용하여 지정할 수 있습니다:

```json
{
  "item": "rare_artifact",
  "prob": 1
}
```

더 높은 `prob` 값은 아이템이 스폰될 가능성이 더 높다는 것을 의미합니다.

## 아이템 스폰 위치

아이템 스폰 위치는 `mapgen` 정의에 지정됩니다:

```json
{
  "type": "mapgen",
  "method": "json",
  "om_terrain": "house_01",
  "object": {
    "place_items": [
      { "item": "bedroom", "x": 5, "y": 5, "chance": 70 }
    ]
  }
}
```

### place_items 필드

| 필드     | 타입    | 설명                                  |
| -------- | ------- | ------------------------------------- |
| `item`   | string  | 스폰할 아이템 그룹 ID.                |
| `x`, `y` | integer | 타일 좌표 (또는 영역에 대한 범위).    |
| `chance` | integer | 각 아이템이 스폰될 확률 (0-100).      |
| `repeat` | integer | 선택 사항. 스폰 시도 횟수. 기본값: 1. |

## 아이템 스폰 예시

### 예시 1: 부엌 아이템

```json
{
  "type": "item_group",
  "id": "kitchen_items",
  "items": [
    ["knife_butcher", 20],
    ["pot", 30],
    ["pan", 30],
    ["bowl_plastic", 40],
    ["cup_plastic", 40]
  ]
}
```

### 예시 2: 총기 상점 무기

```json
{
  "type": "item_group",
  "subtype": "distribution",
  "id": "guns_gunstore_common",
  "entries": [
    { "item": "glock_19", "prob": 30 },
    { "item": "mossberg_500", "prob": 25 },
    { "item": "remington_870", "prob": 25 },
    { "item": "ar15", "prob": 15 },
    { "item": "ruger_1022", "prob": 35 }
  ]
}
```

### 예시 3: 희귀 전리품

```json
{
  "type": "item_group",
  "subtype": "collection",
  "id": "rare_artifacts",
  "entries": [
    { "item": "sword_legendary", "prob": 1, "count": 1 },
    { "item": "armor_artifact", "prob": 1, "count": 1 },
    { "item": "ring_power", "prob": 5, "count": 1 }
  ]
}
```

## 중첩된 그룹

아이템 그룹은 다른 아이템 그룹을 포함할 수 있습니다:

```json
{
  "type": "item_group",
  "id": "survivor_gear",
  "groups": [
    ["clothing_survivor", 50],
    ["tools_survival", 40],
    ["weapons_survival", 30]
  ]
}
```

## 수량 조절자

스폰된 아이템의 수량을 지정할 수 있습니다:

```json
{
  "item": "9mm",
  "prob": 50,
  "count": [10, 30]
}
```

이것은 10-30개의 9mm 탄약을 스폰합니다.

## 충전량 조절자

아이템의 충전량을 설정할 수 있습니다:

```json
{
  "item": "flashlight",
  "prob": 40,
  "charges": [20, 80]
}
```

이것은 20-80% 충전된 손전등을 스폰합니다.

## 손상 조절자

아이템의 손상 상태를 설정할 수 있습니다:

```json
{
  "item": "jeans",
  "prob": 30,
  "damage": [1, 3]
}
```

이것은 약간 손상된 청바지를 스폰합니다.

## 컨테이너 조절자

아이템을 컨테이너에 스폰할 수 있습니다:

```json
{
  "item": "aspirin",
  "prob": 20,
  "container-item": "bottle_plastic_pill_small"
}
```

이것은 플라스틱 약병에 든 아스피린을 스폰합니다.

## 조건부 스폰

조건에 따라 아이템을 스폰할 수 있습니다:

```json
{
  "item": "hazmat_suit",
  "prob": 10,
  "condition": "has_effect_radiation"
}
```

## 맵 특수(Map Specials)

맵 특수는 지도 생성 중 특수 배치를 정의합니다:

```json
{
  "type": "map_special",
  "id": "military_crash_site",
  "spawns": {
    "group": "mon_zombie_soldier",
    "density": 0.5
  },
  "items": {
    "item_groups": ["military_gear"]
  }
}
```

## 지역 설정(Region Settings)

지역 설정은 다양한 지역의 아이템 스폰을 제어합니다:

```json
{
  "type": "region_settings",
  "id": "default",
  "field_coverage": {
    "percent_coverage": 0.5,
    "default_ter": "t_grass"
  }
}
```

## 전리품 구역(Loot Zones)

전리품 구역은 자동 정렬 및 전리품 수집을 정의합니다:

```json
{
  "type": "LOOT_ZONE",
  "id": "LOOT_FOOD",
  "name": "음식 구역"
}
```

## 디버깅 아이템 스폰

아이템 스폰을 디버깅하려면:

1. 디버그 메뉴를 사용하여 특정 아이템 그룹 스폰
2. 아이템 그룹 ID 및 확률 확인
3. 스폰 위치 및 빈도 검증
4. 가중치 및 확률 테스트

## 모범 사례

1. **균형잡힌 가중치** - 아이템 확률이 균형잡히고 공정한지 확인
2. **테마 그룹** - 논리적 위치에 대한 테마 아이템 그룹 생성
3. **희소성 제어** - 적절한 가중치로 강력한 아이템을 희귀하게 만들기
4. **중첩 사용** - 재사용성을 위해 아이템 그룹을 논리적으로 구조화
5. **테스트** - 게임에서 스폰 빈도 및 분배 테스트

## 일반적인 실수

- 가중치가 너무 높아 일반적인 아이템이 됨
- 아이템 그룹 ID를 잘못 참조함
- 스폰 확률을 테스트하지 않음
- 조건부 스폰을 잊음
- 손상/충전량 범위를 고려하지 않음

## 참고사항

- 아이템 그룹은 `data/json/itemgroups/` 디렉토리에 정의됩니다.
- 스폰 위치는 `data/json/mapgen/` 디렉토리에 정의됩니다.
- 확률은 상대적 가중치입니다 (절대 백분율이 아님).
- 아이템은 여러 그룹에 나타날 수 있습니다.
- 그룹은 다른 그룹을 참조하여 복잡한 전리품 테이블을 만들 수 있습니다.

## 추가 자료

- [맵 생성 문서](../../../game/mapgen.md)
- [아이템 정의](../../items/items.md)
- [JSON 상속](json_inheritance.md)

# JSON INFO

> [!NOTE]
>
> 이 문서는 여러 페이지로 분할되어 있습니다.

> [!CAUTION]
>
> 많은 JSON 파일이 아직 문서화되지 않았거나 오래되었습니다. 관련 소스 파일을 확인하십시오.
> 물론이죠.

이 문서는 Cataclysm: Bright Nights에서 사용되는 json 파일의 내용을 설명합니다. Cataclysm: Bright Nights의 콘텐츠를 추가하거나 변경하고 어디에서 무엇을 찾아야 하는지, 각 파일과 속성이 무엇을 하는지 알아야 한다면 이 문서를 읽으면 됩니다.
각 파일과 속성의 위치와 기능에 대해 자세히 알아보세요.

## JSON 탐색하기

많은 JSON에는 다른 JSON 엔터티에 대한 상호 참조가 포함됩니다. 더 쉽게 탐색할 수 있도록,
우리는 `tags` 파일을 빌드할 수 있는 `tools/json_tools/cddatags.py` 스크립트를 제공합니다.

스크립트를 실행하려면 Python 3이 필요합니다. Windows에서는 아마도 Python 3을 설치해야 할 것입니다.
`.py` 파일을 Python과 연결합니다. 그런 다음 명령 프롬프트를 열고 Cataclysm: Bright Nights 폴더로 이동하여 다음을 실행합니다.
`tools\json_tools\cddatags.py`.

이 기능을 사용하려면 편집기에 [ctags 지원](http://ctags.sourceforge.net/)이 필요합니다. 언제
작동합니다. 어떤 엔터티의 정의로 쉽게 이동할 수 있어야 합니다. 예를 들어,
커서를 ID 위에 놓고 적절한 키 조합을 누르십시오.

- Vim에서는 이 기능이 기본적으로 존재하며 다음을 사용하여 정의로 이동할 수 있습니다.
  [`^\]`](http://vimdoc.sourceforge.net/htmldoc/tagsrch.html#tagsrch.txt).
- Notepad++에서 "Plugins" -> "Plugins Admin"로 이동하여 "TagLEET" 플러그인을 활성화합니다. 그런 다음 아무거나 선택하세요.
  id를 입력하고 Alt+Space를 눌러 참조 창을 엽니다.

## `type` 속성

항목은 `type` 속성으로 구별됩니다. 이 속성은 모든 항목에 필수입니다.
예를 들어 이 항목을 'armor'로 설정하면 게임이 갑옷과 관련된 속성을 기대한다는 의미입니다.
그 항목. 또한 원하는 경우 [`copy-from`](./items/json_inheritance.md#copy-from)과 연결됩니다.
[다른 객체의 속성 상속](./items/json_inheritance.md), 동일한 객체여야 합니다.

## 서식 지정

JSON 파일을 편집할 때 아래와 같이 올바른 형식을 적용했는지 확인하세요.

### 지속 시간

하나 이상의 숫자 및 기간 단위 쌍을 포함하는 문자열입니다. 숫자와 단위는 물론이고
각 쌍은 임의의 공백으로 구분될 수 있습니다. 사용 가능한 단위:

- "hours", "hour", "h" - 1시간
- "days", "day", "d" - 하루
- "minutes", "minute", "m" - 1분
- "turns", "turn", "t" - 한 턴,

예:

- " +1일 -23시간 50분 " `(1*24*60 - 23*60 + 50 == 110 minutes)`
- "1 turn 1 minutes 9 turns" (1회전이 1초이므로 1분 10초)

### 기타 형식

```json
"//" : "comment", // json 파일 내에 주석을 남기는 데 선호되는 방법입니다.
```

항목 이름, 설명 등과 같은 일부 json 문자열이 번역을 위해 추출됩니다.
정확한 추출은 `lang/extract_json_strings.py`에서 처리됩니다. 뻔한 글쓰기 방식과는 별개로
번역 컨텍스트가 없는 문자열인 경우 문자열은 선택적 번역 컨텍스트를 가질 수도 있습니다(그리고
때로는 복수형) 다음과 같이 작성합니다.

```json
"name": { "ctxt": "foo", "str": "bar", "str_pl": "baz" }
```

또는 복수형이 단수형과 동일한 경우:

```json
"name": { "ctxt": "foo", "str_sp": "foo" }
```

아래와 같이 "//~" 항목을 추가하여 번역자를 위한 설명을 추가할 수도 있습니다. 순서는
항목은 중요하지 않습니다.

```json
"name": {
    "//~": "as in 'foobar'",
    "str": "bar"
}
```

[현재 일부 JSON 값만 이 구문을 지원합니다](./../../../i18n/reference/translation#supported-json-values).

## 각 JSON 파일의 설명과 내용

이 섹션에서는 각 json 파일과 해당 내용을 설명합니다. 각 json에는 고유한 속성이 있습니다.
다른 Json 파일과 공유되지 않는 파일(예: 책에 사용되는 'chapters' 속성은
갑옷에 적용). 이렇게 하면 속성이 다음 컨텍스트 내에서만 설명되고 사용됩니다.
적절한 JSON 파일.

## `data/json/` JSON

### Ascii_arts

| 식별자  | 설명                                                                         |
| ------- | ---------------------------------------------------------------------------- |
| id      | 고유 ID. 하나의 연속된 단어여야 하며 필요한 경우 밑줄을 사용하세요.          |
| picture | 문자열 배열, 각 항목은 ASCII 그림의 한 줄이며 길이는 최대 42열이어야 합니다. |

```json
{
  "type": "ascii_art",
  "id": "cashcard",
  "picture": [
    "",
    "",
    "",
    "       <color_white>╔═══════════════════╗",
    "       <color_white>║                   ║",
    "       <color_white>║</color> <color_yellow>╔═   ╔═╔═╗╔═║ ║</color>   <color_white>║",
    "       <color_white>║</color> <color_yellow>║═ ┼ ║ ║═║╚╗║═║</color>   <color_white>║",
    "       <color_white>║</color> <color_yellow>╚═   ╚═║ ║═╝║ ║</color>   <color_white>║",
    "       <color_white>║                   ║",
    "       <color_white>║   RIVTECH TRUST   ║",
    "       <color_white>║                   ║",
    "       <color_white>║                   ║",
    "       <color_white>║ 555 993 55221 066 ║",
    "       <color_white>╚═══════════════════╝"
  ]
}
```

### 신체 부위

| 식별자            | 설명                                                                                                                                                                                                                                                                                                    |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| id                | (_필수_) 고유 ID입니다. 하나의 연속된 단어여야 하며 필요한 경우 밑줄을 사용하세요.                                                                                                                                                                                                                      |
| name              | (_필수_) 게임 내 이름이 표시됩니다.                                                                                                                                                                                                                                                                     |
| accusative        | (_필수_) 이 본문 부분에 대한 대격형입니다.                                                                                                                                                                                                                                                              |
| heading           | (_필수_) 제목에 표시되는 방법입니다.                                                                                                                                                                                                                                                                    |
| heading_multiple  | (_선택 사항_) 표제의 복수형입니다. (기본값: 제목 값)                                                                                                                                                                                                                                                    |
| hp_bar_ui_text    | (_선택 사항_) 패널의 HP 바 옆에 표시되는 방식입니다. (기본값: 빈 문자열)                                                                                                                                                                                                                                |
| encumbrance_text  | (_선택적_) 방해가 되었을 때의 효과에 대한 설명입니다. (기본값: 빈 문자열)                                                                                                                                                                                                                               |
| main_part         | (_선택적_) 이것이 붙어 있는 주요 부분은 무엇입니까? (기본값: 자기)                                                                                                                                                                                                                                      |
| base_hp           | (_선택 사항_) 수정 전 이 부품의 HP 양입니다. (기본값: `60`)                                                                                                                                                                                                                                             |
| opposite_part     | (_선택 사항_) 쌍인 경우 이것의 반대 부분은 무엇입니까? (기본값: 자기)                                                                                                                                                                                                                                   |
| essential         | (_선택 사항_) 이 부분의 HP가 `0`으로 떨어지면 캐릭터가 죽는지 여부입니다.                                                                                                                                                                                                                               |
| hit_size          | (_선택적_) 부동. 가중치가 적용되지 않은 선택을 수행할 때 신체 부분의 크기입니다. (기본값: `0.`)                                                                                                                                                                                                         |
| hit_size_relative | (_선택적_) 부동. 더 작고, 크기가 동일하며, 더 큰 공격자의 히트 크기입니다. (기본값: `[ 0, 0, 0 ]`                                                                                                                                                                                                       |
| hit_difficulty    | (_선택적_) 부동. "owner"가 맞았다고 가정할 때 특정 신체 부위를 치는 것이 얼마나 어려운가요? 숫자가 높을수록 좋은 타격이 이 부분으로 향할 것임을 의미하고, 낮을수록 이 부분이 부정확한 공격에 맞을 가능성이 낮다는 것을 의미합니다. 수식은 `chance *= pow(hit_roll, hit_difficulty)`입니다(기본값: `0`). |
| side              | (_선택 사항_) 이 신체 부위가 어느 쪽에 있는지. 둘 다 기본값입니다.                                                                                                                                                                                                                                      |
| stylish_bonus     | (_선택 사항_) 이 부분에 화려한 옷을 입으면 기분 보너스가 부여됩니다. (기본값: `0`)                                                                                                                                                                                                                      |
| hot_morale_mod    | (_선택 사항_) 이 부분에 너무 뜨거워지는 기분 효과입니다. (기본값: `0`)                                                                                                                                                                                                                                  |
| cold_morale_mod   | (_선택 사항_) 이 부분에 너무 차가워지는 기분 효과입니다. (기본값: `0`)                                                                                                                                                                                                                                  |
| bionic_slots      | (_선택 사항_) 이 부품에는 생체 공학 슬롯이 몇 개 있습니까?                                                                                                                                                                                                                                              |

```json
{
  "id": "torso",
  "type": "body_part",
  "name": "torso",
  "accusative": { "ctxt": "bodypart_accusative", "str": "torso" },
  "heading": "Torso",
  "heading_multiple": "Torso",
  "hp_bar_ui_text": "TORSO",
  "encumbrance_text": "Dodging and melee is hampered.",
  "main_part": "torso",
  "opposite_part": "torso",
  "hit_size": 45,
  "hit_size_relative": [20, 33.33, 36.57],
  "hit_difficulty": 1,
  "side": "both",
  "legacy_id": "TORSO",
  "stylish_bonus": 6,
  "hot_morale_mod": 2,
  "cold_morale_mod": 2,
  "base_hp": 60,
  "bionic_slots": 80
}
```

### 꿈

| 식별자   | 설명                                                            |
| -------- | --------------------------------------------------------------- |
| messages | 잠재적인 꿈의 목록입니다.                                       |
| category | 꿈을 꾸기 위해서는 돌연변이 카테고리가 필요합니다.              |
| strength | 돌연변이 범주 강도가 필요합니다(1 = 20-34, 2 = 35-49, 3 = 50+). |

````json
{
  "messages": [
    "You have a strange dream about birds.",
    "Your dreams give you a strange feathered feeling."
  ],
  "category": "MUTCAT_BIRD",
  "strength": 1
}

### Item Category

When you sort your inventory by category, these are the categories that are displayed.
| Identifier      | Description
|---              |---
| id              | Unique ID. Must be one continuous word, use underscores if necessary
| name            | The name of the category. This is what shows up in-game when you open the inventory.
| zone            | The corresponding loot_zone (see loot_zones.json)
| sort_rank       | Used to sort categories when displaying.  Lower values are shown first
| priority_zones  | When set, items in this category will be sorted to the priority zone if the conditions are met. If the user does not have the priority zone in the zone manager, the items get sorted into zone set in the 'zone' property. It is a list of objects. Each object has 3 properties: ID: The id of a LOOT_ZONE (see LOOT_ZONES.json), filthy: boolean. setting this means filthy items of this category will be sorted to the priority zone, flags: array of flags
|spawn_rate       | Sets amount of items from item category that might spawn.  Checks for `spawn_rate` value for item category.  If `spawn_chance` is less than 1.0, it will make a random roll (0.1-1.0) to check if the item will have a chance to spawn.  If `spawn_chance` is more than or equal to 1.0, it will add a chance to spawn additional items from the same category.  Items will be taken from item group which original item was located in.  Therefore this parameter won't affect chance to spawn additional items for items set to spawn solitary in mapgen (e.g. through use of `item` or `place_item`).

```C++
{
  "id":"armor",
  "name": "ARMOR",
  "zone": "LOOT_ARMOR",
  "sort_rank": -21,
  "priority_zones": [ { "id": "LOOT_FARMOR", "filthy": true, "flags": [ "RAINPROOF" ] } ],
}
````

### 질병

| 식별자             | 설명                                                                          |
| ------------------ | ----------------------------------------------------------------------------- |
| id                 | 고유 ID. 하나의 연속된 단어여야 하며 필요한 경우 밑줄을 사용하세요.           |
| min_duration       | 질병이 지속될 수 있는 최소 기간. 문자열 "x m", "x s","x d"를 사용합니다.      |
| max_duration       | 질병이 지속될 수 있는 최대 기간.                                              |
| min_intensity      | 질병에 의해 적용되는 효과의 최소 강도                                         |
| max_intensity      | 효과의 최대 강도입니다.                                                       |
| health_threshold   | 질병에 면역이 되는 체력의 양입니다. -200에서 200 사이여야 합니다. (선택 사항) |
| symptoms           | 질병에 의해 적용되는 효과.                                                    |
| affected_bodyparts | 효과가 적용되는 신체 부위 목록입니다. (선택사항, 기본값은 num_bp)             |

```json
{
  "type": "disease_type",
  "id": "bad_food",
  "min_duration": "6 m",
  "max_duration": "1 h",
  "min_intensity": 1,
  "max_intensity": 1,
  "affected_bodyparts": ["TORSO"],
  "health_threshold": 100,
  "symptoms": "foodpoison"
}
```

### 이름

```json
{ "name" : "Aaliyah", "gender" : "female", "usage" : "given" }, // 이름, 성별, "given"/"family"/"city"(이름/성/도시 이름).
```

### 명명된 색상

```json
{
  "type": "named_color", // 예상대로
  "name": "Cataclysm Red", // 표시할 이름
  "value": "#622625" // 색상의 16진수 값
}
```

### 향기 유형

| 식별자            | 설명                                                                              |
| ----------------- | --------------------------------------------------------------------------------- |
| id                | 고유 ID. 하나의 연속된 단어여야 하며 필요한 경우 밑줄을 사용하세요.               |
| receptive_species | 이 냄새를 추적할 수 있는 종. `species.json`에 정의된 유효한 ID를 사용해야 합니다. |

```json
{
  "type": "scent_type",
  "id": "sc_flower",
  "receptive_species": ["MAMMAL", "INSECT", "MOLLUSK", "BIRD"]
}
```

### 점수 및 성과

점수는 _events_를 기준으로 2~3단계로 정의됩니다. 어떤 이벤트가 존재하는지, 어떤 데이터가 있는지 확인하려면
여기에는 [`event.h`](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/src/event.h)이 포함되어 있습니다. 읽어보세요.

각 이벤트에는 특정 필드 집합이 포함되어 있습니다. 각 필드에는 문자열 키와 `cata_variant` 값이 있습니다.
필드는 이벤트에 대한 모든 관련 정보를 제공해야 합니다.

예를 들어 `gains_skill_level` 이벤트를 생각해 보세요. 이 사양은 다음에서 볼 수 있습니다.
`event.h`:

```json
template<>
struct event_spec<event_type::gains_skill_level> {
    static constexpr std::array<std::pair<const char *, cata_variant_type>, 3> fields = {{
            { "character", cata_variant_type::character_id },
            { "skill", cata_variant_type::skill_id },
            { "new_level", cata_variant_type::int_ },
        }
    };
};
```

여기에서 이 이벤트 유형에 세 가지 필드가 있음을 알 수 있습니다.

- `character`, 레벨을 획득한 캐릭터의 ID입니다.
- `skill`, 획득한 스킬의 ID입니다.
- `new_level`, 해당 스킬에서 새로 획득한 정수 레벨입니다.

이벤트는 게임 내 상황에 따라 게임에서 생성됩니다. 이러한 이벤트는 변형될 수 있습니다.
그리고 다양한 방법으로 요약했습니다. 관련된 세 가지 개념은 이벤트 스트림, 이벤트 통계,
그리고 점수.

- 게임에서 정의한 각 `event_type`는 이벤트 스트림을 생성합니다.
- 기존 이벤트에 `event_transformation`를 적용하여 json에서 추가 이벤트 스트림을 정의할 수 있습니다.
  이벤트 스트림.
- `event_statistic`은 이벤트 스트림을 단일 값(보통 숫자이지만 다른 값)으로 요약합니다.
  값 유형이 가능합니다).
- `score`는 이러한 통계를 사용하여 플레이어가 볼 수 있는 게임 내 점수를 정의합니다.

#### `event_transformation`

`event_transformation`은 이벤트 스트림을 수정하여 다른 이벤트 스트림을 생성할 수 있습니다.

변환될 입력 스트림은 다음 중 하나를 사용하기 위해 `"event_type"`로 지정됩니다.
다른 json 정의 변환을 사용하기 위한 내장 이벤트 유형 스트림 또는 `"event_transformation"`
이벤트 스트림.

이벤트 스트림에 다음 변경 사항 중 일부 또는 전부를 적용할 수 있습니다.

- 이벤트 필드 변환을 기반으로 각 이벤트에 새 필드를 추가합니다. 이벤트 필드 변환
  에서 찾을 수 있습니다
  [`event_field_transformation.cpp`](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/src/event_field_transformations.cpp).
- 이벤트의 일부 하위 집합을 포함하는 스트림을 생성하기 위해 포함된 값을 기반으로 이벤트를 필터링합니다.
  입력 스트림.
- 출력 스트림에 관심이 없는 일부 필드를 삭제합니다.

각 수정 사항의 예는 다음과 같습니다.

```json
"id": "avatar_kills_with_species",
"type": "event_transformation",
"event_type": "character_kills_monster", // 변환은 이 유형의 이벤트에 작용합니다.
"new_fields": { // 이벤트에 추가할 새 필드 사전
    // 키는 새 필드 이름입니다. 값은 한 요소의 사전이어야 합니다.
    "species": {
        // 키는 적용할 event_field_transformation을 지정합니다. 값은 지정합니다
        // 해당 변환에 값을 제공해야 하는 입력 필드입니다.
        // 따라서 이 경우에는 'species'라는 새 필드를 추가합니다.
        // 이 살해 사건의 희생자 종족을 포함합니다.
        "species_of_monster": "victim_type"
    }
}
```

```json
"id": "moves_on_horse",
"type": "event_transformation",
"event_type" : "avatar_moves", // 이벤트 유형입니다.  변환은 이 유형의 이벤트에 적용됩니다.
"value_constraints" : { // 제약 조건 사전
    // 각 키는 제약조건이 적용되는 필드입니다.
    // 값은 제약 조건을 지정합니다.
    // "equals"은 필드가 사용해야 하는 상수 문자열 값을 지정하는 데 사용할 수 있습니다.
    // "equals_statistic"은 값이 일부 통계 값과 일치해야 함을 지정합니다(아래 참조).
    "mount" : { "equals": "mon_horse" }
}
// 'mount'가 'mon_horse'인 이벤트만 필터링하므로
// 유용한 정보를 제공하지 않으므로 'mount' 필드를 삭제하는 것이 좋습니다.
"drop_fields" : [ "mount" ]
```

#### `event_statistic`

`event_transformation`과 마찬가지로 `event_statistic`에도 입력 이벤트 스트림이 필요합니다. 그 입력
스트림은 다음 두 가지 중 하나를 통해 `event_transformation`과 동일하게 지정할 수 있습니다.
항목:

```json
"event_type" : "avatar_moves" // 이 내장 유형의 이벤트
"event_transformation" : "moves_on_horse" // 이 json 정의 변환으로 인해 발생하는 이벤트
```

그런 다음 특정 `stat_type` 및 잠재적으로 다음과 같은 추가 세부 정보를 지정합니다.

이벤트 수:

```json
"stat_type" : "count"
```

모든 이벤트에 걸쳐 지정된 필드의 숫자 값 합계:

```json
"stat_type" : "total"
"field" : "damage"
```

모든 이벤트에서 지정된 필드에 있는 숫자 값의 최대값:

```json
"stat_type" : "maximum"
"field" : "damage"
```

모든 이벤트에서 지정된 필드에 있는 숫자 값의 최소값:

```json
"stat_type" : "minimum"
"field" : "damage"
```

고려해야 할 이벤트가 하나만 있다고 가정하고 해당 이벤트에 대해 지정된 필드의 값을 사용합니다.
독특한 이벤트:

```json
"stat_type": "unique_value",
"field": "avatar_id"
```

`stat_type`에 관계없이 각 `event_statistic`에는 다음이 포함될 수도 있습니다.

```json
// 점수 및 성취 요건을 설명하는 데 사용됩니다.
"description": "Number of things"
```

#### `score`

점수는 단순히 점수 표 형식화를 위해 설명을 이벤트에 연결합니다. 는
`description`은 `%s` 형식 지정자를 포함할 것으로 예상되는 문자열을 지정합니다.
통계값이 삽입됩니다.

대부분의 통계가 정수를 산출하더라도 `%s`을 계속 사용해야 합니다.

기본 통계에 설명이 있는 경우 점수 설명은 선택 사항입니다. 기본값
"<statistic description>: <value>"로.

```json
"id": "score_headshots",
"type": "score",
"description": "Headshots: %s",
"statistic": "avatar_num_headshots"
```

#### `achievement`

업적은 플레이어가 열망하는 목표이며, 게임에서 대중화된 용어의 일반적인 의미입니다.
다른 게임.

업적은 요구 사항을 통해 지정되며 각 요구 사항은 `event_statistic`에 대한 제약 조건입니다.
예를 들면:

```json
{
  "id": "achievement_kill_zombie",
  "type": "achievement",
  // 업적 이름과 설명은 UI에 사용됩니다.
  // 설명은 선택사항이며 원하는 경우 추가 세부정보를 제공할 수 있습니다.
  "name": "One down, billions to go\u2026",
  "description": "Kill a zombie",
  "requirements": [
    // 각 요구 사항은 제한되는 통계를 지정해야 하며
    // 일부 목표 값과의 비교 측면에서 제약이 있습니다.
    { "event_statistic": "num_avatar_zombie_kills", "is": ">=", "target": 1 }
  ]
},
```

`"is"` 필드는 `">="`, `"<="` 또는 `"anything"`이어야 합니다. `"anything"`가 아닌 경우 `"target"`
존재해야 하며 정수여야 합니다.

추가 선택 필드가 있습니다.

```json
"hidden_by": [ "other_achievement_id" ]
```

다른 업적 ID 목록을 제공하세요. 이 업적은 숨겨집니다(예:
업적 UI) 나열된 모든 업적이 완료될 때까지.

스포일러를 방지하거나 업적 목록의 혼란을 줄이려면 이 기능을 사용하세요.

```json
"skill_requirements": [ { "skill": "archery", "is": ">=", "level": 5 } ]
```

이를 통해 성취가 언제 달성될 수 있는지에 대한 기술 수준 요구 사항(상한 또는 하한)을 허용합니다.
청구됩니다. `"skill"` 필드는 스킬의 ID를 사용합니다.

아래의 `"time_constraint"`와 같은 업적은 통계가 나열된 경우에만 캡처할 수 있습니다.
`"requirements"`에서 변경됩니다.

```json
"kill_requirements": [ { "faction": "ZOMBIE", "is": ">=", "count": 1 }, { "monster": "mon_sludge_crawler", "is": ">=", "count": 1 } ],
```

이를 통해 달성할 수 있는 시기에 대한 처치 요구 사항(상한 또는 하한)을 허용합니다.
주장했다. 다음의 종 ID를 사용하여 `"faction"` 또는 `"monster"`을 대상으로 정의할 수 있습니다.
`species.json` 또는 특정 몬스터 ID.

항목당 `"monster"`/`"faction"` 필드 중 하나만 사용할 수 있습니다. 둘 다 사용되지 않는 경우,
몬스터가 요구 사항을 충족합니다.

NPC는 현재 대상으로 정의할 수 없습니다.

아래의 `"time_constraint"`와 같은 업적은 통계가 나열된 경우에만 캡처할 수 있습니다.
`"requirements"`에서 변경됩니다.

```json
"time_constraint": { "since": "game_start", "is": "<=", "target": "1 minute" }
```

이를 통해 성취를 달성할 수 있는 시간 제한(하한 또는 상한)을 설정할 수 있습니다.
주장했다. `"since"` 필드는 `"game_start"` 또는 `"cataclysm"`일 수 있습니다. `"target"`에서는 설명합니다.
해당 기준점 이후의 시간.

업적은 요구 사항에 나열된 통계가 변경되는 경우에만 캡처할 수 있습니다.
따라서 일반적으로 특정 시간 임계값에 도달하면 트리거되는 업적을 원하는 경우
(예: "survived a certain amount of time") 그런 다음 그 옆에 몇 가지 요구 사항을 배치해야 합니다.
해당 시간이 지난 후에 트리거하십시오. 자주 변경될 수 있는 통계를 선택하고 추가하세요.
`"anything"` 제약 조건. 예를 들면:

```json
{
  "id": "achievement_survive_one_day",
  "type": "achievement",
  "description": "The first day of the rest of their unlives",
  "time_constraint": { "since": "game_start", "is": ">=", "target": "1 day" },
  "requirements": [ { "event_statistic": "num_avatar_wake_ups", "is": "anything" } ]
},
```

이것은 단순한 "survive a day"이지만 깨어나면 트리거되므로 다음과 같이 완료됩니다.
게임을 시작한 지 24시간 만에 처음으로 일어났습니다.

### `requirements`

Cataclysm의 요구 사항 시스템은 제작, 건설 및 기타 게임 메커니즘에 필요한 재사용 가능한 구성 요소, 도구 및 품질 세트를 정의합니다. 요구사항은 JSON 형식으로 정의되며 `data/json/requirements/`에 위치합니다.

## JSON 구조

### 기본 형식

```json
{
  "id": "unique_id",
  "type": "requirement",
  "//": "Optional comment",
  "components": [
    [
      ["gasoline", 1],
      ["diesel", 1],
      ["biodiesel", 1]
    ]
  ],
  "tools": [
    [
      ["soldering_iron", 1],
      ["soldering_ethanol", 10],
      ["toolset", 1]
    ]
  ],
  "qualities": [
    { "id": "CUT", "level": 1 },
    { "id": "HAMMER", "level": 2 }
  ]
}
```

#### 형식

마지막 세 개의 배열 중 하나가 존재하는 한 선택 사항입니다. (요구 사항은 결국 _something_에 따라 달라져야 합니다!)

**구성 요소**는 대안과 요구 사항을 나타내는 중첩 배열로 구성됩니다.

```json
"components": [
  [ /* Group 1: ONE of these required */ ],
  [ /* Group 2: ONE of these required */ ]
]
```

- `[ "item_id", quantity ]` - 특정 항목
- `[ "item_id", quantity, "LIST" ]` - 다른 요구사항 정의를 참조합니다.

**도구**는 사용되었지만 소비되지 않는 항목과 청구 비용을 지정합니다.

```json
"tools": [
  [
    [ "tool_id", charges ],
    [ "alternative", charges, "LIST" ]
  ]
]
```

`charges`는 양의 정수일 수 있습니다. 이 경우 소비된 요금 수를 참조하거나 `-1`는 요금이 사용되지 않음을 나타냅니다.

**품질**은 필요한 최소 도구 품질 수준을 지정합니다.

````json
"qualities": [
  { "id": "QUALITY_ID", "level": min_level }
]

#### Usage in Recipes

Here is an example of a requirement in a recipe:

```json
"using": [ [ "requirement_id", multiplier ] ]
````

다양한 요구사항:

```json
"using": [
  [ "welding_standard", 1 ],
  [ "forging_standard", 2 ]
]
```

`/data/json/requirements` 폴더의 파일을 탐색하여 특정 유형의 항목 또는 구성 요소에 대한 요구 사항의 일반적인 예를 볼 수 있습니다. 요구 사항이 순환 종속성을 형성하지 않는지 확인하세요.

### 스킬

```json
"id" : "smg",  // 고유 ID. 하나의 연속된 단어여야 합니다. 필요한 경우 밑줄을 사용하세요.
"name" : "submachine guns",  // 게임 내 이름이 표시됨
"description" : "Your skill with submachine guns and machine pistols. Halfway between a pistol and an assault rifle, these weapons fire and reload quickly, and may fire in bursts, but they are not very accurate.", // 게임 내 설명
"tags" : ["gun_type"]  // 특수 플래그(기본값: 없음)
```

### 임무

(선택사항, 임무 ID 배열)

이 직업/취미에 대한 시작 임무 목록입니다.

예:

```JSON
"missions": [ "MISSION_LAST_DELIVERY" ]
```

## `json/` JSON

### 수확

```json
{
    "id": "jabberwock",
    "type": "harvest",
    "message": "You messily hack apart the colossal mass of fused, rancid flesh, taking note of anything that stands out.",
    "entries": [
      { "drop": "meat_tainted", "type": "flesh", "mass_ratio": 0.33 },
      { "drop": "fat_tainted", "type": "flesh", "mass_ratio": 0.1 },
      { "drop": "jabberwock_heart", "base_num": [ 0, 1 ], "scale_num": [ 0.6, 0.9 ], "max": 3, "type": "flesh" }
    ],
},
{
  "id": "mammal_large_fur",
  "//": "drops large stomach",
  "type": "harvest",
  "entries": [
    { "drop": "meat", "type": "flesh", "mass_ratio": 0.32 },
    { "drop": "meat_scrap", "type": "flesh", "mass_ratio": 0.01 },
    { "drop": "lung", "type": "flesh", "mass_ratio": 0.0035 },
    { "drop": "liver", "type": "offal", "mass_ratio": 0.01 },
    { "drop": "brain", "type": "flesh", "mass_ratio": 0.005 },
    { "drop": "sweetbread", "type": "flesh", "mass_ratio": 0.002 },
    { "drop": "kidney", "type": "offal", "mass_ratio": 0.002 },
    { "drop": "stomach_large", "scale_num": [ 1, 1 ], "max": 1, "type": "offal" },
    { "drop": "bone", "type": "bone", "mass_ratio": 0.15 },
    { "drop": "sinew", "type": "bone", "mass_ratio": 0.00035 },
    { "drop": "fur", "type": "skin", "mass_ratio": 0.02 },
    { "drop": "fat", "type": "flesh", "mass_ratio": 0.07 }
  ]
},
{
  "id": "CBM_SCI",
  "type": "harvest",
  "entries": [
    {
      "drop": "bionics_sci",
      "type": "bionic_group",
      "flags": [ "NO_STERILE", "NO_PACKED" ],
      "faults": [ "fault_bionic_salvaged" ]
    },
    { "drop": "meat_tainted", "type": "flesh", "mass_ratio": 0.25 },
    { "drop": "fat_tainted", "type": "flesh", "mass_ratio": 0.08 },
    { "drop": "bone_tainted", "type": "bone", "mass_ratio": 0.1 }
  ]
},
```

#### `id`

수확 정의의 고유 ID입니다.

#### `type`

개체를 수확 정의로 표시하려면 항상 `harvest`이어야 합니다.

#### `message`

수확 정의를 사용하는 생물이 도살될 때 인쇄될 선택적 메시지입니다. 어쩌면
정의에서 생략되었습니다.

#### `entries`

도축 시 생산될 수 있는 품목과 그 가능성을 정의하는 사전 배열
생산. `drop` 값은 생산할 아이템의 `id` 문자열이어야 합니다.

`type` 값은 항목의 출처와 관련된 본문 부분이 포함된 문자열이어야 합니다. 허용되는 값
`flesh`: 생물의 "meat"입니다. `offal`: 생물의 "organs"입니다. 이것들
현장 드레싱시 제거됩니다. `skin`: 생물의 "skin"입니다. 이게 망하는 동안에
4등분. `bone`: 생물의 "bones"입니다. 당신은 현장에서 이것들 중 일부를 얻을 것입니다
드레싱을 하고 나머지는 시체를 도살한 것입니다. `bionic`: 해부하여 얻은 아이템
생물. CBM에만 국한되지 않습니다. `bionic_group`: 아이템을 주는 아이템 그룹
생물을 해부하는 것. CBM을 포함하는 그룹에만 국한되지 않습니다.

`flags` 값은 문자열 배열이어야 합니다. 그 항목에 추가될 플래그입니다.
수확 후 입장.

`faults` 값은 `fault_id` 문자열의 배열이어야 합니다. te에 추가되는 결함입니다.
수확 시 해당 항목의 항목.

다음 항목의 `bionic` 및 `bionic_group`를 제외한 모든 `type`에 대해 결과 크기를 조정합니다.
`base_num` 값은 첫 번째 요소가 최소 수를 정의하는 두 개의 요소로 구성된 배열이어야 합니다.
생산된 해당 품목의 수와 두 번째는 최대 수를 정의합니다. `scale_num` 값
최소 및 최대 드롭 수를 각각 증가시키는 두 개의 요소가 있는 배열이어야 합니다.
요소값 \* 생존 스킬 기준. `bas_num` 및 `scale_num` 계산 후 `max` 상한
`mass_ratio` 값을 사용하는 것은 몬스터의 무게가 몬스터를 구성하는 양의 배수입니다.
관련 항목. 질량을 보존하려면 모든 방울과 결합하여 0과 1 사이를 유지하십시오. 이는 재정의됩니다.
`base_num`, `scale_num` 및 `max`

`type`의 경우: `bionic` 및 `bionic_group` 다음 항목은 결과를 확장할 수 있습니다. `max` 이 값
(다른 `type`의 경우 `max`와 반대)는 전달될 최대 도살 롤에 해당합니다.
Activity_handlers.cpp의 check_butcher_cbm(); 해당 내용을 보려면 check_butcher_cbm()을 확인하세요.
해당 함수에 전달된 롤 값의 분포 확률

### 무기 카테고리

주로 무술에 사용하기 위해 무기(총 또는 근접 공격)를 그룹으로 분류하는 데 사용됩니다.

```json
{
  "type": "weapon_category",
  "id": "WEAP_CAT",
  "name": "Weapon Category"
}
```

`"name"`은 무술 UI에서 UI 표시에 사용되는 번역 가능한 문자열이고, JSON에는 ID가 사용됩니다.
항목.

## 폐기 및 마이그레이션

지도의 경우 항목이 생성될 수 있는 모든 장소에서 항목을 제거하고, mapgen 항목을 제거하고, 추가합니다.
oter_id를 마이그레이션하기 위해 오버맵 지형 ID를 `data/json/obsoletion/migration_oter_ids.json`에 넣습니다.
`underground_sub_station` 및 `sewer_sub_station`을 회전 가능한 버전으로 변환합니다. mapgen이
이미 이 영역을 생성했습니다. 이는 오버맵에 표시된 타일만 변경합니다.

```json
{
  "type": "oter_id_migration",
  "//": "obsoleted in 0.4",
  "old_directions": false,
  "new_directions": false,
  "oter_ids": {
    "underground_sub_station": "underground_sub_station_north",
    "sewer_sub_station": "sewer_sub_station_north"
  }
}
```

`old_directions` 옵션이 활성화되면 각 항목은 4개의 마이그레이션을 생성합니다: `old_north`에 대해,
`old_west`, `old_south`, `old_east`. 마이그레이션 대상은 다음의 가치에 따라 달라집니다.
`new_directions` 옵션. `true`로 설정되면 지형이 동일한 방향으로 마이그레이션됩니다.
`old_north`에서 `new_north`, `old_east`에서 `new_east` 등. `new_directions`가 다음으로 설정된 경우
`false`이면 4개의 지형이 모두 하나의 일반 `new`으로 마이그레이션됩니다. 두 경우 모두에 대해 당신은
접미사 없이 `oter_ids` 맵에 일반 `old` 및 `new` 이름만 지정하면 됩니다.

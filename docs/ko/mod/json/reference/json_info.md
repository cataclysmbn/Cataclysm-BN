# JSON INFO

> [!NOTE]
>
> 이 문서는 여러 페이지로 분리 중입니다.

> [!CAUTION]
>
> 많은 JSON 파일이 아직 문서화되지 않았거나 오래되었습니다. 확실히 하려면 관련 소스 파일을 확인하세요.

이 문서는 Cataclysm: Dark days ahead에서 사용하는 json 파일의 내용을 설명합니다. 아마도 여러분은
Catacysm: Dark days ahead의 콘텐츠를 추가/수정하려고 하며, 어떤 내용을 어디서 찾아야 하는지와 각 파일 및
속성이 무엇을 하는지 더 알아보려고 이 문서를 읽고 있을 것입니다.

## JSON 탐색

많은 JSON은 다른 JSON 엔티티를 교차 참조합니다. 탐색을 쉽게 하기 위해 `tags` 파일을 생성하는
스크립트 `tools/json_tools/cddatags.py`를 제공합니다.

스크립트를 실행하려면 Python 3가 필요합니다. Windows에서는 아마 설치가 필요하고, `.py` 파일을 Python과
연결해야 할 수 있습니다. 그런 다음 명령 프롬프트를 열고 CDDA 폴더로 이동해
`tools\json_tools\cddatags.py`를 실행하세요.

이 기능을 사용하려면 에디터가 [ctags support](http://ctags.sourceforge.net/)를 지원해야 합니다.
설정이 완료되면 어떤 엔티티의 정의로도 쉽게 점프할 수 있습니다. 예를 들어 id 위에 커서를 두고 해당
단축키를 누르면 됩니다.

- Vim에서는 기본 제공되며,
  [`^\]`](http://vimdoc.sourceforge.net/htmldoc/tagsrch.html#tagsrch.txt)로 정의로 점프할 수 있습니다.
- Notepad++에서는 "Plugins" -> "Plugins Admin"에서 "TagLEET" 플러그인을 활성화하세요. 그런 다음
  id를 선택하고 Alt+Space를 눌러 references 창을 엽니다.

## `type` 속성

항목은 `type` 속성으로 구분됩니다. 이 속성은 모든 항목에서 필수입니다.
예를 들어 이 항목을 'armor'로 설정하면 게임은 해당 항목에서 armor 전용 속성을 기대합니다.
또한 [`copy-from`](./items/json_inheritance.md#copy-from)과도 연결되며,
[inherit properties of another object](./items/json_inheritance.md)하려면 같은 tipe여야 합니다.

## 포맷팅

JSON 파일을 편집할 때는 아래와 같은 올바른 포맷팅을 적용하세요.

### 시간 길이

숫자와 시간 단위 쌍을 하나 이상 포함하는 문자열입니다. 숫자와 단위 사이, 그리고 각 쌍 사이는
임의 개수의 공백이 허용됩니다. 사용 가능한 단위:

- "hours", "hour", "h" - 1시간
- "days", "day", "d" - 1일
- "minutes", "minute", "m" - 1분
- "turns", "turn", "t" - 1턴,

예시:

- " +1 day -23 hours 50m " `(1*24*60 - 23*60 + 50 == 110 minutes)`
- "1 turn 1 minutes 9 turns" (1턴이 1초이므로 1분 10초)

### 기타 포맷

```json
"//" : "comment", // Preferred method of leaving comments inside json files.
```

일부 json 문자열(아이템 이름, 설명 등)은 번역 추출 대상입니다. 정확한 추출 방식은
`lang/extract_json_strings.py`에서 처리됩니다. 번역 컨텍스트 없이 문자열을 쓰는 일반적인 방식 외에도,
아래처럼 선택적 번역 컨텍스트(그리고 때때로 복수형)를 지정할 수 있습니다:

```json
"name": { "ctxt": "foo", "str": "bar", "str_pl": "baz" }
```

또는 복수형이 단수형과 같다면:

```json
"name": { "ctxt": "foo", "str_sp": "foo" }
```

아래처럼 `"//~"` 항목을 추가해 번역자용 주석을 넣을 수도 있습니다. 항목 순서는 중요하지 않습니다.

```json
"name": {
    "//~": "as in 'foobar'",
    "str": "bar"
}
```

[Currently, only some JSON values support this syntax](./../../../i18n/reference/translation#supported-json-values).

## 각 JSON 파일의 설명과 내용

이 섹션은 각 json 파일과 그 내용을 설명합니다. 각 json은 다른 Json 파일과 공유되지 않는 고유 속성을
가집니다(예: 책의 'chapters' 속성은 armor에 적용되지 않음). 이를 통해 속성이 올바른 JSON 파일
컨텍스트에서만 설명되고 사용되도록 합니다.

## `data/json/` JSONs

### Ascii_arts

| Identifier | Description                                                          |
| ---------- | -------------------------------------------------------------------- |
| id         | 고유 ID. 하나의 연속된 단어여야 하며 필요하면 밑줄을 사용합니다.     |
| picture    | 문자열 배열. 각 항목은 아스키 그림 한 줄이며 최대 42열이어야 합니다. |

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

### Body_parts

| Identifier        | Description                                                                                                                                                                                                                               |
| ----------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| id                | (_mandatory_) 고유 ID. 하나의 연속된 단어여야 하며 필요하면 밑줄을 사용합니다.                                                                                                                                                            |
| name              | (_mandatory_) 게임 내 표시 이름.                                                                                                                                                                                                          |
| accusative        | (_mandatory_) 이 신체 부위의 목적격 형태.                                                                                                                                                                                                 |
| heading           | (_mandatory_) 헤딩에서 표시되는 방식.                                                                                                                                                                                                     |
| heading_multiple  | (_optional_) heading의 복수형. (기본값: heading 값)                                                                                                                                                                                       |
| hp_bar_ui_text    | (_optional_) 패널의 HP 바 옆 표시 문자열. (기본값: 빈 문자열)                                                                                                                                                                             |
| encumbrance_text  | (_optional_) 과부하(encumbered) 시 효과 설명. (기본값: 빈 문자열)                                                                                                                                                                         |
| main_part         | (_optional_) 이 부위가 부착된 주 부위. (기본값: self)                                                                                                                                                                                     |
| base_hp           | (_optional_) 수정 전 이 부위의 HP 양. (기본값: `60`)                                                                                                                                                                                      |
| opposite_part     | (_optional_) 쌍을 이룰 때 반대 부위. (기본값: self)                                                                                                                                                                                       |
| essential         | (_optional_) 이 부위 HP가 `0`이 되면 캐릭터가 사망하는지 여부.                                                                                                                                                                            |
| hit_size          | (_optional_) Float. 가중치 없는 선택 시 신체 부위 크기. (기본값: `0.`)                                                                                                                                                                    |
| hit_size_relative | (_optional_) Float. 공격자가 더 작음/같음/더 큼일 때의 타격 크기. (기본값: `[ 0, 0, 0 ]`                                                                                                                                                  |
| hit_difficulty    | (_optional_) Float. "owner"가 맞았다고 가정할 때 해당 부위를 맞히기 어려운 정도. 숫자가 높을수록 좋은 타격이 이 부위로 치우치고, 낮을수록 부정확한 공격에 맞기 어렵습니다. 공식은 `chance *= pow(hit_roll, hit_difficulty)` (기본값: `0`) |
| side              | (_optional_) 이 신체 부위가 어느 쪽인지. 기본 both.                                                                                                                                                                                       |
| stylish_bonus     | (_optional_) 이 부위에 멋진 의류 착용 시 기분 보너스. (기본값: `0`)                                                                                                                                                                       |
| hot_morale_mod    | (_optional_) 이 부위가 너무 더울 때 기분 효과. (기본값: `0`)                                                                                                                                                                              |
| cold_morale_mod   | (_optional_) 이 부위가 너무 추울 때 기분 효과. (기본값: `0`)                                                                                                                                                                              |
| bionic_slots      | (_optional_) 이 부위의 바이오닉 슬롯 수.                                                                                                                                                                                                  |

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

### Bionics

| Identifier                                                                                           | Description                                                                                                                       |
| ---------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| id                                                                                                   | (_mandatory_) 고유 ID. 하나의 연속된 단어여야 하며 필요하면 밑줄을 사용합니다.                                                    |
| name                                                                                                 | (_mandatory_) 게임 내 표시 이름.                                                                                                  |
| description                                                                                          | (_mandatory_) 게임 내 설명.                                                                                                       |
| flags                                                                                                | (_optional_) 플래그 목록. 지원 값은 json_flags.md를 참고하세요.                                                                   |
| act_cost                                                                                             | (_optional_) 바이오닉 활성화에 드는 kJ. 문자열 "1 kJ"/"1000 J"/"1000000 mJ" 사용 가능 (기본값: `0`)                               |
| deact_cost                                                                                           | (_optional_) 바이오닉 비활성화에 드는 kJ. 문자열 "1 kJ"/"1000 J"/"1000000 mJ" 사용 가능 (기본값: `0`)                             |
| react_cost                                                                                           | (_optional_) 활성 유지 시 시간당 소모 kJ. "time"이 0이 아니어야 동작. 문자열 "1 kJ"/"1000 J"/"1000000 mJ" 사용 가능 (기본값: `0`) |
| time                                                                                                 | (_optional_) 활성화 시 비용을 소모하는 간격. 0이면 한 번만 소모. (기본값: `0`)                                                    |
| upgraded_bionic                                                                                      | (_optional_) 이것을 설치해 업그레이드 가능한 바이오닉.                                                                            |
| required_bionics                                                                                     | (_optional_) 이 바이오닉 설치에 필요한 바이오닉이며, 이것이 설치되면 제거할 수 없는 바이오닉들                                    |
| can_uninstall                                                                                        | (_optional_) 설치 후 제거 가능 여부                                                                                               |
| no_uninstall_reason                                                                                  | (_optional_) can_uninstall이 false일 때 필수. CBM 제거 시도 시 표시할 문자열                                                      |
| available_upgrades                                                                                   | (_optional_) 이 바이오닉에 사용할 수 있는 업그레이드(바이오닉 목록)                                                               |
| starting_bionic                                                                                      | (_optional_) 시작 시 선택 가능 여부를 결정하는 Bool                                                                               |
| points                                                                                               | (_optional_) 이 바이오닉 선택에 드는 포인트 수                                                                                    |
| having this one referenced by `upgraded_bionic`.                                                     |                                                                                                                                   |
| and how much this bionic encumber them.                                                              |                                                                                                                                   |
| carrying capacity in grams, can be negative. Strings can be used - "5000 g" or "5 kg" (default: `0`) |                                                                                                                                   |
|                                                                                                      | weight_capacity_modifier                                                                                                          |
| (default: `1`)                                                                                       |                                                                                                                                   |
| when this bionic is installed (e.g. because it replaces the fault biological part).                  |                                                                                                                                   |
| included_bionics                                                                                     | (_optional_) 이 바이오닉 설치 시 자동으로 함께 설치되는 추가 바이오닉                                                             |
| is installed. This can be used to install several bionics from one CBM item, which is useful as each |                                                                                                                                   |
| of those can be activated independently.                                                             |                                                                                                                                   |
| with another. If true this bionic does not require a CBM item to be defined. (default: `false`)      |                                                                                                                                   |
| env_protec                                                                                           | (_optional_) 지정된 신체 부위에 이 바이오닉이 제공하는 환경 보호량                                                                |
| specified body parts.                                                                                |                                                                                                                                   |
| provide on the specified body parts.                                                                 |                                                                                                                                   |
| bionic provide on the specified body parts.                                                          |                                                                                                                                   |
| protect does this bionic provide on the specified body parts.                                        |                                                                                                                                   |
| A list of body parts occupied by this bionic, and the number of bionic slots it take on those parts. |                                                                                                                                   |
|                                                                                                      | capacity                                                                                                                          |
| kJ"/"1000 J"/"1000000 mJ" (default: `0`)                                                             |                                                                                                                                   |
| bionic can use to produce bionic power.                                                              |                                                                                                                                   |
| allows you to plug your power banks to an external power source (solar backpack, UPS, vehicle etc)   |                                                                                                                                   |
| via a cable. (default: `false`)                                                                      |                                                                                                                                   |
| store.                                                                                               |                                                                                                                                   |
| `0`)                                                                                                 |                                                                                                                                   |
| (default: `1`)                                                                                       |                                                                                                                                   |
| converted into power. Useful for CBM using PERPETUAL fuel like `muscle`, `wind` or `sun_light`.      |                                                                                                                                   |
| (default: `0`)                                                                                       |                                                                                                                                   |
| power. (default: `false`)                                                                            |                                                                                                                                   |
| diminishing fuel_efficiency. Float between 0.0 and 1.0. (default: `nullopt`)                         |                                                                                                                                   |
| (_optional_) `emit_id` of the field emitted by this bionic when it produces energy. Emit_ids are     |                                                                                                                                   |
| defined in `emit.json`.                                                                              |                                                                                                                                   |
| designated as follow: "DEX", "INT", "STR", "PER".                                                    |                                                                                                                                   |
| enchantments applied by this CBM (see MAGIC.md for instructions on enchantment. NB: enchantments are |                                                                                                                                   |
| not necessarily magic.)                                                                              |                                                                                                                                   |
| installing this CBM, and lose when you uninstall this CBM. Spell classes are automatically gained.   |                                                                                                                                   |
| fake_item                                                                                            | (_optional_) 이 바이오닉에서 사용하는 가짜 아이템 ID. 총/무기 바이오닉에서는 필수                                                 |
| bionics.                                                                                             |                                                                                                                                   |

```json
{
    "id"           : "bio_batteries",
    "name"         : "Battery System",
    "active"       : false,
    "act_cost"     : 0,
    "time"         : 1,
    "fuel_efficiency": 1,
    "stat_bonus": [ [ "INT", 2 ], [ "STR", 2 ] ],
    "fuel_options": [ "battery" ],
    "fuel_capacity": 500,
    "encumbrance"  : [ [ "torso", 10 ], [ "arm_l", 10 ], [ "arm_r", 10 ], [ "leg_l", 10 ], [ "leg_r", 10 ], [ "foot_l", 10 ], [ "foot_r", 10 ] ],
    "description"  : "You have a battery draining attachment, and thus can make use of the energy contained in normal, everyday batteries. Use 'E' to consume batteries.",
    "canceled_mutations": ["HYPEROPIC"],
    "included_bionics": ["bio_blindfold"]
},
{
    "id": "bio_purifier",
    "type": "bionic",
    "name": "Air Filtration System",
    "description": "Surgically implanted in your trachea is an advanced filtration system.  If toxins, or airborne diseases find their way into your windpipe, the filter will attempt to remove them.",
    "occupied_bodyparts": [ [ "torso", 4 ], [ "mouth", 2 ] ],
    "env_protec": [ [ "mouth", 7 ] ],
    "bash_protec": [ [ "leg_l", 3 ], [ "leg_r", 3 ] ],
    "cut_protec": [ [ "leg_l", 3 ], [ "leg_r", 3 ] ],
    "bullet_protec": [ [ "leg_l", 3 ], [ "leg_r", 3 ] ],
    "learned_spells": [ [ "mint_breath", 2 ] ],
    "flags": [ "BIONIC_NPC_USABLE" ]
}
```

바이오닉 효과는 코드에서 정의되며, JSON만으로 새 효과를 만들 수는 없습니다. 새 바이오닉을 추가할 때,
다른 것에 포함되는 경우가 아니라면 `data/json/items/bionics.json`에 해당 CBM 아이템도 함께 추가해야
합니다. 고장 바이오닉도 마찬가지입니다.

### Dreams

| Identifier | Description                                                    |
| ---------- | -------------------------------------------------------------- |
| messages   | 가능한 꿈 목록.                                                |
| category   | 꿈을 꾸기 위해 필요한 돌연변이 카테고리.                       |
| strength   | 필요한 돌연변이 카테고리 강도 (1 = 20-34, 2 = 35-49, 3 = 50+). |

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

### Disease

| Identifier         | Description                                                            |
| ------------------ | ---------------------------------------------------------------------- |
| id                 | 고유 ID. 하나의 연속된 단어여야 하며 필요하면 밑줄을 사용합니다.       |
| min_duration       | 질병의 최소 지속 시간. 문자열 "x m", "x s","x d"를 사용합니다.         |
| max_duration       | 질병의 최대 지속 시간.                                                 |
| min_intensity      | 질병이 적용하는 효과의 최소 강도                                       |
| max_intensity      | 효과의 최대 강도.                                                      |
| health_threshold   | 이 값보다 건강이 높으면 질병에 면역. -200~200 사이여야 함. (optional ) |
| symptoms           | 질병이 적용하는 효과.                                                  |
| affected_bodyparts | 효과가 적용되는 신체 부위 목록. (optional, 기본값 num_bp)              |

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

### Names

```json
{ "name" : "Aaliyah", "gender" : "female", "usage" : "given" }, // Name, gender, "given"/"family"/"city" (first/last/city name).
```

### Scent_types

| Identifier        | Description                                                                     |
| ----------------- | ------------------------------------------------------------------------------- |
| id                | 고유 ID. 하나의 연속된 단어여야 하며 필요하면 밑줄을 사용합니다.                |
| receptive_species | 이 냄새를 추적할 수 있는 종. `species.json`에 정의된 유효 id를 사용해야 합니다. |

```json
{
  "type": "scent_type",
  "id": "sc_flower",
  "receptive_species": ["MAMMAL", "INSECT", "MOLLUSK", "BIRD"]
}
```

### Scores and Achievements

점수는 _events_를 기반으로 두세 단계로 정의됩니다. 어떤 이벤트가 있고 어떤 데이터를 담는지 보려면
[`event.h`](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/src/event.h)를 읽으세요.

각 이벤트는 특정 필드 집합을 포함합니다. 각 필드는 문자열 키와 `cata_variant` 값을 가집니다.
필드는 이벤트에 관한 관련 정보를 모두 제공해야 합니다.

예를 들어 `gains_skill_level` 이벤트를 보겠습니다. `event.h`에는 다음과 같이 정의되어 있습니다:

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

여기서 이 이벤트 타입에 세 필드가 있음을 알 수 있습니다:

- `character`: 레벨을 얻는 캐릭터의 id
- `skill`: 획득한 스킬의 id
- `new_level`: 해당 스킬에서 새로 획득한 정수 레벨

이벤트는 게임 내 상황에 따라 생성됩니다. 이 이벤트는 다양한 방식으로 변환/요약될 수 있습니다.
여기에는 event streams, event statistics, scores라는 세 개념이 있습니다.

- 게임이 정의한 각 `event_type`은 하나의 event stream을 생성합니다.
- 기존 event stream에 `event_transformation`을 적용해 json에서 추가 event stream을 정의할 수 있습니다.
- `event_statistic`은 event stream을 하나의 값(대개 숫자지만 다른 값 타입도 가능)으로 요약합니다.
- `score`는 이런 statistic을 사용해 플레이어가 볼 수 있는 게임 내 점수를 정의합니다.

#### `event_transformation`

`event_transformation`은 event stream을 수정해 다른 event stream을 생성할 수 있습니다.

변환할 입력 stream은 내장 event type stream을 쓰는 `"event_type"` 또는,
다른 json 정의 변환 stream을 쓰는 `"event_transformation"`으로 지정합니다.

다음 변경을 일부 또는 전부 적용할 수 있습니다:

- event field transformation을 기반으로 각 이벤트에 새 필드를 추가
  (event field transformation은
  [`event_field_transformation.cpp`](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/src/event_field_transformations.cpp)에 있음)
- 이벤트 값 조건으로 필터링해 입력 stream의 일부만 포함하는 stream 생성
- 출력 stream에서 관심 없는 필드 제거

각 수정 예시는 다음과 같습니다:

```json
"id": "avatar_kills_with_species",
"type": "event_transformation",
"event_type": "character_kills_monster", // Transformation acts upon events of this type
"new_fields": { // A dictionary of new fields to add to the event
    // The key is the new field name; the value should be a dictionary of one element
    "species": {
        // The key specifies the event_field_transformation to apply; the value specifies
        // the input field whose value should be provided to that transformation.
        // So, in this case, we are adding a new field 'species' which will
        // contain the species of the victim of this kill event.
        "species_of_monster": "victim_type"
    }
}
```

```json
"id": "moves_on_horse",
"type": "event_transformation",
"event_type" : "avatar_moves", // An event type.  The transformation will act on events of this type
"value_constraints" : { // A dictionary of constraints
    // Each key is the field to which the constraint applies
    // The value specifies the constraint.
    // "equals" can be used to specify a constant string value the field must take.
    // "equals_statistic" specifies that the value must match the value of some statistic (see below)
    "mount" : { "equals": "mon_horse" }
}
// Since we are filtering to only those events where 'mount' is 'mon_horse', we
// might as well drop the 'mount' field, since it provides no useful information.
"drop_fields" : [ "mount" ]
```

#### `event_statistic`

`event_transformation`과 마찬가지로 `event_statistic`도 입력 event stream이 필요합니다.
입력 stream은 아래 두 항목 중 하나로 지정할 수 있습니다:

```json
"event_type" : "avatar_moves" // Events of this built-in type
"event_transformation" : "moves_on_horse" // Events resulting from this json-defined transformation
```

그 다음 `stat_type`과 추가 세부사항을 다음과 같이 지정합니다:

이벤트 수:

```json
"stat_type" : "count"
```

모든 이벤트에서 지정 필드 숫자값의 합:

```json
"stat_type" : "total"
"field" : "damage"
```

모든 이벤트에서 지정 필드 숫자값의 최댓값:

```json
"stat_type" : "maximum"
"field" : "damage"
```

모든 이벤트에서 지정 필드 숫자값의 최솟값:

```json
"stat_type" : "minimum"
"field" : "damage"
```

고려할 이벤트가 하나뿐이라고 가정하고, 그 유일한 이벤트의 지정 필드 값을 사용:

```json
"stat_type": "unique_value",
"field": "avatar_id"
```

`stat_type`과 무관하게 각 `event_statistic`에는 다음도 가질 수 있습니다:

```json
// Intended for use in describing scores and achievement requirements.
"description": "Number of things"
```

#### `score`

Score는 점수 표시에 사용할 설명을 이벤트와 연결합니다.
`description`은 statistic 값이 삽입될 `%s` 포맷 지정자를 포함해야 합니다.

대부분의 statistic이 정수를 반환하더라도 `%s`를 사용해야 합니다.

기반 statistic에 description이 있으면 score description은 선택입니다.
기본값은 "<statistic description>: <value>"입니다.

```json
"id": "score_headshots",
"type": "score",
"description": "Headshots: %s",
"statistic": "avatar_num_headshots"
```

#### `achievement`

Achievement는 일반적인 의미에서 플레이어가 달성하도록 설계된 목표입니다.

Achievement는 요구사항으로 정의되며, 각 요구사항은 `event_statistic`에 대한 제약입니다.
예:

```json
{
  "id": "achievement_kill_zombie",
  "type": "achievement",
  // The achievement name and description are used for the UI.
  // Description is optional and can provide extra details if you wish.
  "name": "One down, billions to go\u2026",
  "description": "Kill a zombie",
  "requirements": [
    // Each requirement must specify the statistic being constrained, and the
    // constraint in terms of a comparison against some target value.
    { "event_statistic": "num_avatar_zombie_kills", "is": ">=", "target": 1 }
  ]
},
```

`"is"` 필드는 `">="`, `"<="`, `"anything"` 중 하나여야 합니다. `"anything"`이 아니면
`"target"`이 있어야 하며 정수여야 합니다.

추가 선택 필드는 다음과 같습니다:

```json
"hidden_by": [ "other_achievement_id" ]
```

다른 achievement id 목록을 지정합니다. 목록의 achievement가 모두 완료될 때까지 이 achievement는
숨겨집니다(UI에 표시되지 않음).

스포일러 방지 또는 achievement 목록의 혼잡도를 줄일 때 사용하세요.

```json
"skill_requirements": [ { "skill": "archery", "is": ">=", "level": 5 } ]
```

Achievement를 획득할 수 있는 시점에 스킬 레벨 요구사항(상한/하한)을 추가할 수 있습니다.
`"skill"` 필드는 스킬 id를 사용합니다.

아래 `"time_constraint"`와 마찬가지로 achievement는 `"requirements"`에 나열된 statistic이 변경될 때만
획득할 수 있습니다.

```json
"kill_requirements": [ { "faction": "ZOMBIE", "is": ">=", "count": 1 }, { "monster": "mon_sludge_crawler", "is": ">=", "count": 1 } ],
```

Achievement를 획득할 수 있는 시점에 처치 요구사항(상한/하한)을 추가할 수 있습니다.
대상은 `"faction"` 또는 `"monster"`로 정의하며, `species.json`의 species id 또는 특정 monster id를 사용합니다.

각 항목당 `"monster"`/`"faction"` 중 하나만 사용할 수 있습니다. 둘 다 없으면 아무 몬스터나 조건을
충족합니다.

현재 NPC는 대상으로 지정할 수 없습니다.

아래 `"time_constraint"`와 마찬가지로 achievement는 `"requirements"`에 나열된 statistic이 변경될 때만
획득할 수 있습니다.

```json
"time_constraint": { "since": "game_start", "is": "<=", "target": "1 minute" }
```

Achievement를 획득할 수 있는 시점에 시간 제한(상한/하한)을 둘 수 있습니다.
`"since"` 필드는 `"game_start"` 또는 `"cataclysm"`을 사용할 수 있습니다.
`"target"`은 그 기준 시점 이후의 시간량을 설명합니다.

Achievement는 요구사항의 statistic이 변경될 때만 획득 판정이 이루어집니다.
따라서 시간 임계값 도달로 트리거되는 achievement(예: "일정 시간 생존")를 만들고 싶다면,
시간이 지난 뒤 판정을 일으킬 수 있는 다른 요구사항을 함께 넣어야 합니다.
자주 변하는 statistic을 골라 `"anything"` 제약을 추가하세요. 예:

```json
{
  "id": "achievement_survive_one_day",
  "type": "achievement",
  "description": "The first day of the rest of their unlives",
  "time_constraint": { "since": "game_start", "is": ">=", "target": "1 day" },
  "requirements": [ { "event_statistic": "num_avatar_wake_ups", "is": "anything" } ]
},
```

이는 단순한 "하루 생존" 업적이지만, 기상 이벤트로 트리거되므로 게임 시작 24시간 이후 처음 잠에서
깨어날 때 완료됩니다.

### Skills

```json
"id" : "smg",  // Unique ID. Must be one continuous word, use underscores if necessary
"name" : "submachine guns",  // In-game name displayed
"description" : "Your skill with submachine guns and machine pistols. Halfway between a pistol and an assault rifle, these weapons fire and reload quickly, and may fire in bursts, but they are not very accurate.", // In-game description
"tags" : ["gun_type"]  // Special flags (default: none)
```

### Missions

(선택, mission id 배열)

이 profession/hobby의 시작 미션 목록입니다.

예시:

```JSON
"missions": [ "MISSION_LAST_DELIVERY" ]
```

## `json/` JSONs

### Harvest

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

수확 정의의 고유 id입니다.

#### `type`

객체를 수확 정의로 표시하려면 항상 `harvest`여야 합니다.

#### `message`

해당 수확 정의를 사용하는 생물을 도축할 때 출력할 선택 메시지입니다.
정의에서 생략할 수 있습니다.

#### `entries`

도축 시 생성될 수 있는 아이템과 생성 가능성을 정의하는 사전 배열입니다.
`drop` 값은 생성할 아이템의 `id` 문자열이어야 합니다.

`type` 값은 아이템이 나오는 신체 부위를 나타내는 문자열이어야 합니다. 허용 값:
`flesh`: 생물의 "고기". `offal`: 생물의 "내장"(필드 드레싱 시 제거됨). `skin`: 생물의 "가죽"(쿼터링 시
손상됨). `bone`: 생물의 "뼈"(일부는 필드 드레싱, 나머지는 시체 도축에서 획득). `bionic`: 생물 해부로 얻는
아이템(CBM에 한정되지 않음). `bionic_group`: 생물 해부로 아이템을 주는 아이템 그룹(CBM 포함 그룹에 한정되지 않음).

`flags` 값은 문자열 배열이어야 합니다. 수확 시 해당 항목의 아이템에 추가되는 플래그입니다.

`faults` 값은 `fault_id` 문자열 배열이어야 합니다. 수확 시 해당 항목의 아이템에 추가되는 고장입니다.

`bionic`/`bionic_group` 외의 모든 `type`에 대해 다음 항목이 결과를 스케일합니다:
`base_num`은 두 요소 배열로 최소/최대 생성 수를 정의합니다. `scale_num`은 두 요소 배열로,
각 요소값 \\* survival 스킬만큼 최소/최대 드롭 수를 증가시킵니다. `max`는 `bas_num`/`scale_num` 계산 후 상한입니다.
`mass_ratio`는 몬스터 무게 중 해당 아이템이 차지하는 비율 배수입니다. 질량 보존을 위해 모든 드롭의 합을
0~1 사이로 유지하세요. 이 값이 있으면 `base_num`, `scale_num`, `max`를 덮어씁니다.

`bionic`/`bionic_group` `type`에서는 다음 항목으로 결과를 스케일할 수 있습니다:
`max`는(다른 `type`의 `max`와 달리) activity_handlers.cpp의 check_butcher_cbm()에 전달될 최대 도축 롤에
해당합니다. 해당 함수에 전달된 롤 값별 분포 확률은 check_butcher_cbm()를 확인하세요.

### Weapon Category

무기(총기 또는 근접)를 분류해 그룹화할 때 사용하며, 주로 무술에서 사용됩니다.

```json
{
  "type": "weapon_category",
  "id": "WEAP_CAT",
  "name": "Weapon Category"
}
```

`"name"`은 무술 UI 표시용 번역 문자열이며, ID는 JSON 항목에서 사용됩니다.

## 폐기 및 마이그레이션

맵의 경우, 스폰 가능한 모든 위치에서 항목을 제거하고 mapgen 항목을 제거한 다음,
오버맵 지형 id를 `data/json/obsoletion/migration_oter_ids.json`에 추가합니다.
예를 들어 oter_id `underground_sub_station`, `sewer_sub_station`을 회전형 버전으로 마이그레이션할 수 있습니다.
단, 해당 영역이 이미 생성된 경우 오버맵에 표시되는 타일만 변경됩니다:

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

`old_directions` 옵션을 켜면 각 항목마다 `old_north`, `old_west`, `old_south`, `old_east`에 대한
마이그레이션 4개가 생성됩니다. 대상은 `new_directions` 값에 따라 달라집니다.
`true`면 같은 방향으로 마이그레이션됩니다(`old_north` -> `new_north`, `old_east` -> `new_east` 등).
`false`면 네 방향 모두 단일 `new`로 마이그레이션됩니다. 두 경우 모두 `oter_ids` 맵에는 접미사 없는
순수 `old`/`new` 이름만 지정하면 됩니다.

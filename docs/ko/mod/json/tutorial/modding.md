# 모딩 가이드

> [!CAUTION]
>
> #### 일부 문서가 완전하지 않습니다
>
> 문서의 일부는 불완전하거나 이전 문서에서 제대로 업데이트되지 않았습니다. 이를 수정하는 기여는 대단히 환영합니다.

게임의 특정 기능은 소스 코드에서 게임을 다시 빌드하지 않고도 수정할 수 있습니다. 여기에는 직업, 몬스터, NPC 등이 포함됩니다. 관련 파일을 수정하고 게임을 실행하여 변경 사항을 확인하세요.

대부분의 모딩은 JSON 파일을 편집하여 수행됩니다. 모든 json 파일과 적절한 필드에 대한 심층 검토는 [JSON_INFO](./../reference/json_info.md)에서 확인할 수 있습니다.

## 기초

### 기본 모드 만들기

모드는 Cataclysm의 `data/mods` 디렉토리 내에 폴더를 생성하여 만들어집니다. 모드의 속성은 해당 폴더 내에 있는 `modinfo.json` 파일로 설정됩니다. Cataclysm이 폴더를 모드로 인식하려면 **반드시** 그 안에 `modinfo.json` 파일이 있어야 합니다.

<!--엄밀히 말하면 이것은 사실이 아닙니다. 모드는 MOD_INFO 구조를 가진 JSON 파일이 있는 한 작동합니다. 파일 이름이 "modinfo.json"일 필요는 없습니다-->

### Modinfo.json

modinfo.json 파일은 모드의 메타데이터를 포함하는 파일입니다. 모든 모드는 Cataclysm이 찾을 수 있도록 `modinfo.json` 파일이 있어야 합니다. 기본적인 `modinfo.json` 파일은 다음과 같습니다:

```json
[
  {
    "type": "MOD_INFO",
    "id": "Mod_ID",
    "name": "Mod's Display Name",
    "authors": ["Your name here", "Your friend's name if you want"],
    "description": "Your description here",
    "category": "content",
    "dependencies": ["bn"]
  }
]
```

`category` 속성은 모드가 모드 선택 메뉴에서 나타날 위치를 나타냅니다. 다음은 선택할 수 있는 사용 가능한 카테고리이며, 이 문서가 작성될 당시 존재했던 모드에서 선택한 몇 가지 예시가 포함되어 있습니다. modinfo 파일을 작성할 때 모드에 가장 적합한 것을 선택하세요.

- `content` - 많은 것을 추가하는 모드. 일반적으로 매우 큰 모드나 완전한 게임 개편에 예약됨 (예: 핵심 게임 파일, Aftershock)
- `items` - 게임에 새로운 아이템과 레시피를 추가하는 모드 (예: More survival tools)
- `creatures` - 게임에 새로운 생물이나 NPC를 추가하는 모드 (예: Modular turrets)
- `misc_additions` - 게임에 대한 기타 콘텐츠 추가 (예: Alternative map key, Crazy cataclysm)
- `buildings` - 새로운 오버맵 위치와 구조물 (예: Fuji's more buildings). 건물이 생성되지 않도록 블랙리스트를 작성하는 경우에도 사용 가능한 카테고리입니다 (예: No rail stations).
- `vehicles` - 새로운 자동차 또는 차량 부품 (예: Tanks and other vehicles)
- `rebalance` - 어떤 식으로든 게임을 재균형화하도록 설계된 모드 (예: Safe autodocs).
- `magical` - 게임에 마법 관련 항목을 추가하는 모드 (예: Necromancy)
- `item_exclude` - 세계에서 아이템이 생성되지 않도록 하는 모드 (예: No survivor armor, No drugs)
- `monster_exclude` - 특정 몬스터 종류가 세계에서 생성되지 않도록 하는 모드 (예: No fungal monsters, No ants)
- `graphical` - 어떤 식으로든 게임 그래픽을 조정하는 모드 (예: Graphical overmap)

`dependencies` 속성은 Cataclysm에 모드가 다른 모드에 있는 것에 의존한다는 것을 알려주는 데 사용됩니다. 핵심 게임 외에 의존성이 없으면 목록에 `bn`만 포함하는 것으로 충분합니다. 모드가 제대로 작동하기 위해 다른 모드에 의존하는 경우 해당 모드의 `id` 속성을 배열에 추가하면 Cataclysm이 해당 모드를 먼저 로드하도록 강제합니다.

`MOD_INFO` 객체에 대한 자세한 내용은 [JSON_INFO.md](./../reference/mod_info)를 참조하세요.

## 실제로 모드에 것들 추가하기

이제 기본 모드가 있으므로 실제로 그 안에 물건을 넣을 수 있습니다!

### 파일 구조

다양한 카테고리의 추가 사항을 다른 json 파일에 넣는 것이 좋습니다. 모드 폴더 또는 하위 폴더에 있는 모든 json 파일은 Cataclysm에 의해 감지되고 읽히지만, 그 외에는 어디에 무엇을 넣을 수 있는지에 대한 제한이 없습니다.

### JSON_INFO.md

이 모드로 할 수 있는 모든 것의 포괄적인 목록을 얻으려면 [JSON_INFO.md](./../reference/json_info)를 읽는 것이 좋습니다. 이 문서의 나머지 부분에는 복사하여 붙여넣을 수 있는 몇 가지 예시가 있지만 결코 포괄적이지 않습니다. 기본 게임의 데이터도 작성하는 모든 모드와 동일한 방식으로 정의되므로 게임의 json 파일(`data/json`에 있음)을 살펴보면 많은 것을 배울 수 있습니다. 게임 세계를 로드하려고 할 때 게임이 JSON 구문에서 문제를 발견하면 오류 메시지를 출력하고 문제가 해결될 때까지 해당 게임을 로드할 수 없습니다.

### 시나리오 추가

시나리오는 게임이 캐릭터를 생성할 때 일반적인 상황을 결정하는 데 사용하는 것입니다. 캐릭터가 세계에서 생성될 수 있는 시기와 장소, 어떤 직업을 사용할 수 있는지 결정합니다. 또한 해당 직업이 시작 시 돌연변이를 가질 수 있는지 여부를 결정하는 데 사용됩니다. 아래에서 게임의 내장 `Large Building` 시나리오에 대한 JSON 정의를 찾을 수 있습니다.

```json
[
  {
    "type": "scenario",
    "id": "largebuilding",
    "name": "Large Building",
    "points": -2,
    "description": "Whether due to stubbornness, ignorance, or just plain bad luck, you missed the evacuation, and are stuck in a large building full of the risen dead.",
    "allowed_locs": [
      "mall_a_12",
      "mall_a_30",
      "apartments_con_tower_114",
      "apartments_con_tower_014",
      "apartments_con_tower_104",
      "apartments_con_tower_004",
      "hospital_1",
      "hospital_2",
      "hospital_3",
      "hospital_4",
      "hospital_5",
      "hospital_6",
      "hospital_7",
      "hospital_8",
      "hospital_9"
    ],
    "start_name": "In Large Building",
    "surround_groups": [["GROUP_BLACK_ROAD", 70.0]],
    "flags": ["CITY_START", "LONE_START"]
  }
]
```

### 직업 추가

직업은 게임이 게임 시작 시 선택할 수 있는 캐릭터 클래스를 부르는 것입니다. 직업은 특성, 스킬, 아이템, 심지어 애완동물로 시작할 수 있습니다! 아래는 Police Officer 직업의 정의입니다:

```json
[
  {
    "type": "profession",
    "id": "cop",
    "name": "Police Officer",
    "description": "Just a small-town deputy when you got the call, you were still ready to come to the rescue.  Except that soon it was you who needed rescuing - you were lucky to escape with your life.  Who's going to respect your authority when the government this badge represents might not even exist anymore?",
    "points": 2,
    "skills": [{ "level": 3, "name": "gun" }, { "level": 3, "name": "pistol" }],
    "traits": ["PROF_POLICE"],
    "items": {
      "both": {
        "items": [
          "pants_army",
          "socks",
          "badge_deputy",
          "sheriffshirt",
          "police_belt",
          "smart_phone",
          "boots",
          "whistle",
          "wristwatch"
        ],
        "entries": [
          { "group": "charged_two_way_radio" },
          { "item": "ear_plugs", "custom-flags": ["no_auto_equip"] },
          { "item": "usp_45", "ammo-item": "45_acp", "charges": 12, "container-item": "holster" },
          { "item": "legpouch_large", "contents-group": "army_mags_usp45" }
        ]
      },
      "male": ["boxer_shorts"],
      "female": ["bra", "boy_shorts"]
    }
  }
]
```

### 아이템 추가

아이템은 정말로 [JSON_INFO](./../reference/json_info) 파일을 읽고 싶은 곳입니다. 왜냐하면 할 수 있는 것이 너무 많고 모든 아이템 카테고리가 약간씩 다르기 때문입니다.

<!--가능한 한 기본적인 아이템으로 선택했습니다. 다른 모든 것은 무언가를 합니다.-->

```json
[
  {
    "id": "family_photo",
    "type": "GENERIC",
    "//": "Unique mission item for the CITY_COP.",
    "category": "other",
    "name": "family photo",
    "description": "A photo of a smiling family on a camping trip.  One of the parents looks like a cleaner, happier version of the person you know.",
    "weight": "1 g",
    "volume": 0,
    "price": 800,
    "material": ["paper"],
    "symbol": "*",
    "color": "light_gray"
  }
]
```

### 몬스터가 생성되지 않도록 방지

이러한 종류의 모드는 비교적 간단하지만 매우 유용합니다. 세계에서 특정 유형의 몬스터를 처리하고 싶지 않다면 이렇게 하면 됩니다. 이를 수행하는 두 가지 방법이 있으며 둘 다 아래에 자세히 설명되어 있습니다. 전체 몬스터 그룹을 블랙리스트에 올리거나 특정 몬스터를 블랙리스트에 올릴 수 있습니다. 이러한 작업 중 하나를 수행하려면 해당 몬스터의 ID가 필요합니다. 이것들은 관련 데이터 파일에서 찾을 수 있습니다. 핵심 게임의 경우 이것들은 `data/json/monsters` 디렉토리에 있습니다. 아래 예시는 `No Ants` 모드에서 가져온 것이며 게임 내에서 모든 종류의 개미가 생성되지 않도록 합니다.

```json
[
  {
    "type": "MONSTER_BLACKLIST",
    "categories": ["GROUP_ANT", "GROUP_ANT_ACID"]
  },
  {
    "type": "MONSTER_BLACKLIST",
    "monsters": [
      "mon_ant_acid_larva",
      "mon_ant_acid_soldier",
      "mon_ant_acid_queen",
      "mon_ant_larva",
      "mon_ant_soldier",
      "mon_ant_queen",
      "mon_ant_acid",
      "mon_ant"
    ]
  }
]
```

### 위치가 생성되지 않도록 방지

<!--이 섹션에 대해 특별히 만족하지 않습니다. 맵에서 것들을 블랙리스트에 올리는 것은 블랙리스트에 올리는 것에 따라 다르게 작동합니다. 오버맵 스페셜은 오버맵 엑스트라, 도시 건물 및 건물 그룹과 다릅니다.-->

게임 내에서 특정 유형의 위치가 생성되지 않도록 방지하는 것은 대상으로 하려는 것의 유형에 따라 약간 더 까다롭습니다. 오버맵 건물은 표준 건물이거나 오버맵 스페셜일 수 있습니다. 특정 플래그가 있는 것들이 생성되지 않도록 차단하려면 몬스터와 매우 유사한 방식으로 블랙리스트에 올릴 수 있습니다. 아래 예시도 `No Ants` 모드에서 가져온 것이며 모든 개미집이 생성되지 않도록 합니다.

```json
[
  {
    "type": "region_overlay",
    "regions": ["all"],
    "overmap_feature_flag_settings": { "blacklist": ["ANT"] }
  }
]
```

블랙리스트에 올리려는 위치가 오버맵 스페셜인 경우 해당 정의에서 복사하고 `occurrences` 속성을 `[ 0, 0 ]`로 수동으로 설정해야 할 가능성이 높습니다.

마지막으로 도시 내부에서 생성되는 것을 블랙리스트에 올리려는 경우 지역 오버레이를 사용하여 이를 수행할 수 있습니다. 아래 예시는 `No rail stations` 모드에서 사용되며 도시 내부에서 철도역이 생성되지 않도록 합니다. 그러나 철도역 오버맵 스페셜이 생성되는 것을 막지는 않습니다.

```json
[
  {
    "type": "region_overlay",
    "regions": ["all"],
    "city": { "houses": { "railroad_city": 0 } }
  }
]
```

### 특정 시나리오 비활성화

`SCENARIO_BLACKLIST`는 블랙리스트 또는 화이트리스트일 수 있습니다. 화이트리스트인 경우 지정된 것을 제외한 모든 시나리오를 블랙리스트에 올립니다. 한 번에 둘 이상의 블랙리스트를 지정할 수 없습니다 - 이것은 특정 모드뿐만 아니라 특정 게임에 대해 로드된 모든 json(모든 모드 + 기본 게임)에 해당됩니다. 형식은 다음과 같습니다:

```json
[
  {
    "type": "SCENARIO_BLACKLIST",
    "subtype": "whitelist",
    "scenarios": ["largebuilding"]
  }
]
```

`subtype`의 유효한 값은 `whitelist`와 `blacklist`입니다. `scenarios`는 블랙리스트에 올리거나 화이트리스트에 올리려는 시나리오 id의 배열입니다.

### 몬스터 조정

```json
[
  {
    "type": "monster_adjustment",
    "species": "ZOMBIE",
    "flag": { "name": "REVIVES", "value": false },
    // 여기서는 모든 플래그가 작동합니다.
    "stat": { "name": "speed", "modifier": 10 },
    // stat은 speed와 HP만 stat 이름으로 허용합니다.
    "special": "nightvision",
    // nightvision은 이 타입의 모든 몬스터에게 야간 투시를 부여합니다.
    // no_zombify는 몬스터에 대한 "zombify_into" 항목을 제거합니다.
  }
```

## json 파일에 대한 중요 참고 사항

다음 문자들: `[ { , } ] : "`은 JSON 파일을 추가하거나 수정할 때 _매우_ 중요합니다.
이것은 단일 누락된 `,` 또는 `[` 또는 `}`가 작동하는 파일과 시작 시 중단되는 게임의 차이가 될 수 있음을 의미합니다. 하고 있는 작업에 이러한 문자를 포함하려면(예를 들어 아이템 설명에 인용 부호를 넣으려는 경우) 관련 문자 앞에 백슬래시를 넣어 수행할 수 있습니다. 이것을 해당 문자를 "이스케이프"한다고 합니다. 예를 들어 원한다면 다음과 같이 하여 아이템 설명에 인용 부호를 포함할 수 있습니다:

```json
...
"description": "This is a shirt that says \"I wanna kill ALL the zombies\" on the front.",
...
```

게임에서는 다음과 같이 나타납니다:
`This is a shirt that says "I wanna kill ALL the zombies" on the front.`

많은 편집기에는 `{ [`와 `] }`를 추적하여 균형이 맞는지(즉, 일치하는 반대가 있는지) 확인할 수 있는 기능이 있습니다. 이러한 편집기는 이스케이프된 문자도 올바르게 존중합니다.
[Notepad++](https://notepad-plus-plus.org/)는 이 기능을 포함하는 Windows의 인기 있는 무료 편집기입니다. Linux에서는 많은 옵션이 있으며 이미 선호하는 것이 있을 것입니다 🙂

온라인 또는 오프라인 JSON 검증기를 사용할 수도 있습니다. 예를 들어
https://dev.narc.ro/cataclysm/format.html 또는 게임의 모든 릴리스와 함께 번들로 제공되는 `json_formatter`(그냥 .json 파일을 드래그 앤 드롭하세요).

## 부록

<!-- 이것이 여기 있어야 하는지 정말 모르겠습니다. 알려주세요. -->

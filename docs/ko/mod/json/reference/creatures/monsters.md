---
title: 몬스터
tableOfContents:
  maxHeadingLevel: 4
---

## 몬스터 타입

몬스터 타입은 "type" 멤버가 "MONSTER"로 설정된 JSON 객체로 지정됩니다:

```json
{
    "type": "MONSTER",
    "id": "mon_foo",
    ...
}
```

id 멤버는 타입의 고유 id여야 합니다. 어떤 문자열이든 가능하지만, 관례상 "mon_" 접두사를 사용합니다. 이 id는 몬스터 그룹이나 맵 생성에서 특정 몬스터를 스폰하기 위해 다양한 곳에서 참조될 수 있습니다.

수량 문자열(부피, 무게 등)의 경우, 전체 정밀도를 유지할 수 있는 가장 큰 단위를 사용하세요.

몬스터 타입은 다음 속성들을 지원합니다(별도 표시가 없으면 필수):

## "name"

(string 또는 object)

```json
"name": "cow"
```

```json
"name": { "ctxt": "fish", "str": "pike", "str_pl": "pikes" }
```

또는 단수형과 복수형이 같은 경우:

```json
"name": { "ctxt": "fish", "str_sp": "bass" }
```

게임 내 표시되는 이름, 그리고 선택적으로 복수형 이름과 번역 컨텍스트(ctxt).

복수형 이름이 지정되지 않으면, 단수형 이름 + "s"가 기본값입니다.

Ctxt는 동음이의어(같은 이름을 가진 두 가지 다른 것)의 경우 번역자를 돕기 위해 사용됩니다. 예를 들어, 물고기 파이크와 무기 파이크가 있습니다.

## "description"

(string)

몬스터의 게임 내 설명.

## "categories"

(array of strings, optional)

몬스터 카테고리. NULL, CLASSIC (클래식 좀비 영화에서만 발견되는 몹) 또는 WILDLIFE (자연 동물)가 될 수 있습니다. CLASSIC이나 WILDLIFE가 아니면 클래식 모드에서 스폰되지 않습니다. 모드에서 "add:flags"와 "remove:flags"를 통해 항목을 추가하거나 제거할 수 있습니다.

## "species"

(array of strings, optional)

종(species) ID 목록. 모드에서 "add:species"와 "remove:species"를 통해 항목을 추가하거나 제거할 수 있습니다. 아래 Modding 섹션을 참조하세요. 종의 속성(현재는 트리거만)이 해당 종에 속하는 각 몬스터의 속성에 추가됩니다.

본 게임에서는 HUMAN, ROBOT, ZOMBIE, MAMMAL, BIRD, FISH, REPTILE, WORM, MOLLUSK, AMPHIBIAN, INSECT, SPIDER, FUNGUS, PLANT, NETHER, MUTANT, BLOB, HORROR, ABERRATION, HALLUCINATION, UNKNOWN이 될 수 있습니다.

## "volume"

(string)

```json
"volume": "40 L"
```

문자열의 숫자 부분은 정수여야 합니다. L과 ml을 단위로 받습니다. 참고로 l과 mL은 현재 허용되지 않습니다.

## "weight"

(string)

```json
"weight": "3 kg"
```

문자열의 숫자 부분은 정수여야 합니다. 전체 정밀도를 유지할 수 있는 가장 큰 단위를 사용하세요. 예: 3000 g이 아닌 3 kg. g와 kg을 단위로 받습니다.

## "scent_tracked"

(array of strings, optional)

이 몬스터가 추적하는 scent_type_id 목록. scent_type은 scent_types.json에 정의되어 있습니다.

## "scent_ignored"

(array of strings, optional)

이 몬스터가 무시하는 scent_type_id 목록. scent_type은 scent_types.json에 정의되어 있습니다.

## "symbol", "color"

(string)

게임 내 몬스터를 나타내는 심볼과 색상. 심볼은 정확히 하나의 콘솔 셀 너비(여러 유니코드 문자일 수 있음)의 UTF-8 문자열이어야 합니다. 자세한 내용은 [COLOR.md](../graphics/COLOR)를 참조하세요.

## "size"

(string, optional)

크기 플래그, json_flags.md를 참조하세요.

## "material"

(array of strings, optional)

몬스터가 주로 구성된 재료. 유효한 material id를 포함해야 합니다. 빈 배열(기본값)도 허용되며, 몬스터는 특정 재료로 만들어지지 않습니다. 모드에서 "add:material"과 "remove:material"을 통해 항목을 추가하거나 제거할 수 있습니다.

## "phase"

(string, optional)

몬스터의 물질 상태를 설명합니다. 그러나 현재는 게임플레이 목적이 없는 것 같습니다. SOLID, LIQUID, GAS, PLASMA 또는 NULL이 될 수 있습니다.

## "default_faction"

(string)

몬스터가 속한 진영의 ID, 이것은 다른 몬스터들과 싸울지 여부에 영향을 줍니다. Monster factions를 참조하세요.

## "bodytype"

(string)

몬스터 신체 유형의 ID, 몬스터 신체 레이아웃에 대한 일반적인 설명입니다.

값은 다음 중 하나여야 합니다:

| value         | description                                                           |
| ------------- | --------------------------------------------------------------------- |
| angel         | 날개 달린 인간                                                        |
| bear          | 뒷다리로 설 수 있는 네발 동물                                         |
| bird          | 두 다리와 두 날개를 가진 동물                                         |
| blob          | 물질 덩어리                                                           |
| crab          | 두 개의 큰 팔을 가진 다지 동물                                        |
| quadruped     | 네발 동물                                                             |
| elephant      | 머리와 몸통이 큰 매우 큰 네발 동물, 팔다리 크기가 동일함              |
| fish          | 유선형 몸체와 지느러미를 가진 수생 동물                               |
| flying insect | 머리, 두 개의 몸체 부분, 날개를 가진 여섯 다리 동물                   |
| frog          | 목이 있고 매우 큰 뒷다리와 작은 앞다리를 가진 네발 동물               |
| gator         | 매우 긴 몸체와 짧은 다리를 가진 네발 동물                             |
| human         | 두 팔을 가진 이족 동물                                                |
| insect        | 머리와 두 개의 몸체 부분을 가진 여섯 다리 동물                        |
| kangaroo      | 매우 큰 뒷다리와 작은 앞다리를 가지고 큰 꼬리로 균형을 잡는 오지 동물 |
| lizard        | 'gator'의 작은 형태                                                   |
| migo          | 미고가 가진 형태                                                      |
| pig           | 머리가 몸과 같은 선상에 있는 네발 동물                                |
| spider        | 큰 복부에 작은 머리를 가진 여덟 다리 동물                             |
| snake         | 팔다리가 없는 긴 몸체를 가진 동물                                     |

## "attack_cost"

(integer, optional)

일반 공격당 이동 수.

## "diff"

(integer, optional)

몬스터 기본 난이도. 몬스터에 레이블을 지정하는 데 사용되는 색조에 영향을 미치며, 30 이상이면 처치가 추모 로그에 기록됩니다. 몬스터 난이도는 예상 근접 피해, 회피, 방어, 체력, 속도, 사기, 공격성, 시야 범위를 기반으로 계산됩니다. 계산은 원거리 특수 공격이나 고유 특수 공격을 잘 처리하지 못하며, 기본 난이도로 이를 보정할 수 있습니다. 권장 값:

| value | description                                                                                                                                                                  |
| ----- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2     | 스키터봇의 테이저와 같은 제한적인 방어 능력, 또는 근처 몬스터에게 경고하는 쉬리커 좀비의 특수 능력과 같은 약한 특수 능력, 또는 독이나 독액과 같은 공격에 대한 사소한 보너스. |
| 5     | 스피터 좀비의 침보다 약한 제한적인 원거리 공격, 또는 쇼커 좀비의 zapback이나 산성 좀비의 산성 분무와 같은 강력한 방어 능력.                                                  |
| 10    | 스피터 좀비의 침이나 터렛의 9mm SMG와 같은 강력한 원거리 공격.                                                                                                               |
| 15    | 부식성 좀비의 침과 같은 추가 위험이 있는 강력한 원거리 공격                                                                                                                  |
| 20    | 레이저 터렛이나 군사 터렛의 5.56mm 소총과 같은 매우 강력한 원거리 공격, 또는 좀비 네크로맨서의 다른 좀비를 부활시키는 능력과 같은 강력한 특수 능력.                          |
| 30    | 대물 저격 터렛의 .50 BMG 소총과 같이 방어구를 입은 캐릭터에게도 치명적인 원거리 공격.                                                                                        |

좀비 헐크나 레이저클로 알파와 같은 위험한 몬스터도 대부분 난이도가 0이어야 합니다. 난이도는 예외적인 원거리 특수 공격에만 사용되어야 합니다.

## "aggression"

(integer, optional)

몬스터의 공격성을 정의합니다. -99(완전 수동적)에서 100(감지 시 적대 보장)까지 범위입니다.

## "morale"

(integer, optional)

몬스터 사기. 몬스터가 후퇴하기 전에 HP가 얼마나 낮아질 수 있는지 정의합니다. 이 숫자는 최대 HP의 %로 처리됩니다.

## "aggro_character"

(bool, optional, default true)

몬스터가 대상을 결정할 때 몬스터와 캐릭터(NPC, 플레이어)를 구별할지 여부 - false이면 캐릭터가 분노 트리거를 발동시킬 때까지 몬스터는 현재 분노/사기에 관계없이 캐릭터를 무시합니다. 몬스터가 기본 분노 수준일 때 무작위로 재설정됩니다.

## "speed"

(integer)

몬스터 속도. 100은 인간의 정상 속도입니다 - 높은 값은 더 빠르고 낮은 값은 더 느립니다.

## "mountable_weight_ratio"

(float, optional)

허용 가능한 탑승자 대 탑승물 무게 백분율 비율로 사용됩니다. 기본값은 "0.3"이며, 이는 탑승물이 탑승물 무게의 30% 이하인 탑승자를 운반할 수 있음을 의미합니다.

## "melee_skill"

(integer, optional)

몬스터 근접 스킬, 기본 게임에서는 일반적으로 0 - 10 범위이며, 4가 평균 몹입니다. 10을 넘으면 마법이나 변칙적일 가능성이 높습니다.
더 많은 예제는 GAME_BALANCE.txt를 참조하세요.

## "dodge"

(integer, optional)

몬스터 회피 스킬. 기본 게임에서 0에서 10. 회피 메커니즘에 대한 설명은 GAME_BALANCE.txt를 참조하세요.

## "melee_damage"

(array of objects, optional)

| Property            | Description                                                                                                                                                          |
| ------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `damage_type`       | 피해 유형. 유효한 항목: `"true"`, `"bash"`, `"cut"`, `"stab"`, `"bullet"`, `"biological"`, `"acid"`, `"heat"`, `"cold"`, `"dark"`, `"light"`, `"psi"`, `"electric"`. |
| `amount`            | 가한 피해량.                                                                                                                                                         |
| `armor_penetration` | 이 피해 인스턴스가 무시하는 방어 양.                                                                                                                                 |
| `armor_multiplier`  | `armor_penetration`에 적용되는 승수.                                                                                                                                 |
| `damage_multiplier` | `amount`에 적용되는 승수.                                                                                                                                            |

```json
"melee_damage": [ { "damage_type": "electric", "amount": 4.0, "armor_penetration": 1, "armor_multiplier": 1.2, "damage_multiplier": 1.4 } ],
```

## "melee_dice", "melee_dice_sides"

(integer, optional)

몬스터 근접 공격 시 굴려지는 주사위의 수와 면. 이것은 타격 피해량을 정의합니다.

## "grab_strength"

(integer, optional)

이 몬스터가 적용하는 잡기 효과의 강도. 기본값은 1이며, GRAB 특수 공격과 GRABS 플래그가 있는 몬스터에만 유용합니다. grab_strength = n인 몬스터는 n마리의 좀비처럼 잡기를 적용합니다. max(Str,Dex)<=n인 플레이어는 그 잡기를 풀 수 없습니다.

## "melee_cut"

(integer, optional)

몬스터 근접 공격 시 주사위 굴림에 추가되는 베기 피해량.

## "armor_bash", "armor_cut", "armor_stab", "armor_acid", "armor_fire", "armor_cold", "armor_electric"

(integer, optional)

타격, 베기, 찌르기, 산성, 화염, 냉기 및 전기 피해로부터의 몬스터 보호.

## "vision_day", "vision_night"

(integer, optional)

완전한 낮과 완전한 어둠에서의 시야 범위.

## "luminance"

(integer, optional)

몬스터가 수동적으로 출력하는 빛의 양. 0에서 10까지입니다.

## "hp"

(integer)

몬스터 체력.

## "death_drops"

(string or item group, optional)

몬스터가 죽을 때 아이템을 스폰하는 데 사용되는 아이템 그룹. 이것은 인라인 아이템 그룹이 될 수 있습니다. ITEM_SPAWN.md를 참조하세요. 기본 하위 유형은 "distribution"입니다.

## "death_function"

(array of strings, optional)

몬스터가 죽을 때 행동하는 방식. 가능한 함수 목록은 json_flags.md를 참조하세요. 모드에서 "add:death_function"과 "remove:death_function"을 통해 항목을 추가하거나 제거할 수 있습니다.

## "on_death"

(dictionary, optional)

죽을 때 적용할 행동의 모음. 내부에 "death_function"을 포함할 수 있습니다. 이 필드가 있으면 "death_function" 필드는 건너뜁니다.

```json
"on_death": {
  "death_function": [ "THING" ]
}
```

또한 지원합니다:

### "spawn_mon"

(string, optional) 죽는 몬스터의 위치에 단일 몬스터를 스폰합니다

```json
"on_death": {
  "spawn_mon": "mon_thing"
}
```

### "spawn_mon_near"

(dictionary, optional)

두 값으로 구성됩니다:

- "distance": (integer, required) 몬스터를 스폰할 수 있는 죽음 위치로부터의 최대 거리
- "ids": (string or array of strings, optional) 설정된 거리 내에 스폰을 시도할 몬스터의 ID

단일 몬스터를 스폰할 수 있습니다

```json
"on_death": {
  "spawn_mon_near": {
    "distance": 5,
    "ids": "mon_thing"
  }
}
```

또는 여러 몬스터

```json
"on_death": {
  "spawn_mon_near": {
    "distance": 3,
    "ids": [ "mon_thing", "mon_zombie", "mon_zombie_anklebiter" ]
  }
}
```

## "harvest"

(string, optional)

이 몬스터의 죽음 함수가 시체를 남기면, 이것은 시체를 도살하거나 해부할 때 생산되는 아이템을 정의합니다. 지정되지 않으면 기본값은 `human`입니다. 수확 항목, 수확량 및 질량 비율은 harvest.json에 정의되어 있습니다.

## "emit_field"

(array of objects of emit_id and time_duration, optional) "emit_fields": [ { "emit_id": "emit_gum_web", "delay": "30 m" } ],

몬스터가 방출하는 필드와 얼마나 자주 방출하는지. 시간 지속 시간은 문자열을 사용할 수 있습니다: "1 h", "60 m", "3600 s" 등...

## "regenerates"

(integer, optional)

턴당 재생되는 체력.

## "regeneration_modifiers"

( array of objects consisting of effect, base_mod (float) and scaling_mod (float), optional )
"regeneration_modifiers": [ { "effect": "on_fire", "base_mod": -0.3, "scaling_mod": -0.15 } ],

어떤 효과(있는 경우)가 몬스터의 재생에 긍정적/부정적으로 영향을 미치는지. 수정자는 가산적으로 쌓입니다(강도 2 on_fire는 0.45의 승수 생성) 하지만 곱셈적으로 적용됩니다.

## "regenerates_in_dark"

(boolean, optional)

어둡게 조명된 타일에서 몬스터가 매우 빠르게 재생합니다.

## "regen_morale"

(boolean, optional)

최대 HP일 때 도주를 중단하고 분노와 사기를 재생합니다.

## "special_attacks"

(array of special attack definitions, optional)

몬스터의 특수 공격. 이것은 배열이어야 하며, 각 요소는 객체(새 스타일) 또는 배열(오래된 스타일)이어야 합니다.

오래된 스타일 배열은 2개의 요소를 포함해야 합니다: 공격의 ID(목록은 json_flags.md 참조)와 해당 공격의 재사용 대기 시간. 예제(10턴마다 잡기 공격):

```json
"special_attacks": [ [ "GRAB", 10 ] ]
```

새 스타일 객체는 최소한 "type" 멤버(string)와 "cooldown" 멤버(integer)를 포함해야 합니다. 특정 유형에서 요구하는 추가 멤버를 포함할 수 있습니다. 가능한 유형은 아래에 나열되어 있습니다. 예제:

```json
"special_attacks": [
    { "type": "leap", "cooldown": 10, "max_range": 4 }
]
```

"special_attacks"는 오래된 스타일과 새 스타일 항목의 혼합을 포함할 수 있습니다:

```json
"special_attacks": [
    [ "GRAB", 10 ],
    { "type": "leap", "cooldown": 10, "max_range": 4 }
]
```

`"extend"` 필드를 통해 (상속을 수행할 때) 항목을 추가할 수 있습니다.

```json
"extend": { "special_attacks": [ [ "PARROT", 50 ] ] }
```

항목은 `"delete"` 필드를 통해 제거할 수 있습니다. 그러나 특수 공격을 추가하는 것(수동으로 또는 extends를 통해)과 달리 구문이 약간 다릅니다. 두 번째 괄호 세트가 필요하지 않으며, 재사용 대기 시간을 포함해서는 안 됩니다.

```json
"delete": { "special_attacks": [ "FUNGUS" ] },
```

## "flags"

(array of strings, optional)

몬스터 플래그. 전체 목록은 json_flags.md를 참조하세요. 모드에서 "add:flags"와 "remove:flags"를 통해 항목을 추가하거나 제거할 수 있습니다.

## "fear_triggers", "anger_triggers", "placate_triggers"

(array of strings, optional)

몬스터를 두렵게 만들거나 / 화나게 만들거나 / 진정시키는 것. 전체 목록은 json_flags.md를 참조하세요.

## "revert_to_itype"

(string, optional)

비어 있지 않고 유효한 아이템 ID인 경우, 몬스터가 우호적일 때 플레이어가 이 아이템으로 변환할 수 있습니다. 이것은 일반적으로 터렛 몬스터를 터렛 아이템으로 되돌리는 데 사용되며, 이는 다른 곳에 줍고 배치할 수 있습니다.

## "starting_ammo"

(object, optional)

새로 스폰된 몬스터가 시작하는 탄약을 포함하는 객체. 이것은 탄약을 소비하는 특수 공격을 가진 몬스터에 유용합니다. 예제:

```json
"starting_ammo": { "9mm": 100, "40mm_frag": 100 }
```

## "upgrades"

(boolean or object, optional)

이 몬스터가 시간이 지남에 따라 업그레이드되는 방식을 제어합니다. 단일 값 `false`(기본값이며 업그레이드를 비활성화)이거나 업그레이드를 설명하는 객체일 수 있습니다.

예제:

```json
"upgrades": {
    "into_group": "GROUP_ZOMBIE_UPGRADE",
    "half_life": 28
}
```

upgrades 객체는 다음 멤버를 가질 수 있습니다:

### "half_life"

(int) 근사 지수 진행에 따라 몬스터의 절반이 업그레이드되는 시간. 기본값이 4일인 진화 스케일링 요소로 스케일링됩니다.

### "into_group"

(string, optional) 업그레이드된 몬스터의 유형은 지정된 그룹에서 가져옵니다. 이 그룹의 비용은 스폰 프로세스의 업그레이드(드문 "replace_monster_group" 및 스폰 그룹의 "new_monster_group_id" 속성과 관련)에 대한 것입니다.

### "into"

(string, optional) 업그레이드된 몬스터의 유형.

### "age_grow"

(int, optional) 몬스터가 다른 몬스터로 변경하는 데 필요한 일수.

## "reproduction"

(dictionary, optional) 몬스터의 번식 주기(있는 경우). 지원:

### "baby_monster"

(string, optional) 출산을 하는 몬스터의 번식 시 스폰되는 몬스터의 ID. 번식이 작동하려면 이것 또는 `baby_egg`를 선언해야 합니다.

### "baby_egg"

(string, optional) 알을 낳는 몬스터를 위해 스폰할 알 유형의 ID. 번식이 작동하려면 이것 또는 "baby_monster"를 선언해야 합니다.

### "baby_count"

(int) 번식 시 스폰할 새로운 생물 또는 알의 수.

### "baby_timer"

(int) 번식 이벤트 사이의 일수.

## "baby_flags"

(Array, optional) 이 몬스터가 번식할 수 있는 계절을 지정합니다. 예: `[ "SPRING", "SUMMER" ]`

## "special_when_hit"

(array, optional)

몬스터가 공격받을 때 발동되는 특수 방어 공격. 방어의 ID(json_flags.md의 Monster defense attacks 참조)와 해당 방어가 실제로 발동될 확률을 포함하는 배열을 포함해야 합니다. 예제:

```json
"special_when_hit": [ "ZAPBACK", 100 ]
```

## "attack_effs"

(array of objects, optional)

몬스터가 성공적으로 공격할 때 공격받은 생물에게 적용될 수 있는 효과 세트. 예제:

```json
"attack_effs": [
    {
        "id": "paralyzepoison",
        "duration": 33,
        "chance": 50
    }
]
```

배열의 각 요소는 다음 멤버를 포함하는 객체여야 합니다:

### "id"

(string)

적용할 효과의 ID.

### "duration"

(integer, optional)

효과가 지속되어야 하는 시간(턴).

### "affect_hit_bp"

(boolean, optional)

효과를 아래에 설정된 것 대신 맞은 신체 부위에 적용할지 여부.

### "bp"

(string, optional)

효과가 적용되는 신체 부위. 기본값은 전신에 효과를 적용하는 것입니다. 일부 효과는 특정 신체 부위(예: "hot")가 필요하고 다른 효과는 전신(예: "meth")이 필요할 수 있습니다.

### "chance"

(integer, optional)

효과가 적용될 확률.

# Modding

몬스터 타입은 모드에서 재정의하거나 수정할 수 있습니다. 이를 위해 "edit-mode" 멤버를 추가해야 하며, 다음 중 하나를 포함할 수 있습니다:

- "create" (멤버가 존재하지 않으면 기본값), 주어진 ID를 가진 타입이 이미 존재하면 오류가 표시됩니다.
- "override", 주어진 ID를 가진 기존 타입(있는 경우)이 제거되고 새 데이터가 완전히 새로운 타입으로 로드됩니다.
- "modify", 기존 타입이 수정됩니다. 주어진 ID를 가진 타입이 없으면 오류가 표시됩니다.

필수 속성(optional로 표시되지 않은 모든 것)은 편집 모드가 "create" 또는 "override"인 경우에만 필요합니다.

예제(좀비 몬스터 이름 변경, 다른 모든 속성은 그대로 유지):

```json
{
  "type": "MONSTER",
  "edit-mode": "modify",
  "id": "mon_zombie",
  "name": "clown"
}
```

기본 편집 모드("create")는 새 타입에 적합하며, ID가 다른 모드나 핵심 데이터의 타입과 충돌하면 오류가 표시됩니다. 편집 모드 "override"는 타입을 처음부터 재정의하는 데 적합하며, 모든 필수 멤버가 나열되어 있고 이전 정의의 흔적을 남기지 않습니다. 편집 모드 "modify"는 플래그 추가 또는 특수 공격 제거와 같은 작은 변경 사항에 사용됩니다.

타입을 수정하면 속성이 새 값으로 재정의되며, 이 예제는 특수 공격을 _only_ "SHRIEK" 공격을 포함하도록 설정합니다:

```json
{
  "type": "MONSTER",
  "edit-mode": "modify",
  "id": "mon_zombie",
  "special_attacks": [["SHRIEK", 20]]
}
```

일부 속성은 위에 문서화된 대로 항목을 추가하고 제거할 수 있으며, 일반적으로 "add:"/"remove:" 접두사가 있는 멤버를 통해 가능합니다.

## Monster special attack types

나열된 공격 유형은 몬스터 특수 공격으로 사용할 수 있습니다("special_attacks" 참조).

### "leap"

몬스터가 몇 타일을 뛰어오르게 합니다. 다음 추가 속성을 지원합니다:

#### "max_range"

(Required) 공격의 최대 범위.

#### "min_range"

(Required) 공격에 필요한 최소 범위.

#### "allow_no_target"

이것은 몬스터가 빈 공간에서 능력을 사용하는 것을 방지합니다.

#### "move_cost"

특수 공격을 완료하는 데 필요한 턴. 100 속도로 100 move_cost는 1초/턴과 같습니다.

#### "min_consider_range", "max_consider_range"

특정 공격을 사용하기 위해 고려할 최소 범위 및 최대 범위.

### "bite"

몬스터가 이빨을 사용하여 상대를 물게 합니다. 일부 몬스터는 이렇게 감염을 줄 수 있습니다.

#### "damage_max_instance"

한 번 물 때 가할 수 있는 최대 피해.

#### "min_mul", "max_mul"

공격자를 죽이지 않고 물림에서 빠져나오기가 얼마나 어려운지.

#### "move_cost"

특수 공격을 완료하는 데 필요한 턴. 100 속도로 100 move_cost는 1초/턴과 같습니다.

#### "accuracy"

(Integer) 얼마나 정확한지. 그러나 많은 몬스터가 사용하지 않습니다.

#### "no_infection_chance"

감염을 주지 않을 확률.

### "gun"

대상에게 총을 발사합니다. 우호적이면 플레이어에게 피해를 주지 않습니다.

#### "gun_type"

(Required) 공격을 수행하는 데 사용될 총의 유효한 아이템 ID.

#### "ammo_type"

(Required) 총에 장전될 탄약의 유효한 아이템 ID. 몬스터는 또한 이 탄약을 가진 "starting_ammo" 필드를 가져야 합니다. 예:

```
"ammo_type" : "50bmg",
"starting_ammo" : {"50bmg":100}
```

#### "max_ammo"

탄약 상한선. 어떤 이유로든 탄약이 이 값을 초과하면 디버그 메시지가 출력됩니다.

#### "fake_str"

공격을 실행할 가짜 NPC의 힘 스탯. 지정되지 않으면 8.

#### "fake_dex"

공격을 실행할 가짜 NPC의 민첩 스탯. 지정되지 않으면 8.

#### "fake_int"

공격을 실행할 가짜 NPC의 지능 스탯. 지정되지 않으면 8.

#### "fake_per"

공격을 실행할 가짜 NPC의 지각 스탯. 지정되지 않으면 8.

#### "fake_skills"

스킬 ID와 스킬 레벨 쌍의 2개 요소 배열의 배열.

#### "move_cost"

공격 실행의 이동 비용

// true이면 플레이어에게 "유예 기간"을 제공합니다

#### "require_targeting_player"

true이면, 몬스터는 플레이어를 "대상 지정"해야 하며, `targeting_cost` 이동을 낭비하고, 공격을 재사용 대기 시간에 넣고, 경고 소리를 내야 합니다. 최근에 대상 지정이 필요한 것을 공격하지 않았다면 말입니다.

#### "require_targeting_npc"

위와 같지만 NPC와 함께.

#### "require_targeting_monster"

위와 같지만 몬스터와 함께.

#### "targeting_timeout"

대상 지정 상태가 이 턴 수 동안 적용됩니다. 대상 지정은 터렛에 적용되지 대상에는 적용되지 않습니다.

#### "targeting_timeout_extend"

성공적으로 공격하면 대상 지정이 이 턴 수 동안 연장됩니다. 음수일 수 있습니다.

#### "targeting_cost"

플레이어를 대상 지정하는 이동 비용. 플레이어를 공격하고 지난 5턴 내에 플레이어를 대상 지정하지 않은 경우에만 적용됩니다.

#### "no_crits"

true이면 공격하는 생물은 대상에 대한 원거리 치명타를 칠 수 없으며, 좋은 히트는 더 이상 머리로 갈 확률이 높지 않고 대신 몸과 팔다리에 히트할 확률이 높습니다. false이면 기본 동작이 사용됩니다 - 치명타가 허용되고 좋은 히트는 머리에 도달할 확률이 높습니다.

#### "laser_lock"

true이고 레이저 잠금이 필요하지만 없는 생물을 공격하는 경우, 몬스터는 대상 지정 상태가 없는 것처럼 행동하고(시간을 낭비하며 대상 지정), 대상이 레이저 잠금되고, 대상이 플레이어인 경우 경고를 발생시킵니다.

레이저 잠금은 대상에 영향을 미치지만 특정 공격자에 묶여 있지 않습니다.

#### "range"

대상이 획득될 최대 범위.

#### "range_no_burst"

대상이 연발(해당하는 경우)로 공격될 최대 범위.

#### "burst_limit"

연발 크기 제한.

#### "description"

플레이어가 본 경우 실행되는 공격에 대한 설명.

#### "targeting_sound"

대상 지정 시 만드는 소리에 대한 설명.

#### "targeting_volume"

대상 지정 시 만드는 소리의 볼륨.

#### "no_ammo_sound"

탄약이 떨어졌을 때 만드는 소리에 대한 설명.

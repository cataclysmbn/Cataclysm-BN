---
title: 몬스터
tableOfContents:
  maxHeadingLevel: 4
---

## 몬스터 종류

몬스터 타입은 "type" 멤버가 "MONSTER"로 설정된 JSON 객체로 지정됩니다.

```json
{
    "type": "MONSTER",
    "id": "mon_foo",
    ...
}
```

id 멤버는 해당 유형의 고유 ID여야 합니다. 어떤 문자열이든 될 수 있으며, 관례상 다음과 같습니다.
접두사 "mon_". 이 ID는 몬스터 그룹이나 mapgen 등 다양한 위치에서 참조될 수 있습니다.
특정 몬스터를 생성합니다.

수량 문자열(예: 부피, 무게)의 경우 전체 정밀도를 유지할 수 있는 가장 큰 단위를 사용하세요.

몬스터 타입은 다음 속성을 지원합니다(달리 명시된 경우를 제외하고 필수).

## "name"

(문자열 또는 객체)

```json
"name": "cow"
```

```json
"name": { "ctxt": "fish", "str": "pike", "str_pl": "pikes" }
```

또는 단수형과 복수형이 동일한 경우:

```json
"name": { "ctxt": "fish", "str_sp": "bass" }
```

게임 내 표시되는 이름, 선택적으로 복수형 이름 및 번역 컨텍스트(ctxt).

복수형 이름을 지정하지 않으면 기본적으로 단수형 이름 + "s"이 적용됩니다.

Ctxt는 동음이의어(동일한 이름을 가진 두 가지 다른 것)의 경우 번역자를 돕는 데 사용됩니다. 에 대한
예를 들어, 물고기를 파이크하고 무기를 파이크하십시오.

## "description"

(문자열)

몬스터에 대한 게임 내 설명입니다.

## "categories"

(문자열 배열, 선택 사항)

몬스터 카테고리. NULL, CLASSIC(클래식 좀비 영화에 나오는 몹만 해당) 또는 WILDLIFE일 수 있습니다.
(자연동물). CLASSIC 또는 WILDLIFE이 아닌 경우 클래식 모드에서 생성되지 않습니다. 하나는 할 수 있다
"add:flags" 및 "remove:flags"을 통해 모드에 항목을 추가하거나 제거합니다.

## "species"

(문자열 배열, 선택 사항)

종 ID 목록입니다. "add:species" 및 "remove:species"을 통해 모드에 항목을 추가하거나 제거할 수 있습니다.
아래 모딩을 참조하세요. 종의 속성(현재는 트리거만 해당)이 다음의 속성에 추가됩니다.
해당 종에 속하는 각 몬스터.

메인라인 게임에서는 HUMAN, ROBOT, ZOMBIE, MAMMAL, BIRD, FISH, REPTILE, WORM, MOLLUSK,
AMPHIBIAN, INSECT, SPIDER, FUNGUS, PLANT, NETHER, MUTANT, BLOB, HORROR, ABERRATION, HALLUCINATION
그리고 UNKNOWN.

## "volume"

(문자열)

```json
"volume": "40 L"
```

문자열의 숫자 부분은 정수여야 합니다. L과 ml를 단위로 사용합니다. l과 mL에 주목하세요.
현재는 허용되지 않습니다.

## "weight"

(문자열)

```json
"weight": "3 kg"
```

문자열의 숫자 부분은 정수여야 합니다. 전체 정밀도를 유지할 수 있는 가장 큰 단위를 사용하십시오.
와. 예: 3000g이 아닌 3kg. g와 kg을 단위로 받아들입니다.

## "scent_tracked"

(문자열 배열, 선택 사항)

이 몬스터가 추적하는 scenttype_id 목록입니다. scent_types는 scent_types.json에 정의되어 있습니다.

## "scent_ignored"

(문자열 배열, 선택 사항)

이 몬스터가 무시하는 scenttype_id 목록입니다. scent_types는 scent_types.json에 정의되어 있습니다.

## "symbol", "color"

(문자열)

게임 내 몬스터를 나타내는 기호와 색상입니다. 기호는 UTF-8 문자열이어야 합니다. 즉,
하나의 콘솔 셀 너비(여러 개의 유니코드 문자일 수 있음) 자세한 내용은 [COLOR.md](../graphics/color)를 참조하세요.
세부 사항.

## "size"

(문자열, 선택사항)

크기 플래그는 json_flags.md를 참조하세요.

## "material"

(문자열 배열, 선택 사항)

몬스터가 주로 구성되어 있는 재료입니다. 유효한 재료 ID를 포함해야 합니다. 빈 배열
(기본값)도 허용되며, 몬스터는 특정 재료로 만들어지지 않습니다. 추가하거나
"add:material" 및 "remove:material"을 통해 모드의 항목을 제거합니다.

## "phase"

(문자열, 선택사항)

몬스터의 신체 물질 상태를 묘사합니다. 다만, 게임 플레이 목적은 전혀 없는 것 같으니,
지금 당장. SOLID, LIQUID, GAS, PLASMA 또는 NULL일 수 있습니다.

## "default_faction"

(문자열)

몬스터가 속한 진영의 ID는 해당 몬스터가 싸울 다른 몬스터에 영향을 미칩니다. 참조
괴물 세력.

## "bodytype"

(문자열)

몬스터의 체형에 대한 이드, 몬스터의 레이아웃에 대한 일반적인 설명
몸.

값은 다음 중 하나여야 합니다.

| value         | 설명                                                                      |
| ------------- | ------------------------------------------------------------------------- |
| angel         | 날개 달린 인간                                                            |
| bear          | 뒷다리로 설 수 있는 네발 달린 동물                                        |
| bird          | 두 날개를 가진 두 다리 동물                                               |
| blob          | 물질 덩어리                                                               |
| crab          | 두 개의 큰 팔을 가진 다발 동물                                            |
| quadruped     | 다리가 네 개인 동물                                                       |
| elephant      | 큰 머리와 몸통, 같은 크기의 팔다리를 가진 매우 큰 네 발 달린 동물         |
| fish          | 유선형의 몸과 지느러미를 가진 수생 동물                                   |
| flying insect | 다리가 6개 달린 동물로 머리와 두 개의 몸체 부분, 날개가 있습니다.         |
| frog          | 목이 있고 뒷다리가 매우 크며 앞다리가 작은 네 다리 달린 동물              |
| gator         | 몸은 매우 길고 다리는 짧은 네발 달린 동물                                 |
| human         | 두 팔을 가진 이족보행 동물                                                |
| insect        | 다리가 여섯 개 있고 머리 하나와 몸통 두 부분이 있는 동물                  |
| kangaroo      | 매우 큰 뒷다리와 작은 팔뚝으로 안정성을 위해 큰 꼬리를 활용하는 오족 동물 |
| lizard        | 더 작은 형태의 '게이터'                                                   |
| migo          | 미고가 어떤 형태를 가지고 있든                                            |
| pig           | 다리가 네 개 달린 동물로 머리와 몸이 같은 줄에 있다                       |
| spider        | 큰 복부에 작은 머리를 가진 여덟 다리의 동물                               |
| snake         | 몸이 길고 팔다리가 없는 동물                                              |

## "attack_cost"

(정수, 선택사항)

일반 공격당 이동 횟수입니다.

## "diff"

(정수, 선택사항)

몬스터 기본 난이도. 몬스터 라벨에 사용된 색상에 영향을 미치며, 30a 이상인 경우
살인은 추모일지에 기록됩니다. 몬스터 난이도는 예상 근접 공격을 기준으로 계산됩니다.
손상, 회피, 방어구, 체력, 속도, 사기, 공격성 및 시야 범위. 계산은
원거리 특수 공격이나 고유 특수 공격을 잘 처리하지 못하며 기본 난이도는
그것을 설명하는 데 사용됩니다. 제안된 값:

| value | 설명                                                                                                                                                 |
| ----- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2     | Skitterbot의 테이저와 같은 제한된 방어 능력, 근처 몬스터에게 경고하는 비명을 지르는 좀비의 특별한 능력, 독이나 독과 같은 공격에 대한 사소한 보너스.  |
| 5     | 스피터 좀비의 침보다 약한 제한된 원거리 공격, 쇼커 좀비의 잽백 또는 산성 좀비의 산성 스프레이와 같은 강력한 방어 능력.                               |
| 10    | 스피터 좀비의 침이나 포탑의 9mm SMG와 같은 강력한 원거리 공격입니다.                                                                                 |
| 15    | 부식성 좀비의 침과 같은 추가 위험이 있는 강력한 원거리 공격                                                                                          |
| 20    | 레이저 포탑이나 군용 포탑의 5.56mm 소총과 같은 매우 강력한 원거리 공격 또는 다른 좀비를 키우는 좀비 네크로맨서의 능력과 같은 강력한 특수 능력입니다. |
| 30    | 대물 포탑의 .50 BMG 소총과 같이 장갑을 착용한 캐릭터에게도 치명적인 원거리 공격입니다.                                                               |

대부분의 몬스터는 난이도가 0이어야 합니다. 심지어 좀비 헐크나 면도날 발톱과 같은 위험한 몬스터도 마찬가지입니다.
알파. 난이도는 예외적, 원거리, 특수 공격에만 사용해야 합니다.

## "aggression"

(정수, 선택사항)

몬스터가 얼마나 공격적인지를 정의합니다. 범위는 -99(완전히 수동적)부터 100(보장됨)까지입니다.
탐지에 대한 적대감)

## "morale"

(정수, 선택사항)

괴물 사기. 몬스터가 후퇴하기 전에 얼마나 낮은 몬스터 HP를 얻을 수 있는지 정의합니다. 이 숫자는 %로 처리됩니다.
최대 HP의.

## "aggro_character"

(부울, 선택 사항, 기본값은 true)

몬스터가 몬스터와 캐릭터(NPC, Player)를 구별하여 결정할 것인지 여부
목표 - false인 경우 괴물은 현재 분노/사기에 관계없이 캐릭터를 무시합니다.
캐릭터 여행 및 분노 유발. 몬스터가 기본 분노 수준에 도달하면 무작위로 재설정됩니다.

## "lua_attitude"

(문자열, 선택사항)

설정된 경우 `game.monster_attitude_functions`의 Lua 함수를 사용하여 몬스터의
기본 C++ 논리 대신 태도를 사용합니다. Lua 함수는 `(monster, target)`으로 호출됩니다.
여기서 `target`은 `nil`일 수 있으며 `MonsterAttitude` 값을 반환해야 합니다. `nil` 반환 후 대체
기본 동작으로.

```json
"lua_attitude": "my_attitude"
```

```lua
game.monster_attitude_functions["my_attitude"] = function(mon, target)
  if target ~= nil and target:is_avatar() then
    return MonsterAttitude.MATT_ATTACK
  end
  return nil
end
```

## "lua_ai"

(문자열, 선택사항)

설정되면 각 몬스터 턴에서 `game.monster_ai_functions` Lua 기능을 실행합니다. 기능
`(monster)`으로 호출되며 부울 값을 반환해야 합니다. `true`은 Lua가 회전을 처리하고
기본 C++ AI를 건너뛰고 `false` 또는 `nil`가 기본 AI로 대체됩니다.

```json
"lua_ai": "my_ai"
```

```lua
game.monster_ai_functions["my_ai"] = function(mon)
  local avatar = gapi.get_avatar()
  if avatar ~= nil then
    local function sign(val)
      if val > 0 then
        return 1
      end
      if val < 0 then
        return -1
      end
      return 0
    end
    local pos = mon:get_pos_ms()
    local tgt = avatar:get_pos_ms()
    local step = Tripoint.new(pos.x + sign(tgt.x - pos.x), pos.y + sign(tgt.y - pos.y), pos.z)
    mon:set_target(avatar)
    mon:move_to(step, false, true, 1.0)
  end
  return true
end
```

## "speed"

(정수)

괴물같은 속도. 100은 인간의 정상적인 속도입니다. 값이 높을수록 빠르고 값이 낮을수록 좋습니다.
더 느립니다.

## "mountable_weight_ratio"

(플로트, 선택 사항)

허용 가능한 라이더 대 마운트 중량 비율로 사용됩니다. 기본값은 "0.3"입니다.
마운트는 마운트 무게의 <= 30% 무게의 라이더를 운반할 수 있습니다.

## "melee_skill"

(정수, 선택사항)

몬스터 근접 스킬의 범위는 일반적으로 기본 게임에서 0~10이며, 평균 몹은 4입니다. 과거 10은 마법적이거나 변칙적일 가능성이 높습니다.
더 많은 예를 보려면 GAME_BALANCE.txt를 참조하세요.

## "dodge"

(정수, 선택사항)

몬스터 회피 스킬. 기본 게임에서는 0~10입니다. 회피 메커니즘에 대한 설명은 GAME_BALANCE.txt를 참조하세요.

## "melee_damage"

(객체 배열, 선택 사항)

| Property            | 설명                                                                                |
| ------------------- | ----------------------------------------------------------------------------------- |
| `damage_type`       | 손상 유형. [손상 유형](/mod/json/reference/misc/damage/#damage-types)을 참조하세요. |
| `amount`            | 피해량                                                                              |
| `armor_penetration` | 이 피해 인스턴스가 무시하는 방어구의 양입니다.                                      |
| `armor_multiplier`  | `armor_penetration`에 승수를 적용했습니다.                                          |
| `damage_multiplier` | `amount`에 승수를 적용했습니다.                                                     |

```json
"melee_damage": [ { "damage_type": "electric", "amount": 4.0, "armor_penetration": 1, "armor_multiplier": 1.2, "damage_multiplier": 1.4 } ],
```

## "melee_dice", "melee_dice_sides"

(정수, 선택사항)

몬스터의 근접 공격 시 굴려지는 주사위와 주사위 면의 수입니다. 이는 다음의 양을 정의합니다.
배쉬 손상.

## "grab_strength"

(정수, 선택사항)

이 몬스터가 적용하는 잡기 효과의 강도입니다. 기본값은 1이며, 다음과 같은 몬스터에게만 유용합니다.
GRAB 특수 공격 및 GRABS 플래그. Grab_strength = n인 몬스터는 마치 잡기를 적용합니다.
n 좀비였습니다. max(Str,Dex)<=n을 가진 플레이어는 그 잡기를 깨뜨릴 가능성이 없습니다.

## "melee_cut"

(정수, 선택사항)

몬스터 근접 공격 시 주사위 굴림에 절단 피해량이 추가됩니다.

## "armor_bash", "armor_cut", "armor_stab", "armor_acid", "armor_fire", "armor_cold", "armor_electric"

(정수, 선택사항)

강타, 절단, 찌르기, 산, 화재, 냉기 및 전기 손상으로부터 몬스터를 보호합니다.

## "vision_day", "vision_night"

(정수, 선택사항)

완전한 일광 및 완전한 어둠 속에서의 시야 범위.

## "luminance"

(정수, 선택사항)

몬스터가 수동적으로 출력하는 빛의 양입니다. 범위는 0부터 10까지입니다.

## "hp"

(정수)

몬스터 히트 포인트.

## "monster_weapon"

(항목 그룹, 선택 사항)

몬스터가 사용하는 무기로 추정되는 아이템을 스폰하는데 사용되는 아이템 그룹입니다. 이것을 정의하면 해당 몬스터가 무장 해제 기술에 취약해집니다. 무장 해제된 경우 몬스터의 강타 피해 굴림이 크게 감소하고 특수 근접 피해(절단, 전기 등)가 0으로 감소하며 특정 특수 공격(특히 `"type": "gun"`의 공격뿐만 아니라 `TAZER`, `SHOCKSTORM`, `FLAMETHROWER` 등과 같은 일부 공격)을 사용할 수 없게 됩니다.

기술에 `take_weapon`가 설정된 경우에도 공격자가 휘두르는 대신 아이템이 땅에 떨어지기 때문에 양손에 두 개의 무기를 휘두르는 것을 암시하는 컬렉션이 될 수 있습니다. 아이템 그룹은 몬스터가 무장 해제되지 않고 죽은 경우 `death_drops`과 마찬가지로 사망 시에도 생성됩니다.

## "death_drops"

(문자열 또는 항목 그룹, 선택 사항)

몬스터가 죽을 때 아이템을 생성하는 데 사용되는 아이템 그룹입니다. 이는 인라인된 항목 그룹일 수 있습니다.
ITEM_SPAWN.md를 참조하세요. 기본 하위 유형은 "distribution"입니다.

## "death_function"

(문자열 배열, 선택 사항)

괴물이 죽었을 때 어떻게 행동하는지. 가능한 기능 목록은 json_flags.md를 참조하세요. 추가하거나
"add:death_function" 및 "remove:death_function"을 통해 모드의 항목을 제거합니다.

## "on_death"

(사전, 선택사항)

사망 시 적용할 동작 모음입니다. "death_function"을 포함할 수 있습니다. 이 필드가
현재 "death_function" 필드는 건너뜁니다.

```json
"on_death": {
  "death_function": [ "THING" ]
}
```

또한 지원합니다

### "spawn_mon"

(문자열, 선택) 죽어가는 몬스터가 있는 위치에 몬스터 한 마리를 생성합니다.

```json
"on_death": {
  "spawn_mon": "mon_thing"
}
```

### "spawn_mon_near"

(사전, 선택사항)

두 가지 값으로 구성됩니다.

- "distance": (정수, 필수) 몬스터가 스폰될 수 있는 사망 위치로부터의 최대 거리
- "ids": (문자열 또는 문자열 배열, 선택 사항) 설정된 거리 내에서 스폰하려는 몬스터의 ID

단일 몬스터를 생성할 수 있음

```json
"on_death": {
  "spawn_mon_near": {
    "distance": 5,
    "ids": "mon_thing"
  }
}
```

아니면 몬스터가 여러마리

```json
"on_death": {
  "spawn_mon_near": {
    "distance": 3,
    "ids": [ "mon_thing", "mon_zombie", "mon_zombie_anklebiter" ]
  }
}
```

## "harvest"

(문자열, 선택사항)

이 몬스터의 사망 기능으로 인해 시체가 남을 경우 어떤 아이템이 생산될지 정의됩니다.
시체를 도살하거나 해부할 때. 아무것도 지정하지 않으면 기본값은 `human`입니다. 수확
항목, 수율 및 질량 비율은 Harvest.json에 정의되어 있습니다.

## "emit_field"

(emit_id 및 time_duration의 객체 배열, 선택 사항) "emit_fields": [ { "emit_id":
"emit_gum_web", "delay": "30 m" } ],

몬스터가 방출하는 필드는 무엇이며 얼마나 자주 방출합니까? 기간은 "1 h", "60 m" 등의 문자열을 사용할 수 있습니다.
"3600 s" 등...

## "regenerates"

(정수, 선택사항)

턴당 재생되는 체력 수입니다.

## "regeneration_modifiers"

(효과, base_mod(float) 및 scale_mod(float)로 구성된 객체 배열, 선택 사항)
"regeneration_modifiers": [ { "effect": "on_fire", "base_mod": -0.3, "scaling_mod": -0.15 } ],

몬스터의 재생에 긍정적/부정적으로 영향을 미치는 효과(있는 경우)는 무엇입니까? 모드 스택
가산적으로(Intensity 2 on_fire는 0.45의 승수를 생성함) 곱셈적으로 적용됩니다.

## "regenerates_in_dark"

(부울, 선택 사항)

조명이 약한 타일에서는 몬스터가 매우 빠르게 재생성됩니다.

## "regen_morale"

(부울, 선택 사항)

최대 HP에 도달하면 도망을 멈추고 분노와 사기가 재생됩니다.

## "special_attacks"

(특수 공격 정의 배열, 선택 사항)

몬스터의 특수 공격. 이는 배열이어야 하며, 각 요소는 객체(새 항목)여야 합니다.
스타일) 또는 배열(이전 스타일)입니다.

이전 스타일 배열에는 공격 ID(목록은 json_flags.md 참조)라는 두 가지 요소가 포함되어야 합니다.
그리고 그 공격의 쿨타임. 예(10턴마다 잡기 공격):

```json
"special_attacks": [ [ "GRAB", 10 ] ]
```

새 스타일 개체에는 최소한 "type" 멤버(문자열) 및 "cooldown" 멤버가 포함되어야 합니다.
(정수). 특정 유형에 필요한 추가 멤버를 포함할 수 있습니다. 가능한 유형은 다음과 같습니다.
아래에 나열되어 있습니다. 예:

```json
"special_attacks": [
    { "type": "leap", "cooldown": 10, "max_range": 4 }
]
```

"special_attacks"에는 이전 스타일 항목과 새 스타일 항목이 혼합되어 포함될 수 있습니다.

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

`"delete"` 필드를 통해 항목을 제거할 수 있습니다. 하지만 특수 공격을 추가하는 것과는 달리(수동
또는 확장을 통해) 구문이 약간 다릅니다. 두 번째 브래킷 세트가 필요하지 않습니다.
쿨타임을 포함하면 안 됩니다.

```json
"delete": { "special_attacks": [ "FUNGUS" ] },
```

## "flags"

(문자열 배열, 선택 사항)

몬스터 플래그. 전체 목록은 json_flags.md를 참조하세요. 다음을 통해 모드에 항목을 추가하거나 제거할 수 있습니다.
"add:flags" 및 "remove:flags".

## "fear_triggers", "anger_triggers", "placate_triggers"

(문자열 배열, 선택 사항)

무엇이 괴물을 두려워하게 만드는가 / 화나게 하는가 / 무엇이 괴물을 진정시키는가? 전체 목록은 json_flags.md를 참조하세요.

## "revert_to_itype"

(문자열, 선택사항)

비어 있지 않고 유효한 아이템 ID가 있는 경우 플레이어가 몬스터를 이 아이템으로 변환할 수 있습니다.
친절해요. 이것은 일반적으로 포탑에 사용되며 포탑 몬스터를 다시 원래 상태로 되돌리는 것과 유사합니다.
집어 들고 다른 곳에 배치할 수 있는 포탑 아이템.

## "starting_ammo"

(객체, 선택사항)

새로 생성된 몬스터가 시작하는 탄약이 포함된 개체입니다. 이것은 몬스터에게 유용합니다.
탄약을 소모하는 특수 공격이 있습니다. 예:

```json
"starting_ammo": { "9mm": 100, "40mm_frag": 100 }
```

## "upgrades"

(부울 또는 객체, 선택 사항)

시간이 지남에 따라 이 몬스터가 업그레이드되는 방식을 제어합니다. 단일 값 `false`일 수 있습니다(이는
기본값이며 업그레이드를 비활성화합니다) 또는 업그레이드를 설명하는 개체입니다.

예:

```json
"upgrades": {
    "into_group": "GROUP_ZOMBIE_UPGRADE",
    "half_life": 28
}
```

업그레이드 객체에는 다음 멤버가 있을 수 있습니다.

### "half_life"

(int) 대략적인 지수에 따라 몬스터의 절반이 업그레이드되는 시간
진행. 기본값은 4일인 진화 배율 인수로 조정됩니다.

### "into_group"

(문자열, 선택) 지정된 그룹에서 업그레이드된 몬스터의 유형을 가져옵니다. 이들의 비용
그룹은 생성 프로세스의 업그레이드를 위한 것입니다(드문 "replace_monster_group" 및
생성 그룹의 "new_monster_group_id" 속성).

### "into"

(문자열, 선택) 업그레이드된 몬스터의 종류입니다.

### "age_grow"

(int, option) 몬스터가 다른 몬스터로 변신하는 데 필요한 일수입니다.

## "reproduction"

(사전, 선택사항) 몬스터의 재생산 주기(있는 경우)입니다. 지원:

### "baby_monster"

(문자열, 선택) 산란하는 몬스터의 번식 시 생성되는 몬스터의 ID입니다.
재생산이 작동하려면 이 항목이나 `baby_egg` 중 하나를 선언해야 합니다.

### "baby_egg"

(문자열, 선택 사항) 알을 낳는 몬스터를 위해 생성할 알 유형의 ID입니다. 다음 중 하나를 선언해야 합니다.
재생산이 작동하려면 이것을 사용하거나 "baby_monster"을 사용하세요.

### "baby_count"

(int) 번식 시 생성될 새로운 생물 또는 알의 수입니다.

### "baby_timer"

(int) 재생산 이벤트 사이의 일수입니다.

## "baby_flags"

(배열, 선택) 해당 몬스터가 번식할 수 있는 계절을 지정합니다. 즉:
`[ "SPRING", "SUMMER" ]`

## "special_when_hit"

(배열, 선택 사항)

몬스터가 공격을 받았을 때 발동하는 특수 방어 공격입니다. 다음과 같은 배열을 포함해야 합니다.
방어 ID(json_flags.md의 몬스터 방어 공격 참조) 및 해당 방어 기회
실제로 트리거됩니다. 예:

```json
"special_when_hit": [ "ZAPBACK", 100 ]
```

## "attack_effs"

(객체 배열, 선택 사항)

몬스터가 성공했을 때 공격받은 생물에게 적용될 수 있는 일련의 효과입니다.
공격. 예:

```json
"attack_effs": [
    {
        "id": "paralyzepoison",
        "duration": 33,
        "chance": 50
    }
]
```

배열의 각 요소는 다음 멤버를 포함하는 객체여야 합니다.

### "id"

(문자열)

적용할 효과의 ID입니다.

### "duration"

(정수, 선택사항)

효과가 지속되는 시간(교대로).

### "affect_hit_bp"

(부울, 선택 사항)

아래에 설정된 부분이 아닌 타격된 신체 부위에 효과를 적용할지 여부입니다.

### "bp"

(문자열, 선택사항)

효과가 적용되는 신체 부위입니다. 기본값은 효과를 전체에 적용하는 것입니다.
몸. 일부 효과에는 특정 신체 부위(예: "hot")가 필요할 수 있고 다른 효과에는 필요할 수 있습니다.
몸 전체(예: "meth").

### "chance"

(정수, 선택사항)

효과가 적용될 확률입니다.

# 모딩

몬스터 타입은 모드에서 재정의하거나 수정할 수 있습니다. 그렇게 하려면 "edit-mode" 멤버를 추가해야 합니다.
다음 중 하나를 포함할 수 있습니다.

- "create"(멤버가 존재하지 않는 경우 기본값), 유형이 다음과 같은 경우 오류가 표시됩니다.
  해당 ID가 이미 존재합니다.
- "override", 주어진 ID를 가진 기존 유형(있는 경우)이 제거되고 새 데이터가
  완전히 새로운 유형으로 로드되었습니다.
- "modify", 기존 유형이 수정됩니다. 주어진 ID를 가진 유형이 없으면 오류가 발생합니다.
  표시됩니다.

필수 속성(선택 사항으로 표시되지 않은 모든 속성)은 편집 모드가 다음인 경우에만 필요합니다.
"create" 또는 "override".

예(좀비 몬스터의 이름을 바꾸고 다른 모든 속성은 그대로 유지):

```json
{
  "type": "MONSTER",
  "edit-mode": "modify",
  "id": "mon_zombie",
  "name": "clown"
}
```

기본 편집 모드("create")는 ID가 유형과 충돌하는 경우 새 유형에 적합합니다.
다른 모드나 핵심 데이터에서는 오류가 표시됩니다. 편집 모드 "override"가 적합합니다.
유형을 처음부터 다시 정의하려면 모든 필수 구성원이 나열되고 아무 것도 남지 않도록 해야 합니다.
이전 정의의 흔적. 편집 모드 "modify"는 플래그 추가 또는
특수 공격을 제거합니다.

유형을 수정하면 속성이 새 값으로 재정의됩니다. 이 예에서는 특수 공격을 설정합니다.
"SHRIEK" 공격만_만_ 억제하려면:

```json
{
  "type": "MONSTER",
  "edit-mode": "modify",
  "id": "mon_zombie",
  "special_attacks": [["SHRIEK", 20]]
}
```

일부 속성에서는 위에 설명된 대로 일반적으로 다음과 같은 멤버를 통해 항목을 추가하고 제거할 수 있습니다.
"add:"/"remove:" 접두사.

## 몬스터 특수 공격 유형

나열된 공격 유형은 몬스터 특수 공격일 수 있습니다("special_attacks" 참조).

### "leap"

몬스터가 타일 몇 개를 뛰어오르게 합니다. 다음과 같은 추가 속성을 지원합니다.

#### "max_range"

(필수) 최대 공격 범위.

#### "min_range"

(필수) 공격에 필요한 최소 범위입니다.

#### "allow_no_target"

몬스터가 빈 공간에서 능력을 사용하는 것을 방지합니다.

#### "move_cost"

특수 공격을 완료하려면 턴이 필요합니다. 100의 이동 비용은 100의 속도로 1초/회전과 같습니다.

#### "min_consider_range", "max_consider_range"

특정 공격을 사용할 때 고려해야 할 최소 범위와 최대 범위입니다.

### "bite"

몬스터가 이빨을 이용해 상대를 물어뜯게 만듭니다. 일부 몬스터는 그렇게 함으로써 감염을 일으킬 수 있습니다.

#### "damage_max_instance"

한 입에 입힐 수 있는 최대 피해량입니다.

#### "min_mul", "max_mul"

공격자를 죽이지 않고 물린 곳에서 벗어나는 것이 얼마나 어려운지.

#### "move_cost"

특수 공격을 완료하려면 턴이 필요합니다. 100의 이동 비용은 100의 속도로 1초/회전과 같습니다.

#### "accuracy"

(정수) 얼마나 정확한지입니다. 하지만 그것을 사용하는 몬스터는 많지 않습니다.

#### "no_infection_chance"

감염되지 않을 확률.

### "gun"

대상에게 총을 발사합니다. 우호적이라면 플레이어에게 해를 끼치는 일을 피할 수 있습니다.

#### "gun_type"

(필수) 공격을 수행하는 데 사용될 총의 유효한 항목 ID입니다.

#### "ammo_type"

(필수) 총에 장전될 탄약의 유효한 항목 ID입니다. 몬스터도 있어야합니다
이 탄약이 포함된 "starting_ammo" 필드입니다. 예를 들어

```
"ammo_type" : "50bmg",
"starting_ammo" : {"50bmg":100}
```

#### "max_ammo"

탄약에 모자를 씌우십시오. 어떤 이유로든 탄약이 이 값을 초과하면 디버그 메시지가 인쇄됩니다.

#### "fake_str"

공격을 실행할 가짜 NPC의 강도 통계입니다. 지정하지 않은 경우 8입니다.

#### "fake_dex"

공격을 실행할 가짜 NPC의 민첩성 통계입니다. 지정하지 않은 경우 8입니다.

#### "fake_int"

공격을 실행할 가짜 NPC의 지능 통계입니다. 지정하지 않은 경우 8입니다.

#### "fake_per"

공격을 실행할 가짜 NPC의 인식 통계입니다. 지정하지 않은 경우 8입니다.

#### "fake_skills"

스킬 ID와 스킬 레벨 쌍으로 구성된 2개 요소 배열의 배열입니다.

#### "move_cost"

공격을 실행하는 데 드는 이동 비용

// true인 경우 플레이어에게 "grace period"를 제공합니다.

#### "require_targeting_player"

true인 경우 몬스터는 플레이어를 "target"해야 하며 `targeting_cost` 이동을 낭비하고
공격 대상이 되는 대상을 공격하지 않는 한 재사용 대기시간이 있는 공격 및 경고 소리 생성
최근.

#### "require_targeting_npc"

위와 같지만 npcs가 있습니다.

#### "require_targeting_monster"

위와 같지만 몬스터가 있습니다.

#### "targeting_timeout"

이 턴만큼 타겟팅 상태가 적용됩니다. 타겟팅은 터렛에 적용되지만
목표.

#### "targeting_timeout_extend"

공격에 성공하면 이 턴 동안 타겟팅이 확장됩니다. 부정적일 수 있습니다.

#### "targeting_cost"

플레이어를 타겟팅하는 데 드는 비용을 이동합니다. 플레이어를 공격하고 플레이어를 타겟팅하지 않은 경우에만 적용됩니다.
최근 5턴 이내.

#### "no_crits"

true인 경우 공격 생성은 대상에 대한 원거리 치명타를 기록할 수 없으며 좋은 히트도 기록할 수 없습니다.
더 이상 머리로 가는 높은 변화가 없으며 대신 몸에 타격을 가하는 높은 변화가 있습니다.
그리고 팔다리. 거짓이면 기본 동작이 사용됩니다. 치명타는 허용되고 좋은 적중은 높습니다.
머리에 닿을 기회.

#### "laser_lock"

True이고 레이저로 고정되어 있지 않지만 목표로 삼아야 하는 생물을 공격하는 경우, 괴물은
타겟팅 상태가 없는 것처럼 행동하면(및 타겟팅 시간 낭비) 타겟은 다음과 같이 됩니다.
레이저로 잠겨 있으며 대상이 플레이어인 경우 경고가 발생합니다.

레이저 잠금은 대상에게 영향을 주지만 특정 공격자에게만 국한되지는 않습니다.

#### "range"

목표물을 획득할 수 있는 최대 범위.

#### "range_no_burst"

대상이 버스트 공격을 받을 수 있는 최대 범위(해당하는 경우).

#### "burst_limit"

버스트 크기를 제한합니다.

#### "description"

플레이어가 볼 경우 실행되는 공격에 대한 설명입니다.

#### "targeting_sound"

타겟팅 시 발생하는 소리에 대한 설명입니다.

#### "targeting_volume"

OBSOLETE

타겟팅할 때 나는 소리의 볼륨입니다. (볼륨 타일에서)

#### "targeting_volume_dB"

dB 단위로 타겟팅할 때 생성되는 소리의 볼륨

#### "no_ammo_sound"

탄약이 떨어졌을 때 나는 소리에 대한 설명입니다.

### "deployer"

```json
{
  "type": "deployer",
  "deployables": {
    "bot_c4_hack": { // 배포할 탄약의 항목 ID
      "message": "The elite zombie grenadier deploys a c4 hack", // 배포 시 표시할 메시지
      "chance": 1, // 사용 가능한 배치물의 총 무게를 기준으로 무작위로 선택된 스폰 무게
      "ammo_percentage": 0.6, // 무작위 선택을 위해 이 배포 가능 항목을 풀에 추가할 시기에 대한 임계값입니다. 총 배포 가능 항목의 40%가 이미 사용되기 전까지는 배포를 시작하지 않습니다.
      "range": 10 // 배치 시 스폰 반경
    }
  },
  "cooldown": 3 // 배포 간 전환
}
```

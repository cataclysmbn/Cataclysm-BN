# 주문, 인챈트 및 기타 커스텀 효과

# 주문 (Spells)

`data/json/debug_spells.json`에 템플릿 주문이 있으며, 여기에 복사되어 있습니다:

```json
{
  // This spell exists in json as a template for contributors to see the possible values of the spell
  "id": "example_template", // id of the spell, used internally. not translated
  "type": "SPELL",
  "name": "Template Spell", // name of the spell that shows in game
  "description": "This is a template to show off all the available values",
  "blocker_mutations": ["THRESH_RAT"], // list of mutations that will not allow you to cast the spell
  "valid_targets": ["hostile", "ground", "self", "ally"], // if a valid target is not included, you cannot cast the spell on that target.
  "effect": "shallow_pit", // effects are coded in C++. A list will be provided below of possible effects that have been coded.
  "effect_str": "template", // special. see below
  "extra_effects": [{ "id": "fireball", "hit_self": false, "max_level": 3 }], // this allows you to cast multiple spells with only one spell
  "melee_dam": ["cut", "elemental"], // the melee damage of these listed types will be added to spell damage. Supports standard damage type strings and also the categories of 'physical' and 'elemental' for convenience
  "affected_body_parts": [
    "HEAD",
    "TORSO",
    "MOUTH",
    "EYES",
    "ARM_L",
    "ARM_R",
    "HAND_R",
    "HAND_L",
    "LEG_L",
    "FOOT_L",
    "FOOT_R"
  ], // body parts affected by effects
  "spell_class": "NONE", //
  "scale_str": true, // The spell will scale off of strength. Also valid: scale_dex, scale_per, scale_int
  "base_casting_time": 100, // this is the casting time (in moves)
  "base_energy_cost": 10, // the amount of energy (of the requisite type) to cast the spell
  "energy_source": "MANA", // the type of energy used to cast the spell. types are: MANA, BIONIC, HP, STAMINA, FATIGUE, NONE (none will not use mana)
  "components": [requirement_id],                            // an id from a requirement, like the ones you use for crafting. spell components require to cast.
  "difficulty": 12, // the difficulty to learn/cast the spell
  "max_level": 10, // maximum level you can achieve in the spell
  "min_damage": 0, // minimum damage (or "starting" damage)
  "max_damage": 100, // maximum damage the spell can achieve
  "damage_increment": 2.5, // to get damage (and any of the other below stats) multiply this by spell's level and add to minimum damage
  "min_aoe": 0, // area of effect (currently not implemented)
  "max_aoe": 5,
  "aoe_increment": 0.1,
  "min_range": 1, // range of the spell
  "max_range": 10,
  "range_increment": 2,
  "min_accuracy": 50, // percentage "accuracy" of the spell. used for determining which body part was hit
  "max_accuracy": 70,
  "accuracy_increment": 2,
  "min_dot": 0, // damage over time (currently not implemented)
  "max_dot": 2,
  "dot_increment": 0.1,
  "min_duration": 0, // duration of spell effect (if the spell has a special effect)
  "max_duration": 1000,
  "duration_increment": 4,
  "min_pierce": 0, // how much of the spell pierces armor (currently not implemented)
  "max_pierce": 1,
  "pierce_increment": 0.1,
  "field_id": "fd_blood", // the string id of the field (currently hardcoded)
  "field_chance": 100, // one_in( field_chance ) chance of spawning a field per tile in aoe
  "min_field_intensity": 10, // field intensity of fields generated
  "max_field_intensity": 10,
  "field_intensity_increment": 1,
  "field_intensity_variance": 0.1, // the field can range in intensity from -variance as a percent to +variance as a percent i.e. this spell would be 9-11
  "sound_type": "combat", // the type of sound. possible types are: background, weather, music, movement, speech, activity, destructive_activity, alarm, combat, alert, order
  "sound_description": "a whoosh", // the sound description. in the form of "You hear %s" by default it is "an explosion"
  "sound_ambient": true, // whether or not this is treated as an ambient sound or not
  "sound_id": "misc", // the sound id
  "sound_variant": "shockwave", // the sound variant
  "sprite": "fd_electricity", // This changes the spell sprite to any valid field ID. Do not use with NO_EXPLOSION_VFX
  "learn_spells": { "acid_resistance_greater": 15 } // You learn the specified spell once your level in this spell is greater than or equal to the number shown.
}
```

위의 대부분의 기본값은 0 또는 "NONE"이므로, 주문과 관련이 없는 경우 대부분의 값을 생략할 수 있습니다.

이들 중 일부의 값을 결정할 때, 일부 공식은 선형이 아니라는 점에 유의하는 것이 중요합니다. 예를 들어, 주문 실패 확률에 대한 공식은 다음과 같습니다:

`( ( ( ( spell_level - spell_difficulty ) * 2 + intelligence + spellcraft_skill ) - 30 ) / 30 ) ^ 2`

즉, 난이도 0인 주문을 지능 8, 주문술 0, 주문 레벨 0인 플레이어가 시전하면 53%의 주문 실패 확률을 갖게 됩니다. 반면에, 지능 12, 주문술 6, 동일한 주문의 레벨 6인 플레이어는 0%의 주문 실패 확률을 갖게 됩니다.

그러나 경험치 획득은 계산하기가 조금 더 복잡합니다. 레벨에 도달하는 데 필요한 경험치에 대한 공식은 다음과 같습니다:

`e ^ ( ( level + 62.5 ) * 0.146661 ) ) - 6200`

#### 현재 구현된 주문 플래그

- `PERMANENT` - 이 주문으로 생성된 아이템이나 생물은 사라지지 않고 정상적으로 죽지 않습니다

- `IGNORE_WALLS` - 주문의 범위 효과가 벽을 통과합니다

- `SWAP_POS` - 원거리 주문에 사용될 때 시전자를 대상 위치로 순간이동시키며, 방해가 되는 생물과 위치를 바꿉니다

- `HOSTILE_SUMMON` - 소환 주문은 항상 적대적인 몬스터를 생성합니다

- `HOSTILE_50` - 소환된 몬스터가 50%의 확률로 우호적으로 생성됩니다

- `SILENT` - 주문이 대상에서 소음을 내지 않습니다

- `NO_EXPLOSION_VFX` - 주문에 시각적 폭발이 없습니다

- `LOUD` - 주문이 대상에서 추가 소음을 냅니다

- `VERBAL` - 주문이 시전자 위치에서 소음을 내며, 입 방해가 실패율에 영향을 줍니다

- `SOMATIC` - 팔 방해가 실패율과 시전 시간에 (약간) 영향을 줍니다

- `NO_HANDS` - 손이 주문 에너지 비용에 영향을 주지 않습니다

- `UNSAFE_TELEPORT` - 순간이동 주문이 시전자나 다른 사람을 죽일 위험이 있습니다

- `NO_LEGS` - 다리가 시전 시간에 영향을 주지 않습니다

- `CONCENTRATE` - 집중력이 주문 실패율에 영향을 줍니다

- `RANDOM_AOE` - 일반 동작 대신 min+increment\*level과 max 사이의 무작위 숫자를 선택합니다

- `RANDOM_DAMAGE` - 일반 동작 대신 min+increment\*level과 max 사이의 무작위 숫자를 선택합니다

- `DIVIDE_DAMAGE` - 주문의 피해를 맞은 모든 생물에게 균등하게 나눕니다

- `RANDOM_DURATION` - 일반 동작 대신 min+increment\*level과 max 사이의 무작위 숫자를 선택합니다

- `RANDOM_TARGET` - 일반 동작 대신 범위 내의 무작위 유효 대상을 선택합니다.

- `MUTATE_TRAIT` - 변이 spell_effect를 재정의하여 카테고리 대신 특정 trait_id를 사용합니다

- `WONDER` - 각 extra_spells를 시전하는 대신, 그 중 N개를 선택하여 시전합니다(여기서 N은 std::min( damage(), number_of_spells )입니다)

- `PAIN_NORESIST` - 고통을 변경하는 주문은 저항될 수 없습니다(둔화된 특성과 같이)

- `BRAWL` - Brawler 특성을 가진 캐릭터가 주문을 시전할 수 있도록 허용합니다(그렇지 않으면 불가능)

- `DUPE_SOUND` - 주문이 같은 소리를 여러 번 재생할 수 있도록 허용합니다(즉, 영향을 받은 각 대상에 대한 소리)

- `ADD_MELEE_DAM` - 현재 장착한 아이템의 최고 근접 피해 카테고리를 주문의 피해에 추가합니다(레거시, Bash Stab 및 Cut만 처리합니다. 대신 `melee_dam` 필드를 사용하세요)

- `PHYSICAL` - BRAWL을 의미하며 또한 Brawler가 주문을 사용할 때 보너스를 받는다는 것을 의미합니다. 주로 "Physical Techniques"용입니다

- `MOD_MELEE_MOVES` - 무기의 근접 이동 비용이 "주문"의 시전 비용에 추가됩니다. 특수 동작: 이 플래그와 함께 base_casting_time이 음수인 경우, "increment"는 대신 근접 비용에 대한 배수입니다(주문 형태로 빠른 공격을 복제하는 데 유용합니다)

- `MOD_MEELE_STAM` - `MOD_MELEE_MOVES`와 유사하지만 주문의 시전 비용과 무기의 스태미나 비용에 적용됩니다. 주로 스태미나 기술용입니다. 음수 기본 비용과 increment 필드와 관련된 동일한 특수 동작을 가집니다.

- `NO_FAIL` - 이 주문은 시전할 때 실패할 수 없습니다

#### 현재 구현된 효과 및 특수 규칙

- `pain_split` - 모든 사지의 피해를 균등하게 만듭니다.

- `move_earth` - 대상 위치를 "파냅니다". 일부 지형은 이 방식으로 파낼 수 없습니다.

- `target_attack` - 대상에게 피해를 입힙니다(벽을 무시합니다). 음수 피해는 대상을 치유합니다. "effect_str"이 포함된 경우, 가능하면 affected_body_parts에 정의된 신체 부위에 해당 효과(json의 다른 곳에 정의됨)를 대상에 추가합니다. 모든 범위 효과는 대상을 중심으로 한 원형 영역으로 나타나며, valid_targets에만 피해를 입힙니다. (범위 효과는 벽을 무시하지 않습니다)

- `projectile_attack` - target_attack과 유사하지만, 발사한 투사체가 통과할 수 없는 지형에서 멈춥니다. "effect_str"이 포함된 경우, 가능하면 affected_body_parts에 정의된 신체 부위에 해당 효과(json의 다른 곳에 정의됨)를 대상에 추가합니다.

- `cone_attack` - 대상을 향해 범위까지 원뿔을 발사합니다. 원뿔의 호는 도 단위로 범위 효과입니다. 벽에서 멈춥니다. "effect_str"이 포함된 경우, 가능하면 affected_body_parts에 정의된 신체 부위에 해당 효과(json의 다른 곳에 정의됨)를 대상에 추가합니다.

- `line_attack` - 대상을 향해 너비가 범위 효과인 선을 발사하며, 도중에 벽에 의해 차단됩니다. "effect_str"이 포함된 경우, 가능하면 affected_body_parts에 정의된 신체 부위에 해당 효과(json의 다른 곳에 정의됨)를 대상에 추가합니다.

- `spawn_item` - 지속 시간이 끝나면 사라질 아이템을 생성합니다. 기본 지속 시간은 0입니다. 피해가 수량을 결정합니다.

- `summon_vehicle` - 지속 시간 후 사라져야 하는 지정된 차량을 생성합니다

- `summon` - 지속 시간이 끝나면 사라질 몬스터를 생성합니다. 기본적으로 플레이어에게 우호적입니다. 피해가 수량을 결정합니다.

- `translocate` - Magical Nights의 게이트와 같이 등록된 'translocators' 사이에서 플레이어를 순간이동시킵니다

- `teleport_random` - 플레이어를 범위 공간만큼 무작위로 순간이동시키며 범위 효과 변화가 있습니다

- `recover_energy` - 주문의 피해와 같은 에너지 소스(effect_str에 정의됨, 아래 참조)를 회복합니다

* "MANA"
* "STAMINA"
* "FATIGUE"
* "PAIN"
* "BIONIC"

- `ter_transform` - 대상을 중심으로 한 영역의 지형과 가구를 변환합니다. 범위 효과 영역의 점 중 하나가 변경될 확률은 one_in( damage )입니다. effect_str은 ter_furn_transform의 id입니다.

- `vomit` - 범위 효과 내의 모든 생물이 가능하면 즉시 구토합니다.

- `timed_event` - 플레이어에게만 시간 제한 이벤트를 추가합니다. 유효한 시간 제한 이벤트: "help", "wanted", "robot_attack", "spawn_wyrms", "amigara", "roots_die", "temple_open", "temple_flood", "temple_spawn", "dim", "artifact_light" 참고: 이것은 아티팩트 활성 효과에만 추가되었습니다. 지원이 제한되어 있으며, 자신의 책임 하에 사용하세요.

- `explosion` - 폭발이 대상을 중심으로 하며, 위력은 damage()이고 계수는 aoe()/10입니다

- `flashbang` - 섬광탄 효과가 대상을 중심으로 하며, 위력은 damage()이고 계수는 aoe()/10입니다

- `mod_moves` - 대상에 damage() 이동을 추가합니다. 음수일 수 있어 해당 시간 동안 대상을 "동결"할 수 있습니다

- `map` - 플레이어를 중심으로 aoe()의 반경까지 오버맵을 매핑합니다

- `morale` - 범위 효과 내의 모든 npc 또는 아바타에게 값이 damage()인 사기 효과를 부여합니다. decay_start는 duration() / 10입니다.

- `charm_monster` - damage()보다 hp가 적은 몬스터를 대략 duration() 동안 매혹시킵니다

- `mutate` - 대상을 변이시킵니다. effect_str이 정의된 경우, 무작위로 선택하는 대신 해당 카테고리로 변이합니다. "MUTATE_TRAIT" 플래그는 effect_str이 카테고리 대신 특정 특성이 되도록 허용합니다. damage() / 100은 변이가 성공할 백분율 확률입니다(값 10000은 100.00%를 나타냅니다)

- `bash` - 대상의 지형을 때립니다. damage()를 타격의 강도로 사용합니다.

- `dash` - 플레이어를 대상 타일로 이동시키며, 필드를 남길 수 있습니다.

- `area_push` - 단일 지점에서 바깥쪽으로 밀어냅니다

- `directed_push` 당신으로부터 멀어지는 단일 방향으로 밀어냅니다.

- `noise` 주문의 피해와 같은 소음 크기로 소음을 냅니다.

- `WONDER` - 위와 달리, 이것은 "효과"가 아니라 "플래그"입니다. 이것은 부모 주문의 동작을 크게 변경합니다: 주문 자체는 시전되지 않지만, 피해와 범위 정보는 extra_effects를 시전하는 데 사용됩니다. N개의 extra_effects가 무작위로 선택되어 시전되며, 여기서 N은 주문의 현재 피해입니다(RANDOM_DAMAGE 플래그와 스택됩니다) 그리고 이 주문에 의해 시전된 주문의 메시지도 표시됩니다. 이 주문의 메시지가 표시되지 않기를 원하면 메시지가 빈 문자열인지 확인하세요.

- `RANDOM_TARGET` - (wonder와 같은) 특수 주문 플래그로, 시전자가 대상을 선택하는 대신 주문이 범위 내의 무작위 유효 대상을 선택하도록 강제합니다. 이것은 extra_effects에도 영향을 줍니다.

##### 공격 타입을 가진 주문의 경우, 사용 가능한 피해 타입(대소문자 구분 안 함):

- `fire`
- `acid`
- `bash`
- `bullet`
- `bio` - 독과 같은 내부 피해
- `cold`
- `cut`
- `electric`
- `light` - 실제 빛과 '신성함' 모두에 사용됨
- `dark`
- `psi` - 정신
- `stab`
- `true` - 이 피해 타입은 방어구를 완전히 통과하므로 매우 강력합니다. 지정되지 않은 경우 기본 피해 타입입니다.

#### 레벨업하는 주문

레벨업할 때 효과가 변경되는 주문은 최소 및 최대 효과와 증분이 있어야 합니다. 최소 효과는 주문이 레벨 0에서 수행할 것이고, 최대 효과는 성장을 멈추는 곳입니다. 증분은 레벨당 얼마나 변경되는지입니다. 예:

```json
"min_range": 1,
"max_range": 25,
"range_increment": 5,
```

최소값과 최대값은 항상 같은 부호를 가져야 하지만, 음수일 수 있습니다. 예를 들어 고통이나 스태미나 피해를 일으키기 위해 음수 'recover' 효과를 사용하는 주문의 경우입니다. 예:

```json
{
  "id": "stamina_damage",
  "type": "SPELL",
  "name": "Tired",
  "description": "decreases stamina",
  "valid_targets": ["hostile"],
  "min_damage": -2000,
  "max_damage": -10000,
  "damage_increment": -3000,
  "max_level": 10,
  "effect": "recover_energy",
  "effect_str": "STAMINA"
}
```

### 주문 학습

구현된 주문 부여 방법은 세 가지입니다: 변이는 "spells_learned" 필드로 주문을 부여할 수 있으며, 부여된 레벨도 지정할 수 있습니다. 특정 주문은 "learn_spells" 변수를 통해 적절한 레벨에 도달하면 주문을 가르칠 수도 있습니다. 마지막으로, use_action을 통해 아이템에서 주문을 배울 수 있으며, 이것은 주문을 사용하는 것 외에 주문을 훈련하는 유일한 방법이기도 합니다. 세 가지 모두의 예는 아래에 나와 있습니다:

```json
{
  "id": "DEBUG_spellbook",
  "type": "GENERIC",
  "name": "A Technomancer's Guide to Debugging C:BN",
  "description": "static std::string description( spell sp ) const;",
  "weight": 1,
  "volume": "1 ml",
  "symbol": "?",
  "color": "magenta",
  "use_action": {
    "type": "learn_spell",
    "spells": [ "debug_hp", "debug_stamina", "example_template", "debug_bionic", "pain_split", "fireball" ] // this is a list of spells you can learn from the item
  }
},
```

아래 예의 경우 Acid Resistance가 레벨 15에 도달하면 Greater Acid Resistance 주문을 배웁니다

```json
{
    "id": "acid_resistance",
    "type": "SPELL",
    "name": { "str": "Acid Resistance" },
    "description": "Protects the user from acid.",
    ...
    "learn_spells": { "acid_resistance_greater": 15 }
}
```

이 주문서를 지능, 주문술, 집중력에 따라 턴당 ~1 경험치의 비율로 공부할 수 있습니다.

```json
"spells_learned": [ [ "debug_hp", 1 ], [ "debug_stamina", 1 ], [ "example_template", 1 ], [ "pain_split", 1 ] ],
```

#### 직업 및 NPC 클래스의 주문

다음과 같이 직업이나 NPC 클래스 정의에 "spell" 멤버를 추가할 수 있습니다:

```json
"spells": [ { "id": "summon_zombie", "level": 0 }, { "id": "magic_missile", "level": 10 } ]
```

참고: 이것은 클래스와 충돌하는 주문을 배울 수 있게 합니다. 또한 클래스를 획득하는 프롬프트를 제공하지 않습니다. 직업에 이것을 추가할 때 신중하세요!

#### 몬스터의 주문

몬스터의 특수 공격으로 주문을 할당할 수 있습니다.

```json
{ "type": "spell", "spell_id": "burning_hands", "spell_level": 10, "cooldown": 10 }
```

- spell_id: 시전되는 주문의 id.
- spell_level: 주문이 시전되는 레벨. 몬스터가 시전하는 주문은 플레이어 주문처럼 레벨을 얻지 않습니다.
- cooldown: 몬스터가 이 주문을 얼마나 자주 시전할 수 있는지

# 인챈트 (Enchantments)

인챈트는 아이템, 생체공학 또는 변이에 의해 제공되는 커스텀 효과를 지정할 수 있게 합니다.

## 필드

### id

(string) 이 인챈트의 고유 식별자.

### has

(string) 인챈트가 활성화되기 위한 올바른 위치에 있는지 여부를 결정하는 방법.

이 필드는 아이템에만 관련됩니다.

값:

- `HELD` (기본값) - 인벤토리에 있을 때
- `WIELD` - 손에 장착했을 때
- `WORN` - 방어구로 착용했을 때

### condition

(string) 인챈트가 활성화되기 위한 올바른 환경에 있는지 여부를 결정하는 방법.

값:

- `ALWAYS` (기본값) - 항상 활성
- `UNDERGROUND` - 아이템 소유자가 Z-레벨 0 아래에 있을 때
- `ABOVEGROUND` - 아이템 소유자가 Z-레벨 0 이상에 있을 때
- `UNDERWATER` - 소유자가 수영 가능한 지형에 있을 때
- `NIGHT` - 밤일 때
- `DUSK` - 황혼일 때
- `DAY` - 낮일 때
- `DAWN` - 새벽일 때
- `ACTIVE` - 인챈트가 부착된 아이템, 변이, 생체공학 등이 활성화되어 있을 때.
- `INACTIVE` - `ACTIVE`의 반대

### emitter

(string) 이 인챈트가 활성화되어 있는 한 활성화되는 이미터의 식별자. 기본값: 이미터 없음.

### ench_effects

(array) 이 인챈트가 활성화되어 있는 한 지정된 강도의 효과를 부여합니다.

단일 항목에 대한 구문:

```json
{
  // (필수) 효과의 식별자
  "effect": "effect_identifier",

  // (필수) 강도. 실제로 강도가 없는 효과의 경우 1로 설정하면 됩니다.
  "intensity": 2
}
```

### hit_you_effect

(array) 인챈트가 활성화되어 있고 캐릭터가 생물을 근접 공격할 때 시전될 수 있는 주문 목록.

단일 항목에 대한 구문:

```json
{
  // (필수) 주문의 식별자
  "id": "spell_identifier",

  // true인 경우, 주문은 캐릭터의 위치를 중심으로 합니다.
  // false인 경우, 주문은 공격하는 생물을 중심으로 합니다.
  // 기본값: false
  "hit_self": false,

  // 트리거 확률, X 중 1.
  // 기본값: 1
  "once_in": 1,

  // 당신을 위해 주문이 트리거될 때의 메시지.
  // %1$s는 당신의 이름, %2$s는 생물의 이름
  // 기본값: 메시지 없음
  "message": "You pierce %2$s with Magic Piercing!",

  // NPC를 위해 주문이 트리거될 때의 메시지.
  // %1$s는 그들의 이름, %2$s는 생물의 이름
  // 기본값: 메시지 없음
  "npc_message": "%1$s pierces %2$s with Magic Piercing!",

  // TODO: 깨졌나요?
  "min_level": 1,

  // TODO: 깨졌나요?
  "max_level": 2
}
```

### hit_me_effect

(array) 인챈트가 활성화되어 있고 캐릭터가 생물에게 근접 공격당할 때 시전될 수 있는 주문 목록.

`hit_you_effect`와 동일한 구문.

### mutations

(array) 인챈트가 활성화되어 있는 동안 일시적으로 부여되는 변이 목록.

### intermittent_activation

(object) 인챈트가 활성화되어 있는 동안 발생하는 무작위 효과를 지정하는 규칙.

구문:

```json
{
  // 인챈트가 활성화되어 있는 동안 매 턴마다 실행할 검사 목록.
  "effects": [
    {
      // 평균 활성화 빈도.
      // 통과할 정확한 확률은 턴당 "X를 턴으로 변환한 것 중 1" 입니다.
      "frequency": "5 minutes",

      // 검사가 통과하면 시전할 주문 목록.
      "spell_effects": [
        {
          // (필수) 주문의 식별자
          "id": "nasty_random_effect",

          // TODO: 깨졌나요?
          "min_level": 1,

          // TODO: 깨졌나요?
          "max_level": 5
          // TODO: 다른 필드들이 로드되는 것으로 보이지만 사용되지 않음
        }
      ]
    }
  ]
}
```

### values

(array) 수정할 기타 캐릭터/아이템 값 목록.

단일 항목에 대한 구문:

```json
{
  // (필수) 수정할 값 ID, 아래 목록 참조.
  "value": "VALUE_ID_STRING",

  // 가산 보너스. 선택적 정수, 기본값은 0.
  // 일부 값에서는 무시될 수 있습니다.
  "add": 13,

  // 곱셈 보너스. 선택적, 기본값은 0.
  "multiply": -0.3
}
```

가산 보너스는 곱셈과 별도로 적용되며, 다음과 같습니다:

```json
bonus = add + base_value * multiply
```

따라서, `multiply` 값 -0.8은 -80%이고, `multiply` 2.5는 +250%입니다. 정수 값을 수정할 때, 최종 보너스는 0으로 반올림됩니다(잘림).

여러 인챈트(예: 아이템에서 하나, 생체공학에서 하나)가 동일한 값을 수정하는 경우, 보너스는 반올림 없이 함께 더해지며, 그런 다음 합계가 기본 값에 적용되기 전에 반올림됩니다(필요한 경우).

캐릭터가 한 번에 가질 수 있는 인챈트 수에 제한이 없으므로, 최종 계산된 값에는 의도하지 않은 동작을 방지하기 위해 하드코딩된 경계가 있습니다.

#### 수정 가능한 값의 ID

#### 캐릭터 값

##### STRENGTH

힘 스탯. 여기서 `base_value`는 기본 스탯 값입니다. 최종 값은 0 아래로 내려갈 수 없습니다.

##### DEXTERITY

민첩성 스탯. 여기서 `base_value`는 기본 스탯 값입니다. 최종 값은 0 아래로 내려갈 수 없습니다.

##### PERCEPTION

지각 스탯. 여기서 `base_value`는 기본 스탯 값입니다. 최종 값은 0 아래로 내려갈 수 없습니다.

##### INTELLIGENCE

지능 스탯. 여기서 `base_value`는 기본 스탯 값입니다. 최종 값은 0 아래로 내려갈 수 없습니다.

##### SPEED

캐릭터 속도. 여기서 `base_value`는 고통/배고픔/무게 페널티를 포함한 캐릭터 속도입니다. 최종 속도 값은 기본 속도의 25% 아래로 내려갈 수 없습니다.

##### ATTACK_COST

근접 공격 비용. 낮을수록 좋습니다. 여기서 `base_value`는 스탯 및 기술의 수정자를 포함한 주어진 무기의 공격 비용입니다. 최종 값은 25 아래로 내려갈 수 없습니다.

##### MOVE_COST

이동 비용. 여기서 `base_value`는 옷과 특성의 수정자를 포함한 타일 이동 비용입니다. 최종 값은 20 아래로 내려갈 수 없습니다.

##### METABOLISM

신진대사율. 이 수정자는 `add` 필드를 무시합니다. 여기서 `base_value`는 특성에 의해 수정된 `PLAYER_HUNGER_RATE`입니다. 최종 값은 0 아래로 내려갈 수 없습니다.

##### MANA_CAP

마나 용량. 여기서 `base_value`는 특성에 의해 수정된 캐릭터의 기본 마나 용량입니다. 최종 값은 0 아래로 내려갈 수 없습니다.

##### MANA_REGEN

마나 재생 속도. 이 수정자는 `add` 필드를 무시합니다. 여기서 `base_value`는 특성에 의해 수정된 캐릭터의 기본 마나 획득 속도입니다. 최종 값은 0 아래로 내려갈 수 없습니다.

##### STAMINA_CAP

스태미나 용량. 이 수정자는 `add` 필드를 무시합니다. 여기서 `base_value`는 특성에 의해 수정된 캐릭터의 기본 스태미나 용량입니다. 최종 값은 `PLAYER_MAX_STAMINA`의 10% 아래로 내려갈 수 없습니다.

##### STAMINA_REGEN

스태미나 재생 속도. 이 수정자는 `add` 필드를 무시합니다. 여기서 `base_value`는 입 방해에 의해 수정된 캐릭터의 기본 스태미나 획득 속도입니다. 최종 값은 0 아래로 내려갈 수 없습니다.

##### THIRST

갈증 획득 속도. 이 수정자는 `add` 필드를 무시합니다. 여기서 `base_value`는 캐릭터의 기본 갈증 획득 속도입니다. 최종 값은 0 아래로 내려갈 수 없습니다.

##### FATIGUE

피로 획득 속도. 이 수정자는 `add` 필드를 무시합니다. 여기서 `base_value`는 캐릭터의 기본 피로 획득 속도입니다. 최종 값은 0 아래로 내려갈 수 없습니다.

##### BONUS_DODGE

회피 페널티가 시작되기 전 턴당 추가 회피 수. 여기서 `base_value`는 페널티 전 캐릭터의 기본 턴당 회피 수입니다(보통 1). 최종 값은 0 아래로 내려갈 수 있으며, 이는 회피 굴림에 페널티를 초래합니다.

##### ARMOR_X

들어오는 피해 수정자. Active Defense System 생체공학 이후에 적용되지만 피해가 아이템에 의해 흡수되기 전입니다. 여기서 `base_value`는 해당 타입의 들어오는 피해 값이므로, 양수 `add`와 1보다 큰 `mul`은 캐릭터가 받는 피해를 **증가**시킵니다. 각 피해 타입에는 자체 인챈트 값이 있습니다:

- `ARMOR_ACID`
- `ARMOR_BASH`
- `ARMOR_BIO`
- `ARMOR_BULLET`
- `ARMOR_COLD`
- `ARMOR_CUT`
- `ARMOR_LIGHT`
- `ARMOR_DARK`
- `ARMOR_PSI`
- `ARMOR_ELEC`
- `ARMOR_HEAT`
- `ARMOR_STAB`

#### 아이템 값

##### ITEM_ATTACK_COST

이 아이템의 공격 비용(근접 또는 던지기). 조건 / 위치를 무시하며 항상 활성입니다. 여기서 `base_value`는 기본 아이템 공격 비용입니다. 최종 값은 0 아래로 내려갈 수 없습니다.

##### ITEM_DAMAGE_X

이 아이템의 근접 피해. 조건 / 위치를 무시하며 항상 활성입니다. 여기서 `base_value`는 해당 타입의 기본 아이템 피해입니다. 최종 값은 0 아래로 내려갈 수 없습니다. 지원되는 피해 타입:

- `ITEM_DAMAGE_BASH`
- `ITEM_DAMAGE_CUT`
- `ITEM_DAMAGE_STAB`
- `ITEM_DAMAGE_BULLET`
- `ITEM_DAMAGE_ACID`
- `ITEM_DAMAGE_BIO`
- `ITEM_DAMAGE_COLD`
- `ITEM_DAMAGE_DARK`
- `ITEM_DAMAGE_ELECTRIC`
- `ITEM_DAMAGE_FIRE`
- `ITEM_DAMAGE_LIGHT`
- `ITEM_DAMAGE_PSI`
- `ITEM_DAMAGE_TRUE`

##### ITEM_ARMOR_X

이 아이템의 들어오는 피해 수정자, 피해가 아이템에 의해 흡수되기 전에 적용됩니다. 여기서 `base_value`는 해당 타입의 들어오는 피해 값이므로, 양수 `add`와 1보다 큰 `mul`은 캐릭터가 받는 피해를 **증가**시킵니다. 각 피해 타입에는 자체 인챈트 값이 있습니다:

- `ITEM_ARMOR_ACID`
- `ITEM_ARMOR_BASH`
- `ITEM_ARMOR_BIO`
- `ITEM_ARMOR_BULLET`
- `ITEM_ARMOR_COLD`
- `ITEM_ARMOR_CUT`
- `ITEM_ARMOR_LIGHT`
- `ITEM_ARMOR_DARK`
- `ITEM_ARMOR_PSI`
- `ITEM_ARMOR_ELEC`
- `ITEM_ARMOR_HEAT`
- `ITEM_ARMOR_STAB`

## 예제

```json
[
  {
    "//": "On-hit effect for ink glands mutation, implemented via enchantment.",
    "type": "enchantment",
    "id": "MEP_INK_GLAND_SPRAY",
    "hit_me_effect": [
      {
        "id": "generic_blinding_spray_1",
        "hit_self": false,
        "once_in": 15,
        "message": "Your ink glands spray some ink into %2$s's eyes.",
        "npc_message": "%1$s's ink glands spay some ink into %2$s's eyes."
      }
    ]
  },
  {
    "//": "This one would look good on a katana for an anime mod.",
    "type": "enchantment",
    "id": "ENCH_ULTIMATE_ASSKICK",
    "has": "WIELD",
    "condition": "ALWAYS",
    "ench_effects": [{ "effect": "invisibility", "intensity": 1 }],
    "hit_you_effect": [{ "id": "AEA_FIREBALL" }],
    "hit_me_effect": [{ "id": "AEA_HEAL" }],
    "mutations": ["KILLER", "PARKOUR"],
    "values": [{ "value": "STRENGTH", "multiply": 1.1, "add": -5 }],
    "intermittent_activation": {
      "effects": [
        {
          "frequency": "1 hour",
          "spell_effects": [
            { "id": "AEA_ADRENALINE" }
          ]
        }
      ]
    }
  }
]
```

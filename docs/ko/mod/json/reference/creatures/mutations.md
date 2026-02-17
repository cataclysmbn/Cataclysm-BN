# 돌연변이 & 특성

> [!NOTE]
>
> 이 페이지는 현재 작성 중이며 최근 `JSON INFO`에서 분리되었습니다

### 특성/돌연변이 필드

```json
"id": "LIGHTEATER",  // 고유 ID
"name": "Optimist",  // 게임 내 표시되는 이름
"points": 2,         // 특성의 포인트 비용. 양수 값은 포인트를 소비하고 음수 값은 포인트를 제공합니다
"visibility": 0,     // NPC 상호작용 목적을 위한 특성의 가시성 (기본값: 0)
"ugliness": 0,       // NPC 상호작용 목적을 위한 특성의 추함 (기본값: 0)
"cut_dmg_bonus": 3, // 비무장 베기 피해 보너스 (기본값: 0)
"pierce_dmg_bonus": 3, // 비무장 찌르기 피해 보너스 (기본값: 0.0)
"bash_dmg_bonus": 3, // 비무장 타격 피해 보너스 (기본값: 0)
"butchering_quality": 4, // 이 돌연변이의 도축 품질 (기본값: 0)
"rand_cut_bonus": { "min": 2, "max": 3 }, // 최소와 최대 사이의 비무장 베기 피해에 대한 무작위 보너스.
"rand_bash_bonus": { "min": 2, "max": 3 }, // 최소와 최대 사이의 비무장 타격 피해에 대한 무작위 보너스.
"bodytemp_modifiers" : [100, 150], // 추가 체온 단위 범위 (이 단위는 'weather.h'에 설명되어 있습니다. 첫 번째 값은 사람이 이미 과열된 경우 사용되고, 두 번째 값은 그렇지 않은 경우 사용됩니다.
"bodytemp_sleep" : 50, // 수면 시 적용되는 추가 체온 단위
"initial_ma_styles": [ "style_crane" ], // (optional) 게임 시작 시 플레이어가 선택할 수 있는 무술 스타일의 ID 목록.
"mixed_effect": false, // 특성이 긍정적 효과와 부정적 효과를 모두 가지고 있는지 여부. 이것은 순전히 선언적이며 사용자 인터페이스에만 사용됩니다. (기본값: false)
"description": "Nothing gets you down!" // 게임 내 설명
"starting_trait": true, // 캐릭터 생성 시 선택할 수 있습니다 (기본값: false)
"valid": false,      // 게임 내에서 돌연변이할 수 있습니다 (기본값: true)
"purifiable": false, //돌연변이가 정화될 수 있는지 설정합니다 (기본값: true)
"profession": true, //특성이 시작 직업 특수 특성입니다. (기본값: false)
"debug": false,     //특성은 디버그 목적입니다 (기본값: false)
"player_display": true, //특성이 `@` 플레이어 표시 메뉴에 표시됩니다
"category": ["BIRD", "INSECT"], // 이 돌연변이를 포함하는 카테고리
"prereqs": ["SKIN_ROUGH"], // 이 돌연변이를 향해 돌연변이하기 전에 이러한 돌연변이가 필요합니다
"prereqs2": ["LEAVES"], //이 돌연변이를 향해 돌연변이하기 전에 이러한 돌연변이도 필요합니다. 둘 다 설정되면 2개의 다른 돌연변이 경로를 생성하며, 하나에서 무작위로 선택됩니다. "prereqs"와 함께만 사용하세요
"threshold_tier": 1, //돌연변이하기 위해 카테고리 임계값 중 하나의 필요한 계층
"cancels": ["ROT1", "ROT2", "ROT3"], // 돌연변이 시 이러한 돌연변이를 취소합니다
"prevents": ["ROT1", "ROT2", "ROT3"], // 돌연변이 시 이러한 돌연변이를 받는 것을 방지합니다
"changes_to": ["FASTHEALER2"], // 추가 돌연변이 시 이러한 돌연변이로 변경될 수 있습니다
"leads_to": [], // 이것에 추가되는 돌연변이
"passive_mods" : { //나열된 값으로 스탯을 증가시킵니다. 음수는 스탯 감소를 의미합니다
            "per_mod" : 1, //가능한 값 per_mod, str_mod, dex_mod, int_mod
            "str_mod" : 2
},
"wet_protection":[{ "part": "HEAD", // 특정 신체 부위에 대한 젖음 보호
                    "good": 1 } ] // "neutral/good/ignored" // Good은 pos를 증가시키고 neg를 취소하고, neut는 neg를 취소하고, ignored는 둘 다 취소합니다
"vitamin_rates": [ [ "vitC", -1200 ] ], // 분당 추가로 소비하는 비타민 양. 음수 값은 생산을 의미합니다
"vitamins_absorb_multi": [ [ "flesh", [ [ "vitA", 0 ], [ "vitB", 0 ], [ "vitC", 0 ], [ "calcium", 0 ], [ "iron", 0 ] ], [ "all", [ [ "vitA", 2 ], [ "vitB", 2 ], [ "vitC", 2 ], [ "calcium", 2 ], [ "iron", 2 ] ] ] ], // 재료에 기반한 비타민 흡수의 승수. "all"은 모든 재료입니다. 여러 재료를 지원합니다.
"craft_skill_bonus": [ [ "electronics", -2 ], [ "tailor", -2 ], [ "mechanics", -2 ] ], // 돌연변이에 영향을 받는 스킬과 그 보너스. 보너스는 음수일 수 있으며, 4의 보너스는 1 전체 스킬 레벨의 가치가 있습니다.
"restricts_gear" : [ "TORSO" ], //이 돌연변이에 의해 제한되는 신체 부위 목록
"allowed_items" : [ "TORSO" ], //이러한 플래그가 있는 아이템은 제한과 관계없이 착용할 수 있습니다.
"allowed_items_only" : true, //true이고 이 돌연변이가 장비를 제한하는 경우, `allowed_items`만 착용할 수 있으며 그렇지 않으면 착용할 수 있는 OVERSIZE 아이템도 제외됩니다. (기본값: false)
"allow_soft_gear" : true, //'restricts_gear' 목록이 있는 경우 이 위치가 여전히 부드러운 재료로 만든 아이템을 허용하는지 설정합니다 (하나의 타입만 부드러워도 부드러운 것으로 간주됩니다). (기본값: false)
"destroys_gear" : true, //true이면 돌연변이할 때 'restricts_gear' 위치의 장비를 파괴합니다. (기본값: false)
"encumbrance_always" : [ // 선택된 신체 부위에 이만큼의 방해를 추가합니다
    [ "ARM_L", 20 ],
    [ "ARM_R", 20 ]
],
"encumbrance_covered" : [ // 선택된 신체 부위에 이만큼의 방해를 추가하지만, 부위가 OVERSIZE가 아닌 착용 장비로 덮여 있는 경우에만
    [ "HAND_L", 50 ],
    [ "HAND_R", 50 ]
],
"armor" : [ // 선택된 신체 부위를 이만큼 보호합니다. 저항은 아래의 `PART RESISTANCE`와 같은 구문을 사용합니다.
    [
        [ "ALL" ], // 선택된 저항을 전신에 적용하는 약어
        { "bash" : 2 } // 위에서 선택된 신체 부위에 제공되는 저항
    ],
    [   // 참고: 저항은 순서대로 적용되며 적용 사이에 ZEROED됩니다!
        [ "ARM_L", "ARM_R" ], // 해당 신체 부위에 대한 위 설정을 재정의합니다
        { "bash" : 1 }        // ...그리고 대신 해당 저항을 제공합니다
    ]
],
"dodge_modifier" : 1, // 회피에 대한 효과적인 변화에 부여되는 보너스. 음수 값은 페널티를 부과합니다.
"speed_modifier" : 2.0, // 현재 속도에 이 양을 곱합니다. 높을수록 더 빠르며, 1.0은 변화 없음입니다.
"movecost_modifier" : 0.5, // 대부분의 행동을 수행하는 데 걸리는 이동 양을 곱합니다. 낮을수록 더 빠른 행동입니다. 
"movecost_flatground_modifier" : 0.5, // 이동하기 쉬운 지형/가구의 movecost를 곱합니다. 낮을수록 더 빠른 이동입니다.
"movecost_obstacle_modifier" : 0.5, // 거친 지형을 가로지르는 movecost를 곱합니다. 낮을수록 더 빠른 이동입니다
"movecost_swim_modifier" : 0.5, // 수영하는 데 필요한 movecost를 곱합니다. 낮을수록 더 빠릅니다.
"falling_damage_multiplier" : 0.5, // 구덩이에 떨어지거나, 절벽에서 떨어지거나, 헐크에 의해 던져질 때 받는 피해에 대한 승수. 0은 면역을 부여합니다.
"attackcost_modifier" : 0.5, // 공격의 movecost에 대한 승수. 낮을수록 좋습니다.
"weight_capacity_modifier" : 0.5, // 페널티를 받기 전에 운반할 수 있는 무게를 곱합니다.
"hearing_modifier" : 0.5, // 청력이 얼마나 좋은지에 대한 승수, 더 높은 값은 더 먼 거리에서 소리를 감지한다는 것을 의미합니다. 0 값은 완전히 귀머거리로 만듭니다.
"noise_modifier" : 0.5, // 이동 중에 만드는 소음에 대한 승수, 0은 발소리를 조용하게 만듭니다.
"packmule_modifier" : 1.0, // 배낭 및 유사한 착용 컨테이너에 의해 부여되는 저장 공간을 곱합니다.
"crafting_speed_modifier" : 1.0, // 플레이어의 제작 속도를 곱합니다.
"construction_speed_modifier" : 1.0, // 플레이어의 건설 속도를 곱합니다.
"stealth_modifier" : 0, // 플레이어의 가시성 범위에서 빼질 백분율, 60으로 제한됩니다. 음수 값은 작동하지만 시야 범위가 제한되는 방식으로 인해 그다지 효과적이지 않습니다
"night_vision_range" : 0.0, // 야간 범위 시야. 모든 돌연변이 중 최고 및 최악의 값만 추가됩니다. 8 이상의 값은 완전한 어둠에서 읽을 수 있으며 플레이어가 희미한 조명에 있는 것처럼 보입니다. (기본값: 0.0)
"active" : true, //설정되면 돌연변이는 플레이어가 활성화해야 하는 활성 돌연변이입니다 (기본값: false)
"starts_active" : true, //true이면 이 '활성' 돌연변이는 활성 상태로 시작합니다 (기본값: false, 'active' 필요)
"cost" : 8, // 이 돌연변이를 활성화하는 비용. hunger, thirst 또는 fatigue 값 중 하나를 true로 설정해야 합니다. (기본값: 0)
"time" : 100, //비용을 다시 지불하기 전에 통과해야 하는 (턴 * 현재 플레이어 속도) 시간 단위의 양을 설정합니다. 효과를 보려면 1보다 커야 합니다. (기본값: 0)
"hunger" : true, //true이면 활성화된 돌연변이가 비용만큼 배고픔을 증가시킵니다. (기본값: false)
"thirst" : true, //true이면 활성화된 돌연변이가 비용만큼 갈증을 증가시킵니다. (기본값: false)
"fatigue" : true, //true이면 활성화된 돌연변이가 비용만큼 피로를 증가시킵니다. (기본값: false)
"stamina" : true, //true이면 활성화된 돌연변이가 비용만큼 체력을 감소시킵니다. (기본값: false)
"mana" : true, //true이면 활성화된 돌연변이가 비용만큼 마나를 감소시킵니다. (기본값: false)
"bionic" : true, //true이면 활성화된 돌연변이가 비용만큼 바이오닉 전력을 감소시킵니다. (기본값: false)
"pain" : true, //true이면 활성화된 돌연변이가 비용만큼 고통을 증가시킵니다. (기본값: false)
"health" : true, //true이면 활성화된 돌연변이가 비용만큼 건강을 감소시킵니다. (기본값: false)
"scent_modifier": 0.0,// 냄새 강도에 영향을 미치는 float. (기본값: 1.0)
"scent_intensity": 800,// 현재 냄새가 중력을 받는 대상 냄새에 영향을 미치는 int. (기본값: 500)
"scent_mask": -200,// 대상 냄새 값에 추가되는 int. (기본값: 0)
"scent_type": "sc_flower",// scent_typeid, scent_types.json에 정의됨, 방출하는 냄새 유형. (기본값: empty)
"bleed_resist": 1000, // 출혈 효과에 대한 저항을 정량화하는 Int, 효과의 강도보다 크면 출혈이 발생하지 않습니다. (기본값: 0)
"fat_to_max_hp": 1.0, // character_weight_category::normal 이상의 bmi 단위당 얻는 hp_max 양. (기본값: 0.0)
"healthy_rate": 0.0, // 건강이 얼마나 빨리 변할 수 있는지. 0으로 설정하면 절대 변하지 않습니다. (기본값: 1.0)
"weakness_to_water": 5, // 물이 당신에게 가하는 피해, 음수 값은 치유합니다. (기본값: 0)
"ignored_by": [ "ZOMBIE" ], // 당신을 무시하는 종 목록. (기본값: empty)
"anger_relations": [ [ "MARSHMALLOW", 20 ], [ "GUMMY", 5 ], [ "CHEWGUM", 20 ] ], // 당신에 의해 화난 종과 얼마나 화났는지의 목록, 음수 값을 사용하여 진정시킬 수 있습니다. (기본값: empty)
"can_only_eat": [ "junk" ], // 당신이 먹을 수 있는 음식에 필요한 재료 목록. (기본값: empty)
"can_only_heal_with": [ "bandage" ], // 당신이 제한되는 약 목록, 여기에는 mutagen, serum, aspirin, bandages 등이 포함됩니다... (기본값: empty)
"can_heal_with": [ "caramel_ointement" ], // 당신에게는 효과가 있지만 다른 사람에게는 효과가 없는 약 목록. 아이템의 `CANT_HEAL_EVERYONE` 플래그를 참조하세요. (기본값: empty)
"allowed_category": [ "ALPHA" ], // 당신이 돌연변이할 수 있는 카테고리 목록. (기본값: empty)
"no_cbm_on_bp": [ "TORSO", "HEAD", "EYES", "MOUTH", "ARM_L" ], // cbm을 받을 수 없는 신체 부위 목록. (기본값: empty)
"body_size": "LARGE", // 크기를 증가 또는 감소시키며, 한 번에 하나의 크기 돌연변이만 유효합니다. 허용되는 값: `TINY`, `SMALL`, `LARGE`, `HUGE`. `MEDIUM`은 지정할 수 있지만 효과가 없으며, 이것은 플레이어와 NPC의 기본 크기입니다.
"lumination": [ [ "HEAD", 20 ], [ "ARM_L", 10 ] ], // 빛나는 신체 부위와 빛의 강도를 float로 나타낸 목록. (기본값: empty)
"metabolism_modifier": 0.333, // 추가 대사율 승수. 1.0은 사용량을 두 배로, -0.5는 반으로 줄입니다.
"fatigue_modifier": 0.5, // 추가 피로율 승수. 1.0은 사용량을 두 배로, -0.5는 반으로 줄입니다.
"fatigue_regen_modifier": 0.333, // 휴식 시 피로와 수면 부족이 떨어지는 비율에 대한 수정자.
"healing_awake": 1.0, //  깨어 있는 동안의 치유에 영향을 줍니다. 이것과 휴식은 건강에 영향을 받습니다 awake_rate = heal_rate * healing_awake
"healing_resting": 0.5, // 수면 중 치유에 영향을 줍니다. asleep_rate = at_rest_quality * heal_rate * ( 1.0f + healing_resting)
"mending_modifier": 1.2 // 팔다리가 회복되는 속도에 대한 승수 - 이 값은 팔다리를 20% 더 빨리 회복하게 만듭니다
"stamina_regen_modifier": 0.5 // 추가 체력 재생률 승수. 1.0은 재생을 두 배로, -0.5는 반으로 줄입니다.
"max_stamina_modifier": 1.5 // 최대 체력에 대한 승수. 체력 재생은 동일한 비율로 비례적으로 수정되므로 stamina_regen_modifier를 암묵적으로 포함합니다. Character.cpp의 Character::update_stamina를 참조하세요.
"transform": { "target": "BIOLUM1", // 이것이 변환될 돌연변이의 Trait_id
               "msg_transform": "You turn your photophore OFF.", // 변환 시 표시되는 메시지
               "active": false , // 대상 돌연변이가 powered(켜짐)로 시작될 것인가.
               "moves": 100 // 이것이 소비하는 이동 수. (기본값: 0)
"enchantments": [ "MEP_INK_GLAND_SPRAY" ], // 이 마법 부여를 플레이어에게 적용합니다. magic.md와 effects_json.md를 참조하세요
"mutagen_target_modifier": 5         // 돌연변이 독소로 돌연변이할 때 돌연변이가 균형을 잡는 것을 선호하는 방식을 증가 또는 감소시킵니다. 음수 값은 대상 값을 낮게 푸시합니다 (기본값: 0)
}
```

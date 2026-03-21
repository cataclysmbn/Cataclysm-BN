# 格闘術と技

### 格闘術

```json
"type" : "martial_art",
"id" : "style_debug",       // 固有ID。空白を含まない1語である必要があり
                            // 必要な場合は、アンダースコアを使用すること。
"name" : "Debug Mastery",   // ゲーム内での表示名
"description": "A secret martial art used only by developers and cheaters.",    // ゲーム内での説明
"initiate": [ "You stand ready.", "%s stands ready." ],     // あなた、またはNPCがこの格闘術を選択した時のメッセージ
"autolearn": [ [ "unarmed", "2" ] ],     // スキル条件を満たすと自動的にこの格闘術を習得するリスト
"learn_difficulty": 5,      // 主要スキルに基づく、教本から格闘術を習得する難易度
                            // 教本を1回読むことでその格闘術を習得できる確率は、1 / (10 + learn_difficulty − 主要スキルレベル) に等しい
"arm_block" : 99,           // 腕ブロックが解禁される素手格闘スキルのレベル
"leg_block" : 99,           // 脚ブロックが解禁される素手格闘スキルのレベル
"static_buffs" : [          // 毎ターン自動的に適用されるバフの一覧
    "id" : "debug_elem_resist",
    "heat_arm_per" : 1.0
],
"ondodge_buffs" : [],        // 回避成功時に自動的に適用されるバフの一覧
"onattack_buffs" : [],       // 攻撃時 (命中・ミス問わず) に自動適用されるバフの一覧
"onhit_buffs" : [],          // 命中成功時に自動適用されるバフの一覧
"onmove_buffs" : [],         // 移動時に自動適用されるバフの一覧
"onmiss_buffs" : [],         // 攻撃を外した時に自動適用されるバフの一覧
"oncrit_buffs" : [],         // 会心時に自動適用されるバフの一覧
"onkill_buffs" : [],         // 敵を殺害した時に自動適用されるバフの一覧
"techniques" : [            // この格闘術を使用する際に利用可能な技の一覧
    "tec_debug_slow",
    "tec_debug_arpen"
],
"weapons": [ "tonfa" ],      // この格闘術で使用可能な武器の一覧
"weapon_category": [ "WEAPON_CAT1" ], // ここに指定されたカテゴリのいずれかを持つ武器が、この格闘術で使用可能になります。
"mutation": [ "UNSTYLISH" ] // この格闘術を使用するために必要な変異のリスト(少なくとも1つ必要)
```

### 技 (Techniques)

```json
"id" : "tec_debug_arpen",   // 固有ID。空白を含まない1語である必要があります
"name" : "phasing strike",  // ゲーム内で表示される名前
"unarmed_allowed" : true,   // 素手のキャラクターがこの技を使用できるかどうか
"unarmed_weapons_allowed" : true,    // この技の使用に完全な素手が必要か、あるいは素手系武器の使用を許可するかどうか
"weapon_categories_allowed" : [ "BLADES", "KNIVES" ], // この技を特定の武器カテゴリのみに制限します。省略した場合、すべての武器カテゴリが許可されます。unarmed_allowedがtrueの場合、素手は常に許可されます。
"melee_allowed" : true,     // 格闘術固有の武器だけでなく、あらゆる近接武器で使用可能であることを意味します。
"skill_requirements": [ { "name": "melee", "level": 3 } ],     // この技の使用に必要なスキルとその最小レベル。任意のスキルを指定できます。
"weapon_damage_requirements": [ { "type": "bash", "min": 5 } ],     // この技の使用に必要な武器の最小ダメージ。任意のダメージタイプを指定できます。
"req_buffs": [ "eskrima_hit_buff" ],    // この技の発動に特定のバフが有効である必要があります
"crit_tec" : true,          // この技が会心時のみ発動します
"crit_ok" : true,           // この技が通常命中時と会心時の両方で発動します
"downed_target": true,      // 転倒している標的に対してのみ発動します
"stunned_target": true,     // スタン状態の標的に対してのみ発動します
"human_target": true,       // 人間型の標的に対してのみ発動します
"knockback_dist": 1,        // 標標をノックバックさせる距離
"knockback_spread": 1,      // ノックバック時に標的が真後ろに飛ばない可能性がある範囲
"knockback_follow": 1,      // 標的をノックバックさせた際、攻撃者が追撃移動します
"stun_dur": 2,              // 標的をスタンさせる持続時間
"down_dur": 2,              // 標的を転倒させる持続時間
"side_switch": true,        // 標的を自分の背後に移動させます
"disarms": true,            // この技で相手を武装解除できます
"take_weapon": true,        // 手が空いている場合、武装解除した相手の武器を装備します
"grab_break": true,         // 自分に対する掴み状態を解除できる可能性があります
"aoe": "spin",              // この技には範囲効果(AoE)があります。単体の標的には機能しません
"block_counter": true,      // 防御成功時に自動的にカウンター攻撃を行う可能性があります
"dodge_counter": true,      // 回避成功時に自動的にカウンター攻撃を行う可能性があります
"weighting": 2,             // 複数の技が利用可能な際、この技が選択される確率に影響します
"defensive": true,          // 攻撃時にこの技が選択されないようになります
"wall_adjacent": true,      // 壁に隣接している必要があります
"miss_recovery": true,      // 攻撃を外した際の移動コスト消費を抑えます
"messages" : [              // プレイヤーまたはNPCがこの技を使用した際に表示されるメッセージ
    "You phase-strike %s",
    "<npcname> phase-strikes %s"
]
"movecost_mult" : 0.3,       // 後述する各種ボーナス
"mutations_required": [ "MASOCHIST" ] // この技の使用に必要な変異のリスト(少なくとも1つ必要)
```

### Buffs

```json
"id" : "debug_elem_resist",         // 固有ID。空白を含まない1語である必要があります。
"name" : "Elemental resistance",    // ゲーム内で表示される名前
"description" : "+Strength bash armor, +Dexterity acid armor, +Intelligence electricity armor, +Perception fire armor.",    // ゲーム内での説明
"buff_duration": 2,                 // このバフの持続ターン数
"unarmed_allowed" : true,           // 素手のキャラクターにこのバフを適用できるか
"melee_allowed" : false,          // 武器を装備したキャラクターにこのバフを適用できるか
"weapon_categories_allowed" : [ "BLADES", "KNIVES" ], // このバフを特定の武器カテゴリのみに制限します。省略した場合、すべての武器カテゴリが許可されます。unarmed_allowedがtrueの場合、素手は常に許可されます
"unarmed_weapons_allowed" : true,          // このバフに完全な素手が必要かどうか。trueの場合、素手系武器(ブラスナックル、パンチナイフ等)の使用を許可します
"max_stacks" : 8,                   // バフの最大累積数。バフのボーナス値は現在の累積数に応じて乗算されます
"bonus_blocks": 1       // 1ターンあたりの追加防御回数
"bonus_dodges": 1       // 1ターンあたりの追加回避回数
"flat_bonuses" : [                  // 加算ボーナス(後述)
],
"mult_bonuses" : [                  // 乗算ボーナス(後述)
]
```

### ボーナス (Bonuses)

ボーナス配列には、以下のようなボーナス項目を任意数含めることができます:

```json
{
  "stat": "damage",
  "type": "bash",
  "scaling-stat": "per",
  "scale": 0.15
}
```

- `"stat"`: 影響を受けるステータス。以下のいずれか："hit" (命中), "dodge" (回避), "block" (防御), "speed" (速度), "movecost" (移動コスト), "damage" (ダメージ), "armor" (装甲), "arpen" (装甲貫通), "target_armor_multiplier" (標的装甲倍率)。
- `"type"`: 影響を受けるステータスのダメージタイプ("bash" (打撃), "cut" (斬撃), "heat" (熱)など)。"damage" (ダメージ), "armor" (装甲), "arpen" (装甲貫通), "target_armor_multiplier" (標的装甲倍率)を指定する場合のみ必要です。
- `"scale"`: ボーナス自体の数値。
- `"scaling-stat"`: スケーリングに使用する能力値。以下のいずれか："str" (筋力), "dex" (器用), "int" (知力), "per" (感覚)。任意項目です。指定した場合、ボーナスの値にユーザーの対応する能力値が乗算されます。

ボーナスは正しい順序で記述する必要があります。

`useless` タイプのトークンはエラーにはなりませんが、効果も発揮しません。例えば、技に`speed`
を指定しても効果はありません (技の場合は`movecost` を使用してください)。

現在のところ、追加の属性ダメージは適用されませんが、追加の属性装甲は適用されます (通常の装甲の後に計算されます)。

例: 受ける打撃ダメージを筋力の30%分減少させる。バフでのみ有効です:

- `flat_bonuses : [ { "stat": "armor", "type": "bash", "scaling-stat": "str", "scale": 0.3 } ]`

与えるすべての斬撃ダメージを `(10% of dexterity)*(damage)` 倍にする:

- `mult_bonuses : [ { "stat": "damage", "type": "cut", "scaling-stat": "dex", "scale": 0.1 } ]`

移動コストを筋力の100%分減少させる

- `flat_bonuses : [ { "stat": "movecost", "scaling-stat": "str", "scale": -1.0 } ]`

static_bonusesで使用可能な追加フィールド:

```json
"stealthy": true, // すべての移動時の騒音が減少します 
"quiet": true, // 攻撃が完全に無音になります
"wall_adjacent": true, // 壁に隣接している必要があります
"throw_immune": true, // 投げ飛ばしを無効化します
```

### 世界への配置とキャラクター作成時の設定

格闘術の初期トレイト選択肢は mutations.jsonで設定します。作成した格闘術を適切なカテゴリ (護身術、少林拳、近接戦闘術など)に分類してください。

json/itemgroups/を使用して、作成した格闘術の教本や専用の近接武器を、世界各地のスポーン場所に配置してください。ここに武器を配置しない場合、プレイヤーは製作レシピから作るしか入手手段がなくなります。

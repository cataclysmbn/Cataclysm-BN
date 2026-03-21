# 変異と特性

> [!NOTE]
>
> このページは現在作成中であり、最近 `JSON INFO`から分割されたものです。

### 特性/変異のフィールド定義

```json
"id": "LIGHTEATER",  // 固有のID
"name": "Optimist",  // ゲーム内で表示される名称
"points": 2,         // 特性の取得コスト。正の値はポイントを消費し、負の値はポイントを獲得します
"visibility": 0,     // NPCとのやり取りに影響する、見た目の目立ちやすさ (既定値: 0)
"ugliness": 0,       // NPCとのやり取りに影響する、外見の醜悪さ (既定値: 0)
"cut_dmg_bonus": 3, // 素手時の斬撃ダメージボーナス (既定値: 0)
"pierce_dmg_bonus": 3, // 素手時の刺突ダメージボーナス (既定値: 0.0)
"bash_dmg_bonus": 3, // 素手時の打撃ダメージボーナス (既定値: 0)
"butchering_quality": 4, // この変異による解体品質 (既定値: 0)
"rand_cut_bonus": { "min": 2, "max": 3 }, // 素手時の斬撃ダメージに対する、最小値から最大値までのランダムボーナス
"rand_bash_bonus": { "min": 2, "max": 3 }, // 素手時の打撃ダメージに対する、最小値から最大値までのランダムボーナス
"bodytemp_modifiers" : [100, 150], // 追加の体温ユニットの範囲を指定します（これらの単位は 'weather.h' で定義されています）。1つ目の値はキャラクターが既にオーバーヒート（過熱）状態にある場合に適用され、2つ目の値はそうでない場合に適用されます。
"bodytemp_sleep" : 50, // 睡眠中に適用される追加の体温ユニット
"initial_ma_styles": [ "style_crane" ], // (任意) ゲーム開始時にプレイヤーが選択できる武術スタイルのIDリスト
"mixed_effect": false, // この特性がメリットとデメリットの両面を持つかどうか。UI表示専用の宣言的な設定です (既定値: false)
"description": "Nothing gets you down!" // ゲーム内の説明文
"starting_trait": true, // キャラクター作成時に選択可能か (既定値: false)
"valid": false,      // ゲーム内で変異によって取得可能か (既定値: true)
"purifiable": false, // 精製剤で除去可能か (既定値: true)
"profession": true, // 職業専用の初期特質か (既定値: false)
"debug": false,     // デバッグ用の特質か (既定値: false)
"player_display": true, // プレイヤー情報画面(@)で表示するか
"category": ["MUTCAT_BIRD", "MUTCAT_INSECT"], // この変異が属する変異カテゴリ
"prereqs": ["SKIN_ROUGH"], // この変異を取得するために事前に必要な変異
"prereqs2": ["LEAVES"], //この変異を取得するために事前に必要な変異（第2候補）。prereqsと併用すると2つの変異ルートが作成され、どちらかがランダムに選ばれます
"threshreq": ["THRESH_SPIDER"], // この変異の取得に必要な閾値（しきい値）
"cancels": ["ROT1", "ROT2", "ROT3"], // 変異時に、これら既存の変異を上書きして消去します
"prevents": ["ROT1", "ROT2", "ROT3"], // この変異がある限り、指定された変異の取得を防ぎます
"changes_to": ["FASTHEALER2"], // さらに変異が進んだ際、この変異に変化する可能性があります
"leads_to": [], // この変異から派生する変異
"passive_mods" : { // 指定した能力値を上昇させます。負の値は減少を意味します
            "per_mod" : 1, // 指定可能な値: per_mod(感覚), str_mod(筋力), dex_mod(器用), int_mod(知力)
            "str_mod" : 2
},
"wet_protection":[{ "part": "HEAD", // 特定部位の濡れ保護
                    "good": 1 } ] // "neutral/good/ignored" // goodはメリットを増大させデメリットを相殺、neutはデメリットのみ相殺、ignoredは両方を相殺
"vitamin_rates": [ [ "vitC", -1200 ] ], //1分あたりの追加ビタミン消費量。負の値は体内で生成されることを意味します
"vitamins_absorb_multi": [ [ "flesh", [ [ "vitA", 0 ], [ "vitB", 0 ], [ "vitC", 0 ], [ "calcium", 0 ], [ "iron", 0 ] ], [ "all", [ [ "vitA", 2 ], [ "vitB", 2 ], [ "vitC", 2 ], [ "calcium", 2 ], [ "iron", 2 ] ] ] ], // multiplier of vitamin absorption based on material. "all" is every material. supports multiple materials.
"craft_skill_bonus": [ [ "electronics", -2 ], [ "tailor", -2 ], [ "mechanics", -2 ] ], // 影響を受けるスキルとボーナス。4ポイントでスキルレベル1に相当します
"restricts_gear" : [ "TORSO" ], // この変異によって装備が制限される部位のリスト
"allowed_items" : [ "TORSO" ], // 特定のフラグを持つアイテムであれば制限に関わらず装備可能にします
"allowed_items_only" : true, // trueの場合、装備制限がある部位には `allowed_items` 以外の装備を一切禁止します。通常は装備可能な「特大(OVERSIZE)」サイズも対象外となります (既定値: false)
"allow_soft_gear" : true, // 装備制限があっても、布などの柔らかい素材でできた装備を許可するか (既定値: false)
"destroys_gear" : true, // trueの場合、この変異が発生した瞬間に制限部位の装備を破壊します (既定値: false)
"encumbrance_always" : [ // 指定部位に常時加算される動作制限
    [ "ARM_L", 20 ],
    [ "ARM_R", 20 ]
],
"encumbrance_covered" : [ // 指定部位が「特大(OVERSIZE)」以外の装備で覆われている場合のみ加算される動作制限
    [ "HAND_L", 50 ],
    [ "HAND_R", 50 ]
],
"armor" : [ // 指定した部位を保護する強度です。耐性の設定には、後述の `PART RESISTANCE` below.
    [
        [ "ALL" ], // 選択した耐性を全身に一括適用するショートハンド
        { "bash" : 2 } // 上記で選択した部位に付与される耐性値
    ],
    [   // 注意: 耐性は記述順に適用され、適用ごとに上書きされます！
        [ "ARM_L", "ARM_R" ], // 指定した部位の耐性を上記の設定から上書きします
        { "bash" : 1 }        // ...そして、代わりにこれらの耐性値を付与します
    ]
],
"dodge_modifier" : 1, // 回避率に対するボーナス。負の値はペナルティとなります。
"speed_modifier" : 2.0, // 現在の移動速度(Speed)を指定倍率で乗算します。1.0で変化なし、数値が高いほど速くなります。
"movecost_modifier" : 0.5, // ほとんどのアクションに要する移動コスト(Moves)を乗算します。数値が低いほどアクションが高速になります。 
"movecost_flatground_modifier" : 0.5, // 移動しやすい地形や家具の上での移動コストを乗算します。数値が低いほど移動が速くなります。
"movecost_obstacle_modifier" : 0.5, // 荒地などの移動コストを乗算します。数値が低いほど移動が速くなります。
"movecost_swim_modifier" : 0.5, // 泳ぐのに必要な移動コストを乗算します。数値が低いほど速くなります。
"falling_damage_multiplier" : 0.5, // 落とし穴、崖からの転落、ハルクに投げ飛ばされた際のダメージ倍率。0で無効化します。
"attackcost_modifier" : 0.5, // 攻撃時の移動コスト倍率。低いほど優秀です。
"weight_capacity_modifier" : 0.5, // ペナルティなしで持ち運べる重量の倍率。
"hearing_modifier" : 0.5, // 聴力の倍率。値が高いほど遠くの音を検知できます。0にすると完全に耳が聞こえなくなります。
"noise_modifier" : 0.5, // 移動中に発する騒音の倍率。0にすると足音が無音になります。
"packmule_modifier" : 1.0, // バックパック等の装備品から得られる収納スペースの倍率。
"crafting_speed_modifier" : 1.0, // プレイヤーの製作速度の倍率。
"construction_speed_modifier" : 1.0, // プレイヤーの建設速度の倍率。
"stealth_modifier" : 0, // プレイヤーの被視認距離から減算されるパーセンテージ（最大60）。負の値も設定可能ですが、視界範囲の制限仕様により効果は限定的です。
"night_vision_range" : 0.0, // 夜間視界の範囲。全変異の中で「最も高い値」と「最も低い値」のみが適用されます。8以上の値は、薄暗い場所にいるかのように暗闇での読書を可能にします (既定値: 0.0)
"active" : true, // trueに設定すると、プレイヤーが手動で発動させる必要がある「アクティブ変異」になります (既定値: false)
"starts_active" : true, // trueの場合、このアクティブ変異は発動した状態で開始します (既定値: false, `active` が必須)
"cost" : 8, // 発動コスト。空腹、喉の渇き、疲労のいずれかの値をtrueにする必要があります (既定値: 0)
"time" : 100, // 再びコストを支払うまでに経過させる時間ユニット（ターン数 * 現在の移動速度）を指定します。効果を出すには1より大きい値が必要です (既定値: 0)
"hunger" : true, // trueの場合、発動時に空腹度がコスト分上昇します (既定値: false)
"thirst" : true, // trueの場合、発動時に喉の渇きがコスト分上昇します (既定値: false)
"fatigue" : true, // trueの場合、発動時に疲労度がコスト分上昇します (既定値: false)
"stamina" : true, // trueの場合、発動時にスタミナがコスト分減少します (既定値: false)
"mana" : true, // trueの場合、発動時にマナがコスト分減少します (既定値: false)
"bionic" : true, // trueの場合、発動時に義体電力がコスト分減少します (既定値: false)
"pain" : true, // trueの場合、発動時に痛みがコスト分上昇します (既定値: false)
"health" : true, // trueの場合、発動時に健康値がコスト分減少します (既定値: false)
"scent_modifier": 0.0,// 自身の臭いの強さに影響する浮動小数点数 (既定値: 1.0)
"scent_intensity": 800,// 現在の臭いが収束していく目標値 (既定値: 500)
"scent_mask": -200,// 目標とする臭いの値に加算される整数値 (既定値: 0)
"scent_type": "sc_flower",// scent_types.jsonで定義された臭いの種類ID。放出する臭いのタイプを指定します (既定値: 空)
"bleed_resist": 1000, // 出血状態への耐性値。出血強度がこの値以下であれば出血しません (既定値: 0)
"fat_to_max_hp": 1.0, // 標準体重(BMI)を上回るユニットごとに増加する最大HPの量 (既定値: 0.0)
"healthy_rate": 0.0, // 健康値の変化速度。0に設定すると変動しなくなります (既定値: 1.0)
"weakness_to_water": 5, // 水から受けるダメージ。負の値は回復を意味します (既定値: 0)
"ignored_by": [ "ZOMBIE" ], // プレイヤーを無視する種族のリスト (既定値: 空)
"anger_relations": [ [ "MARSHMALLOW", 20 ], [ "GUMMY", 5 ], [ "CHEWGUM", 20 ] ], // プレイヤーに敵対する種族とその上昇値のリスト（負の値は沈静化） (既定値: 空)
"can_only_eat": [ "junk" ], // 食用とするために必要な素材のリスト (既定値: 空)
"can_only_heal_with": [ "bandage" ], // 使用可能な医薬品の制限（変異薬、血清、アスピリン、包帯などを含む） (既定値: 空)
"can_heal_with": [ "caramel_ointement" ], // 本来は自分にしか効果がないが、自分に限って使用可能になる医薬品のリスト。アイテム側の `CANT_HEAL_EVERYONE` フラグを参照してください。 (既定値: 空)
"allowed_category": [ "ALPHA" ], // 変異可能なカテゴリのリスト (既定値: 空)
"no_cbm_on_bp": [ "TORSO", "HEAD", "EYES", "MOUTH", "ARM_L" ], // CBMを埋め込めない部位のリスト (既定値: 空)
"body_size": "LARGE", // 体格を増減させます。有効な変異は一度に1つのみです。指定可能な値: `TINY`, `SMALL`, `LARGE`, `HUGE`（`MEDIUM`は既定値のため効果なし）
"lumination": [ [ "HEAD", 20 ], [ "ARM_L", 10 ] ], // 発光する部位と、その強度のリスト (既定値: 空)
"metabolism_modifier": 0.333, // 代謝率の追加倍率。1.0は消費量倍増、-0.5は半減を意味します。
"fatigue_modifier": 0.5, // 疲労蓄積率の追加倍率。1.0は消費量倍増、-0.5は半減を意味します。
"fatigue_regen_modifier": 0.333, // 休息時に疲労や睡眠不足が回復する速度の修正値。
"healing_awake": 1.0, //  起きている時の回復速度に影響します。健康値の影響を受け、式は「覚醒時回復率 = 基本回復率 * healing_awake」となります。
"healing_resting": 0.5, // 睡眠時の回復速度に影響します。式は「睡眠時回復率 = 休息品質 * 基本回復率 * (1.0 + healing_resting)」となります。
"mending_modifier": 1.2 // 肢体の治癒（骨折など）速度の倍率。この値なら20%速く治癒します。
"stamina_regen_modifier": 0.5 // スタミナ回復率の追加倍率。1.0は回復量倍増、-0.5は半減を意味します。
"max_stamina_modifier": 1.5 // 最大スタミナの倍率。スタミナ回復量もこれに比例して修正されるため、暗黙的に `stamina_regen_modifier` の効果も含みます。
"transform": { "target": "BIOLUM1", // この変異が変化（変身）した後の変異ID
               "msg_transform": "You turn your photophore OFF.", // 変化時に表示されるメッセージ
               "active": false , //変化後の変異が発動状態で開始するか
               "moves": 100 // 変化に要する移動コスト (既定値: 0)
"enchantments": [ "MEP_INK_GLAND_SPRAY" ], // プレイヤーに付与するエンチャント。詳細は magic.md 等を参照。
"mutagen_target_modifier": 5         // 変異原による変異のバランス調整。負の値は目標値を低く押し下げます (既定値: 0)
}
```

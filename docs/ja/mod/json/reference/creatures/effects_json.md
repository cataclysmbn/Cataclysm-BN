# 効果

## ゲーム内での効果の付与方法

### 食用アイテム

プレイヤーに効果を付与する最も一般的な方法は、薬物システムを介することです。これを行うには、アイテムの use_action のタイプを "consume_drug" に設定する必要があります。

```json
"use_action" : {
    "type" : "consume_drug",
    "activation_message" : "You take some oxycodone.", // オキシコドンを摂取したメッセージ
    "effects" : [
        {
            "id": "pkill3",
            "duration": 20
        },
        {
            "id": "pkill2",
            "duration": 200
        }
    ]
},
```

"effects" フィールドに注目してください。各効果には、以下の潜在的なフィールドがあります。

```json
"id" // 必須
"duration" // 必須
"bp" // 効果を特定の身体部位に付与させる
```

有効な "bp" エントリ (エントリがない場合は効果はなし):

```json
"torso"
"head"
"eyes"
"mouth"
"arm_l"
"arm_r"
"hand_l"
"hand_r"
"leg_l"
"leg_r"
"foot_l"
"foot_r"
```

### クリーチャーの攻撃

クリーチャーも、アイテムの "consume_drug" エントリと同様の効果フィールドを持っています。
クリーチャーに "attack_effs" エントリを追加することで、攻撃にも効果を適用することができます。

```json
"attack_effs": [
    {           // これを複数回適用すると、強度が 1 ではなく 3 ずつ増加する
        "//": "applying this multiple times makes intensity go up by 3 instead of 1",
        "id": "paralyzepoison",
        "duration": 33
    },
    {
        "id": "paralyzepoison",
        "duration": 33
    },
    {
        "id": "paralyzepoison",
        "duration": 33
    }
],
```

"attack_effs" のフィールドは、"consume_drug" のフィールドと同一に機能します。
ただし、クリーチャーには追加のフィールドがあります。

```json
"chance" - 命中時に効果が適用されるパーセンテージの確率 100%
```

クリーチャーがプレイヤーにダメージを与えることに成功し、かつ確率判定に成功した場合、リストされているすべての効果がプレイヤーに適用されます。効果は一つずつ順番に追加されます。

### 変異

変異は、"enchantments" フィールドを使用して効果を与えることができます。
以下の変異は特殊攻撃を付与します。また、墨袋 (ink gland) 変異の後に付与されるエンチャントは、
プレイヤーの装甲値を永続的に変更します。

```json
{
    "type": "mutation",
    "id": "INK_GLANDS",
    "name": { "str": "Ink glands" },
    "points": 1,
    "visibility": 1,
    "ugliness": 1,
    "description": "Several ink glands have grown onto your torso.  They can be used to spray defensive ink and blind an attacker in an emergency, as long as the torso isn't covered.",
    "enchantments": [ "MEP_INK_GLAND_SPRAY" ], // 特殊攻撃を付与するエンチャント
    "category": [ "CEPHALOPOD" ]
  },
  {
    "type": "enchantment",
    "id": "ENCH_BIO_CARBON",
    "condition": "ALWAYS",
    "values": [
      { "value": "ARMOR_BASH", "multiply": -0.05 },
      { "value": "ARMOR_CUT", "multiply": -0.1 },
      { "value": "ARMOR_STAB", "multiply": -0.08 },
      { "value": "ARMOR_BULLET", "multiply": -0.15 }
    ]
  }
```

## 必須フィールド

```json
"type": "effect_type",      // 必須: タイプは "effect_type"
"id": "xxxx"                // 必須: 固有IDであること
```

## Optional fields

### Max intensity

```json
"max_intensity": 3          // 最大強度。後の多くのフィールドで使用されます。既定値: 1
"max_effective_intensity"   // 適用される効果の強度レベル数。これを超える強度レベルは持続時間のみを増加させます。
```

### 名前

```json
"name": ["XYZ"]
or
"name": [
    "ABC",
    "XYZ",
    "123"
]
```

`"max_intensity"`が１より大きく、`"name"` のエントリ数が `"max_intensity"`
以上の場合、適切な強度の名前が使用されます（例：強度 1 で "ABC"、2 で "XYZ"）。
`"max_intensity"`が１の場合、またはエントリ数が不足している場合は、最初の名前が使用されます。
現在の強度が 1 より大きい場合に `"[2]", "[3]"` のように括弧内に強度が付加されます（例: "ABC [2]"）。
"name" の任意のエントリが空の文字列 ("") であるか、"name" がない場合、効果はステータス画面に表示されません。

各 "name" エントリはオプションのコンテキストを持つことができます。

```json
"name": [ { "ctxt": "ECIG", "str": "Smoke" } ]
```

この場合、ゲームはコンテキスト "ECIG" で名前を翻訳します。
これにより、他の言語で動詞の "Smoke" と名詞の "Smoke" を区別することが可能になります。

```json
"speed_name" : "XYZ"        // プレイヤーのスピードに対する修飾子リストで使用される値。
```

"speed_name" が存在しない場合、または空の文字列 ("") の場合、プレイヤーのスピードの修飾子リストには表示されません（ただし、効果自体は適用されます）。

### 説明

```json
"desc": ["XYZ"]
or
"desc": [
    "ABC",
    "XYZ",
    "123"
]
```

説明の選択: 説明 ("desc") は、使用するものを選択する際に、名前フィールドと同一の動作をします。

- 説明は一般的に 1 行にすべきです。
- ステータスや効果の変更は、自動的に生成されるため含める必要はありません。
- 説明行が空の文字列 ("") の場合、効果の説明には能力値の変更のみが表示されます。

説明には、修飾子として機能する 2 番目のフィールドもあります。

```json
"part_descs": true     // 存在しない場合のデフォルトは false です
```

"part_descs" が true の場合、説明の前に「Your X」(X は身体部位名)が付加されます。これは、以前の説明が「Your left arm ABC」のように表示されることを意味します。

また、説明には短縮形式を持たせることもできます。

```json
"reduced_desc": ["XYZ"]
or
"reduced_desc": [
    "ABC",
    "XYZ",
    "123"
]
```

これは、効果が軽減された場合の説明です。既定では、存在しない場合に通常の説明が使用されます。

### 評価

```json
"rating": "good"       // 欠落している場合の既定値は "neutral" です
```

効果が適用または除去されたとき表示されるメッセージです。
また、「血液分析の説明 (blood_analysis_description)」（後述）フィールドにも影響します。
キャラクターが何らかの方法で血液分析を実施するとき、「good」の評価を持つ効果は緑色で表示されます。
それ以外の評価を持つ効果は赤色で表示されます。

有効なエントリは以下の通りです。

```json
"good"    // 良い
"neutral" // 中立
"bad"     // 悪い
"mixed"   // 混合
```

### 外見 (Looks_like)

```json
"looks_like": "drunk"
```

"looks_like" フィールドが存在する場合、指定されたタグ（例: "drunk"）と同じように見えます。
これは、NPC やキャラクターの外見のみに影響し、内部の動作には影響しません。

### メッセージ (Messages)

```json
"apply_message": "message",
"remove_message": "message"
```

"apply_message" フィールは効果の追加時にメッセージが表示されます。
"remove_message" フィールドは効果の除去時にメッセージが表示されます。
注釈: "apply_message" は、新たに効果が追加された場合にのみ表示され、現在と同じ効果を受けた場合には表示されません。
例：新たな噛み傷など

### 記念ログ (Memorial Log)

```json
"apply_memorial_log": "log",
"remove_memorial_log": "log"
```

"apply_memorial_log" フィールドは効果の追加時にメッセージが記念ログに追加されます。
"remove_memorial_log" フィールドは効果の除去時にメッセージが記念ログに追加されます。
メッセージフィールドと同様に、"apply_memorial_log" は、新たに効果が追加された場合にのみログに追加されます。

### 耐性 (Resistances)

```json
"resist_trait": "NOPAIN", // プレイヤーが一致する特性を持っている場合、効果に抵抗します。
"resist_effect": "flumed" // プレイヤーが一致する効果を持っている場合、効果に抵抗します。
```

これらのフィールドは、効果が抵抗されているかどうかを判断するために使用されます。
プレイヤーが一致する特性または効果を持っている場合、その効果に「抵抗」し、その結果、効果の内容や説明が変化します。
効果は、一度に1つの "resist_trait" と1つの "resist_effect" しか持つことができません。

### 効果の除去 (Removes effects)

```json
"removes_effects": ["bite", "flu"]
```

このフィールドは、効果が適用された際に、リストされている他の効果がプレイヤーに存在する場合、それらのコピーを自動的にすべて除去します。上記の例では、この効果が適用されると、プレイヤーが負っている噛み傷やインフルエンザを自動的に治癒することになります。
ここにリストされている値はすべて、自動的に "blocks_effects" としてもカウントされるため、手動で"blocks_effects"に重複記述する必要はありません。

### 効果のブロック (Removes effects)

```json
"blocks_effects": ["cold", "flu"]
```

このフィールドは、効果がリストされている効果の配置を防ぐようにします。
上記の例では、この効果はプレイヤーが風邪やインフルエンザにかかるのを防ぎます
(ただし、進行中の風邪やインフルエンザを治癒することはありません)。
"removes_effects" に存在する効果は、自動的に "blocks_effects" に追加されるため、手動で重複記述する必要はありません。

### 効果の制限 (Effect limiters)

```json
"max_duration": 100, // 最大持続時間
"dur_add_perc": 150  // デフォルトは 100% です
```

これらは、すでに存在する効果に持続時間を追加する際に利用されます。
"max_duration" は、効果の全体的な最大持続時間を制限します。
"dur_add_perc" は、既存の効果に追加する際の、通常の持続時間に対するパーセンテージ値です。

例:

1. プレイヤーに効果 A を 100 ターン分追加します。
2. プレイヤーに効果 A を再度 100 ターン分追加します。上記の例では "dur_add_perc" が 150 なので、2 回目の追加は合
   計 100 ✕ 150% = 150 ターンを追加し、合計持続時間は 250 ターンになります。
   この値は 100% 未満に設定することも可能であり、負の値にすることもできるはずです。
   負の値に設定した場合、将来の適用は全体の残り時間を減少させることになります。

### 強度 (Intensities)

強度は、効果の作用、名前、および説明を制御するために使用されます。これらは以下のフィールドで定義されます。

```json
"int_add_val": 2         // 既定値は0です！変更しない限り、強度は増加しません！
and/or
"int_decay_step": -2,    // 既定値は -1 です
"int_decay_tick": 10
or
"int_dur_factor": 700
```

最初の値は、すでに存在する効果に持続時間を追加する際に強度がインクリメントされる量です。

例:

1. プレイヤーに効果 A を追加します（強度は 1 になる）。
2. プレイヤーに効果 A を再度追加します。 上記の例では "int_add_val" が2なので、2 回目の追加により、効果の強度は
   1 + 2 = 3 に変化します。 注釈: 強度が機能するためには、この3つの強度データセットのうち少なくとも 1 つを含める必要があります。

"int_decay_step" と "int_decay_tick" は、両方揃って初めて機能します。
両方が存在する場合、ゲームは "int_decay_step" ティックごとに現在の効果の強度を
"int_decay_tick" だけ自動的に増減させます。結果は `[1, "max_intensity"]`の範囲に制限されます。
これは、効果が時間とともに自動的に強度を増減させるために使用できます。

"int_dur_factor" は、他の3つの強度フィールドを上書きし、強度を以下の式で定義された数値に強制します。
intensity = duration / "int_dur_factor"
(つまり、0から "int_dur_factor"までは強度が1となります)。

### 永続性

永続的な効果は、時間とともに持続時間を失いません。
つまり、その持続時間が1ターンであっても、除去されるまで持続します。

```json
"permanent": true
```

### ミスメッセージ

```json
"miss_messages": [["Your blisters distract you", 1]]
or
"miss_messages": [
    ["Your blisters distract you", 1],
    ["Your blisters don't like you", 10],
]
```

これは、効果が有効である間、指定された確率で以下のミスメッセージを追加します。

### 減衰メッセージ

```json
"decay_messages": [["The jet injector's chemicals wear off.  You feel AWFUL!", "bad"]]
or
"decay_messages": [
    ["The jet injector's chemicals wear off.  You feel AWFUL!", "bad"],
    ["OOGA-BOOGA.  You feel AWFUL!", "bad"],
]
```

メッセージは強度と一致します。つまり、最初のメッセージは強度1に対応し、2番目のメッセージは強度2に対応します。
これらのメッセージは、より高い強度から、一致する強度まで減少した場合に常に表示されます。
これは、減衰ティック (int_decay_tick) を介して減少した場合でも、"int_dur_factor" を介して減少した場合でも同様です。

効果の強度が3以上から2に減少した場合、メッセージ(例: "OOGA-BOOGA. You feel AWFUL!")がプレイヤーに対して「bad」のメッセージとして表示されます。

### ターゲット修飾子

```json
"main_parts_only": true     // 既定値は false です
```

true に設定されている場合、非主要部位(手、目、足など)に適用された効果を、対応する主要部位(腕、頭、脚など)に自動的に再適用します。

### 効果修飾子 (Effect modifiers)

```json
"pkill_addict_reduces": true,   // 既定値は false です
"pain_sizing": true,            // 既定値は false です
"hurt_sizing": true,            // 既定値は false です
"harmful_cough": true           // 既定値は false です
```

`"pkill_addict_reduces"`は、プレイヤーの鎮痛剤中毒によって、追加の鎮痛効果を与える確率を減少させます。
`"pain_sizing"` および `"hurt_sizing"` は、Large/Huge 変異が痛みや負傷の効果をトリガーする確率に影響を与えるようにします。
`"harmful_cough"` は、引き起こされる咳が、プレイヤーにダメージを与える可能性があることを意味します。

### 士気 (morale)

```json
"morale": "morale_high"
```

提供される士気効果のタイプです。
士気効果がある場合は必須であり、そうでない場合は指定してはなりません。

### 除去時の他の効果 (effects_on_remove)

```json
"effects_on_remove": [
    {
        "intensity_requirement": 0, // 既定値は 0
        "effect_type": "cold",      // (必須) 適用される効果
        "allow_on_decay": false,    // 既定値は true
        "allow_on_remove": true,    // 既定値は false
        "intensity": 5,             // 既定値は 0
        "inherit_intensity": false, // 既定値は false
        "duration": "10 s",         // 既定値は 0
        "inherit_duration": true,   // 既定値は true
        "body_part": "hand_r",      // 既定値は null
        "inherit_body_part": false  // 既定値は true
    }
]
```

"intensity_requirement" 現在の効果の強度がこれより低い場合、新しい効果の追加を防止します。
"allow_on_decay" 親効果が減衰した（持続時間 0 のために除去された）場合に、効果の追加を有効にします。
"allow_on_remove" 親効果が持続時間 0 より前に除去された場合に、効果の追加を有効にします。
"inherit_duration"、"inherit_intensity"、"inherit_body_part" 親効果から強度を継承させます。

### 効果の本体 (Effect effects)

```json
"base_mods" : {
    arguments
},
"scaling_mods": {
    arguments
}
```

ここに、効果の JSON 定義の中核があります。それぞれが様々な引数を取ることができます。
小数点も有効ですが、"0.X" または "-0.X" の形式で記述する必要があります。
ゲームは、実際に適用される値を計算する際、最終的にゼロ方向に丸めます。

基本的な定義:

```json
"X_amount"      - 効果が適用されたときに増減する X の量。新しい効果が発生したときのみ反映されます。
"X_min"         - 判定が発生したときに適用される X の最小量。
"X_max"         - 判定が発生したときに適用される X の最大量。未設定の場合は、乱数ではなく毎回正確に X_min が適用されます。
"X_min_val"     - 効果によって最終的に適用される X の下限値。0 は制限なしを意味します。一部の X では存在しません。
"X_max_val"     - 効果によって最終的に適用される X の上限値。0 は制限なしを意味します。一部の X では存在しません。
"X_chance"      - X が発生する基本確率。正確な計算式は "X_chance_bot" に依存します。
"X_chance_bot"  - 存在しない場合、発生確率は "1 / X_chance" です。存在する場合は、"X_chance_bot" の計算式に従って決まります。
"X_tick"        - X の発生判定が行われる間隔。デフォルトは Y ティックごと。
```

有効な引数:

```json
"str_mod"           - 筋力 ステータスの修正値。正の値で筋力が増加、負の値で筋力が減少。
"dex_mod"           - 器用 ステータスの修正値。正の値で器用が増加、負の値で器用が減少。
"per_mod"           - 知覚 ステータスの修正値。正の値で知覚が増加、負の値で知覚が減少。
"int_mod"           - 知性 ステータスの修正値。正の値で知性が増加、負の値で知性が減少。
"speed_mod"         - 速度 ステータスの修正値。正の値で速度が増加、負の値で速度が減少。

"pain_amount"       - 効果が適用されたときに増加する痛みの量。正の値で痛みが増加し、負の値では変化しません。※値を高く設定しすぎると危険です。
"pain_min"          - 効果によって付与される痛みの最小量。
"pain_max"          - 効果によって付与される痛みの最大量。0 または未設定の場合、毎回 "pain_min" が適用されます。
"pain_max_val"      - プレイヤーに適用される痛みの上限値。0 は制限なし (無制限) を意味します。
"pain_chance"       - 痛みが増加する基本確率。正確な計算は pain_chance_bot に依存します。
"pain_chance_bot"   - 発生確率計算の補助値。存在しない場合は確率 = 1 / pain_chance、存在する場合は計算式に従います。
"pain_tick"         - 痛みの発生判定が行われる間隔。

"hurt_amount"       - 効果が適用されたときに発生するダメージ量。正の値でダメージ、負の値で回復。
※値を高く設定しすぎると危険です。
"hurt_min"          - 効果によって与えられる最小のダメージ量(負の値で回復)。
"hurt_max"          - 効果によって与えられる最大のダメージ量。0 または未設定の場合、毎回 "hurt_min" が適用されます。
"hurt_chance"       - ダメージが発生する基本確率。正確な計算は hurt_chance_bot に依存します。
"hurt_chance_bot"   - hurt_chance_bot：発生確率計算の補助値。存在しない場合は確率 = 1 /hurt_chance、存在する場合は計算式に従います。
"hurt_tick"         - ダメージ発生判定が行われる間隔。

"sleep_amount"      - 効果が適用されたときに消費される睡眠ターン数。
"sleep_min"         - 効果によって付与される最小睡眠ターン数。
"sleep_max"         - 効果によって付与される最大睡眠ターン数。0 または未設定の場合、毎回 "sleep_min" が適用されます。
"sleep_chance"      - 眠りに落ちる確率の基本値。正確な計算は sleep_chance_bot に依存します。
"sleep_chance_bot"  - 眠りに落ちる確率の補助値。未設定の場合は確率 = 1 / sleep_chance、存在する場合は計算式に従います。
"sleep_tick"        - 睡眠判定が行われる間隔。

"pkill_amount"      - 効果が適用されたときに与えられる鎮痛効果量。
※値を高く設定しすぎるとゲームバランスに影響します
"pkill_min"         - 効果によって付与される最小鎮痛量。
"pkill_max"         - 効果によって付与される最大鎮痛量。0 または未設定の場合、毎回 "pkill_min" が適用されます。
"pkill_max_val"     - プレイヤーに適用される鎮痛効果の上限値。0 は無制限を意味します。
"pkill_chance"      - 鎮痛効果が発生する基本確率。正確な計算は pkill_chance_bot に依存します。
"pkill_chance_bot"  - 発生確率計算の補助値。未設定の場合は確率 = 1 / pkill_chance、存在する場合は計算式に従います。
"pkill_tick"        - 鎮痛効果判定が行われる間隔。

"stim_amount"       - 効果が適用されたときに与えられる刺激/鎮静量。正の値で刺激効果、負の値で鎮静効果になります。
"stim_min"          - 効果によって付与される最小刺激量。
"stim_max"          - 効果によって付与される最大刺激量。0 または未設定の場合、毎回 "stim_min" が適用されます
"stim_min_val"      - プレイヤーに適用される刺激/鎮静効果の下限値。0 は無制限を意味します。
"stim_max_val"      - プレイヤーに適用される刺激/鎮静効果の上限値。0 は無制限を意味します。
"stim_chance"       - 刺激または鎮静効果が発生する基本確率。正確な計算は stim_chance_bot に依存します。
"stim_chance_bot"   - 発生確率計算の補助値。未設定の場合は確率 = 1 / stim_chance、存在する場合は計算式に従います。
"stim_tick"         - 効果判定が行われる間隔。

"health_amount"     - 効果が適用されたときに変化する健康値。正の値で増加、負の値で減少。※半隠しステータスで回復に影響します。
"health_min"        - 効果によって付与/減少する最小健康値。
"health_max"        - 効果によって付与/減少する最大健康値。0 または未設定の場合、毎回 health_min が適用されます。
"health_min_val"    - プレイヤーに適用される健康値の下限値。0 は無制限を意味します。
"health_max_val"    - プレイヤーに適用される健康値の上限値。0 は無制限を意味します。
"health_chance"     - 健康値が変化する基本確率。正確な計算は health_chance_bot に依存。
"health_chance_bot" - 発生確率計算の補助値。未設定の場合は確率 = 1 / health_chance、存在する場合は計算式に従います。
"health_tick"       - 健康値変化判定が行われる間隔。

"h_mod_amount"      - 効果が適用されたときに変化する health_modifier の量。正の値で増加、負の値で減少します。
"h_mod_min"         - 効果によって付与/減少される health_modifier の最小量。
"h_mod_max"         - 効果によって付与/減少される health_modifier の最大量。0 または未設定の場合、h_mod_min と同値になります。
"h_mod_min_val"     - プレイヤーに適用される health_modifier の下限値。0 は下限なし。
"h_mod_max_val"     - プレイヤーに適用される health_modifier の上限値。0 は上限なし。
"h_mod_chance"      - health_modifier が変化する基本確率。正確な計算は h_mod_chance_bot に依存。
"h_mod_chance_bot"  - 発生確率計算の補助値。未設定の場合は確率 = 1 / h_mod_chance、存在する場合は計算式に従います。
"h_mod_tick"        - health_modifier 変化判定が行われる間隔。

"rad_amount"        - 放射線を付与または減少させる量。[50]を超える値は致命的なので注意が必要です。
"rad_min"           - 特定の効果によって付与または減少する放射線の最小量。この値以下にはなりません。
"rad_max"           - 特定の効果によって付与または減少する放射線の最大量。0または値が未設定の場合、"rad_min" と同じ値になります。
"rad_max_val"       - 放射線の上限値。既定値は 0。0 の場合、上限は設定されていない。
"rad_chance"        - 追加で放射線を受ける確率。
"rad_chance_bot"    - 発生確率計算の補助値。未設定の場合は確率 = 1 / rad_chance、存在する場合は計算式に従います。
"rad_tick"          - 放射線ダメージ判定が行われる間隔。

"hunger_amount"     - 効果が適用されたときに変化する空腹値の量。正の値で空腹が増加、負の値で空腹が減少。
"hunger_min"        - 効果によって付与/減少される空腹値の最小量。
"hunger_max"        - 効果によって付与/減少される空腹値の最大量。0 または未設定の場合、"hunger_min" と同値になります。
"hunger_min_val"    - プレイヤーに適用される空腹値の下限値。0 は下限なし。
"hunger_max_val"    - プレイヤーに適用される空腹値の上限値。0 は上限なし。
"hunger_chance"     - 空腹値が変化する基本確率。正確な計算は hunger_chance_bot に依存します。
"hunger_chance_bot" - 発生確率計算の補助値。未設定の場合は確率 = 1 / hunger_chance、存在する場合は計算式に従います。
"hunger_tick"       - 空腹値変化判定が行われる間隔。

"thirst_amount"     - 効果が適用されたときに変化する渇き値の量。正の値で渇きが増加、負の値で減少。
"thirst_min"        - 効果によって付与/減少される渇き値の最小量。
"thirst_max"        - 効果によって付与/減少される渇き値の最大量。0 または未設定の場合、"thirst_min" と同値になります。
"thirst_min_val"    - プレイヤーに適用される渇き値の下限値。0 は下限なし。
"thirst_max_val"    - プレイヤーに適用される渇き値の上限値。0 は上限なし。
"thirst_chance"     - 渇き値が変化する基本確率。正確な計算は thirst_chance_bot に依存します。
"thirst_chance_bot" - 発生確率計算の補助値。未設定の場合は確率 = 1 / thirst_chance、存在する場合は計算式に従います。
"thirst_tick"       - 渇き値変化判定が行われる間隔。

"sleepdebt_amount"     - 効果が適用されたときに増減する睡眠不足の量。正の値で睡眠不足が増加、負の値で減少。
"sleepdebt_min"        - 効果によって付与/減少される睡眠不足の最小量。
"sleepdebt_max"        - 効果によって付与/減少される睡眠不足の最大量。0 または未設定の場合、"sleepdebt_min" と同値になります。
"sleepdebt_min_val"    - プレイヤーに適用される睡眠不足の下限値。0 は下限なし。
"sleepdebt_max_val"    - プレイヤーに適用される睡眠不足の上限値。0 は上限なし。
"sleepdebt_chance"     - 睡眠不足が変化する基本確率。正確な計算は sleepdebt_chance_bot に依存します。
"sleepdebt_chance_bot" - 最小確率の補助値。詳細は不明で、監査が必要です。
"sleepdebt_tick"       - 睡眠不足変化判定が行われる間隔。

"fatigue_amount"    - 効果が適用されたときに変化する疲労値の量。正の値で疲労が増加、負の値で減少します。
※疲労が一定値に達するとキャラクターは睡眠が必要になります。
"fatigue_min"       - 効果によって付与/減少される疲労値の最小量。
"fatigue_max"       - 効果によって付与/減少される疲労値の最大量。0 または未設定の場合、"fatigue_min" と同値になります。
"fatigue_min_val"   - プレイヤーに適用される疲労値の下限値。0 は下限なし。
"fatigue_max_val"   - プレイヤーに適用される疲労値の上限値。0 は上限なし。
"fatigue_chance"    - 疲労値が変化する基本確率。正確な計算は fatigue_chance_bot に依存。
"fatigue_chance_bot"- 発生確率計算の補助値。未設定の場合は確率 = 1 / fatigue_chance、存在する場合は計算式に従います。
"fatigue_tick"      - 疲労値変化判定が行われる間隔。

"stamina_amount"    - 効果が適用されたときに変化するスタミナの量。正の値でスタミナが増加、負の値で減少します。
"stamina_min"       - 効果によって付与/減少されるスタミナの最小量。
"stamina_max"       - 効果によって付与/減少されるスタミナの最大量。0 または未設定の場合、"stamina_min" と同値になります
"stamina_min_val"   - プレイヤーに適用されるスタミナの下限値。0 は下限なし。
"stamina_max_val"   - プレイヤーに適用されるスタミナの上限値。0 は上限なし。
"stamina_chance"    - スタミナ値が変化する基本確率。正確な計算は stamina_chance_bot に依存します。
"stamina_chance_bot"- 発生確率計算の補助値。未設定の場合は確率 = 1 / stamina_chance、存在する場合は計算式に従います。
"stamina_tick"      - スタミナ変化判定が行われる間隔。

"cough_chance"      - 咳が発生する確率。
"cough_chance_bot"  - 咳が発生する確率の補助値。
"cough_tick"        - 咳の発生判定が行われる間隔。

"vomit_chance"      - 嘔吐が発生する確率。
"vomit_chance_bot"  - 嘔吐が発生する確率の補助値。
"vomit_tick"        - 嘔吐の発生判定が行われる間隔。

"healing_rate"      - 1 日あたりの治癒量。
"healing_head"      - 頭部の治癒割合。
"healing_torso"     - 胴部の治癒割合。

"morale"            - 提供される士気値。 単一の数値で指定（耐性や補正は非対応）。

これらはプレイヤーではなく、モンスターにのみ適用されます:

"hit_mod"           - モンスターの近接攻撃スキルを増減する値
"dodge_mod"         - モンスターの回避能力を増減する値
"bash_mod"          - モンスターの基本攻撃および近接特殊攻撃による打撃ダメージを増減する値。0 にするとダメージを与えなくなる。
"cut_mod"           - モンスターの基本攻撃および近接特殊攻撃による切断ダメージを増減する値。0 にするとダメージを与えなくなる。
"size_mod"          - モンスターのサイズを縮小または拡大する値。Tiny 未満、Huge 以上にはできない。
```

各引数は 1 つまたは 2 つの値を取ることもできます。

```json
"thirst_min": [1]
or
"thirst_min": [1, 2]
```

ある効果が“抵抗された”場合(“resist_effect” または “resist_trait” による抵抗)には、2 つ目の値が使用されます。1 つの値しか指定されていない場合は、常にその値が使用されます。

ベース Mod とスケーリング Mod: 強度 (intensity)＝1 の場合、効果には “base_mods” の基本効果だけが適用されます。
しかし、強度が 1 増えるごとに、計算には “scaling_mods” のそれぞれの値が加算されます。
つまり:

```json
Intensity 1 values = base_mods values
Intensity 2 values = base_mods values + scaling_mods values
Intensity 3 values = base_mods values + 2 * scaling_mods values
Intensity 4 values = base_mods values + 3 * scaling_mods values
```

のように続きます。

特例: 唯一の特例は、base_mods の "X_chance_bot" + intensity * scaling_mods の "X_chance_bot" が 0 の場合、
その値を 1 (＝毎回トリガーする)として扱う という点です。

## Example Effect

```json
"type": "effect_type",
"id": "drunk",
"name": [
    "Tipsy",
    "Drunk",
    "Trashed",
    "Wasted"
],
"looks_like": "drunk",
"max_intensity": 4,
"apply_message": "You feel lightheaded.",
"int_dur_factor": 1000,
"miss_messages": [["You feel woozy.", 1]],
"morale": "morale_drunk",
"base_mods": {
    "str_mod": [1],
    "vomit_chance": [-43],
    "sleep_chance": [-1003],
    "sleep_min": [2500],
    "sleep_max": [3500],
    "morale": [ 5 ]
},
"scaling_mods": {
    "str_mod": [-0.67],
    "per_mod": [-1],
    "dex_mod": [-1],
    "int_mod": [-1.42],
    "vomit_chance": [21],
    "sleep_chance": [501],
    "morale": [ 10 ]
}
```

まず、「飲酒(ほろ酔い)」 がプレイヤーに付与されたとき、もしまだ酔っていなければ "頭がふらつく"というメッセージが表示されます。また、効果が続いているあいだは 「頭がふらふらする。」というメッセージが追加されます。
この効果には "int_dur_factor": 1000 が設定されており、
強度は「持続時間 / 1000」を切り上げた値 になります。
さらに "max_intensity": 4 が設定されているため、
強度が上昇しても 最大 4 (＝持続時間が 3000 以上) までしか上がらない。
強度が増えていくと、効果名が変化します。
説明文にはステータス変化だけが表示され、追加の説明が付くことはありません。

強度レベルが上がるにつれて、その効果は次のようになります:

```json
Intensity 1
    +1 STR
    +5 morale
     他の効果なし ("X_chance"がどちらも負の値のため)
Intensity 2
    1 - .67 = .33 =         0 STR (Round towards zero)
    0 - 1 =                 -1 PER
    0 - 1 =                 -1 DEX
    0 -1.42 =               -1 INT
    -43 + 21 =               まだ負なので、嘔吐なし
    -1003 + 501 =            まだ負なので、気絶なし
    5 + 10 =                15 morale
Intensity 3
    1 - 2 * .67 = -.34 =    0 STR (round towards zero)
    0 - 2 * 1 =             -2 PER
    0 - 2 * 1 =             -2 DEX
    0 - 2 * 1.43 =          -2 INT
    -43 + 2 * 21 = -1       まだ負なので、嘔吐なし
    -1003 + 2 * 501 = -1    まだ負なので、気絶なし
    5 + 2 * 10 =            25 morale
Intensity 4
    1 - 3 * .67 = - 1.01 =  -1 STR
    0 - 3 * 1 =             -3 PER
    0 - 3 * 1 =             -3 DEX
    0 - 3 * 1.43 =          -4 INT
    -43 + 3 * 21 = 20       "vomit_chance_bot" が存在しないので、20 分の 1 の確率で嘔吐。"vomit_tick" も存在しないため、毎ターン判定します。
    -1003 + 3 * 501 = 500   "sleep_chance_bot" が存在しないので、500 分の 1 の確率で気絶（持続時間は rng(2500, 3500)）。 "sleep_tick" も存在しないため、毎ターン判定します。
    5 + 3 * 10 =            35 morale
```

### 血液分析時の説明

```json
"blood_analysis_description": "Minor Painkiller"
```

この説明文は、キャラクターが血液分析 (例：Blood Analysis CBM を使用) を行った際に、
このフィールドを持つすべての効果について表示されます。

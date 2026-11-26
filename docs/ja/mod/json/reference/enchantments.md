# エンチャント (`Enchantments`)

エンチャントを使うと、アイテム、バイオニック、突然変異が与える独自の効果を指定できます。

### フィールド (`Fields`)

#### ID (`id`)

(文字列) このエンチャントの一意な識別子。

#### 条件 (`conditions`)

(文字列の配列) エンチャントを有効にする条件。

すべての条件を満たす必要があります。条件がない場合は常に有効です。

ベースゲームの全値については、[こちら](#Basegame-Enchantment-Condition-ID-List)を参照してください。

#### エミッター (`emitter`)

(文字列) このエンチャントが有効な間に作動するエミッターの識別子。デフォルトではエミッターなし。

#### エフェクト (`ench_effects`)

(配列) このエンチャントが有効な間、指定した強度のエフェクトを付与します。

各エントリの構文:

```json
{
  // (必須) エフェクトの識別子
  "effect": "effect_identifier",

  // (必須) 強度。実際には強度を持たないエフェクトでは 1 を指定します。
  "intensity": 2
}
```

#### 攻撃命中時のエフェクト (`hit_you_effect`)

(配列) エンチャントが有効な間、キャラクターがクリーチャーに近接攻撃を行ったときに発動しうる呪文の一覧。

各エントリの構文:

```json
{
  // (必須) 呪文の識別子
  "id": "spell_identifier",

  // true の場合、キャラクターの位置を中心に呪文が発動します。
  // false の場合、攻撃対象のクリーチャーを中心に呪文が発動します。
  // デフォルト: false
  "hit_self": false,

  // 発動確率。X 回に 1 回。
  // デフォルト: 1
  "once_in": 1,

  // プレイヤーの呪文が発動したときのメッセージ。
  // %1$s はプレイヤー名、%2$s はクリーチャー名
  // デフォルト: メッセージなし
  "message": "You pierce %2$s with Magic Piercing!",

  // NPC の呪文が発動したときのメッセージ。
  // %1$s は NPC 名、%2$s はクリーチャー名
  // デフォルト: メッセージなし
  "npc_message": "%1$s pierces %2$s with Magic Piercing!",

  // TODO: 壊れている？
  "min_level": 1,

  // TODO: 壊れている？
  "max_level": 2
}
```

#### 被攻撃時のエフェクト (`hit_me_effect`)

(配列) エンチャントが有効な間、キャラクターがクリーチャーから近接攻撃を受けたときに発動しうる呪文の一覧。

構文は `hit_you_effect` と同じです。

#### 突然変異 (`mutations`)

(配列) エンチャントが有効な間、一時的に付与される突然変異の一覧。

#### 間欠的な発動 (`intermittent_activation`)

(オブジェクト) エンチャントが有効な間にランダムで発生する効果の規則。

構文:

```json
{
  // エンチャントが有効な間、毎ターン実行する判定の一覧。
  "effects": [
    {
      // 平均発動間隔。
      // 毎ターンの正確な発動確率は「X をターン数に換算した値分の 1」です。
      "frequency": "5 minutes",

      // 判定に成功した場合に発動する呪文の一覧。
      "spell_effects": [
        {
          // (必須) 呪文の識別子
          "id": "nasty_random_effect",

          // TODO: 壊れている？
          "min_level": 1,

          // TODO: 壊れている？
          "max_level": 5
          // TODO: ほかのフィールドも読み込まれるようですが、使用されていません
        }
      ]
    }
  ]
}
```

#### 値 (`values`)

(配列) 変更するキャラクターまたはアイテムの各種値の一覧。

各エントリの構文:

```json
{
  // (必須) 変更する値の ID。下記の一覧を参照してください。
  "value": "VALUE_ID_STRING",

  // 加算ボーナス。省略可能な整数で、デフォルトは 0 です。
  // 以下では無視されます:
  // METABOLISM, MANA_REGEN, STAMINA_CAP, STAMINA_REGEN, THIRST, FATIGUE
  "add": 13,

  // 乗算ボーナス。省略可能で、デフォルトは 0 です。
  "multiply": -0.3
}
```

加算ボーナスと乗算ボーナスは別々に、次のように適用されます。

```json
bonus = add + base_value * multiply
```

したがって、`multiply` が -0.8 なら -80%、2.5 なら +250% です。整数値を変更する場合、最終的なボーナスは 0 に近づく方向に丸められます（小数部分を切り捨てます）。

複数のエンチャント（アイテム由来とバイオニック由来など）が同じ値を変更する場合、丸めずに各ボーナスを加算し、その合計を必要に応じて丸めてから基本値に適用します。

キャラクターが同時に持てるエンチャントの数には制限がないため、意図しない挙動を防ぐ目的で、最終的な計算値にはハードコードされた上下限があります。

ベースゲームの全値については、[こちら](#Basegame-Enchantment-Value-ID-List)を参照してください。

#### フラグ (`Flags`)

(配列) `enchantment_flag_id` の値。

ベースゲームの全値については、[こちら](#Basegame-Enchantment-Flag-ID-List)を参照してください。

#### 視界

(配列) `enchantment_vision_id` の値。

定義方法については、[こちら](#Enchantment-Vision)を参照してください。

#### 無効化するエフェクト (`Immune Effects`)

(配列) `effect_type_id` の値。

指定したエフェクトを新たに受けなくなります。ただし、すでに受けているエフェクトは持続します。

#### 無効化するフィールド (`Immune Fields`)

(配列) `field_type_id` の値。

フィールドによる環境効果が適用されなくなります。

#### 仮想アイテム (`Fake Items`)

(配列) `itype_id` の値。

指定したアイテムをクラフト用インベントリに追加します。`USES_BIONIC_POWER` があればバイオニック電力を使用できます。

### 例 (`Examples`)

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
    },
    "flags": ["FOOD_POISON_IMMUNE"],
    "immune_fields": ["fd_fire"],
    "immune_effects": ["poison"],
    "fake_items": ["fake_burrowing"]
    }
  }
]
```

## エンチャント値 (`Enchantment Values`)

```jsonc
{
  "id": "CLIMATE_CONTROL", // エンチャントの ID
  "type": "enchantment_value", // 必須の型
  "can_add": true, // このエンチャント値への加算が作用するか。デフォルト: true
  "can_mult": true, // このエンチャント値への乗算が作用するか。デフォルト: true
  "can_max": false, // この型の最大値を取る処理が作用するか。デフォルト: false
  "suffixes": [ // すべての接尾辞。ここが最も複雑な部分です
    [
      { "suffix": "COOLING", "desc_insert": [ "hot", "" ] }, // `CLIMATE_CONTROL_XXX` として現れます
      { "suffix": "HEATING", "desc_insert": [ "cold", "" ] } // desc_insert は下記の `desc_insert` を上書きします。desc も置換できます
    ],
    [ // 2 組目を定義します。組はいくつでも定義できます
      { "suffix": "TORSO", "replace": { "desc_insert": { "idx": 1, "val": "Torse" } } }, // replace は 0 始まりのインデックスで置換するため、"" を "Torso" に置き換えます
      {
        "suffix": "ARM",
        "replace": { "desc_insert": { "idx": 1, "val": "Arm" } },
        "suffixes": [ // この接尾辞に固有の、さらに後ろに付く接尾辞の一覧
          { "suffix": "L", "replace": { "desc_insert": { "idx": 1, "val": "Left Arm" } } }, // これらの接尾辞にも、さらに入れ子の接尾辞を追加できます
          { "suffix": "R", "replace": { "desc_insert": { "idx": 1, "val": "Right Arm" } } } // `CLIMATE_CONTROL_XXX_ARM_XXX` としてのみ現れ、別の型には適用されません
        ]
      },
    ] // 複数の組に分ける理由は下記を参照してください
  ],
  "desc": "Keeps you comfortable against %1$s temperatures ( %2$s )", // 説明。追加文字列を埋め込む書式指定に対応します
  "desc_insert": [ "all bodyparts", "" ], // 書式に埋め込む追加文字列
  "unsupported_conditions": ["character", "item_and_character"] // この値を使用できない条件を指定します
},
```

### 接尾辞の説明 (`Suffixes Explanation`)

まず、上記のツリーを見てください。2 つのグループがあります。

- `COOLING` と `HEATING`
- `TORSO` と `ARM`（および `ARM` の子）

ここでは順序が重要です。`COOLING` が先に定義されているため、`COOLING` 指定子が必要なら必ず先に置きます。

たとえば、`CLIMATE_CONTROL_COOLING_ARM` は有効ですが、`CLIMATE_CONTROL_ARM_COOLING` は無効です。

上記の JSON から生成されるチェーンは次のとおりです。

- `CLIMATE_CONTROL`
  - `CLIMATE_CONTROL_COOLING`
    - `CLIMATE_CONTROL_COOLING_ARM`
      - `CLIMATE_CONTROL_COOLING_ARM_L` -> `CLIMATE_CONTROL_ARM_L` を参照
      - `CLIMATE_CONTROL_COOLING_ARM_R` -> `CLIMATE_CONTROL_ARM_R` を参照
  - `CLIMATE_CONTROL_HEATING`
    - `CLIMATE_CONTROL_HEATING_ARM`
      - `CLIMATE_CONTROL_HEATING_ARM_L` -> `CLIMATE_CONTROL_ARM_L` を参照
      - `CLIMATE_CONTROL_HEATING_ARM_R` -> `CLIMATE_CONTROL_ARM_R` を参照
  - `CLIMATE_CONTROL_ARM` -> 効果の重複適用を防ぐため、`CLIMATE_CONTROL` は参照しない
    - `CLIMATE_CONTROL_ARM_L`
    - `CLIMATE_CONTROL_ARM_R`

チェーンはいくつでも作成できますが、増やすほど複雑になります。

子エンチャントの数は増え続けるため、この文書ではエンチャント値の完全な一覧ではなく、チェーンのグループだけを記載します。

C++ または Lua からこれらの値を参照するときは、常に最も具体的な値を使用してください。より一般的な階層の値は自動的に補完されます。たとえば、値の取得時には `CLIMATE_CONTROL_ARM_R` や `CLIMATE_CONTROL_ARM` ではなく、`CLIMATE_CONTROL_COOLING_ARM_R` を参照します。

### 標準の接尾辞 (`Standard suffixes`)

<a id="bodyparts"></a>

#### 身体部位 (`Bodyparts`)

すべての身体部位に使われる標準的な接尾辞の組です。

- `HEAD`
- `TORSO`
- `EYES`
- `MOUTH`
- `ARM`
  - `L`
  - `R`
- `LEG`
  - `L`
  - `R`
- `HAND`
  - `L`
  - `R`
- `FOOT`
  - `L`
  - `R`

<a id="skills"></a>

#### スキル (`Skills`)

すべてのスキルレベルに使用されます。

- `BARTER`
- `SPEECH`
- `COMPUTER`
- `FIRSTAID`
- `MECHANICS`
- `TRAPS`
- `DRIVING`
- `SWIMMING`
- `FABRICATION`
- `COOKING`
- `TAILOR`
- `SURVIVAL`
- `ELECTRONICS`
- `ARCHERY`
- `GUN`
- `LAUNCHER`
- `PISTOL`
- `RIFLE`
- `SHOTGUN`
- `SMG`
- `THROW`
- `MELEE`
- `BASHING`
- `CUTTING`
- `DODGE`
- `STABBING`
- `UNARMED`

<a id="damage-types"></a>

#### ダメージ種別 (`Damage Types`)

<a id="Basegame-Enchantment-Value-ID-List"></a>

### ベースゲームのエンチャント値 ID 一覧 (`Basegame Enchantment Value ID List`)

#### キャラクターの値 (`Character values`)

##### STRENGTH

筋力。ここでの `base_value` は基本能力値です。最終値は 0 未満になりません。

##### DEXTERITY

器用。ここでの `base_value` は基本能力値です。最終値は 0 未満になりません。

##### PERCEPTION

感覚。ここでの `base_value` は基本能力値です。最終値は 0 未満になりません。

##### INTELLIGENCE

知性。ここでの `base_value` は基本能力値です。最終値は 0 未満になりません。

##### STRENGTH_PERMANENT

基本筋力に作用します。ここでの `base_value` は基本能力値です。最終値は 0 未満になりません。

##### DEXTERITY_PERMANENT

基本器用に作用します。ここでの `base_value` は基本能力値です。最終値は 0 未満になりません。

##### PERCEPTION_PERMANENET

基本感覚に作用します。ここでの `base_value` は基本能力値です。最終値は 0 未満になりません。

##### INTELLIGENCE_PERMANENT

基本知性に作用します。ここでの `base_value` は基本能力値です。最終値は 0 未満になりません。

##### HEALTH_POINTS

HP。ここでの `base_value` は基本 HP です。最終値は 1 未満になりません。

次の子値があります。

| 組       | 値                                                  | 用途                   |
| -------- | --------------------------------------------------- | ---------------------- |
| 身体部位 | `TORSO`, `HEAD`, `ARM` (`L`, `R`), `LEG` (`L`, `R`) | 作用する特定の身体部位 |

##### SPEED

キャラクターの速度。ここでの `base_value` は、痛み、空腹、重量によるペナルティを含むキャラクター速度です。最終的な速度は基本速度の 25% 未満になりません。

##### ATTACK_COST

近接攻撃コスト。低いほど有利です。ここでの `base_value` は、能力値とスキルによる補正を含む、指定した武器の攻撃コストです。最終値は 25 未満になりません。

##### MOVE_COST

移動コスト。ここでの `base_value` は、衣服や特性による補正を含むタイル移動コストです。最終値は 20 未満になりません。

##### FLAT_MOVE_COST

平地での移動コストに作用します。ここでの `base_value` は処理途中の移動コストです。`MOVE_COST` と同様、最終値は 20 未満になりません。

`MOVE_COST` と重複して適用されます。

##### OBSTACLE_MOVE_COST

障害物上での移動コストに作用します。ここでの `base_value` は初期移動コストです。最終値は 100 未満になりません。

`MOVE_COST` より先に処理され、両方が重複して適用されます。

##### SWIM_MOVE_COST

水泳中の移動コストに作用します。最終値は 30 未満になりません。

`MOVE_COST` とは重複して適用されません。

##### READING_SPEED

本を読む速度。`base_value` は行動ポイント単位の最終読書時間です。最終値は 1 秒未満になりません。

##### CRAFTING_SPEED

製作速度。`base_value` は製作速度の倍率です。ほかのすべての倍率の後に計算されます。

##### CONSTRUCTION_SPEED

建設速度。`base_value` は車両および家具・地形の建設速度の倍率です。ほかのすべての倍率の後に計算されます。

| 組       | 値           | 用途                                       |
| -------- | ------------ | ------------------------------------------ |
| 建設種別 | `CON`, `VEH` | 建設速度と車両建造速度のどちらに作用するか |

##### METABOLISM

代謝率。この補正は `add` フィールドを無視します。ここでの `base_value` は、特性で補正された `PLAYER_HUNGER_RATE` です。最終値は 0 未満になりません。

##### MANA_CAP

マナ容量。ここでの `base_value` は、特性で補正されたキャラクターの基本マナ容量です。最終値は 0 未満になりません。

##### MANA_REGEN

マナ回復率。この補正は `add` フィールドを無視します。ここでの `base_value` は、特性で補正されたキャラクターの基本マナ獲得率です。最終値は 0 未満になりません。

##### STAMINA_CAP

スタミナ容量。この補正は `add` フィールドを無視します。ここでの `base_value` は、特性で補正されたキャラクターの基本スタミナ容量です。最終値は `PLAYER_MAX_STAMINA` の 10% 未満になりません。

##### STAMINA_REGEN

スタミナ回復率。この補正は `add` フィールドを無視します。ここでの `base_value` は、口の動作制限で補正されたキャラクターの基本スタミナ獲得率です。最終値は 0 未満になりません。

##### THIRST

喉の渇きの増加率。ここでの `base_value` はキャラクターの基本的な喉の渇き増加率です。最終値は 0 未満になりません。

##### FATIGUE

疲労の増加率。ここでの `base_value` はキャラクターの基本疲労増加率です。最終値は 0 未満になりません。

##### MENDING_MULT

骨折した手足の治癒率に対する倍率を変更します。`base_value` は突然変異適用後の回復補正（デフォルトは 0.25）です。最終値の範囲は 0.0 から 1.0 です。

##### HEARING

聴力に対する倍率。`base_value` は聴力に対する最終倍率です。最終値は 0 未満になりません。

##### NOISE

足音の大きさ。`base_value` は突然変異適用後の足音倍率です。最終値は 0 未満になりません。

##### SCENT

匂いの値。`base_value` は突然変異適用後の匂いの値です。最終値は 0 未満になりません。

##### STEALTH

隠密補正。値が高いほど隠密性が上がり、低いほど下がります。`base_value` は突然変異適用後の値です。20 から 160 の範囲に制限されます。160 では 60% 発見されやすく、20 では 80% 発見されにくくなります。

##### MOTION_ALARM

接近するクリーチャーを何タイル先から通知するかを表します。`base_value` は常に 0 です。通知範囲について最大値を取る値です。

##### BODYTEMP_X

快適とみなす体温範囲への補正です。対応する値は次のとおりです。

- `BODYTEMP_MIN`: 快適な最低温度
- `BODYTEMP_MAX`: 快適な最高温度

##### BODYTEMP_SLEEP

睡眠中に加算される体温です。`base_value` は突然変異および先に適用されたエンチャントの値です。上限・下限はありません。

##### BODYTEMP_SPEED

`COLDBLOOD4` のキャラクターに対する追加の速度変化です。`base_value` は突然変異の値、または 0 です。現在、上限・下限はありません。

##### SLEEP_PAIN_THRESHOLD

目を覚ますために追加で必要となる痛みです。`base_value` は睡眠時の基本痛覚値です。最小値は 1 です。

##### SLEEP_DB_RESIST

目を覚ますのに必要な、環境音を上回る騒音量への補正です。`base_value` は 20 です。上限・下限はありません。

##### CLIMATE_CONTROL

プレイヤーの体感温度を一定の温度へ近づけます。`base_value` はプレイヤーの現在の体感温度です。突然変異を含む通常体温より低いか高いかに応じて、体感温度を上げるか下げます。

次の 2 組の子値があります。

| 組       | 値                                       | 用途                                 |
| -------- | ---------------------------------------- | ------------------------------------ |
| 温度     | `COOLING`, `HEATING`                     | 理想温度に向けて冷却するか加熱するか |
| 身体部位 | [身体部位の標準接尾辞](#bodyparts)を参照 | 作用する身体部位                     |

それぞれ冷却または加熱だけを行います。

##### LIE

嘘が成功する確率への補正です。`base_value` はスキル効果適用後の値です。0 未満または 100 を超える値にしても、それ以上の変化はありません。

##### PERSUADE

説得に作用する点を除き、`LIE` と同じです。

##### INTIMIDATE

威圧に作用する点を除き、`LIE` と同じです。

##### HEALTHY_MULT

健康度への補正です。`base_value` は 1 です。

##### FALL_DAMAGE_MULT

落下ダメージ倍率への補正です。`base_value` は突然変異やほかの補正を適用した後の値です。0 未満になりません。

##### CARRY_STORAGE

持ち運べる収納容量への補正です。`base_value` は現在の収納容量（ミリリットル）です。0 未満になりません。

##### CARRY_WEIGHT

持ち運べる重量への補正です。`base_value` は現在の収納容量（ミリリットル）です。0 未満になりません。

##### WEIGHTMOD

プレイヤーの現在重量への補正です。`base_value` は、下記カテゴリのうち対応する重量です。最小値は 0 で、最大値に上限はありません。

| 組       | 値                                               | 用途                                 |
| -------- | ------------------------------------------------ | ------------------------------------ |
| カテゴリ | `BIONICS`, `WEAPON`, `INVENTORY`, `BODY`, `WORN` | プレイヤー重量のどの部分に作用するか |

##### OVERMAP_SIGHT

オーバーマップの視界への補正です。`base_value` は突然変異による最も高い値です。最大値は 3 です。

##### EFFECTIVE_FOCUS

集中力への補正です。`base_value` は現在の集中力です。上限・下限はありません。

##### MELEE_HIT

近接攻撃の命中値への補正です。`base_value` はすべての補正を適用した後の値です。上限・下限はありません。

##### OVERKILL

ゾンビの死亡後に加わるダメージへの補正です。`base_value` は現在のオーバーキルダメージです。最小値は 0 です。

##### FOOD_FUN

食事による士気への補正です。`base_value` は現在の食事士気です。上限・下限はありません。

##### VOMIT_MOD

嘔吐する確率への補正です。`base_value` は常に 1 です。最小値は 0 です。

##### PAIN

痛みの増減への補正です。`base_value` は痛みの補正値です。

| 組   | 値             | 用途                     |
| ---- | -------------- | ------------------------ |
| 種別 | `GAIN`, `LOSS` | 増加または減少だけに適用 |

##### PAIN_MOD

現在の痛みに一定量を加える補正です。`base_value` は現在の痛みです。最小値は 0 です。

##### CHRONIC_PAIN_MOD

`CHRONIC_PAIN` オプションによる痛みへの補正です。`base_value` は慢性痛です。`PAIN_MOD` 適用後の値に加算した結果の最小値は 0 です。

##### PERCEIVED_PAIN_MOD

知覚する痛みへの補正です。`base_value` は `PAIN_MOD` 適用後の現在の痛みです。最小値は 0 です。

| 組     | 値                                | 用途                                           |
| ------ | --------------------------------- | ---------------------------------------------- |
| 能力値 | `SPD`, `STR`, `DEX`, `INT`, `PER` | それぞれ速度、筋力、器用、知性、感覚だけに適用 |

##### PAIN_PENALTY

強い痛みによる能力値ペナルティに作用します。`base_value` は現在のペナルティです。最大値は 0（影響なし）です。

##### ADDICTION_STRENGTH

依存強度がさらに 1 段階増える確率への補正です。`base_value` は追加される依存強度です。上限・下限はありません。

##### ADDICTION_TIME_PER_ADDITION

依存の付与によって依存時間がどれだけ増えるかへの補正です。`base_value` は、依存が付与されるたびに追加される基本時間（秒）です。上限・下限はありません。

##### ADDICTION_TIME_PER_INTENSITY

依存が続く時間への補正です。`base_value` は依存の段階を 1 つ取り除くまでの時間です。値を減らすと依存時間が増え、値を増やすと依存時間が減ります。上限・下限はありません。

##### UNCANNY_DODGE

銃弾を回避する 0 から 1 の確率です。`base_value` は 0 です。超常回避 1 回につきスタミナを 100 消費します。0 未満または 1 を超える値は、それぞれ 0 または 1 と同じ効果になります。

##### BONUS_DODGE

回避ペナルティが発生するまでに、1 ターン中に行える追加回避回数です。ここでの `base_value` は、ペナルティ発生前に行えるキャラクターの基本回避回数（通常は 1）です。最終値が 0 未満になる場合があり、その場合は回避判定にペナルティがかかります。

##### FORCEFIELD

指定したダメージ種別のダメージを防ぐ 0 から 1 の確率です。`base_value` は 0 です。

| 組           | 値                                              | 用途                 |
| ------------ | ----------------------------------------------- | -------------------- |
| ダメージ種別 | [ダメージ種別の標準接尾辞](#damage-types)を参照 | 作用するダメージ種別 |

##### CROWD_CRUSH_RESIST

群衆圧による圧死への抵抗しやすさへの補正です。`base_value` は常に 5 です。値を増やすと、群衆に押し潰される確率が下がります。範囲は 0（抵抗する確率なし）から 95（抵抗できない確率 5%）です。

##### BLISTER_COUNT

水ぶくれエフェクトを受ける際の実効的な耐熱防具値への補正です。`base_value` は水ぶくれの数です。最終値は 0 未満にでき、その場合キャラクターに水ぶくれは発生しません。反対に値を高くして、常に水ぶくれが発生するようにもできます。

##### LUMINATION

有効な間、プレイヤーの周囲を照らす明るさです。このエンチャントでは加算も乗算もできず、最大値だけが作用します。最終値は 0 未満にならず、最大値に上限はありません。

##### NIGHT_VISION

プレイヤーの暗視値です。`EFFECT_NIGHT_VISION` と `GNV_EFFECT` は 10.0、`GNVE_EFFECT` は 18.0 です。`max` だけが作用し、エンチャントとほかの暗視効果のうち最も高い値を取ります。

##### CLAIRVOYANCE

プレイヤーの透視値です。`CLAIRVOYANCE_SUPER` は 40.0、`CLAIRVOYANCE_PLUS` は 8.0、`CLAIRVOYANCE` は 3 です。`max` だけが作用し、エンチャントとほかの透視効果のうち最も高い値を取ります。

##### FLASH_PROTECTION

プレイヤーの閃光防護値です。アイテムとエフェクトのフラグは 3 を与えます。`max` だけが作用し、エンチャント、アイテム、エフェクトのうち最も高い値を取ります。

##### GROUNDED_CREATURE_SIGHT

地上にいるクリーチャーを、壁越しに赤外線のように視認する視界です。作用する距離をタイル数で指定します。`max` だけが作用します。

##### REACH_RANGE

素手・武器を問わず、すべての近接攻撃にリーチを加える値です。`base_value` は常に 0 です。`max` だけが作用します。

| 組   | 値                 | 用途                             |
| ---- | ------------------ | -------------------------------- |
| 種別 | `UNARMED`, `ARMED` | 素手攻撃または武器攻撃だけに適用 |

##### ARMOR_X

受けるダメージへの補正です。アクティブ・ディフェンス・システムのバイオニック適用後、アイテムにダメージが吸収される前に適用されます。ここでの `base_value` は対応する種別の受けるダメージなので、正の `add` や 1 より大きい `mul` は、キャラクターが受けるダメージを**増加**させます。

| 組           | 値                                              | 用途                 |
| ------------ | ----------------------------------------------- | -------------------- |
| ダメージ種別 | [ダメージ種別の標準接尾辞](#damage-types)を参照 | 作用するダメージ種別 |

##### SKILL_LEVEL

キャラクター全体のスキルレベルへの補正です。`base_value` はプレイヤーの現在のスキルレベルです。

| 組         | 値                                  | 用途           |
| ---------- | ----------------------------------- | -------------- |
| スキル種別 | [スキルの標準接尾辞](#skills)を参照 | 作用するスキル |

##### SKILL_EXP

キャラクター全体のスキル経験値獲得量への補正です。`base_value` は行動によって得られる経験値です。

警告: この値は乗算だけが可能で、加算はできません。

| 組         | 値                                  | 用途           |
| ---------- | ----------------------------------- | -------------- |
| スキル種別 | [スキルの標準接尾辞](#skills)を参照 | 作用するスキル |

##### 動作制限 (`Encumbrance`)

キャラクター全体の動作制限への補正です。子値では特定の身体部位だけを変更します。

| 組       | 値                                       | 用途             |
| -------- | ---------------------------------------- | ---------------- |
| 身体部位 | [身体部位の標準接尾辞](#bodyparts)を参照 | 作用する身体部位 |

##### MELEE_DAMAGE

キャラクター全体の近接ダメージへの補正です（リーチ攻撃を含む）。子値では特定のダメージ種別だけを変更します。

| 組           | 値                                              | 用途                 |
| ------------ | ----------------------------------------------- | -------------------- |
| ダメージ種別 | [ダメージ種別の標準接尾辞](#damage-types)を参照 | 作用するダメージ種別 |

##### MELEE_ARMOR_PENETRATION

キャラクター全体の近接防具貫通への補正です。子値では特定のダメージ種別を変更します。

| 組           | 値                                              | 用途                 |
| ------------ | ----------------------------------------------- | -------------------- |
| ダメージ種別 | [ダメージ種別の標準接尾辞](#damage-types)を参照 | 作用するダメージ種別 |

#### アイテムの値 (`Item values`)

##### ITEM_ATTACK_COST

このアイテムによる攻撃（近接または投擲）のコストです。条件や位置を無視し、常に有効です。ここでの `base_value` はアイテムの基本攻撃コストです。最終値は 0 未満になりません。

##### ITEM_DAMAGE_X

このアイテムの近接ダメージです。条件や位置を無視し、常に有効です。ここでの `base_value` は対応する種別の基本アイテムダメージです。最終値は 0 未満になりません。

対応するダメージ種別に加えて、全種別に作用するダメージ補正 `ITEM_DAMAGE` があります。

| 組           | 値                                              | 用途                 |
| ------------ | ----------------------------------------------- | -------------------- |
| ダメージ種別 | [ダメージ種別の標準接尾辞](#damage-types)を参照 | 作用するダメージ種別 |

##### ITEM_ARMOR_PENETRATION_X

このアイテムの防具貫通です。ここでの `base_value` は対応する種別の基本防具貫通です。最終値は 0 未満になりません。

対応するダメージ種別に加えて、全種別に作用する補正 `ITEM_ARMOR_PENTRATION` があります。

| 組           | 値                                              | 用途                 |
| ------------ | ----------------------------------------------- | -------------------- |
| ダメージ種別 | [ダメージ種別の標準接尾辞](#damage-types)を参照 | 作用するダメージ種別 |

##### ITEM_ARMOR_X

このアイテムが受けるダメージへの補正で、アイテムがダメージを吸収する前に適用されます。ここでの `base_value` は対応する種別の受けるダメージなので、正の `add` や 1 より大きい `mul` は、キャラクターが受けるダメージを**増加**させます。全種別に作用する `ITEM_ARMOR` に加えて、ダメージ種別ごとに固有のエンチャント値があります。

| 組           | 値                                              | 用途                 |
| ------------ | ----------------------------------------------- | -------------------- |
| ダメージ種別 | [ダメージ種別の標準接尾辞](#damage-types)を参照 | 作用するダメージ種別 |

##### ITEM_REACH_RANGE

アイテムのリーチ範囲への補正です。この値とアイテムフラグの値のうち最大の値を取ります。

## エンチャントフラグ (`Enchantment Flag`)

```jsonc
{
  "id": "NEARSIGHTED",               // エンチャントフラグの ID
  "type": "enchantment_flag",        // 必須の型
  "parents": [ "BLIND" ],            // 同時に付与するほかの enchantment_flag の配列
  "conflicts": [ "FIX_NEARSIGHTED" ] // 相殺するほかの enchantment_flag の配列
  "info": "<bad>Causes nearsightedness</bad>" // エンチャント情報に表示する文字列
},
```

記載した効果はすべて、エンチャントを付与するものを所持しているキャラクターに適用されます。

<a id="Basegame-Enchantment-Flag-ID-List"></a>

### ベースゲームのエンチャントフラグ ID 一覧 (`Basegame Enchantment Flag ID List`)

#### 視界 (`Sight`)

##### UNDERWATER_SIGHT

水中でも視界が妨げられなくなります。

##### SLEEP_SIGHT

睡眠中でも視認できるようになります。

##### NEARSIGHTED

視界を大きく制限します。一部の眼鏡で解消できます。

##### FIX_NEARSIGHTED

`NEARSIGHTED` と競合し、その効果を治療して取り除きます。

##### BLIND

どのタイルも見えなくなります。ただし、壁にぶつかるとその壁は判明します。

##### FIX_BLIND

`BLIND` と競合し、その効果を治療して取り除きます。

##### INFRARED_VISION

赤外線視界を得ます。

##### ELECTROSENSE

ロボットや電気を帯びたクリーチャーを壁越しに視認できます。

##### SONAR

穴を掘って進むクリーチャーを `INFRARED_VISION` のスプライトで視認できます。

##### ANTIGLARE

日光などによる眩惑効果を防ぎます。

#### 飲食 (`Consumption`)

##### EAT_ROTTEN

腐った食べ物を安全に食べられるようになります。

##### ONLY_EAT_ROTTEN

新鮮な食べ物を食べると大きなペナルティを受けますが、新鮮な液体は引き続き飲めます。

##### EAT_ROTTEN_MORALE

腐った食べ物を食べても士気ペナルティを受けません。

##### CONSUME_UNCLEAN

不衛生な液体を飲み、不衛生な食べ物を食べられるようになります。

##### FOOD_PARASITE_IMMUNE

食物の摂取による寄生虫への感染を防ぎます。

##### FOOD_POISON_IMMUNE

食物の摂取による食中毒を防ぎます。

#### その他 (`Miscellaneous`)

##### ALARMCLOCK

睡眠中のアラームを設定できるようになります。

##### INTENAL_ALARMCLOCK

`ALARMCLOCK` と同じ効果を持ちますが、音を出しません。また、アラームに気づかず眠り続けることも防ぐはずです。

##### VIEW_DRONE_CAM

`effect_drone_marker` を持つすべてのクリーチャーを視認できます。通常、このエフェクトは `PHOTOGRAPH` ロボットによって付与されます。

##### RADIO

無線機を所持している場合と同じ効果を与えます。

##### THERMOMETER

温度計を所持している場合と同じ効果を与えます。

##### WATCH

正確な時刻を確認できるようになります。

##### FIRE_FIELD_IMMUNE

火炎フィールドへの耐性を与えます。

##### SILENT

プレイヤーが移動時に音を立てなくなります。

##### NO_THERMAL_WAKE

極端な温度でもプレイヤーが目を覚まさなくなります。

##### NO_DAMAGE_WAKE

ダメージを受けてもプレイヤーが目を覚まさなくなります。

##### NO_LIGHT_WAKE

光を受けてもプレイヤーが目を覚まさなくなります。

## エンチャント条件 (`Enchantment Condition`)

```jsonc
{
  "id": "WORN", // 条件の ID
  "type": "enchantment_condition", // 必須の型
  "condition_type": "item_and_character", // 条件の種類。`global`、`item`、`character`、`item_and_character` を指定できます
  "condition_function": "worn", // 使用する関数。通常はハードコードされた関数または Lua 関数を参照します
  "condition_info": "While worn", // アイテムのエンチャント情報に表示する条件の説明
}
```

<a id="Basegame-Enchantment-Condition-ID-List"></a>

### ベースゲームのエンチャント条件 ID 一覧 (`Basegame Enchantment Condition ID List`)

#### アイテムとキャラクター (`Item and Character`)

##### HELD

インベントリ内にあるとき。

##### WIELD

手に持っているとき。

##### WORN

防具として着用しているとき。

#### グローバル (`Global`)

##### ALWAYS

常に有効です（廃止済みですが対応しています。常に真となるため、条件そのものが不要です）。

##### NIGHT

夜の間。

##### DUSK

夕暮れの間。

##### DAY

昼の間。

##### DAWN

夜明けの間。

##### ACTIVE

アイテム、突然変異、バイオニックなど、エンチャントが付与されているものが有効な間。

##### INACTIVE

`ACTIVE` の反対です。

#### キャラクター (`Character`)

##### INSIDE

アイテムの所有者が屋内にいるとき。

##### OUTSIDE

アイテムの所有者が屋外にいるとき。

##### UNDERGROUND

アイテムの所有者が Z レベル 0 より下にいるとき。

##### ABOVEGROUND

アイテムの所有者が Z レベル 0 以上にいるとき。

##### UNDERWATER

所有者が泳げる地形にいるとき。

<a id="Enchantment-Vision"></a>

## エンチャント視界 (`Enchantment Vision`)

以下はエンチャント視界の定義です。表示するには、すべての条件を満たす必要があります。

```jsonc
{
  "id": "ELECTROSENSE",                          // エンチャントフラグの ID
  "type": "enchantment_vision",                  // 必須の型
  "desc": "Allows Sight Of Electric Creatures",  // エンチャント情報に表示する文字列
  "distance": 5,                                 // 作用する最大距離（タイル数）
  "same_z_level": false,                         // 異なる Z レベルにも表示する
  "require_los": true,                           // 視線が通っていることを必須とする
  "detect_heat": true,                           // `is_warm` を必須とする
  "show_with_species": [ "ROBOT" ],              // 種族の一覧。いずれかが真なら条件を満たす
  "show_with_flag": [ "ELECTRIC" ],              // モンスターフラグの一覧。いずれかが真なら条件を満たす
  "show_without_any_flag": [ "FLIES" ],          // モンスターフラグの一覧。いずれかが真なら条件を満たさない
  "show_with_effect": [ "drone_marker" ],        // エフェクトの一覧。いずれかが真なら条件を満たす
  "show_without_any_effect": [ "drone_marker" ], // エフェクトの一覧。いずれかが真なら条件を満たさない
  "show_normal": true,                           // モンスターを通常表示する。これか vision_desc のいずれかが必須
  "vision_desc": {                               // 配列またはオブジェクトを指定できます
    "tile_id": "infrared_creature",              // 表示するタイル
    "description": "You sense electricity."      // サイズを問わず、調べたときに表示するメッセージ
  },
  "vision_desc": [ // `TINY`、`SMALL`、`MEDIUM`、`LARGE`、`HUGE` のそれぞれに 1 つずつ定義する必要があります
    {
      "tile_id": "infrared_creature",                     // タイル ID
      "description": "You sense tiny bits of electricity" // 調べたときに表示するメッセージ
      "size": "TINY"                                      // クリーチャーのサイズ列挙値
    }
  ]
},
```

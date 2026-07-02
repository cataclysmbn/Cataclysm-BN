# 天気の種類

気象タイプは、それが発生する可能性のある条件 (温度、湿度、気圧、
風力、時刻など）、それがゲームの世界と現実のバブルにどのような影響を与えるか。

天気の種類を選択すると、ゲームは地域設定で定義されたリストを調べて、
現在の条件で適格とみなされる最後のエントリ。どのエントリも存在しない場合は、
対象となる、無効な気象タイプ `"none"` が使用されます。

### フィールド

| 識別子         | 説明                                                                                                                              |
| -------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| id             | (_必須_) 一意の ID。連続した 1 つの単語である必要があり、必要に応じてアンダースコアを使用します。                                 |
| name           | (_必須_) ゲーム内名が表示されます。                                                                                               |
| glyph          | (_必須_) オーバーマップで使用されるグリフ。                                                                                       |
| color          | (_任意_) ゲーム内の名前の色。                                                                                                     |
| map_color      | (_任意_) オーバーマップ グリフの色。                                                                                              |
| ranged_penalty | (_必須_) 遠距離攻撃に対するペナルティ。                                                                                           |
| sight_penalty  | (_必須_) 視覚ペナルティ、別名タイルの透明度の乗数。                                                                               |
| light_modifier | (_必須_) 環境光に対する一律のボーナス。                                                                                           |
| sound_attn     | (_必須_) 音の減衰 (音量に対するフラットな減少)。                                                                                  |
| dangerous      | (_必須_) true の場合、アクティビティの中断を要求します。                                                                          |
| precip         | (_必須_) 関連する降水量。有効な値は、`none`、`very_light`、`light`、および `heavy` です。                                         |
| rains          | (_必須_) 当該降水量が雨として降るかどうか。                                                                                       |
| acidic         | (_任意_) 当該沈殿が酸性であるかどうか。                                                                                           |
| sound_category | (_オプション_) 再生する効果音。有効な値は、`silent`、`drizzle`、`rainy`、`thunder`、`flurries`、`snowstorm`、および `snow` です。 |
| sun_intensity  | (_必須_) 太陽光の強さ。有効な値は、`none`、`light`、`normal`、および `high` です。通常と高は "direct sunlight" とみなされます。   |
| animation      | (_オプション_) 現実バブル内の天気アニメーション。 [詳細](#weather_animation)                                                      |
| effects        | (_任意_) 天候が引き起こす影響の `[string, int]` ペア配列。 [詳細](#effects)                                                       |
| requirements   | (_オプション_) この気象タイプが選択される条件。 [詳細](#requirements)                                                             |

<a id="weather_animation"></a>

### アニメーション

メンバー全員が必須です。

| 識別子 | 説明                                        |
| ------ | ------------------------------------------- |
| factor | 表示濃度: 0 はなし、1 は画面を消します。    |
| glyph  | ASCII モードで使用するグリフ。              |
| color  | グリフの色。                                |
| tile   | TILES モードで使用するグラフィカル タイル。 |

<a id="effects"></a>

### 効果

#### ユニバーサルフィールド

| 識別子    | 説明                                                                                       |
| --------- | ------------------------------------------------------------------------------------------ |
| name      | (_必須_) 以下にリストされている効果の 1 つにより、ロードされた json が完全に変更されます。 |
| intensity | (_必須_) 与える効果の強さ                                                                  |

##### 名前

- `morale`: プレイヤーに士気を与えます。追加の定義済みフィールドが必要です
- `effect`: プレイヤーに効果を与えます。追加の定義済みフィールドが必要です
- `wet`: プレイヤーを濡れさせます。量は強度です。
- `thunder`: 雷は強度 1 の確率で発生します。
- `lightning`: 雷は強度 1 の確率で発生します。

#### 士気

| 識別子               | 説明                                                                                           |
| -------------------- | ---------------------------------------------------------------------------------------------- |
| intensity            | (_必須_) この効果は x ターンごとに適用されます                                                 |
| bonus                | (_必須_) 各スタックが与える士気の量                                                            |
| max_bonus            | (_必須_) 得られる士気の最大量                                                                  |
| duration             | (_必須_) 士気効果の持続時間                                                                    |
| decay_start          | (_必須_) 効果の強度が弱まり始めるまでの時間。合計の継続時間はこれに継続時間を加算します。      |
| morale_id_str        | (_必須_) 適用される士気効果の ID                                                               |
| morale_msg           | (_必須_) 士気を獲得するときに与えるメッセージ                                                  |
| morale_msg_frequency | (_必須_) 効果を得るときにメッセージを表示する確率                                              |
| message_type         | (_必須_) メッセージのタイプ。 0 == 良い、1 == 悪い、2 == 混合、3 == 警告、4 == 情報、5 == 中立 |

##### 例:

```json
{
  "name": "morale",
  "intensity": 3,
  "bonus": 2,
  "bonus_max": 60,
  "duration": "180 s",
  "decay_start": "60 s",
  "morale_id_str": "morale_weather_rainbow",
  "morale_msg": "You stare in awe at the rainbow.",
  "morale_msg_frequency": 8,
  "message_type": 0
}
```

#### 効果

| 識別子                       | 説明                                                                                                                            |
| ---------------------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| intensity                    | (_必須_) この効果は x ターンごとに適用されます                                                                                  |
| duration                     | (_必須_) 効果の持続時間                                                                                                         |
| effect_id_str                | (_必須_) 与えるエフェクト ID の文字列                                                                                           |
| effect_intensity             | (_必須_) 与える効果の強さ                                                                                                       |
| bodypart_string              | (_任意_) エフェクトを適用するボディパーツ、デフォルトでは全身                                                                   |
| effect_msg                   | (_必須_) 効果を得る際に与えるメッセージ                                                                                         |
| effect_msg_frequency         | (_必須_) 効果を得るときにメッセージを表示する確率                                                                               |
| effect_msg_blocked_frequency | (_必須_) 効果を防止する際に保護メッセージを表示する確率                                                                         |
| message_type                 | (_必須_) メッセージのタイプ。 0 == 良い、1 == 悪い、2 == 混合、3 == 警告、4 == 情報、5 == 中立                                  |
| precipitation_name           | (_必須_) 文字列に表示する名前「あなたの傘は `precipitation_name` からあなたを守ります」                                         |
| protection_data              | (_任意_) 効果から保護される `check` フラグまたは特性 (常に適用される `DEFAULT` とともに) および `odds` (x の確率で 1 つ) の配列 |

##### 例:

```json
{
  "name": "effect",
  "intensity": 3,
  "duration": "30 s",
  "effect_id_str": "emp",
  "effect_intensity": 0,
  "precipitation_name": "brain waves",
  "bodypart_string": "head",
  "effect_msg": "You feel an odd wave-like sensation pass through your head.",
  "effect_msg_frequency": 16,
  "effect_msg_blocked_frequency": 32,
  "message_type": 2,
  "protection_data": [
    { "check": "RAIN_PROTECT", "odds": 1 },
    { "check": "ACIDPROOF", "odds": 1 },
    { "check": "DEFAULT", "odds": 2 }
  ]
}
```

<a id="requirements"></a>

### 要件

すべてのメンバーはオプションです。

| 識別子                | 説明                                                                                                                                                                                                         |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| pressure_min          | 最低圧力                                                                                                                                                                                                     |
| pressure_max          | 最大圧力                                                                                                                                                                                                     |
| humidity_min          | 最低湿度                                                                                                                                                                                                     |
| humidity_max          | 最大湿度                                                                                                                                                                                                     |
| temperature_min       | 最低温度                                                                                                                                                                                                     |
| temperature_max       | 最高温度                                                                                                                                                                                                     |
| windpower_min         | 最小風力                                                                                                                                                                                                     |
| windpower_max         | 最大風力                                                                                                                                                                                                     |
| humidity_and_pressure | 圧力と湿度の要件がある場合、両方が必要ですか、それとも一方だけが必要ですか                                                                                                                                   |
| acidic                | これには酸性沈殿が必要ですか?                                                                                                                                                                                |
| time                  | 時刻。有効な値は、昼、夜、およびその両方です。                                                                                                                                                               |
| required_weathers     | 条件が指定されたタイプのいずれかに一致する場合にのみ選択されます。つまり、雨は、雲、小霧雨、または霧雨の条件が存在する場合にのみ発生します。必要な天気は、地域の天気リストの "before" である必要があります。 |

### 例

```json
{
  "id": "lightning",
  "type": "weather_type",
  "name": "Lightning Storm",
  "color": "c_yellow",
  "map_color": "h_yellow",
  "glyph": "%",
  "ranged_penalty": 4,
  "sight_penalty": 1.25,
  "light_modifier": -45,
  "sound_attn": 8,
  "dangerous": false,
  "precip": "heavy",
  "rains": true,
  "acidic": false,
  "effects": [{ "name": "thunder", "intensity": 50 }, { "name": "lightning", "intensity": 600 }],
  "tiles_animation": "weather_rain_drop",
  "weather_animation": { "factor": 0.04, "color": "c_light_blue", "glyph": "," },
  "sound_category": "thunder",
  "sun_intensity": "none",
  "requirements": { "pressure_max": 990, "required_weathers": ["thunder"] }
}
```

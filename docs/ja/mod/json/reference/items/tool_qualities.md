### ツール品質

> [!NOTE]
>
> この記事は新しいため、例やリンクがさらに追加される可能性があります。

ツール品質は、レシピなどのシステムで使用する名前付きの機能層を定義します。`data/json/tool_qualities.json` から読み込まれます。

```json
{
  "type": "tool_quality", // ツール品質の定義
  "id": "SEW", // 品質の一意な ID
  "name": { "str": "sewing" }, // 品質の表示名
  "crafting_speed_bonus_per_level": 1.1, // 任意。レベルごとの乗数（下記参照）
  "crafting_speed_level_offset": 2 // 任意。レベルごとのオフセット（下記参照）
}
```

#### フィールド

- `id`: 一意の品質 ID。
- `name`: 品質の表示名。
- `crafting_speed_bonus_per_level`: 任意（デフォルト = 0.0）。レシピの要件を超えた品質レベル 1 つごとに適用する製作速度の乗数（例: 追加レベルごとに 10% 増加する `1.1`）。アイテム自身が `crafting_speed_modifier` を定義していない場合だけ適用されます。
- `crafting_speed_level_offset`: 任意（デフォルト = 0）。レシピがより低いレベルを要求していても、`crafting_speed_bonus_per_level` の適用を開始する最低品質レベルです。

#### クラフトでの使用

ツール品質は、同じ機能を持つ複数のアイテムをレシピ内で一つにまとめるために使用します。`qualities` 配列に品質を記載したアイテムは、レシピの `qualities` 要件を満たすことができます。要求されたレベルを上回ることも可能です。これにより、使用可能な工具をすべて明示的に列挙する必要がなくなります。

レシピ要件の例:

```json
"qualities": [ { "id": "SEW", "level": 2 } ]
```

アイテムの例:

```json
"qualities": [ [ "SEW", 2 ] ]   // 基本的な裁縫道具
"qualities": [ [ "SEW", 4 ] ]   // より高品質な仕立てキット
```

レシピが `SEW` 2 を必要とする場合、`SEW` 2 以上のアイテムを使用できます。品質が `crafting_speed_bonus_per_level` を定義していれば、要求を上回るレベルによって製作速度も上がります。ただし、アイテム自身が `crafting_speed_modifier` を指定している場合を除きます。`crafting_speed_level_offset` を設定すると、速度ボーナスはレシピレベルとオフセットのうち高い方から始まります。

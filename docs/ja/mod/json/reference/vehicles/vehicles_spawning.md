# 車両の生成

車両プロトタイプは標準車両を生成するために使用します。生成後の車両は別の形式で保存されます。

車両プロトタイプは現在 `copy-from` を受け付けません。

## 車両プロトタイプ

```json
"type": "vehicle",
"id": "sample_vehicle",                    // 一意な ID。連続した1語でなければならず、必要ならアンダースコアを使用
"name": "Sample Vehicle",                  // ゲーム内に表示される名前
"blueprint": [                             // 車両のプレビュー。説明として使用可能
    "o#o",                                 // パレットと組み合わせて部品を定義することも可能
    "o#o"                                  // パレットを使った部分的な定義も可能
],
"blueprint_origin": { "x": 10, "y": 3 },   // パレットがオフセットを特定するために使用
"palette": {                               // ブループリントがタイルの部品を設定するためのパレット
  "O": [ "airship_balloon_external" ]      // 配列でなければならず、単一の文字列は不可
  "J": [ { "part": "tank", "fuel": "gasoline" }, "battery_car" ], // すべての部品タイプに対応
},
"color_palette": "car_standard",           // この車両が使用する車両カラーパレット
"parts": [                                 // 部品リスト
    { "x": 0, "y": 0, "part": "frame" },   // 部品定義。正の x 方向は上、正の y 方向は右
    { "x": 0, "y": 0, "part": "seat" },    // 部品 ID は vehicle_parts.json を参照
    { "x": 0, "y": 0, "part": "controls"},
    { "x": 0, "y": 1, "parts: [ "frame", "seat" ] }, // 同じ場所の部品配列
    { "x": 0, "y": 1, "parts: [ { "part": "tank", "fuel": "gasoline" }, "battery_car" },
    { "x": 0, "y": 1, "part": "stereo" },  // 同じ場所で parts 配列と part を混在可能
    { "x": 1, "y": 0, "parts: [ "frame, "wheel" ] },
    { "x": 1, "y": 1, "parts: [ "frame, "wheel" ] },
    { "x": -1, "y": 0, "parts: [ "frame, "wheel" ] },
    { "x": -1, "y": 1, "parts: [ "frame, "wheel" ] }
],
"items": [                                 // アイテム生成リスト
    { "x": 0, "y": 0, "items": "helmet_army" },   // 個別アイテム
    { "x": 0, "y": 0, "item_groups": "army_uniform" }, // item_group のアイテム
    { "x": 0, "y": 1, "items": [ "matchbook", "two_by_four" ] }, // 配列内のすべてのアイテムを生成
    { "x": 0, "y": 0, "item_groups": [ "army_uniform", "rare_guns" ] } // すべての item_group を処理
]
```

**重要:** 車両部品は、ゲーム内で取り付ける順序（フレームとマウントポイントが先）で定義する必要があります。取り付けの通常ルールにも従う必要があり、重ねられない部品フラグを重ねることはできません。

### 部品リスト

部品リストには任意の数の行を指定できます。各行は次の形式です: `{ "x": X, "y": Y, "part": PARTID, ... }` または `{ "x": X, "y": Y, "parts": [ PARTID1, ... ] }`。

最初の形式は、車両部品タイプ PARTID の単一部品を X,Y の位置に定義します。適切な値を指定した任意フィールド `ammo`、`ammo_types`、`ammo_qty`、`fuel` を追加できます。

2つ目の形式は、X,Y の位置に複数の部品を定義します。各部品は PARTID 文字列、または上記の任意フィールドを持つ `{ "part": PARTID, ... }` オブジェクトです。

異なる行で同じ X,Y 座標を指定でき、その場所に追加の部品を追加します。部品は正しい順序で追加する必要があります。たとえばホイールハブは、フレームの後でホイールの前に追加します。

### アイテムリスト

アイテムリストには任意の数の行を指定できます。各行は `{ "x": X, "y": Y, TYPE: DATA }` 形式で、その場所に生成できるアイテムを表します。TYPE と DATA は次のいずれかです。

```json
"items": "itemid"                              // そのタイプの単一アイテム
"items": [ "itemid1", "itemid2", ... ]         // 配列内のすべてのアイテム
"item_groups": "groupid"                       // コレクションか分配かに応じて、グループから1つ以上のアイテム
"item_groups": [ "groupid1", "groupid2" ... ]  // 各グループから1つ以上のアイテム
```

任意のキーワード `chance` は、特定のアイテム定義が生成される確率を 100 分の X で指定します。

複数のアイテム行で同じ X,Y 値を使用できます。

### 車両グループ

```json
"id":"city_parked",            // 一意な ID。連続した1語でなければならず、必要ならアンダースコアを使用
"vehicles":[                 // 生成候補の車両 ID。生成確率は X/T（X は車両の値、T はグループ内の全値の合計）
  ["suv", 600],
  ["pickup", 400],
  ["car", 4700],
  ["road_roller", 300]
]
```

### 車両配置

```json
"id":"road_straight_wrecks",  // 一意な ID
"locations":[ {               // 車両位置の候補。使用時に1つがランダムに選ばれる
  "x" : [0,19],               // x 位置。単一値または候補範囲
  "y" : 8,                    // y 位置。単一値または候補範囲
  "facing" : [90,270]         // 車両の向き。単一値または候補値の配列
} ]
```

### 車両生成

```json
"id":"default_city",            // 一意な ID
"spawn_types":[ {       // 生成タイプのリスト。重みに基づいて1つがランダムに選択される
  "description" : "Clear section of road",           // 生成タイプの説明
  "weight" : 33,          // この生成タイプが使われる確率
  "vehicle_function" : "jack-knifed_semi", // 組み込み JSON 関数を使う場合だけ必要
  "vehicle_json" : {      // JSON で指定する生成タイプの場合だけ必要
  "vehicle" : "car",      // 生成する車両または vehicle_group
  "placement" : "%t_parked",  // 生成時に使用する vehicle_placement。x,y,facing を指定した場合は不要
  "x" : [0,19],     // x 位置。placement 指定時は不要
  "y" : 8,   // y 位置。placement 指定時は不要
  "facing" : [90,270], // 車両の向き。placement 指定時は不要
  "number" : 1, // 生成する車両数
  "fuel" : -1, // 新しい車両の燃料
  "status" : 1  // 新しい車両の状態
} } ]
```

## 車両カラーパレット

```json
"type": "vehicle_color_palette",
"id": "car_standard",                                      // 一意な ID
"palette": [
  {
    "fuzzy_ids": [ "board", "windshield", "door", "roo" ], // 下で選択した色を着色する、あいまい一致 ID のリスト
    "colors": [
      { "color": "White aluminium", "weight": 8 },         // 色と重み。名前付き色
      { "color": "#622625", "weight": 8 },                 // 16進数コードも可能
      { "color": "Grey", "weight": 8 },                    // あいまい一致する名前付き色も可能
    ]
  }
]
```

## 車両ブラックリスト

```jsonc
{
  "type": "vehicle_blacklist", // 必須の type
  "vehicles": [ // ブラックリストに入れる車両プロトタイプ ID
    "car", // デバッグ生成などは可能
    "4x4_car", // vehicle group からの生成だけを禁止
    "beetle",
  ],
}
```

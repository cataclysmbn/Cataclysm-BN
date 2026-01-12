# JSONによる屋根の追加ガイド

建物にJSONの屋根を追加するには、 マップ生成処理中に屋根と建物を連携させるために、いくつかのファイルを使用する必要があります。

編集対象のファイル:

`data/json/mapgen/[name of building].json` : 建物と屋根のマップ。

`data/json/overmap_terrain.json` : 建物のオーバーマップ特性を定義します。

`data/json/regional_map_settings.json` : 建物のオーバーマップスポーン設定を定義します。

`data/json/overmap/multietile_city_buildings.json` : 建物のレイヤーを相互にリンクします。

## 屋根マップの作成

マップ作成が初めての場合、マップ作成については [MAPGEN](../reference/map/mapgen) を参照してください。

建物のマップを含むファイル `data/json/mapgen/[name of building].json` を開き、屋根のために新しいエントリを追加します。
屋根にもメインフロアと同じ基礎のフットプリントが必要なので、建物のエントリをコピーします。

屋根に固有の om_terrain ID を割り当てます。以下はメインフロアと屋根の om_terrain ID の例です。

```json
"om_terrain": [ "abstorefront" ],
"om_terrain": [ "abstorefront_roof" ],
```

注釈: 既存の建物に屋根を追加する場合、その建物が他のマップと共通の om_terrain ID を共有している場合、
既存のフロアの om_terrain ID を固有のものに変更する必要があります。

元のフロアの壁の輪郭を維持し、建物の外側には `t_open_air` を、建物のフットプリントの上には `t_flat_roof` を追加します。
terrain.json には選択できる平らな屋根の地形がいくつかあります。

屋根には、雨樋、煙突、屋根タービン換気扇など、多くの地形や備品、構造物があります。
詳細については `json/terrain.json` および `furniture.json` を参照してください。
屋根へのアクセスを考慮し、はしご、階段、雨樋を設置できます。一部の備品はよじ登ることも可能です。

オプションの入れ子マップチャンクのセットが `data/json/mapgen/nested/roof_nested.json` にあります。
これらを組み込みたい場合は使用してください。屋根用の入れ子チャンクもここに追加します。

屋根のエントリ例:

```json
{
  "type": "mapgen",
  "method": "json",
  "om_terrain": "abstorefront_roof",
  "weight": 200,
  "object": {
    "fill_ter": "t_flat_roof",
    "rows": [
      "                        ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |......&.............3 ",
      " |....................3 ",
      " |....................3 ",
      " |-----------------5--3 ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        "
    ],
    "terrain": {
      ".": "t_flat_roof",
      " ": "t_open_air",
      "|": "t_gutter_west",
      "-": "t_gutter_south",
      "3": "t_gutter_east",
      "5": "t_gutter_drop"
    },
    "furniture": { "&": "f_roof_turbine_vent" },
    "place_items": [
      { "item": "roof_trash", "x": [2, 21], "y": [3, 14], "chance": 50, "repeat": [1, 3] }
    ],
    "place_nested": [
      {
        "chunks": [
          ["null", 50],
          ["roof_4x4_party", 15],
          ["roof_4x4_holdout", 5],
          ["roof_4x4_utility", 40],
          ["roof_5x5_coop", 5]
        ],
        "x": [3, 15],
        "y": [3, 7]
      }
    ]
  }
}
```

## メインフロアと屋根のリンク

`json/overmap/multitile_city_buildings.json` または、1つのZレベルで複数のオーバーマップタイルを占有する建物（学校、邸宅など）の場合は `json/overmap/multitile_buildings_terrain.json` に移動します。メインフロアのエントリを追加します。 `point` 座標は、建物の x、y、z の位置を定義します。1 は、屋根が地上階よりZレベルで1つ上に配置されることを示します。建物を回転させる場合は、Zレベルの向きを決定するために `north` を付加します。

```json
{
  "type": "city_building",
  "id": "abstorefront",
  "locations": ["land"],
  "overmaps": [
    { "point": [0, 0, 0], "overmap": "abstorefront_north" },
    { "point": [0, 0, 1], "overmap": "abstorefront_roof_north" }
  ]
}
```

## オーバーマップスペシャル

オーバーマップスペシャルの扱いは、若干異なります。これらは、Zレベルの関連付けとオーバーマップへのスポーンの両方に `json/overmap/specials.json` を使用します。スペシャルは、
`data/json/regional_map_settings.json` にエントリを必要としません。

オーバーマップスペシャルの例:

```json
{
  "type": "overmap_special",
  "id": "Evac Shelter",
  "overmaps": [
    { "point": [0, 0, 0], "overmap": "shelter" },
    { "point": [0, 0, -1], "overmap": "shelter_under" },
    { "point": [0, 0, 1], "overmap": "shelter_roof" }
  ],
  "connections": [
    { "point": [0, -1, 0], "terrain": "road" }
  ],
  "locations": ["wilderness"],
  "city_distance": [5, 10],
  "city_sizes": [4, 12],
  "occurrences": [1, 3],
  "rotate": false,
  "flags": ["CLASSIC"]
}
```

## オーバーマップ地形エントリの追加

`data/json/overmap_terrain.json` へ移動してください。すべてのZレベルに対して、オーバーマップ上でどのように表示されるかを定義するエントリが必要です。nameは、ゲーム内のオーバーマップ画面で表示される名称です。エントリは、同じ色(color)とシンボル(sym)を共有する必要があります。

```json
  {
    "type": "overmap_terrain",
    "id": "abandonedwarehouse",
    "copy-from": "generic_city_building",
    "name": "abandoned warehouse",
    "sym": 119,
    "color": "brown"
  },
  {
    "type": "overmap_terrain",
    "id": "abandonedwarehouse_roof",
    "copy-from": "generic_city_building",
    "name": "abandoned warehouse roof",
    "sym": 119,
    "color": "brown"
}
```

## regional_map_settings への追加

`data/json/regional_map_settings.json`へ移動してください。

これは、スペシャルではない建物（一般的な建物）のスポーン頻度と位置を決定します。あなたの建物に適切なカテゴリを見つけ、overmap_special ID または city_building ID のいずれかを追加し、スポーン重み（weight）を含めてください。

```json
"abandonedwarehouse": 200,
```

テスト時には、自然スポーンを利用して作業内容を調査したい場合、スポーン率を増やすことができます。

最後に、提出する前に、必ず追加した定義を [lint](http://dev.narc.ro/cataclysm/format.html) してください。

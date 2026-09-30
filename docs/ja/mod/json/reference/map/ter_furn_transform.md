# ter_furn_transform

ter_furn_transform は、ある地形を別の地形へ、またある家具を別の家具へ変換する JSON オブジェクトの型です。

```json
[
  {
    "type": "ter_furn_transform",
    "id": "example",
    "terrain": [
      {
        "result": "t_dirt",
        "valid_terrain": ["t_sand"],
        "message": "sandy!",
        "message_good": true
      }
    ]
  }
]
```

上の例は、直接の地形 ID を比較して「砂」を「土」に変えます。変換失敗時のメッセージも追加できます。砂を「土または草」に変えたい場合は、次のようにします。

```json
"terrain": [
  {
    "fail_message": "no sand!",
    "result": [ "t_dirt", "t_grass" ],
    "valid_terrain": [ "t_sand" ],
    "message": "sandy!"
  }
]
```

`message_good` は任意で、デフォルトは true です。この例では土と草を 1:1 の割合で選びます。4:1 の割合にする場合は次のようにします。

```json
"terrain": [
  {
    "result": [ [ "t_dirt", 4 ], "t_grass" ],
    "valid_terrain": [ "t_sand" ],
    "message": "sandy!"
  }
]
```

このように、重み付き配列と単一の文字列を混在できます。単一の文字列の重みは 1 です。

上記は家具にも同様に適用されます。

```json
"furniture": [
  {
    "result": [ [ "f_null", 4 ], "f_chair" ],
    "valid_furniture": [ "f_hay", "f_woodchips" ],
   "message": "I need a chair"
  }
]
```

家具と地形のどちらでも、特定の ID の代わりにフラグを使用できます。

```json
"terrain": [
  {
    "result": "t_floor",
    "valid_flags": [ "FLAT" ],
    "message": "flooring"
  }
]
```

掘削可能な地形を対象にするには、ブール値を使用します。

```json
"terrain": [
  {
    "result": "t_dirt",
    "diggable": true,
    "message": "digdug"
  }
]
```

ter_furn_transform は terrain と furniture の両フィールドを持てます。これらは別々に扱われるため、「土なら椅子を追加」のような処理にはなりません。

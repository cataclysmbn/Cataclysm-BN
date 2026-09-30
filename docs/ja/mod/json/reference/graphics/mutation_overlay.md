# 突然変異オーバーレイの順序

`mutation_ordering.json` ファイルは、キャラクター上で視覚的な突然変異およびバイオニックのオーバーレイを描画する順序を定義します。0（下）から 9999（上）までのレイヤー値で順序を指定します。

例:

```json
[
  {
    "type": "overlay_order",
    "overlay_ordering": [
      {
        "id": [
          "BEAUTIFUL",
          "BEAUTIFUL2",
          "BEAUTIFUL3",
          "LARGE",
          "PRETTY",
          "RADIOACTIVE1",
          "RADIOACTIVE2",
          "RADIOACTIVE3",
          "REGEN"
        ],
        "order": 1000
      },
      {
        "id": ["HOOVES", "ROOTS1", "ROOTS2", "ROOTS3", "TALONS"],
        "order": 4500
      },
      {
        "id": "FLOWERS",
        "order": 5000
      },
      {
        "id": [
          "PROF_CYBERCOP",
          "PROF_FED",
          "PROF_PD_DET",
          "PROF_POLICE",
          "PROF_SWAT",
          "PHEROMONE_INSECT"
        ],
        "order": 8500
      },
      {
        "id": [
          "bio_armor_arms",
          "bio_armor_legs",
          "bio_armor_torso",
          "bio_armor_head",
          "bio_armor_eyes"
        ],
        "order": 500
      }
    ]
  }
]
```

## `id`

（string）

突然変異の内部 ID。単一の文字列または文字列の配列で指定できます。配列内のすべての項目に指定した順序値が適用されます。

## `order`

（integer）

突然変異オーバーレイの順序値。値の範囲は 0～9999 で、9999 が最上位に描画されるレイヤーです。どのリストにも含まれない突然変異は、デフォルトで 9999 になります。

# 変異オーバーレイの描画順序

`mutation_ordering.json` ファイルは、ゲーム内のキャラクターに表示される変異やバイオニクスの視覚的なオーバーレイの順序を定義します。レイヤー値は 0 (最背面)から 9999 (最前面) の範囲で指定し、数値が大きいほど上に描画されます。

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

(文字列)

変異の内部IDを指定します。単一の文字列として記述することも、配列形式でまとめて記述することも可能です。配列で指定した場合、そのすべての項目に同じ order 値が適用されます。

## `order`

(整数)

変異オーバーレイの描画順序を指定する値です。設定範囲は 0 ～ 9999 で、9999 が最も手前(最前面)に描画されるレイヤーとなります。 このリストのいずれにも含まれていない変異は、デフォルト値として 9999 が割り当てられます。

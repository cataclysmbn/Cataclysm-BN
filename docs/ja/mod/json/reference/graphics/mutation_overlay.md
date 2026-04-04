# 変異オーバーレイの描画順序

`mutation_ordering.json` ファイルは、ゲーム内のキャラクターオーバーレイが描画される順序を定義します。
変異、バイオニクス、効果、装備アイテム、手持ちアイテムの順序を変更できます。レイヤー値は
0 (最背面)から 9999 (最前面) の範囲で指定し、装備および手持ちオーバーレイは上書きしない限り、より高いデフォルト値を使用します。

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
        "id": "worn_backpack",
        "order": 5400
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

オーバーレイIDを指定します。単一の文字列として記述することも、配列形式でまとめて記述することも可能です。配列で指定した場合、そのすべての項目に同じ order 値が適用されます。

`ELFA_EARS` や `bio_armor_head` などのレガシー変異およびバイオニクスIDは引き続き機能します。
他のオーバーレイの順序を変更するには、`worn_backpack`、`wielded_katana`、
`mutation_ELFA_EARS`、`effect_onfire` などの完全なオーバーレイIDを使用してください。

## `order`

(整数)

変異オーバーレイの描画順序を指定する値です。設定範囲は 0 ～ 9999 で、9999 が最も手前(最前面)に描画されるレイヤーとなります。 このリストのいずれにも含まれていない変異は、デフォルト値として 9999 が割り当てられます.

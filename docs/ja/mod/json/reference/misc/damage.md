# ダメージ

一般的に再利用されるインスタンスと同様のインライン値にダメージを与えます。

グローバルな参照のためにここに保管されます。

> [!CAUTION]
> このデータはスタンドアロンではなく、常に別のオブジェクト内にあります

<a id="damage-instance"></a>

## ダメージインスタンス

ダメージインスタンスを定義するには 2 つの方法があります。1 つは [ダメージユニット](#damage-unit) であるかのように定義します。
もう一つは下にあります

### フィールド

| 識別子 | 説明                                            |
| ------ | ----------------------------------------------- |
| values | (_必須_) [ダメージユニット](#damage-unit)の配列 |

### 例

```json
{
  "values": [
    {
      "damage_type": "acid",
      "amount": 10,
      "armor_penetration": 4,
      "armor_multiplier": 2.5
    }
  ]
}
```

<a id="damage-unit"></a>

## ダメージユニット

### フィールド

| 識別子            | 説明                                                 |
| ----------------- | ---------------------------------------------------- |
| damage_type       | (_必須_) [ダメージの種類](#damage-types)             |
| amount            | (_必須_) 与えるダメージの量                          |
| armor_penetration | (_必須_) 装甲ごとに貫通できる装甲の量                |
| armor_multiplier  | (_必須_) armour_penetration 適用後の残りの装甲の乗数 |

### 例

```json
{
  "damage_type": "acid",
  "amount": 10,
  "armor_penetration": 4,
  "armor_multiplier": 2.5
}
```

<a id="damage-types"></a>

## ダメージの種類

以下はゲーム内のすべてのダメージタイプのリストです。

- true
- biological
- bash
- cut
- acid
- stab
- bullet
- heat
- cold
- dark
- light
- psi
- electric

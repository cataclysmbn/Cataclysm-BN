# アイテムに適切な大災厄後の価格を設定する方法

### 価格設定の哲学

価格設定は、「都市で定期的に物資をあさるような危険行為は避けたいが、それでも生き残るための物資を常に必要としている人々が人類の大多数である」という考え方に基づいています。
つまり、食料や医薬品は、それらを得るために差し出す物品と比べて、相対的に非常に高い価値を持つ、ということです。

**標準通貨単位**である「自由商人公認紙幣」は、**ミートジャーキー1枚 (250 セント)**と同等の価格に設定されています。

ミートジャーキー1枚は、24日間新鮮さを保ち（妥当な期間）、347カロリーを含んでいます。

毎日20ドル相当の商品を商人に持ち帰る生存者は、自給自足の生活をなんとか送るのに十分な収入を得ています。

毎日40ドル相当の商品を持ち帰る生存者は、かなり裕福に生活しています。

#### アイテムは、その入手可能性と実用性の組み合わせに基づいて価格が設定されます。

原材料は、ほとんどの場合、価値がないか、大量に入手できる場合にのみいくらかの価値があります（約0〜25/kg）。

実用性のないアイテムには、価値がありません。

理論上の実用性があるアイテムには、ほとんど価値がありません（10 cent）。

有用なアイテムは、(250 cent〜2 USD)の価格帯になる傾向があります。

実用性のない珍しいアイテムには、わずかな価値があります（約10 cent〜50cent）。

理論上の実用性がある珍しいアイテムは、(1 USD〜5 USD)の価値があります。

相当な実用性がある珍しいアイテムは、最も価格が高くなる傾向があります（10 USD〜150 USD）。

**いかなるアイテムも150 USDより高い価値を持つべきではありません。** 経済がより大きければ、それ以上の価値を持つかもしれませんが、どんなに素晴らしいものであっても、誰もが1つのものに費やそうとする上限はほぼこの金額です。

#### 現在実装されている派閥通貨

```json
    "id": "FMCNote", // 自由商人、自由商人公認紙幣
    "description": "The Free Merchant Certified Note, also known by names such as a 'c-note' or 'merch', is a currency based on old American bills.  Fifty dollar bills and larger are printed with a promissory note signed by the treasurer of the Free Merchants, along with a complex design.  The note explains that this can be exchanged for food, water, and other services through the Free Merchants in the Refugee Center.",
    "name": { "str": "Merch" },// マーチ
    "price_postapoc": "250 cent",


    "id": "RobofacCoin",// ハブ01
    "name": { "str": "Hub 01 Gold Coin" },// ハブ01金貨
    "description": "This is a small but surprisingly heavy gold coin.  One side is etched with circuitry and the other side reads 'Hub 01 exchange currency'.",
    "price_postapoc": "50 USD",


    "id": "FlatCoin",
    "name": { "str": "FlatCoin" },// フラットコイン
    "description": "This is a coin that has been flattened in a novelty coin flattening machine.  The machine has been somewhat crudely altered so that the design - which appears to once have been Mickey Mouse - is overlaid with a handwritten emblem of a book.  There is some text that faintly reads 'Campus Exchange Token'.",
    "price_postapoc": "250 cent",


    "id": "signed_chit",
    "name": { "str": "chit" }, // 証明書
    "description": "This is a slip of paper signed by the issuer.",
    "price_postapoc": "250 cent",


    "id": "icon",
    "name": { "str": "icon" }, // イコン
    "description": "This is a small picture, about the same size as an ID card, symbolizing a religious figure.  On the back, there is a text that faintly reads 'New England Church Community'.",
    "price_postapoc": "250 cent",
```

##### ベンチマーク価格の例

```json
{
  "id": "antibiotics",
  "name": { "str_sp": "antibiotics" },// 抗生物質
  "description": "A strong antibacterial medication designed to prevent or stop the spread of infection.  It's the safest way to cure any infections you might have.  One dose lasts twelve hours.",
  "price_postapoc": "400 USD",
  "charges": 15,
  "stack_size": 200,
  "flags": [ "NPC_SAFE" ]
},
{
  "id": "boltcutters",
  "type": "TOOL",
  "name": { "str": "pair of bolt cutters", "str_pl": "pairs of bolt cutters" }, // ボルトカッター
  "description": "This is a large pair of bolt cutters.  You could use them to cut padlocks or heavy gauge wire.",
  "price_postapoc": "250 cent",
},
{
  "id": "armor_lightplate",
  "name": { "str": "plate armor" }, // プレートアーマー
  "description": "A suit of Gothic plate armor.",
  "price_postapoc": "120 USD",
},
{
  "id": "bat_metal",
  "name": { "str": "aluminum bat" }, // アルミバット
  "description": "An aluminum baseball bat, lighter than a wooden bat and a little easier to swing as a result.",
  "price_postapoc": "1250 cent",
},
```

#### 食料の価格設定

食料の価格は、主にカロリー、保存性、および栄養価の組み合わせによって設定されます。

ミートジャーキーは 250です。

缶詰のようにカロリーと栄養価が非常に高い食料は 500 になる可能性があります。

ジャンクフードのようにカロリーが低く、腐敗の早い食料は 50 になる可能性があります。

#### スタックサイズ

アイテムにスタックサイズが設定されている場合、単位あたりの価格を算出するために、価格をスタックサイズで **割る** 必要があります。

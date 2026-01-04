# NPC 派閥

NPC 派閥は次のような形式になります:

```json
{
  "type": "faction",
  "id": "free_merchants",
  "name": "The Free Merchants",
  "likes_u": 30,
  "respects_u": 30,
  "known_by_u": false,
  "size": 100,
  "power": 100,
  "food_supply": 115200,
  "lone_wolf_faction": true,
  "wealth": 75000000,
  "currency": "FMCNote",
  "relations": {
    "free_merchants": {
      "kill on sight": false,
      "watch your back": true,
      "share my stuff": true,
      "guard your stuff": true,
      "lets you in": true,
      "defends your space": true,
      "knows your voice": true
    },
    "old_guard": {
      "kill on sight": false,
      "watch your back": true,
      "share my stuff": false,
      "guard your stuff": true,
      "lets you in": true,
      "defends your space": true,
      "knows your voice": true
    },
    "hells_raiders": {
      "kill on sight": true
    }
  },    
  "description": "A conglomeration of entrepreneurs and businessmen that stand together to hammer-out an existence through trade and industry."
},
```

| フィールド名          | 意味                                                                                                                                 |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| `"type"`              | 文字列。必ず`"faction"`でなければなりません。                                                                                        |
| `"id"`                | 文字列。派閥の固有IDです。                                                                                                           |
| `"name"`              | 文字列。派閥の名称です。                                                                                                             |
| `"likes_u"`           | 整数。プレイヤーに対する派閥の初期評価値。ゲーム中に増減する。`"likes_u"`が-10を下回ると、その派閥のメンバーは敵対的になります。     |
| `"respects_u"`        | 整数。プレイヤーに対する派閥の初期評価値。ゲーム内で意味のある効果はなく、将来的に削除される可能性があります。                       |
| `"known_by_u"`        | bool値。プレイヤーがその派閥のメンバーと出会ったことがあるかどうか。ゲーム中に変化し得る。未知の派閥は派閥メニューに表示されません。 |
| `"size"`              | 整数。派閥のメンバー数のおおよその値。現時点ではゲームプレイに影響はありません。                                                     |
| `"power"`             | 整数。派閥の勢力を大まかに示す値。現時点ではゲームプレイに影響はありません。                                                         |
| `"food_supply"`       | 整数。派閥が保有するカロリー量。現時点ではゲームプレイに影響はない。                                                                 |
| `"wealth"`            | 整数。終末世界における派閥の所持通貨(セント単位)。                                                                                   |
| `"currency"`          | 文字列。派閥が好んで使う通貨のアイテム `"id"`。派閥の商人は、この通貨を売買の双方で額面価値 100% として扱います。                    |
| `"relations"`         | 辞書。派閥が他派閥をどう見ているかを表します。詳細は後述。                                                                           |
| `"mon_faction"`       | 文字列(省略可能)。この派閥が属するとみなされるモンスター派閥の "name"。未指定の場合は `"human"` が既定値になります。                 |
| `"lone_wolf_faction"` | bool値(省略可能)。動的に生成される NPC 用の1人派閥に使用されるプロト／マイクロ派閥テンプレート。未指定の場合は "false"。             |

## 派閥間の関係

派閥は互いに関係性を持つことができ、その関係性は派閥の各メンバーに適用されます。
派閥間の関係は相互的ではありません：例えば、自由商人(Free Merchants) のメンバーは ロビーの乞食(Lobby Beggars)のメンバーを守りますが、
ロビーの乞食 のメンバーは 自由商人 のメンバーを守りません。

派閥間の関係は辞書で管理されます。 辞書のキーは派閥の名前、値はbool値フラグの入った二次辞書です。
すべてのフラグは省略可能で、省略された場合は false が既定値となります。
辞書を持つ派閥が 行動派閥(acting faction)、辞書で指定される相手派閥が 対象派閥(object faction)となります。

フラグの意味は次の通りです:

| フラグ                 | 意味                                                                                       |
| ---------------------- | ------------------------------------------------------------------------------------------ |
| `"kill on sight"`      | 行動派閥のメンバーは、対象派閥のメンバーに対して常に敵対的になります。                     |
| `"watch your back"`    | 行動派閥のメンバーは、対象派閥のメンバーへの攻撃を自分自身への攻撃と見なします。           |
| `"share my stuff"`     | 行動派閥のメンバーは、対象派閥のメンバーが行動派閥の所有物を取っても異議を唱えません。     |
| `"guard your stuff"`   | 行動派閥のメンバーは、誰かが対象派閥の所有物を取ると異議を唱えます。                       |
| `"lets you in"`        | 行動派閥のメンバーは、対象派閥のメンバーが行動派閥の支配する領域に入っても異議を唱えません |
| `"defends your space"` | 行動派閥のメンバーは、誰かが対象派閥の支配する領域に入ると敵対的になります。               |
| `"knows your voice"`   | 行動派閥のメンバーは、対象派閥のメンバーの発言についてコメントしません。                   |

現時点で実装されているのは、"kill on sight"、"knows your voice"、および "watch your back" のみです。

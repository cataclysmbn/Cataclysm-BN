# 爆発データ

これは、さまざまなファイル間で再利用されるインライン データの一部です
グローバルな参照のためにここに保管されます。

> [!CAUTION]
> このデータはスタンドアロンではなく、常に別のオブジェクト内にあります

<a id="fields"></a>

### フィールド

> [!NOTE]
> これには従来のフィールドとその機能がリストされていません。

| 識別子          | 説明                                                                                     |
| --------------- | ---------------------------------------------------------------------------------------- |
| damage          | (_オプション_) 爆発が与えるダメージの量                                                  |
| radius          | (_オプション_) 爆発の半径                                                                |
| fragment        | (_任意_) [projectiles](/mod/json/reference/misc/projectile/#fields) を参照してください。 |
| fragment_effect | (_任意_) [フラグメント効果](#fragment-effect) の配列                                     |
| fire            | (_オプション_) 火のフィールドも生成します                                                |

<a id="fragment-effect"></a>

#### フラグメント効果

| 識別子    | 説明                                  |
| --------- | ------------------------------------- |
| effect    | (_任意_) 適用するエフェクトの ID      |
| odds      | (_任意_) 確率で 1 回効果を適用します  |
| min_turns | (_オプション_) 適用される最小回転数   |
| max_turns | (_オプション_) 適用される最大ターン数 |

### 例

```json
{
  "damage": 50,
  "radius": 2,
  "fire": false,
  "fragment_effect": [{ "effect": "onfire", "odds": 2, "min_turns": 4, "max_turns": 8 }],
  "fragment": {
    "impact": { "damage_type": "bullet", "amount": 70, "armor_multiplier": 1 },
    "range": 5
  }
}
```

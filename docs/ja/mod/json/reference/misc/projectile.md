# 発射体

これは、さまざまなファイル間で再利用されるインライン データの一部です
グローバルな参照のためにここに保管されます。

> [!CAUTION]
> このデータはスタンドアロンではなく、常に別のオブジェクト内にあります

<a id="fields"></a>

### フィールド

| 識別子            | 説明                                                                                          |
| ----------------- | --------------------------------------------------------------------------------------------- |
| impact            | (_必須_) 各発射体の [ダメージ インスタンス](/mod/json/reference/misc/damage/#damage-instance) |
| range             | (_必須_) 発射物の範囲                                                                         |
| speed             | (_オプション_) 発射体の速度 (メートル/秒)                                                     |
| aimedcritbonus    | (_オプション_) クリティカルによる最小ボーナス                                                 |
| aimedcritmaxbonus | (_オプション_) クリティカルによる最大ボーナス                                                 |

### 例

```json
{
  "impact": { "damage_type": "bullet", "amount": 70, "armor_multiplier": 1 },
  "range": 5,
  "speed": 1000,
  "aimedcritbonus": 0.0,
  "aimedcritmaxbonus": 0.0
}
```

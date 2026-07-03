# 職業

## 職業

### フィールド

| 識別子            | 説明                                                                                                                                                                               |
| ----------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| id                | (_必須_) 職業の一意の ID                                                                                                                                                           |
| name              | (_必須_) `male` および `female` の名前を持つオブジェクト、またはそれに設定される両方の性別の文字列                                                                                 |
| description       | (_必須_) 職業の説明、男女共通                                                                                                                                                      |
| vehicle           | (_任意_) 開始する職業の車両プロトタイプ ID                                                                                                                                         |
| pets              | (_任意_) ペットを開始するための `name` (モンスター ID) および `amount` (モンスター ID の数) オブジェクトの配列                                                                     |
| spells            | (_任意_) 呪文を開始するための `id` (呪文 ID) および `level` (呪文のレベル) オブジェクトの配列。                                                                                    |
| points            | (_必須_) 職業を取得するためのポイントコスト                                                                                                                                        |
| items             | (_必須_) 3 つの異なる [項目グループ](/mod/json/reference/items/item_spawn/#format) または文字列 ID の配列 (技術的にはレガシー)。 `male` 男性のみ `female` 女性のみ `both` 男女両方 |
| no_bonus          | (_任意_) 配列により、職業アイテムの置換ボーナスからこれらのアイテムを獲得できなくなります                                                                                          |
| starting_cash     | (_オプション_) 職業に就く際に必要な現金の額                                                                                                                                        |
| skills            | (_任意_) `level` (スキルレベル) および `name` (スキル ID) オブジェクトの配列                                                                                                       |
| addictions        | (_任意_) `type` (中毒タイプ) および `intensity` (整数中毒強度) オブジェクトの配列                                                                                                  |
| CBMs              | (_任意_) 専門家が得られるバイオニックのバイオニック ID の配列                                                                                                                      |
| traits            | (_任意_) 職業が獲得する特性の特性 ID の配列                                                                                                                                        |
| forbidden_traits  | (_任意_) 開始時に選択できない特性 ID の配列                                                                                                                                        |
| allowed_traits    | (_任意_) 開始時に選択できる非開始特性の特性 ID の配列                                                                                                                              |
| forbidden_bionics | (_任意_) 開始時に選択できないバイオニック ID の配列                                                                                                                                |
| allowed_bionics   | (_任意_) 起動時に選択できる非起動バイオニックのバイオニック ID の配列                                                                                                              |
| forbids_bionics   | (_任意_) 起動時にバイオニックを選択しないようにするためのブール値                                                                                                                  |
| flags             | (_任意_) [職業互換](/mod/json/reference/json_flags/#scenarios) であるフラグの配列                                                                                                  |
| missions          | (_任意_) 開始するミッション ID (したがってミッション) の配列                                                                                                                       |
| npcs              | (_任意_) 開始する npc クラス ID (したがってそのクラスの npc) の配列                                                                                                                |
| age               | (_任意_) 開始年齢範囲の `min` と `max` のオブジェクト、または 1 つの可能な年齢のみを表す int                                                                                       |

### 例

```json
{
  "type": "profession",
  "id": "wolfpack_mutant",
  "name": "Mutant Pack Leader",
  "description": "They treated you like an animal in that lab.  Now that you're free, you wonder if they had the right idea.  Your new friends don't seem afraid of you like they are of humans, after all.",
  "vehicle": "covered_wagon",
  "pets": [ { "name": "mon_wolf", "amount": 2 } ],
  "spells": [ { "id": "summon_zombie", "level": 1 }, { "id": "necrotic_gaze", "level": 1 } ],
  "points": 4,
  "items": { "both": [ "subsuit_xl" ], "male": [ "briefs" ], "female": [ "bra", "panties" ] },
  "no_bonus": [ "glasses_eye" ]
  "starting_cash": 0,
  "skills": [ { "level": 1, "name": "survival" } ],
  "addictions": [ { "intensity": 10, "type": "nicotine" } ],
  "CBMs": [ "bio_batteries", "bio_lockpick", "bio_fingerhack", "bio_power_storage_mkII" ],
  "traits": [ "LUPINE_FUR", "TAIL_FLUFFY", "LUPINE_EARS", "THRESH_LUPINE" ],
  "forbidden_traits": [ "CARRY_PERMIT" ],
  "allowed_traits": [ "MUZZLE" ],
  "forbidden_bionics": [ "bio_power_storage" ],
  "allowed_bionics": [ "bio_razors" ],
  "flags": [ "SCEN_ONLY" ],
  "missions": [ "MISSION_HEIST_DRIVER" ],
  "npcs": [ "NC_PROF_ESCAPIST_LAB", "NC_PROF_ESCAPIST_LAB", "NC_PROF_ESCAPIST_LAB" ],
  "age": { "min": 17, "max": 35 }
},
```

## スキルレベルの基準

職業を追加するときの開始スキル値に使う一般的な目安です。これは大まかな指針にすぎないため、必要または面白いと感じるなら調整できます。また、キャラクター作成時にスキルへポイントを割り振れることも忘れず、潜在的な経歴だけではなく職業そのものを基準にスキルを判断してください。

0: 経験なし
社会との関わりや一般的な日常作業から得られる標準的な知識レベルです。訓練は受けていませんが、その話題について一般的な認識はあります。
例: 趣味、上級課程、仕事などを考慮しない平均的な米国の高校卒業者は、すべてのスキルが 0 になります。

1: 趣味レベル
少量の経験がある趣味人です。
例: 銃器雑誌を読み、射撃場に 1、2 回行ったことがある人は、射撃スキル 1 を持つでしょう。

2: 愛好者
ある程度の経験があるか、その分野の基礎訓練を受けています。
例: ある程度頻繁に射撃場へ通い、射撃場の教官から基礎を学んだ人は、射撃スキル 2 を持つでしょう。基礎訓練を終えたばかりの兵士は、訓練は受けているものの専門ではないため応急処置 2 を持つでしょう。一方で戦闘衛生兵のような職なら、より高い応急処置を持つでしょう。

3: 前段階に経験を加えた程度

4: 訓練済み
平均的な専門化の水準でそのスキルを訓練されています。準学士程度の水準です。
例: 歩兵訓練を終えたばかりの兵士は、射撃スキル 4 を持つでしょう。

5: 前段階に経験を加えた程度

6: 高度な訓練
高度な専門化の水準でそのスキルを訓練されています。学士程度の水準です。
例: 選抜射手訓練を終えたばかりの兵士は、射撃スキル 6 を持つでしょう。

7: 前段階に経験を加えた程度

8: 専門家
専門家水準でそのスキルを訓練されています。大学院程度の水準です。
例: 狙撃手訓練を終えたばかりの兵士は、射撃スキル 8 を持つでしょう。"average" なプロアスリートは運動スキル 8 を持つでしょう。

9: 前段階に経験を加えた程度

10: ベテラン
専門家水準の訓練を受け、長年の経験と、その分野で他者を上回る才能を持っています。
例: 著名なプロアスリートは運動スキル 10 を持つでしょう。Tier 1 特殊部隊員は射撃スキル 10 を持つでしょう。著名な化学者は料理スキル 10 を持つでしょう。

## 職業アイテムの置換

### 交代

#### 特性ベース

```jsonc
{
  "type": "profession_item_substitutions", // 必須タイプ
  "trait": "WOOLALLERGY", // 必要な特性
  "sub": [
    {
      "item": "blazer", // 交換するアイテム
      "new": ["jacket_leather_red"], // それに代わるアイテムたち。 1対1の比率
    },
    {
      "item": "hat_hunting", // 交換するアイテム
      "new": [{
        "item": "hat_cotton", // それに代わるアイテム
        "ratio": 2, // アイテムごとに 2 を与えて交換します
      }],
    },
  ],
}
```

上の例では。 `WOOLALLERGY` 特性付き。 `blazer` は 1 `jacket_leather_red` に置き換えられます。
そして、`hat_hunting` は 2 つの `hat_cotton` 項目に置き換えられます。

#### アイテムベース

```jsonc
{
  "type": "profession_item_substitutions", // 必須タイプ
  "item": "sunglasses", // 交換するアイテム
  "sub": [
    {
      "present": [ "HYPEROPIC" ], // これらの特性をすべて備えている必要があります
      "absent": [ "MYOPIC" ], // これらの特性のいずれも持つことはできません
      "new": [ "fitover_sunglasses" ] // これらに置き換えました
    },
    {
      "present": [ "MYOPIC" ], // これらすべての特性を備えている必要があります
      "new": [ { "fitover_sunglasses", "ratio": 2 } ] // 比率置換もサポート
    }
  ]
}
```

したがって、`HYPEROPIC` を持っている場合は、フィットオーバー サングラスを 1 つ入手できます。
`MYOPIC` では、`HYPEROPIC` を使用しても、すべての場合に 2 つのフィットオーバー サングラスが提供されます。

### ボーナス

#### アイテムグループ

```jsonc
{
  "type": "profession_item_substitutions", // 必要なタイプ
  "item_group": "profession_carry_bonus",  // 与えるアイテムグループ
  "bonus": { "present": [ "CARRY_PERMIT" ] } // 上記のように、存在と不在の両方で機能します
},
```

これにより、アイテムグループ `profession_carry_bonus` から `CARRY_PERMIT` 特性を持つキャラクターに無料のスポーンが許可されます。

#### アイテム

```jsonc
{
  "type": "profession_item_substitutions", // 必要なタイプ
  "item": "glasses_eye",                   // 贈るアイテム
  "bonus": { "present": [ "MYOPIC" ], "absent": [ "HYPEROPIC" ] } // 上記のとおり、現在および不在の作品
},
```

これにより、`MYOPIC` 特性を持つキャラクターに無料アイテム `glasses_eye` が付与されますが、`HYPEROPIC` には付与されません。

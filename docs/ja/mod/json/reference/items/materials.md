---
title: Material Definitions
---

> [!NOTE]
>
> この記事は最近 `JSON INFO` から分割されたものであり、さらに加筆修正が必要な可能性があります。

### Materials

| 識別子                 | 説明                                                                                                                                             |
| ---------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `id`                   | 固有のID。小文字のスネークケース(snake_case)で記述。連続した1単語である必要があり、必要に応じてアンダースコア(_)を使用します。                   |
| `name`                 | ゲーム内で表示される名称。                                                                                                                       |
| `bash_resist`          | 打撃ダメージに対する耐性。                                                                                                                       |
| `cut_resist`           | 斬撃ダメージに対する耐性。                                                                                                                       |
| `bullet_resist`        | 銃弾ダメージに対する耐性。                                                                                                                       |
| `acid_resist`          | 酸に対する耐性。                                                                                                                                 |
| `elec_resist`          | 電気に対する耐性。                                                                                                                               |
| `fire_resist`          | 火に対する耐性。                                                                                                                                 |
| `chip_resist`          | アイテム自体に対する攻撃によって、アイテムが損傷することへの耐性。                                                                               |
| `bash_dmg_verb`        | 素材が打撃ダメージを受けた際に使用される動詞。                                                                                                   |
| `cut_dmg_verb`         | 素材が斬撃ダメージを受けた際に使用される動詞。                                                                                                   |
| `dmg_adj`              | アイテムの損傷状態を説明する形容詞(損傷が深刻になる順に記述)。                                                                                   |
| `dmg_adj`              | 素材の損傷状態を表現するために使用される形容詞。                                                                                                 |
| `density`              | 素材の密度。                                                                                                                                     |
| `vitamins`             | 素材に含まれるビタミン。通常、アイテム固有の数値によって上書きされます。                                                                         |
| `wind_resist`          | 風の透過を防ぐ効果(0-100%)。数値が高いほど防風性能が高い。アイテムを構成するどの素材にもこの値が指定されていない場合、既定値 99 が適用されます。 |
| `warmth_when_wet`      | 完全に濡れた際に保持される保温性の割合。既定値は 0.2(20%)。                                                                                      |
| `specific_heat_liquid` | 非凍結時の比熱(J/(g K))。既定値は 4.186                                                                                                          |
| `specific_heat_solid`  | 凍結時の比熱 (J/(g K))。既定値 2.108。                                                                                                           |
| `latent_heat`          | 融解潜熱 (J/g)。既定値 334。334.                                                                                                                 |
| `freeze_point`         | 素材の凝固点(華氏)。既定値は 32 F ( 0 C )。                                                                                                      |
| `edible`               | (任意) 食べられるかどうか。既定値は false。                                                                                                      |
| `rotting`              | (任意) 腐敗するかどうか。既定値は false。                                                                                                        |
| `soft`                 | (任意) 柔らかい素材かどうか。既定値は false。                                                                                                    |
| `reinforces`           | (任意) 補強が可能かどうか。既定値は false。                                                                                                      |

耐性を示すパラメータ(-resist)には、acid(酸)、bash(打撃)、chip(欠け)、cut(斬撃)、elec(電気)、fire(火)の6種類があります。これらは整数値で指定します。既定値は 0 で、負の値を設定すると通常よりも大きなダメージを受けるようになります。

```json
{
  "type": "material",
  "id": "hflesh",
  "name": "Human Flesh",
  "density": 5,
  "specific_heat_liquid": 3.7,
  "specific_heat_solid": 2.15,
  "latent_heat": 260,
  "edible": true,
  "rotting": true,
  "bash_resist": 1,
  "cut_resist": 1,
  "bullet_resist": 1,
  "acid_resist": 1,
  "fire_resist": 1,
  "elec_resist": 1,
  "chip_resist": 2,
  "dmg_adj": ["bruised", "mutilated", "badly mutilated", "thoroughly mutilated"],
  "bash_dmg_verb": "bruised",
  "cut_dmg_verb": "sliced",
  "vitamins": [["calcium", 0.1], ["vitB", 1], ["iron", 1.3]],
  "burn_data": [
    { "fuel": 1, "smoke": 1, "burn": 1, "volume_per_turn": "2500_ml" },
    { "fuel": 2, "smoke": 3, "burn": 2, "volume_per_turn": "10000_ml" },
    { "fuel": 3, "smoke": 10, "burn": 3 }
  ]
}
```

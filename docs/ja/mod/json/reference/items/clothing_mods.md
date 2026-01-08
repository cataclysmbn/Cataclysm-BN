---
title: Clothing Modifications
---

### 衣服の改造

```json
"type": "clothing_mod",
"id": "leather_padded",   // 固有のID
"flag": "leather_padded", // 改造後に衣服に付与されるフラグ
"item": "leather",        // 改造時に消費されるアイテム
"implement_prompt": "Pad with leather",      // 改造を実行する際に表示されるプロンプト
"destroy_prompt": "Destroy leather padding", // 改造を取り消す(破壊する)際に表示されるプロンプト
"restricted": true,       // (任意) trueの場合、衣服側の "valid_mods" リストにこのフラグが含まれていないと改造できません。デフォルトは false。
"mod_value": [            // 改造による効果のリスト
    {
        "type": "bash",   // 効果の種類。"bash"(打撃), "cut"(斬撃), "bullet"(銃弾), "fire"(耐火), "acid"(耐酸), "warmth"(保温性), "storage"(収納容量), "encumbrance"(動作制限)が利用可能。
        "value": 1,       // 効果の数値
        "round_up": false // (任意) 数値を切り上げるかどうか。デフォルトは false。
        "proportion": [   // (任意) 衣服の元々のパラメータに比例して数値を加算する設定.
            "thickness",  // アイテムが持つ "material_thickness"(素材の厚み)1レイヤーごとに数値を加算。
            "volume",     // アイテムのベース容積ごとに数値を加算。
            "coverage"    // アイテムの平均カバー率(%)に応じて数値を軽減。
        ]
    }
]
```

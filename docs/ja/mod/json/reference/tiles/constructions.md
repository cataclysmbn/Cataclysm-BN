---
title: Construction Info
---

> [!NOTE]
>
> この記事は最近 `JSON INFO` から分割されたもので、追加の作業が必要になる可能性があります。

### 建設

```json
"id": "constr_pit_spiked",                                          // 建設の識別子
"group": "spike_pit",                                               // 建設グループ。UI で説明を提供し、関連する建設（建設段階など）をまとめます。
"category": "DIG",                                                  // 建設カテゴリ
"required_skills": [ [ "survival", 1 ] ],                           // 建設に必要なスキルレベル
"time": "30 m",                                                     // 完了までの時間。整数は分として扱われ、時間文字列も使用できます。
"components": [ [ [ "spear_wood", 4 ], [ "pointy_stick", 4 ] ] ],   // 建設に使用するアイテム
"using": [ [ "welding_standard", 5 ] ],                             // （任意）使用に必要な外部要件
"pre_note": "I am a dwarf and I'm digging a hole!",                 // （任意）注釈
"pre_flags": [ "FLAT" ],                                             // （任意）建設場所の地形に必要なフラグ
"needs_diggable": true,                                               // （任意）元の地形が掘削可能である必要があるか
"pre_terrain": "t_pit",                                              // （任意）建設前に必要な地形
"pre_furniture": "f_sandbag_half",                                   // （任意）建設前に必要な家具
"pre_special": "check_down_OK",                                      // （任意）ハードコードされたタイル有効性チェック。construction.cpp を参照
"post_terrain": "t_pit_spiked",                                      // （任意）完了後の地形タイプ
"post_furniture": "f_sandbag_wall",                                  // （任意）完了後の家具タイプ
"post_special": "done_dig_stair",                                    // （任意）ハードコードされた完了関数。construction.cpp を参照
"post_flags": [ "keep_items" ],                                      // （任意）追加のハードコード効果。2022年9月時点で利用可能なのは keep_items のみ
"byproducts": [ { "item": "pebble", "charges": [ 3, 6 ] } ],        // （任意）建設の副産物
"vehicle_start": false,                                               // （任意、デフォルト false）ハードコード目的で車両を作成するか
"on_display": false,                                                  // （任意、デフォルト true）プレイヤーの UI に表示するか
"dark_craftable": true,                                               // （任意、デフォルト false）暗所で建設できるか
```

### 建設グループ

```json
"id": "build_wooden_door",            // グループ識別子
"name": "Build Wooden Door",          // 建設メニューに表示する説明文字列
```

### 建設シーケンス

建設シーケンスは、基地キャンプの設計図に必要な要件を自動計算するために必要です。各建設シーケンスは、空の土のタイル上で指定された地形または家具を作るために必要な手順を表します。1つの地形または家具に対して定義できる建設シーケンスは常に1つだけです。便宜上、空の土のタイルから始まるブラックリストにない各建設レシピについて、同じシーケンスを生成する他のレシピがなく、同じ結果を持つ明示的なシーケンスもない場合に限り、1要素の建設シーケンスが自動生成されます。

```json
"id": "f_workbench",                // シーケンス識別子
"blacklisted": false,               // （任意）このシーケンスをブラックリストに入れるか
"post_furniture": "f_workbench",    // （任意）結果となる家具の識別子
"post_terrain": "t_rootcellar",     // （任意）結果となる地形の識別子
"elems": [                          // 建設識別子の配列
  "constr_pit",                     //   最初の建設は空の土のタイルから始める必要があります
  "constr_rootcellar"               //   最後の建設は指定された post_furniture または
]                                   //   post_terrain のいずれかを生成する必要があります。
                                    //   空にすると、その地形/家具を設計図の自動計算から除外します。
```

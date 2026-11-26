---
title: 家具と地形
---

### 家具

```json
{
  "type": "furniture",
  "id": "f_toilet",
  "name": "toilet",
  "symbol": "&",
  "looks_like": "chair",
  "color": "white",
  "move_cost_mod": 2,
  "light_emitted": 5,
  "required_str": 18,
  "flags": ["TRANSPARENT", "BASHABLE", "FLAMMABLE_HARD"],
  "crafting_pseudo_item": "anvil",
  "examine_action": "toilet",
  "close": "f_foo_closed",
  "open": "f_foo_open",
  "lockpick_result": "f_safe_open",
  "lockpick_message": "With a click, you unlock the safe.",
  "provides_liquids": "beer",
  "bash": "TODO",
  "deconstruct": "TODO",
  "max_volume": "1000 L",
  "examine_action": "workbench",
  "workbench": { "multiplier": 1.1, "mass": 10000, "volume": "50L" },
  "boltcut": {
    "result": "f_safe_open",
    "duration": "1 seconds",
    "message": "The safe opens.",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "count": 3 }]
  },
  "hacksaw": {
    "result": "f_safe_open",
    "duration": "12 seconds",
    "message": "The safe is hacksawed open!",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "count": 13 }]
  },
  "default_vars": {
    "CATEGORYIDS": "[ \"CSC_FOOD_MEAT\", \"CSC_FOOD_VEGGI\", \"CSC_FOOD_PASTA\" ]",
    "CHARGE_PER_MIN": "5",
    "CHARGE_START": "100",
    "CRAFTSPEEDMULT": "1.0"
  }
}
```

#### `type`

固定文字列です。この JSON オブジェクトを家具として識別するため、`furniture` でなければなりません。

`"id", "name", "symbol", "looks_like", "color", "bgcolor", "max_volume", "open", "close", "bash", "deconstruct", "examine_action", "flgs`

地形の場合と同じです。後述の「家具と地形の共通項目」を参照してください。

#### `move_cost_mod`

移動コストの補正値です (`-10` = 通行不可、`0` = 変化なし)。下にある地形の移動コストにこの値が加算されます。

#### `lockpick_result`

(省略可) 家具の解錠に成功したとき、この家具に変化します。

#### `lockpick_message`

(省略可) 家具の解錠に成功したときにプレイヤーへ表示されるメッセージです。省略した場合は、代わりに汎用メッセージ `"The lock opens…"` が表示されます。

#### `oxytorch`

(省略可) 酸素溶断器を使用するときのデータです。

```cpp
oxytorch: {
    "result": "furniture_id", // (optional) furniture it will become when done, defaults to f_null
    "duration": "1 seconds", // ( optional ) time required for oxytorching, default is 1 second
    "message": "You quickly cut the metal", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `light_emitted`

家具が発する光の強さです。10 なら家具のあるタイルを明るく照らし、15 ならそのタイルと周囲のタイルを明るく照らすほか、光源から二タイル離れたタイルもわずかに照らします。たとえば、天井灯は 120、ユーティリティライトは 240、コンソールは 10 です。

#### `boltcut`

(省略可) ボルトカッターを使用するときのデータです。

```cpp
"boltcut": {
    "result": "furniture_id", // (optional) furniture it will become when done, defaults to f_null
    "duration": "1 seconds", // ( optional ) time required for bolt cutting, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "sound": "Gachunk!", // ( optional ) description of the sound when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `hacksaw`

(省略可) 金鋸を使用するときのデータです。

```cpp
"hacksaw": {
    "result": "furniture_id", // (optional) furniture it will become when done, defaults to f_null
    "duration": "1 seconds", // ( optional ) time required for hacksawing, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `required_str`

家具を動かすために必要な筋力です。負の値を指定すると動かせない家具になります。

#### `crafting_pseudo_item`

(省略可) この家具が利用可能な範囲内にあるとき、製作に使用できるアイテム (道具) の ID です (家具がその種類のアイテムとして機能します)。配列にすることもできます。例:
`"crafting_pseudo_item": [ "fake_gridwelder", "fake_gridsolderingiron" ],`

#### `workbench`

(省略可) ここで製作できるようにします。速度倍率、許容質量、許容体積を指定する必要があります。質量または体積が上限を超えると速度にペナルティが生じます。機能させるには `"workbench"` の `examine_action` と組み合わせる必要があります。

#### `plant_data`

(省略可) 植物であることを示します。`transform` と、状況に応じて `base` を指定する必要があります。`GROWTH_HARVEST` フラグがある場合は、`harvest_multiplier` や `growth_multiplier` も追加できます。

#### `surgery_skill_multiplier`

(省略可) 手術時、この家具に隣接して立つ生存者に適用される手術スキル倍率 (浮動小数点数) です。

#### `provides_liquids`

(省略可) 調べたとき、指定した液体アイテムを無限に供給します。機能させるには `"examine_action": "liquid_source"` と併用する必要があります。

#### `enchanter_info`

(省略可) エンチャント情報オブジェクトの配列です。
以下はエンチャント情報オブジェクトの例です。

```jsonc
{
  "id": "CVD_DIAMOND_CUT", // Id of the enchantment info, used for saveload ( mandatory )
  "name": "Cutting Diamonds", // Display name on the enchantment ui ( mandatory )
  "enchant": "ENCH_CVD_MACHINE_CUT", // Enchantment id to give ( mandatory )
  "time_to_enchant": "10 minutes", // Time duration of the enchantment ( mandatory )
  "volume_per_time": "250 ml", // Every x volume will multiply time required
  "volume_time_effect": true, // Weather `volume_per_time` is used
  "using": "cvd_diamond", // Requirement info ( mandatory )
  "volume_per_batch": "250 ml", // Every x volume will multiply requirement info
  "volume_batch_effect": true, // Weather `volume_per_batch` is used
  "count_var": "DIAMONDIZE", // Item variable for counting how many can be applied
  "max_count": 10, // Maximum count
  "applied_flag": "DIAMOND", // Flag to apply ( generally useful for editing description)
  "can_use_on": "cvd_machine", // Name of the `enchanter_can_use_on` lua function ( params: ench_id, item ); Checked per item
  "can_make": "cvd_machine", // Name of the `enchanter_can_make` lua function ( params: ench_id ); Checked once per menu opening
  "required_skills": [ { "skill": "magic", "level": 2 } ] // Array of skills needed to do this
},
```

#### `default_vars`

(省略可) オブジェクトの既定の文字列変数です。必ず文字列と文字列の組にします。任意のデータの保存や、次のような iuse 用データに使用できます。

- マルチクッカー
  - "CATEGORYIDS"; 文字列は適用可能なレシピカテゴリの JSON 配列です
  - "RECIPEIDS": 文字列は有効なレシピの JSON 配列です
  - "CHARGE_PER_MIN": 毎分消費するチャージ数です
  - "CHARGE_START": 開始時に消費するチャージ数です
  - "CRAFTSPEEDMULT": 製作速度の倍率です

### 地形

```json
{
  "type": "terrain",
  "id": "t_spiked_pit",
  "name": "spiked pit",
  "symbol": "0",
  "looks_like": "pit",
  "color": "ltred",
  "move_cost": 10,
  "light_emitted": 10,
  "trap": "spike_pit",
  "fill_result": "t_dirt", // Terrain result from filling this terrain in
  "fill_minutes": 10, // Number of minutes to fill terrain in
  "max_volume": "1000 L",
  "flags": ["TRANSPARENT"],
  "digging_results": {
    "digging_min": 1,
    "result_ter": "t_pit_shallow",
    "num_minutes": 60,
    "items": "digging_sand_50L"
  },
  "connects_to": "WALL",
  "close": "t_foo_closed",
  "open": "t_foo_open",
  "lockpick_result": "t_door_unlocked",
  "lockpick_message": "With a click, you unlock the door.",
  "nail_pull_result": "t_fence_post", // terrain ID to transform into when you use a hammer to pull the nails out
  "nail_pull_items": [8, 5], // nails and planks (respectively) to drop as a result of pulling nails with a hammer. Defaults to 0,0 and thus technically optional even if you add a nail_pull_result.
  "bash": "TODO",
  "deconstruct": "TODO",
  "harvestable": "blueberries",
  "transforms_into": "t_tree_harvested",
  "harvest_season": "WINTER",
  "roof": "t_roof",
  "examine_action": "pit",
  "boltcut": {
    "result": "t_door_unlocked",
    "duration": "1 seconds",
    "message": "The door opens.",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "2x4": 3 }]
  },
  "hacksaw": {
    "result": "t_door_unlocked",
    "duration": "12 seconds",
    "message": "The door is hacksawed open!",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "2x4": 13 }]
  }
}
```

#### `type`

固定文字列です。この JSON オブジェクトを地形として識別するため、`terrain` でなければなりません。

`"id", "name", "symbol", "looks_like", "color", "bgcolor", "max_volume", "open", "close", "bash", "deconstruct", "examine_action", "flgs`

家具の場合と同じです。後述の「家具と地形の共通項目」を参照してください。

#### `move_cost`

通過時の移動コストです。0 は通行不可 (壁など) を意味します。負の値は使用しないでください。正の値は 50 移動ポイントの倍数です。たとえば 2 の場合、プレイヤーがその地形を通過するときに `2 * 50 = 100` 移動ポイントを消費します。

#### `light_emitted`

地形が発する光の強さです。10 ならその地形のあるタイルを明るく照らし、15 ならそのタイルと周囲のタイルを明るく照らすほか、光源から二タイル離れたタイルもわずかに照らします。たとえば、天井灯は 120、ユーティリティライトは 240、コンソールは 10 です。

#### `digging_results`

(省略可) シャベルを使った地形の掘削に関するフィールドです。

- `"digging_min"` - この地形の掘削に必要な最低掘削性能
- `"result_ter"` - 掘削後の地形 ID
- `"num_minutes"` - 掘削にかかる分数
- `"items"` - (省略可) アイテムの配列、または既存のアイテムグループ ID。既定値は `digging_soil_loam_200L`

#### `lockpick_result`

(省略可) 地形の解錠に成功したとき、この地形に変化します。

#### `lockpick_message`

(省略可) 地形の解錠に成功したときにプレイヤーへ表示されるメッセージです。省略した場合は、代わりに汎用メッセージ `"The lock opens…"` が表示されます。

#### `oxytorch`

(省略可) 酸素溶断器を使用するときのデータです。

```cpp
oxytorch: {
    "result": "terrain_id", // terrain it will become when done
    "duration": "1 seconds", // ( optional ) time required for oxytorching, default is 1 second
    "message": "You quickly cut the bars", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `trap`

(省略可) その地形に組み込まれる罠の ID です。

たとえば、地形 `t_pit` には組み込みの罠 `tr_pit` があります。そのため、ゲーム内で地形 `t_pit` になっているすべてのタイルには、暗黙的に罠 `tr_pit` も存在します。両者を切り離すことはできません (プレイヤーは組み込みの罠を解除できず、地形を変更すると組み込みの罠も解除されます)。

組み込みの罠がある場合、プレイヤーやマップ生成によって他の罠を明示的に追加することはできません。

#### `harvestable`

(省略可) 定義すると、その地形から収穫できるようになります。この項目では、収穫される果実などのアイテム種別を定義します。機能させるには、`harvest_*` 系の `examine_action` 関数のいずれかも設定する必要があります。

#### `boltcut`

(省略可) ボルトカッターを使用するときのデータです。

```cpp
"boltcut": {
    "result": "terrain_id", // terrain it will become when done
    "duration": "1 seconds", // ( optional ) time required for bolt cutting, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "sound": "Gachunk!", // ( optional ) description of the sound when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `hacksaw`

(省略可) 金鋸を使用するときのデータです。

```cpp
"hacksaw": {
    "result": "terrain_id", // terrain it will become when done
    "duration": "1 seconds", // ( optional ) time required for hacksawing, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `transforms_into`

(省略可) 地形のさまざまな変化に使用します。定義する場合は、有効な地形 ID でなければなりません。次のような用途があります。

- 果実を収穫したとき (収穫済みの地形に変化させる)。
- `HARVESTED` フラグおよび `harvest_season` と組み合わせ、収穫済みの地形を果実のある地形に戻す。

#### `harvest_season`

(省略可) `"SUMMER"`、`"AUTUMN"`、`"WINTER"`、`"SPRING"` のいずれかです。`"HARVESTED"` フラグと組み合わせ、地形を収穫可能な状態に戻すために使用します。

#### `roof`

(省略可) この地形の上に配置される地形 (屋根) です。

### 家具と地形の共通項目

地形と家具の両方に設定できる、または設定しなければならない値があります。どちらの場合も意味は同じです。

#### `id`

オブジェクトの ID です。同じ種類のすべてのオブジェクト (全地形または全家具) の中で一意にする必要があります。慣例上 (技術的には必須ではありません)、家具の ID には `"f_"`、地形の ID には `"t_"` の接頭辞を付けます。これは翻訳されません。後から変更するとセーブデータとの互換性が失われるため、変更してはいけません。

#### `name`

オブジェクトの表示名です。これは翻訳対象です。

#### `flags`

(省略可) さまざまな追加フラグです。`doc/json_flags.md` を参照してください。

#### `connects_to`

(省略可) この地形が接続する地形グループです。タイルの回転や接続、および `"AUTO_WALL_SYMBOL"` フラグを持つ地形に描画される ASCII 記号に影響します。

現在使用できる値:

- `CHAINFENCE`
- `RAILING`
- `WALL`
- `WATER`
- `WOODFENCE`
- `GUTTER`

例: `-`、`|`、`X`、`Y` は、同じ `connects_to` 値を共有する地形です。`O` にはその値がありません。`X` と `Y` には `AUTO_WALL_SYMBOL` フラグもあります。`X` は T 字路 (西、南、東に接続) として描画され、`Y` は水平線 (西から東へ接続し、南には接続しない) として描画されます。

```
-X-    -Y-
 |      O
```

#### `symbol`

ゲーム内に表示されるオブジェクトの ASCII 記号です。記号の文字列は正確に一文字でなければなりません。季節ごとの記号を定義する 4 つの文字列の配列にすることもできます。最初の要素は春の記号を定義します。配列でない場合は、一年を通して同じ記号が使用されます。

#### `comfort`

この地形または家具の快適度です。その上で眠りにつけるかどうかに影響します。uncomfortable = -999、neutral = 0、slightly_comfortable = 3、comfortable = 5、very_comfortable = 10

#### `floor_bedding_warmth`

この地形または家具で眠るときに得られる追加の暖かさです。

#### `bonus_fire_warmth_feet`

近くの火から足に得られる暖かさの増加量です (既定値 = 300)。

#### `looks_like`

このアイテムに似ている別のアイテムの ID です。このアイテムに対応するタイルがない場合、タイルセットローダーはそのアイテムのタイルを読み込もうとします。`looks_like` の項目は暗黙的に連鎖します。たとえば `throne` の `looks_like` が `big_chair` で、`big_chair` の `looks_like` が `chair` の場合、`throne` と `big_chair` のタイルが存在しなければ、玉座は椅子のタイルで表示されます。タイルセットが `looks_like` の連鎖に含まれるどのアイテムのタイルも見つけられない場合は、既定で ASCII 記号が使用されます。

#### `color` または `bgcolor`

ゲーム内に表示されるオブジェクトの色です。`"color"` は前景色 (背景色なし)、`"bgcolor"` は単色の背景色を定義します。`"symbol"` の値と同様に、季節ごとの色を表す 4 要素の配列にすることもできます。

> **注意**: `"color"` と `"bgcolor"` のどちらか一方だけを使用してください。

#### `max_volume`

(省略可) ここにアイテムを収納できる最大体積です。体積には ml と L を使用でき、`"50 ml"` や `"2 L"` のように指定します。

#### `examine_action`

(省略可) オブジェクトを調べたときに呼び出される JSON 関数です。`src/iexamine.h` を参照してください。

#### `close" And "open`

(省略可) 値には、地形の項目内では地形 ID、家具の項目内では家具 ID を指定します。いずれかを定義すると、プレイヤーはオブジェクトを開閉できるようになります。開閉すると、対象タイルのオブジェクトが指定したものに変化します。たとえば、`"safe_c"` は `"open"` によって `"safe_o"` へ変化し、`"safe_o"` は `"close"` によって `"safe_c"` へ戻るようにできます。ここで `"safe_c"` と `"safe_o"` は、それぞれ異なる特性を持つ地形 (または家具) です。

#### `bash`

(省略可) オブジェクトを叩き壊せるかどうかと、叩き壊した場合に何が起こるかを定義します。`map_bash_info` を参照してください。

#### `deconstruct`

(省略可) オブジェクトを解体できるかどうかと、解体した場合の結果を定義します。`map_deconstruct_info` を参照してください。

#### `pry`

(省略可) オブジェクトをこじ開けられるかどうかと、こじ開けた場合に何が起こるかを定義します。`prying_result` を参照してください。

#### `map_bash_info`

プレイヤーなどが地形や家具を叩いたときに起こるさまざまな事象を定義します。

```json
{
  "str_min": 80,
  "str_max": 180,
  "str_min_blocked": 15,
  "str_max_blocked": 100,
  "str_min_supported": 15,
  "str_max_supported": 100,
  "sound": "crunch!",
  "sound_vol": 2,
  "sound_fail": "whack!",
  "sound_fail_vol": 2,
  "ter_set": "t_dirt",
  "furn_set": "f_rubble",
  "explosive": 1,
  "collapse_radius": 2,
  "destroy_only": true,
  "bash_below": true,
  "tent_centers": ["f_groundsheet", "f_fema_groundsheet", "f_skin_groundsheet"],
  "items": "bashed_item_result_group"
}
```

#### `str_min`, `str_max`, `str_min_blocked`, `str_max_blocked`, `str_min_supported`, `str_max_supported`

TODO

#### `sound`, `sound_fail`, `sound_vol`, `sound_fail_vol`

(省略可) 叩いたオブジェクトを破壊したとき、または叩き壊しに失敗したときに鳴る音と、その音量です。音を表す文字列は翻訳され、プレイヤーに表示されます。

#### `furn_set`, `ter_set`

元のオブジェクトが破壊されたときに設定される地形または家具です。地形の `bash` 項目では必須ですが、家具の項目では省略可能です (既定では家具なし)。

#### `explosive`

(省略可) 0 より大きい場合、オブジェクトを破壊するとこの威力の爆発が発生します (`game::explosion` を参照)。

#### `destroy_only`

TODO

#### `bash_below`

TODO

#### `tent_centers`, `collapse_radius`

(省略可) テントの一部である家具について、その中心部分の ID を定義します。テントの他の部分が叩き壊されると、中心部分も同時に破壊されます。中心部分は指定された `"collapse_radius"` の範囲内で検索されるため、この値はテントの大きさに合わせる必要があります。

#### `items`

(省略可) インラインのアイテムグループ、またはアイテムグループの ID です。`doc/ITEM_SPAWN.md` を参照してください。既定のサブタイプは `"collection"` です。叩き壊しに成功すると、そのグループのアイテムが生成されます。

#### `map_deconstruct_info`

```json
{
  "furn_set": "f_safe",
  "ter_set": "t_dirt",
  "items": "deconstructed_item_result_group"
}
```

#### `furn_set`, `ter_set`

元のオブジェクトを解体した後に設定される地形または家具です。`"furn_set"` は省略可能で、既定では家具なしになります。`"ter_set"` は地形の `"deconstruct"` 項目でのみ使用され、その場合は必須です。

#### `items`

(省略可) インラインのアイテムグループ、またはアイテムグループの ID です。`doc/ITEM_SPAWN.md` を参照してください。既定のサブタイプは `"collection"` です。オブジェクトを解体すると、そのグループのアイテムが生成されます。

#### `prying_result`

```json
{
  "success_message": "You pry open the door.",
  "fail_message": "You pry, but cannot pry open the door.",
  "break_message": "You damage the door!",
  "pry_quality": 2,
  "pry_bonus_mult": 3,
  "noise": 12,
  "break_noise": 10,
  "sound": "crunch!",
  "break_sound": "crack!",
  "breakable": true,
  "difficulty": 8,
  "new_ter_type": "t_door_o",
  "new_furn_type": "f_crate_o",
  "break_ter_type": "t_door_b",
  "break_furn_type": "f_null",
  "break_items": [
    { "item": "2x4", "prob": 25 },
    { "item": "wood_panel", "prob": 10 },
    { "item": "splinter", "count": [1, 2] },
    { "item": "nail", "charges": [0, 2] }
  ]
}
```

#### `new_ter_type`, `new_furn_type`

元のオブジェクトをこじ開けた後に設定される地形または家具です。`"furn_set"` は省略可能で、既定では家具なしになります。`"ter_set"` は地形の `"pry"` 項目でのみ使用され、その場合は必須です。

#### `success_message`, `fail_message`, `break_message`

地形または家具のこじ開けに成功したとき、失敗したとき、または失敗したこじ開けによって地形や家具が別のものに壊れたときに表示されるメッセージです。`break_message` は、`breakable` が true に設定され、かつ `break_ter_type` が定義されている場合にのみ必須です。

#### `pry_quality`, `pry_bonus_mult`, `difficulty`

地形または家具をこじ開けようとするために必要な最低こじ開け性能と、こじ開けに成功する確率を決定します。`iuse.cpp` より:

```cpp
int diff = pry->difficulty;
diff -= ( ( pry_level - pry->pry_quality ) * pry->pry_bonus_mult );
```

```cpp
if( dice( 4, diff ) < dice( 4, p->str_cur ) ) {
    p->add_msg_if_player( m_good, pry->success_message );
```

こじ開けを試みるとき、`difficulty` はキャラクターの現在の筋力と比較されます。道具のこじ開け性能が必要な `pry_quality` を上回っている場合、実効難易度は `pry_bonus_mult` で決まる割合で低下します。`pry_bonus_mult` は省略可能で、既定値は 1 です (つまり、道具のこじ開け性能が最低値を超える性能レベルごとに `diff` が 1 低下します)。

#### `noise`, `break_noise`, 'sound', 'break_sound'

`noise` を指定すると、地形または家具のこじ開けに成功したとき、指定された音量で `sound` が再生されます。`breakable` が true で `break_noise` を指定すると、こじ開けに失敗して地形または家具が壊れたとき、指定された音量で `break_noise` が再生されます。

`noise` または `break_noise` を指定しなければ、対象の地形や家具をこじ開けたり壊したりしても無音になるため、これらは省略可能です。`sound` と `break_sound` も省略可能で、既定のメッセージはそれぞれ `"crunch!"` と `"crack!"` です。

#### `breakable`, `break_ter_type`, `break_furn_type`

`breakable` を true に設定すると、こじ開けに失敗した際に地形または家具が壊れる可能性があります。地形では、`breakable` を true に設定した場合は `break_ter_type` が必須です。`break_furn_type` は省略可能で、既定値は null です。

#### `break_items`

(省略可) インラインのアイテムグループ、またはアイテムグループの ID です。`doc/ITEM_SPAWN.md` を参照してください。既定のサブタイプは `"collection"` です。`breakable` を true に設定した場合、こじ開けの失敗によってオブジェクトが壊れると、そのグループのアイテムが生成されます。

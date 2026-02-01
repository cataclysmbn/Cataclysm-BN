# マップ生成の基本ガイド

本ガイドでは、マップ生成の基本、編集が必要なファイル、各ファイル内のタグ、そしてスペシャル（特殊な構造物）と通常の都市内建物を作成する際の相違点について解説します。

[For full technical information about mapgen entries, visit here](../../reference/map/mapgen)

まず、いくつかの基本的な概念と、追加・編集するファイルについて説明します。

## 一般的なコメント:

BN のマップ生成は、慣れると驚くほど強力です。多くの工夫を凝らして、マップに多様性と面白さを加えることができます。より高度なマップ生成技術については、別のチュートリアルで扱います。本ガイドでは、基本的な概念と、単一 OMT（オーバーマップ地形タイル）サイズの基本的な建物を生成する方法を扱います。パレットの使用法や、屋根の追加方法についても触れます。

## スペシャル vs. 都市内建物:

スペシャルは、都市外に出現し、都市からの距離や有効な OMT 地形タイプなどの、追加情報が必要な建物です。最近の変更でスペシャルタイプの建物が都市内にも出現できるようになりました。昔はゲーム内で唯一の複数タイル建物でした。スペシャルの例としては、農場、山小屋、LMOEシェルターなどがあります。

都市内建物は、シングルまたはマルチタイルサイズが可能で、出現は都市の境界内に限定されます。建物は都市内建物とスペシャルの両方として定義できます。両方のタイプで出現させるには、両方のエントリセットが必要です。一部のモーテル (例：2fmotel_city と 2fmotel) がこれに該当します。

重要な方針: ルーフプロジェクト以降、すべての建物は Z レベルをまたぐマルチタイルとなりました。すべての新しい建物には、必ずJSONファイル内で屋根を追加する必要があります。近いうちに、すべての地下室も地上階のマップ生成に合わせてカスタムフィットされるようになるため、地下室が必要な場合は専用の階下を含めるのが良い慣行です。

## 必須ファイルとその目的:

1. 新しいマップ生成ファイルを次の場所に追加します。
   [data/json/mapgen](https://github.com/cataclysmbnteam/Cataclysm-BN/tree/main/data/json/mapgen) またはサブフォルダ。既存の建物の基礎形状を使用する場合は、その建物のファイルに追記しても構いません。
   - 建物の設計図です。家具や戦利品を追加するための建物データも保持できます（代替案としてパレットを参照）。

2. 作成する Z レベルごとに、オーバーマップ地形ファイル
   ([data/json/overmap/overmap_terrain](https://github.com/cataclysmbnteam/Cataclysm-BN/tree/main/data/json/overmap/overmap_terrain))にエントリを追加します。
   - エントリは、オーバーマップ上で建物がどのように見えるか、シンボル、色、および歩道の追加な
     どの出現要件を定義します。また、一部のマップ生成機能のフラグを制御します。

3. スペシャル、都市内建物に応じて、specials.json または
   multitile_city_buildings.json のいずれかにエントリを追加します。
   - multitile_city_buildings の場合、建物の様々な Z レベルや複数の OMT をリンクします。
   - スペシャルの場合、建物の様々なZレベルや複数のOMTをリンクし、必要な道路/地下鉄などの接続を定義し、出現
     パラメータを定義します。

4. 都市内建物については、regional_settings.json にエントリを追加します。世界に出現できるようになります。

5. 省略可能ですが、大規模なプロジェクトでは推奨されます: mapgen_palettes フォルダに新しいパレットファイル
   を追加します（既存のパレットを使用しても構いません）。
   - これは、複数のマップ間で共有でき、マップ生成ファイルデータの一部を保持するファイルです。地形と家具の
     定義に一般的に使用されます。また、戦利品、車両、モンスターのスポーン、および通常マップ生成ファイルの `"object"` タグに入るその他のデータを配置することもできます。
   - 既存のマップ生成パレットの編集は避けてください。なぜなら、パレットとマップ生成ファイルの組み合わせ
     を使用している既存のマップに影響を与える可能性があるためです。

## マップ生成エントリの開始:

私が新しいマッププロジェクトを開始するときは、ゲーム内に出現するために必要なすべてのエントリを最初から追加します。こうすることで、作業を進めながらテストし、必要に応じて調整できます。したがって、プロジェクトを開始する際には必要なすべてのファイルを設定することをお勧めします。

開始する前に、いくつかの決定を行う必要があります。

1. 全体のサイズはどのくらいか (OMTの数はいくつになるか？)
2. どこに出現させるか？
3. パレットを使用するか、すべてのデータをマップ生成ファイルに直接記述するか。
   - パレットを使用する場合は、できるだけ多くの部分を最初から定義します。

4. より高度な質問:
   - ネストされたマップを使用するか？
   - 地下鉄や道路に接続させたいか？
   - パレットと組み合わせてマップ生成オブジェクトデータを使用するか？(両方のタイプを複合させた
     例については、モールの2階を参照してください)

## マップ生成マップ:

ここでは、マップ生成ファイル内のマップフラグと、何をするかを説明します。より広範な情報については、[MAPGEN](../../reference/map/mapgen)を参照してください。

マップ生成ファイルには、いくつかのメタデータタグと、マップ作成のすべてを定義する `"object"` データが含まれています。

### メタデータ:

サンプル:

```json
"type": "mapgen",
"method": "json",
"om_terrain": "s_restaurant_coffee",
"weight": 250,
```

1. `"type"` は mapgen になります (他のマップタイプについては今後のチュートリアルで扱います)。`"method"`
   は JSONになります。このデータは、プログラムにこのファイルをどのように処理するかを指示します。

2. `"om_terrain"`: マップの内部名です (オーバーマップに表示される名前ではありません)。
   **同じ建物の基礎形状を共有する** 複数のバリアントを作成する予定がない限り、固有名の必要があります。

3. `"weight"`: **同じ om_terrain 名を共有する**他のマップと比較した、このマップ自体の重み
   です。例えば、あるバージョンの家があり、異なる出現を持つ同一の家 (家具が完全に揃っている家と廃墟バージョンのような) を作成するとします。この重みは、それぞれが他のマップに対してどれくらいの頻度で出現するかを決定します。家具付きの家が 100 で、廃墟の家が 20 の場合、廃墟の家は家具付きの家より 5 分の 1 の頻度で出現します。

### オブジェクトデータ:

マップの地形、家具、および様々な出現タイプを定義するタグのセクションです。アイテム（およびネストされたマップ）を配置する方法はいくつかあります。これらは独自のチュートリアルに値します。本ドキュメントでは、戦利品出現に最も使いやすい「明示的なシンボル」配置を使用します。rows と fill_ter を除くオブジェクトセクション内のすべては、mapgen_palette に配置できます。

オブジェクトのサンプルセグメント：オブジェクト内のすべては、オブジェクトの {} 中括弧内に収まる必要があります。そうでない場合、含まれません。終了中括弧を間違った場所に置いても、ロードエラーは発生しない可能性があります。

サンプル:

```json
"object": {
  "fill_ter": "t_floor",
  "rows": [
    "S___________SSTzzzzzzzTS",
    "S_____,_____SSzMbMbMbMzS",
    "S_____,_____SSSSSSSS/MzS",
    "S_____,_____SSSSSSSS/MzS",
    "S_____,_____SSzzzSSS/MzS",
    "S_____,_____SS||V{{{V||S",
    "S_____,_____SS|D     <|S",
    "SSSSSSSSSSSSSS|r      OS",
    "SSSSSSSSSSSSSS|r      |S",
    "SVVVVVVVz./Mzz|   #W##|S",
    "SVD>>>>Vz./bMzV   #ww%|S",
    "SV BBB>Vz.b/..{   xwwF|S",
    "SV   B>Vz.Mb..{   flwl|S",
    "SV   B>Vz....zV   flwU|S",
    "SV   B>Vzbbbzz|X  #wwG|S",
    "S|B ^||||||||||^  ||I||S",
    "SO6  B|=;|;=|99  r|FwC|S",
    "S|B  6|=A|A=|9   r|Fwc|S",
    "S|   B|+|||+|9   D|!ww|S",
    "S|B               |Lwl|S",
    "SO6   6 6 6   B66B|Lwl|S",
    "S|B ^???????^ B66B|Lwl|S",
    "S|||||||||||||||||||3||S",
    "S4SSSSSSSSSSSSSSSSSSSSSS"
  ],
  "terrain": {
    " ": "t_floor",
    "!": "t_linoleum_white",
    "#": "t_linoleum_white",
    "%": "t_linoleum_white",
    "+": "t_door_c",
    ",": "t_pavement_y",
    ".": "t_grass",
    "/": "t_dirt",
    "3": [ "t_door_locked", "t_door_locked_alarm" ],
    ";": "t_linoleum_white",
    "=": "t_linoleum_white",
    "A": "t_linoleum_white",
    "C": "t_linoleum_white",
    "F": "t_linoleum_white",
    "G": "t_linoleum_white",
    "I": "t_door_locked_interior",
    "L": "t_linoleum_white",
    "M": "t_dirt",
    "O": "t_window",
    "S": "t_sidewalk",
    "U": "t_linoleum_white",
    "V": "t_wall_glass",
    "W": "t_fencegate_c",
    "_": "t_pavement",
    "b": "t_dirt",
    "c": "t_linoleum_white",
    "f": "t_linoleum_white",
    "l": "t_linoleum_white",
    "w": "t_linoleum_white",
    "x": "t_console_broken",
    "z": "t_shrub",
    "{": "t_door_glass_c",
    "|": "t_wall_b",
    "<": "t_stairs_up",
    "4": "t_gutter_downspout",
    "T": "t_tree_coffee"
  },
  "furniture": {
    "#": "f_counter",
    "%": "f_trashcan",
    "/": "f_bluebell",
    "6": "f_table",
    "9": "f_rack",
    ">": "f_counter",
    "?": "f_sofa",
    "A": "f_sink",
    "B": "f_chair",
    "C": "f_desk",
    "D": "f_trashcan",
    "F": "f_fridge",
    "G": "f_oven",
    "L": "f_locker",
    "M": "f_dahlia",
    "U": "f_sink",
    "X": "f_rack",
    "^": "f_indoor_plant",
    "b": "f_dandelion",
    "f": "f_glass_fridge",
    "l": "f_rack",
    "r": "f_rack"
  },
  "toilets": { ";": {  } },
  "items": {
    "#": { "item": "coffee_counter", "chance": 10 },
    "6": { "item": "coffee_table", "chance": 35 },
    "9": { "item": "coffee_display_2", "chance": 85, "repeat": [ 1, 8 ] },
    ";": { "item": "coffee_bathroom", "chance": 15 },
    "=": { "item": "coffee_bathroom", "chance": 35 },
    ">": { "item": "coffee_table", "chance": 25 },
    "A": { "item": "coffee_bathroom", "chance": 35 },
    "C": { "item": "office", "chance": 70 },
    "D": { "item": "coffee_trash", "chance": 75 },
    "F": { "item": "coffee_fridge", "chance": 80, "repeat": [ 1, 8 ] },
    "G": { "item": "oven", "chance": 35 },
    "L": { "item": "coffee_locker", "chance": 75 },
    "U": { "item": "coffee_dishes", "chance": 75 },
    "X": { "item": "coffee_newsstand", "chance": 90, "repeat": [ 1, 8 ] },
    "f": { "item": "coffee_freezer", "chance": 85, "repeat": [ 1, 8 ] },
    "l": { "item": "coffee_prep", "chance": 50 },
    "r": { "item": "coffee_condiments", "chance": 80, "repeat": [ 1, 8 ] }
  },
  "monsters": { "!": { "monster": "GROUP_COFFEE_SHOP_ZOMBIE", "chance": 1 } },
  "place_monsters": [ { "monster": "GROUP_ZOMBIE", "x": [ 3, 17 ], "y": [ 13, 18 ], "chance": 1 } ],
  "vehicles": { "c": { "vehicle": "swivel_chair", "chance": 100, "status": 1 } }
}
```

1. `"fill_ter"`: 家具の下や row 内で未定義のシンボルに使われている既定の地形または床材を定義します。
   一般的に、最も多くの家具と関連付けられている地形を選択してください。

2. `"rows"`: 建物の実際の設計図です。標準的な OMT タイル (オーバーマップタイル) は 24x24
   タイルです。rows は、マップの左上隅である 0,0 から X,Y 座標を開始します。

   - ヒント: クロスステッチをしている方や、そのパターンに慣れている方には、非常に馴染み深く見えるはずです。マ
     ップと「凡例」のエリアがあります。

3. `"terrain"`: rows 内の全ての文字が地形である場合の意味を定義します。
   シンボルは単一の地形を返すことも、選択された地形の中から出現するチャンスを提供することもできます。
   簡単な例をいくつか示します。

`"*": "t_door_c",`: これにより、rows 内のすべての * が閉じた木製ドアになります。

`"*": [ [ "t_door_locked_peep", 2 ], "t_door_locked_alarm", [ "t_door_locked", 10 ], "t_door_c" ],`:
この配列は、ドアのグループからランダムに選択します。一部のドアは、他のドアよりも出現率が高くなるように重みが付けられています。施錠されたドアが最も確率が高く、次いで覗き穴付きのドアが続きます。最後に、閉じたドアと施錠/警報付きドアは同じ重みを持ち、最も低い確率で出現します。

注釈: 例では、fill_ter は床用です。別の床に出現させたい家具がある場合は、その家具に定義したシンボルを使用し、新しい床材の地形として定義する必要があります。上記の例では、複数の家具シンボルが白いリノリウムを床材として使用しています。このステップを省略すると、家具が間違った床材を持つことになり、特にそれを破壊したときに目立ちます。私はしばしば、マップを提出する前に、ゲーム内で家具を破壊して地形をチェックする最終マップチェックを行います。これは非常にカタルシスを得られる作業です。

4. `"furniture"` タグ: 地形と同様に、家具IDとマップシンボルの一覧です。地形と同じ種類の配列を扱うこと
   ができます。

5. `"toilets"` およびその他の特殊な定義を持つ家具: いくつかの特殊な定義を持つ家具に出くわすでしょう。
   これにより、配置が容易になります。私たちのサンプルマップの
   `"toilets": { ";": {  } },` というエントリは、シンボルエントリを定義し、トイレに水も自動的に配置します。
   他にもいくつかの特殊な家具エントリがあります。

他に最も一般的なものは以下です:
`"vendingmachines": { "D": { "item_group": "vending_drink" }, "V": { "item_group": "vending_food" } }`
これは、自動販売機に2つのシンボルを割り当て、1つを飲み物用、もう1つを食品用にします。
(注釈: 弾薬グループなど、任意のアイテムグループを機械に入れることができます)。

6. アイテム配置: アイテムを配置する方法はたくさんあります。このチュートリアルでは、最も簡単なシンボル配
   置のみを扱います。さらなる情報については、戦利品配置に関するドキュメント: [ITEM_SPAWN.md](../../reference/items/item_spawn)を参照してください。

サンプルではタグに "items" を使用しています。他には、"place_item"、"place_items"、"place_loot"があります。一部は個別にアイテム配置し、他はグループ、または両方できます。これについては別のチュートリアルで扱います。

ここでは、このサンプルを分析してみましょう。

```json
"items": {
      "a": { "item": "stash_wood", "chance": 30, "repeat": [ 2, 5 ] },
      "d": [
        { "item": "SUS_dresser_mens", "chance": 50, "repeat": [ 1, 2 ] },
        { "item": "SUS_dresser_womens", "chance": 50, "repeat": [ 1, 2 ] }
       ]
      }
```

この場合、`"a"`はマップで定義された暖炉です。暖炉に`stash_wood` アイテムグループを追加したいとします。グループを `"a"` の下に定義することで、マップ内のすべての暖炉がこのグループを受け取ります。すべての冷蔵庫が同じアイテムグループを受け取る家などの一般的なアイテムグループにとって特に強力です。`chance` は出現確率であり、`repeat` はその確率でロールを 2〜5 回繰り返すことを意味します。そのため、この暖炉は薪が満載になることも、少ししかないこと、または確率ロールに失敗すれば何もないこともあります

`"d"` はドレッサーです。ドレッサーに2つのアイテムグループ、男性用と女性用の選択肢から取得させたいと考えました。したがって、このシンボルのすべてのアイテムグループを包含する配列 `[ ... ]` があります。

配列には、好きなだけアイテムグループを追加できます。これは、汎用的な家のパレットにある棚の1つです。

```json
"q": [
        { "item": "tools_home", "chance": 40 },
        { "item": "cleaning", "chance": 30, "repeat": [ 1, 2 ] },
        { "item": "mechanics", "chance": 1, "repeat": [ 1, 2 ] },
        { "item": "camping", "chance": 10 },
        { "item": "tools_survival", "chance": 5, "repeat": [ 1, 2 ] }
      ],
```

注釈: シンボル配置を使用する場合、グループはそのシンボルを使用しているすべての家具に出現するチャンスがあることを忘れないでください。したがって、非常に寛大になる可能性があります。出現アイテムが異なる 2つの本棚が必要な場合は、それぞれに独自のシンボルを与えるか、配置に X,Y 座標を使用する別のアイテム配置形式を使用してください。

7. モンスターの配置: 以下の例には、2種類のモンスター配置方法があります。

```json
"monsters": { "!": { "monster": "GROUP_COFFEE_SHOP_ZOMBIE", "chance": 1 } },
"place_monsters": [ { "monster": "GROUP_ZOMBIE", "x": [ 3, 17 ], "y": [ 13, 18 ], "chance": 1 } ],
```

最初の `"monsters"`は、シンボル配置を使用しています。
最後の `"place_monsters"`は、範囲 X,Y 座標を使用してモンスターを配置しています。
注釈: モンスターをマップ生成ファイルに配置するのではなく、オーバーマップ地形に配置する方向に移行しているため、特別な理由でモンスターグループを出現させたい場合にのみ使用してください。詳細については、`overmap_terrain` セクションを参照してください。

1. 車両の配置:

```json
"vehicles": { "c": { "vehicle": "swivel_chair", "chance": 100, "status": 1 } }
```

車両は、シンボル配置を使用した回転椅子です。

次の例は、`vehicle_groups` と X,Y 配置を使用しています。また、回転と状態も含まれています。回転はマップ上で車両が配置される方向であり、状態 (`status`) は車両全体の損傷度です。`fuel` は燃料です。車両の配置はゲーム内で必ずテストしてください。車両配置は非常に難しく、回転が予想される数値の意味と実際には一致しません。車両の 0,0 ポイントは異なる場合があるため、特に狭いスペースでは、正しい場所に配置させるために試行錯誤する必要があります。

```json
"place_vehicles": [
        { "chance": 75, "fuel": 0, "rotation": 270, "status": 1, "vehicle": "junkyard_vehicles", "x": 12, "y": 18 },
        { "chance": 75, "fuel": 0, "rotation": 270, "status": 1, "vehicle": "junkyard_vehicles", "x": 19, "y": 18 },
        { "chance": 90, "fuel": 0, "rotation": 0, "status": -1, "vehicle": "engine_crane", "x": 28, "y": 3 },
        { "chance": 90, "fuel": 0, "rotation": 90, "status": -1, "vehicle": "handjack", "x": 31, "y": 3 },
        { "chance": 90, "fuel": 30, "rotation": 180, "status": -1, "vehicle": "welding_cart", "x": 34, "y": 3 },
        { "chance": 75, "fuel": 0, "rotation": 180, "status": -1, "vehicle": "junkyard_vehicles", "x": 31, "y": 9 },
        { "chance": 75, "fuel": 0, "rotation": 270, "status": 1, "vehicle": "junkyard_vehicles", "x": 28, "y": 18 },
        { "chance": 75, "fuel": 0, "rotation": 270, "status": 1, "vehicle": "junkyard_vehicles", "x": 35, "y": 18 }
      ]
```

9. 家具内の液体: このエントリは、スタンド式タンク用です。タンクを "g" として定義しました。

`"liquids": { "g": { "liquid": "water_clean", "amount": [ 0, 100 ] } },`

これはタンク内に飲料水を配置し、生成量の範囲を定義します。

10. その他の特殊な配置技術:他にも、より多くのマップを見るにつれて習得できる特殊な配置技術があります。私のお気に入りは、プランターです。

プランターは「密閉されたアイテム」であるため、コンテナに入るものを定義します。この例では、プランターに種を配置します。最初のものは苗を配置し、他は収穫期です。迅速な配置のために、各プランタータイプに明示的なシンボルを与えました。

```json
"sealed_item": {
      "1": { "item": { "item": "seed_rose" }, "furniture": "f_planter_seedling" },
      "2": { "item": { "item": "seed_chamomile" }, "furniture": "f_planter_harvest" },
      "3": { "item": { "item": "seed_thyme" }, "furniture": "f_planter_harvest" },
      "4": { "item": { "item": "seed_bee_balm" }, "furniture": "f_planter_harvest" },
      "5": { "item": { "item": "seed_mugwort" }, "furniture": "f_planter_harvest" },
      "6": { "item": { "item": "seed_tomato" }, "furniture": "f_planter_harvest" },
      "7": { "item": { "item": "seed_cucumber" }, "furniture": "f_planter_harvest" },
      "8": { "item": { "item": "soybean_seed" }, "furniture": "f_planter_harvest" },
      "9": { "item": { "item": "seed_zucchini" }, "furniture": "f_planter_harvest" }
    }
```

11. 最善の方法:

- 新しい家を作成する場合は、"standard_domestic_palette"を使用してください。戦利品はすでに割り当てられており、幅広い
  家庭用備品をカバーしています。これにより、戦利品配置に関して、あなたの家が他のすべての家と同期します。
- すべての建物には屋根エントリも必要です。
- JSONの配置は実際には重要ではありませんが、既存の大多数のマップと同様に、マップ生成ファイルの順序を保つよ
  うに努めてください。将来の貢献者に配慮しましょう。ファイルの上部にメタデータを追加します。object エントリが2番目に来ます。object エントリ内では、一般的に次の順序です。パレット、セットポイント、地形、備品、トイレなどのランダムな特殊エントリ、アイテム配置、車両配置、モンスター配置。
- すべての建物には、屋根に登るために少なくとも 1つの"t_gutter_downspout" が必要です。プレイヤーは少なくと
  も2つあると喜びます。マルチZレベルの建物に雨樋を追加する場合は、中間階を忘れないでください。プレイヤーが登れるように、縦樋を互い違いに配置する必要があります（梯子のように）。
- 屋根の上に持っていくのが難しいものを置く場合は、屋根に登るために梯子や階段などの手段を提供していることを確認
  してください。
- 外側の草地カバーには、マップの境界を越えても一貫性を保つために`"t_region_groundcover_urban",` を使用してく
  ださい。以下は、草、低木、木の標準的な植物エントリです。

```json
".": "t_region_groundcover_urban",
"A": [ "t_region_shrub", "t_region_shrub_fruit", "t_region_shrub_decorative" ],
"Z": [ [ "t_region_tree_fruit", 2 ], [ "t_region_tree_nut", 2 ], "t_region_tree_shade" ],
```

最後に、花 (備品)の場合:

```json
"p": "f_region_flower"
```

## 屋根の追加!

Cataclysm-BN の建物は、ほぼすべて屋根に対応しており、その状態を維持したいと考えています。建物と一緒に屋根マップを提出するようにしてください。これは、地上階および同じ建物の形状/基礎を共有する他の階と同じファイルに入れることができます。

これは、先ほど説明した建物に比べて非常に簡単です。同じ基本コンポーネントをすべて持っています。地上階マップの rows を使用し、`"roof_palette"` シンボルセットに変換することから始めることをお勧めします。基本的には、輪郭を雨樋でなぞり、その下に t_gutter_spout の隣に t_gutter_drop を追加し、いくつかのインフラストラクチャを屋根に配置するだけです。私は商業ビルの屋根に巣（nests）を広範囲に適用しましたが、これについては高度なマップ生成で扱います。

屋根のサンプル:

```json
{
  "type": "mapgen",
  "method": "json",
  "om_terrain": "house_01_roof",
  "object": {
    "fill_ter": "t_shingle_flat_roof",
    "rows": [
      "                        ",
      "                        ",
      "                        ",
      "  |222222222222222223   ",
      "  |.................3   ",
      "  |.................3   ",
      "  |.............N...3   ",
      "  5.................3   ",
      "  |.................3   ",
      "  |.................3   ",
      "  |........&........3   ",
      "  |.................3   ",
      "  |......=..........3   ",
      "  |.................3   ",
      "  |----------------53   ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        "
    ],
    "palettes": ["roof_palette"],
    "terrain": { ".": "t_shingle_flat_roof" }
  }
}
```

1. 私は常に建物IDに `_roof` を付加します。
2. パレットが、先の例すべてのデータの代わりになっている点に注目してください。非常にすっきりしていて簡単です。
3. `"weight"` はありません。何故ならばリンクされていれば建物と一緒に出現するからです。
4. 私のパレットは既定の屋根として "t_flat_roof" を使用しています。家の場合、シングル屋根が欲しかったので、この
   マップ生成に"t_shingle_flat_roof" を追加しました。これにより、パレットの
   `".": "t_flat_roof"`のエントリが上書きされます (これについては高度なマップ生成で詳しく説明します)。

ほかに、屋根に関するドキュメントもあります:[JSON_ROOF_MAPGEN](../json_roof_mapgen.md)。

## multitile_city_buildings.json を使用したマップ生成マップのリンク

このファイルは次の場所にあります:
[data/json/overmap/multitile_city_buildings.json](https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/data/json/overmap/multitile_city_buildings.json)。

注釈:このファイルは都市内建物のみを対象としており、スペシャル用ではありません。

標準的なエントリ:

```json
{
  "type": "city_building",
  "id": "house_dogs",
  "locations": [ "land" ],
  "overmaps": [
    { "point": [ 0, 0, 0 ], "overmap": "house_dogs_north" },
    { "point": [ 0, 0, 1 ], "overmap": "house_dogs_roof_north" },
    { "point": [ 0, 0, -1 ], "overmap": "basement" }
  ]
},
```

1. `"type"` は変わりません。常に"city_building"である必要があります。
2. `"id"`: このIDは、マップ生成IDと同じであることが多いですが、同じ必要はあ
   りません。もっと汎用的な"house"のようなものを使用することもできます。このIDは地域設定の配置で使用されるため、建物がこのIDを使用しているかに留意してください。私は、デバッグ作業がはるかに容易になるため、明確なIDを好みます。
3. `"locations"`: 建物がオーバーマップ地形タイプによって配置できる場所を定義し
   ます。既定値はland です。
4. `"overmaps"`: ここでマップがどのように組み合わされるかを定義します。詳しく見て
   みましょう。
   `{ "point": [ 0, 0, 0 ], "overmap": "house_dogs_north" },` point: 建物の他のマップ生成ファイルに対する相対座標です。座標は`[ x, y, z ]`です。この例では、Z レベルごとに1つのマップしかないため、X, Y は0です。Zがゼロということは、これが地上階であることを意味します。屋根(roof)は1、地下室(basement)は-1であることに注意してください。
5. IDへの `_north` の付加:
   - 建物が回転する場合、フロアが正しく一致するように、コンパス方位が必要で
     す。これは汎用的な地下室マップ生成グループであり、専用の階段を追加するにつれて変更されるため、`_north` は付きません。

## regional_map_settings.json を使用したオーバーマップ配置の設定

[data/json/regional_map_settings.json](https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/data/json/regional_map_settings.json)

1. 都市内建物と家の場合、`"city":` フラグまでスクロールします。
2. 適切なサブタグ (通常は`"houses"` または `"shops"`) を見つけます。
3. multitile_city_buildings エントリからのIDを追加します。マップ生成IDも受
   け入れ、文句を言わないため、同じ名前であることが多いです。
4. 建物に適した重みを選択します。

## スペシャルのリンクとスポーン:

エントリを次の場所に入れます:
[data/json/overmap/overmap_special/specials.json](https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/data/json/overmap/overmap_special/specials.json)。

このエントリは、regional_map_settings と multitile_city_buildings の両方の役割に加え、その他の楽しいオーバーマップ設定も行います。

例:

```json
{
  "type": "overmap_special",
  "id": "pump_station",
  "overmaps": [
    { "point": [0, 0, 0], "overmap": "pump_station_1_north" },
    { "point": [0, 0, 1], "overmap": "pump_station_1_roof_north" },
    { "point": [0, 1, 0], "overmap": "pump_station_2_north" },
    { "point": [0, 1, 1], "overmap": "pump_station_2_roof_north" },
    { "point": [0, -1, -1], "overmap": "pump_station_3_north" },
    { "point": [0, 0, -1], "overmap": "pump_station_4_north" },
    { "point": [0, 1, -1], "overmap": "pump_station_5_north" }
  ],
  "connections": [
    { "point": [0, -1, 0], "connection": "local_road", "from": [0, 0, 0] },
    { "point": [1, -1, -1], "connection": "sewer_tunnel", "from": [0, -1, -1] },
    { "point": [-1, -1, -1], "connection": "sewer_tunnel", "from": [0, -1, -1] },
    { "point": [-1, 1, -1], "connection": "sewer_tunnel", "from": [0, 1, -1] }
  ],
  "locations": ["land"],
  "city_distance": [1, 4],
  "city_sizes": [4, 12],
  "occurrences": [0, 1],
  "flags": ["CLASSIC"]
}
```

1. `"type"`は overmap_special です。
2. `"id"` はオーバーマップ用の建物ID です。また、ゲーム内のオーバーマップにも表示されます。
3. `"overmaps"` は、都市内建物のエントリと同じように機能します。ポンプ場は地上で
   1 OMT よりも大きいため、Y 座標も変化していることに注意してください。
4. `"connections"`: マップの道路、下水道、地下鉄の接続を配置します。
5. `"locations"`: この建物を配置できる有効な OMT タイプです。
6. `"city_distance"`、`"city_sizes"` どちらも、都市との関係でどこに出現するかに関するパラメータです。
7. `"occurrences": [ 0, 1 ],`: occurrences は、"UNIQUE" フラグを使用するかどうかに応じて 2 つの意味を持つ可能性があ
   ります。フラグがない場合、オーバーマップごとに、スペシャルを配置できる回数になります。この場合は 0 から 1 回です。
   UNIQUE フラグを使用する場合、これはパーセンテージになります。したがって、
   `[ 1, 10 ]` はオーバーマップごとに 1〜10 回ではなく、オーバーマップ上で、10%の確率で1回だけ配置されることになります。
8. `"flags"` は、スペシャルをさらに定義するために使用できるフラグです。フラグの一覧については、
   [json_flags](../../reference/json_flags.md)を参照してください。

詳細については、[OVERMAP](../../reference/map/overmap.md) を参照してください。

## オーバーマップ地形エントリ:

建物のタイプに応じて、次の場所でファイルを選択します:
[data/json/overmap/overmap_terrain](https://github.com/cataclysmbnteam/Cataclysm-BN/tree/main/data/json/overmap/overmap_terrain)。

この一連のエントリは、オーバーマップ上で建物がどのように見えるかを定義します。copy-from をサポートしています。

```json
{
  "type": "overmap_terrain",
  "id": "s_music",
  "copy-from": "generic_city_building",
  "name": "music store",
  "sym": "m",
  "color": "brown",
  "mondensity": 2,
  "extend": { "flags": [ "SOURCE_LUXURY" ] }
},
{
  "type": "overmap_terrain",
  "id": "s_music_roof",
  "copy-from": "generic_city_building",
  "name": "music store roof",
  "sym": "m",
  "color": "brown"
}
```

マップ生成ID ごとに 1つのエントリが必要です。

1. `"type"`は常に overmap_terrain になります。
2. `"id"` は、マップ生成ファイルで使用したのと同じ ID になります。
3. `"copy-from"` は、ここで定義したものを除き、別のエントリからすべてのデータをコピーします。
4. `"name"`は、オーバーマップで名前がどのように表示されるかです。
5. `"sym"` は、オーバーマップに表示されるシンボルです。省略された場合、 `v<>^`のキャレットが使用されます。
6. `"color"` は、オーバーマップシンボルの色です。
7. `"mondesntiy"` は、このオーバーマップタイルの既定のモンスター密度を設定します。これはゾンビ出現に使用し、マップ
   生成モンスターエントリは、特定の配置（例：ペットショップのペット）のために予約します。
8. `"extend"` フラグの多くは、将来的に NPC の AI によって使用されるため、場所に適したフラグを追加
   するように努めてください。その他にも、歩道を生成するなど、マップ生成をさらに定義するものがあります。

詳細については、
[Overmap Terrain section of OVERMAP](../../reference/map/overmap#overmap-terrain)を参照してください。

## パレット(Palettes):

前述のように、パレットは`rows` と `fill_ter`を除くobject エントリに含まれるほぼすべての情報を保持できます。主な目的は、関連するマップに同じ基本データを追加する必要性を減らし、シンボルの一貫性を維持することです。

マップ生成ファイルの object に追加されたエントリは、パレットで使用されている同じシンボルを上書きします。このようにして、パレットを使用し、必要に応じて各マップ生成に選択的な変更を加えることができます。これは、カーペット、コンクリートなどの複数の地面の地形にとって特に有用です。

地形は、マップ生成ファイルを介して置き換えると非常にうまく機能します。備品の上書きは成功が少ないですが、意図したとおりに機能するかどうかを明確にするために、さらにテストする必要があります。私は最近、パレットシンボルが値の配列を使用している場合、マップ生成エントリがそれを上書きできないことに気づきました。

例：マップ生成ファイル object のエントリ: `"palettes": [ "roof_palette" ],`

パレットのメタデータ:

```json
"type": "palette",
"id": "roof_palette",
```

その他、一連の object エントリのように見えます。例えば、roof_palette の場合:

```json
{
  "type": "palette",
  "id": "roof_palette",
  "terrain": {
    ".": "t_flat_roof",
    " ": "t_open_air",
    "o": "t_glass_roof",
    "_": "t_floor",
    "2": "t_gutter_north",
    "-": "t_gutter_south",
    "3": "t_gutter_east",
    "4": "t_gutter_downspout",
    "|": "t_gutter_west",
    "5": "t_gutter_drop",
    "#": "t_grate",
    "&": "t_null",
    ":": "t_null",
    "X": "t_null",
    "=": "t_null",
    "A": "t_null",
    "b": "t_null",
    "c": "t_null",
    "t": "t_null",
    "r": "t_null",
    "L": "t_null",
    "C": "t_null",
    "Y": "t_null",
    "y": "t_null"
  },
  "furniture": {
    "&": "f_roof_turbine_vent",
    "N": "f_TV_antenna",
    ":": "f_cellphone_booster",
    "X": "f_small_satelitte_dish",
    "~": "f_chimney",
    "=": "f_vent_pipe",
    "A": "f_air_conditioner",
    "b": "f_bench",
    "c": "f_counter",
    "t": "f_table",
    "r": "f_rack",
    "L": "f_locker",
    "C": ["f_crate_c", "f_cardboard_box"],
    "Y": "f_stool",
    "s": "f_sofa",
    "S": "f_sink",
    "e": "f_oven",
    "F": "f_fridge",
    "y": ["f_indoor_plant_y", "f_indoor_plant"]
  },
  "toilets": { "T": {} }
}
```

より複雑なパレットを確認したい場合は、
[data/json/mapgen_palettes/house_general_palette.json](https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/data/json/mapgen_palettes/house_general_palette.json)にある standard_domestic_palette が、すべての BN の家で機能するように設計されたパレットの良い例となります。これには、戦利品の出現定義が含まれており、家で使用されるほとんどの家具をカバーしています。また、特定の場所のニーズに合わせてマップ生成ファイルで使用できるように、一部のシンボルをオープンな状態にして残しました。

最後に、
[data/json/mapgen_palettes/house_w_palette.json](https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/data/json/mapgen_palettes/house_w_palette.json)にある一連の house_w パレットは、ネストされたマップ生成を使用する家のために連携して機能するように設計されています。基礎専用のパレット、ネスト専用のパレット、そして最後に、私が設計した家庭用の屋外ネスト化チャンク専用のパレットがあります。

## 最終コメント:

ここにある情報は、マップ生成を理解し、開始するのに十分なはずです。まだ多くのバリエーションがあります。

本ドキュメントではカバーされていないトピック:

- ネストされたマップとその配置。
- NPC の配置。
- 複雑な床オプションのための高度な地形トリック。
- トラップ、地形、およびそれらの相互作用。
- uupdate_mapgen (NPC およびプレイヤーによってトリガーされるマップの更新)。
- フィールド放出型の設置物。

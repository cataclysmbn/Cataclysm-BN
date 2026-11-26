# オーバーマップ生成

## 概要

**オーバーマップ**とは、ゲーム内で「_マップを見る (m)_」コマンドを使用したときに表示されるものです。

表示は次のようになります:

```
...>│<......................│..............P.FFFF├─FF...
..┌─┼─┐.....................│..............FFFFF┌┘FFFF..
..│>│^│.....................│..............FF┌─┬┘FFFFF.F
──┘...└──┐..............>│<.│..............F┌┘F│FFFFFFF─
.........└──┐........vvO>│v.│............FF┌┘FF│FFFFFFFF
............└┐.......─┬──┼─>│<........┌─T──┘FFF└┐FFFFFFF
.............│.......>│<$│S.│<.......┌┘..FFFFFFF│FFF┌─┐F
..O.........┌┴──┐....v│gF│O.│<v......│...FFFFFF┌┘F.F│F└─
.OOOOO.....─┘...└─────┼──┼──┼────────┤....FFFFF│FF..│FFF
.O.O.....dddd.......^p│^>│OO│<^......└─┐FFFFFFF├────┴─FF
.........dddd........>│tt│<>│..........│FFF..FF│FFFFF.FF
.........dddd......┌──┘tT├──...........│FFF..F┌┘FFFFFFFF
.........dddd......│....>│^^..H........└┐...FF│FFFFFFFFF
..................┌┘.....│<.............└─┐FF┌┘FFFFFFFFF
..................│......│................│FF│FFFFFFFFFF
.................┌┘......│................│F┌┘FFFFFFFFFF
...FFF...........│......┌┘...............┌┘.│FFFFFFFFFFF
FFFFFF...........│.....┌┘................│FF│FFFFFFFFFFF
FFFFFF..FFFF.....│.....B..........─┐.....│┌─┘F..FFFFFFFF
```

**オーバーマップ生成**における**オーバーマップ**とは、プレイヤーが直接操作するローカルマップよりも
抽象的なレベルでゲーム世界の場所を定義する、データや機能の集合です。ローカルマップを生成するために
必要なコンテキストを提供します。

たとえば、オーバーマップはゲームに次の情報を伝えます:

- 都市がどこにあるか
- 道路がどこにあるか
- 建物がどこにあるか
- 建物がどの種類か

一方、次の情報は伝えません:

- 実際の道路地形がどのような外観か
- 実際の建物配置がどのようなものか
- 建物内にどのアイテムがあるか

オーバーマップはゲーム世界の下書きであり、プレイヤーが探索するにつれて内容が埋められていくもの、と
考えるとわかりやすい場合があります。以降では、その下書きを作成する方法を説明します。

## 用語と型

まず、オーバーマップで使用するデータ型の一部について簡単に説明します。

### overmap_terrain

オーバーマップの基本単位は **overmap_terrain** です。これは、オーバーマップ上の1地点を表すための
ID、名前、シンボル、色などを定義します (その他の項目については後述します)。オーバーマップの大きさは、
overmap terrain で幅180、高さ180、深さ21です (深さは Z レベルを表します)。

次のオーバーマップの例では、各文字が特定の overmap terrain を参照するオーバーマップ上の1エントリに
対応します:

```
.v>│......FFF│FF
──>│<....FFFF│FF
<.>│<...FFFFF│FF
O.v│vv.vvv┌──┘FF
───┼──────┘.FFFF
F^^|^^^.^..F.FFF
```

たとえば、`F` は森林で、次のように定義されています:

```json
{
  "type": "overmap_terrain",
  "id": "forest",
  "name": "forest",
  "sym": "F",
  "color": "green"
}
```

また、`^` は家屋で、次のように定義されています:

```json
{
  "type": "overmap_terrain",
  "id": "house",
  "name": "house",
  "sym": "^",
  "color": "light_green"
}
```

重要なのは、オーバーマップ内にある同じ overmap terrain は、すべて単一の overmap terrain 定義を
参照するという点です。森林の色を `green` から `red` に変更すると、オーバーマップ内の森林はすべて
赤色になります。

### overmap_special / city_building

オーバーマップにおける次の重要な概念は、**overmap_special** と **city_building** です。
この2つは構造が似ており、複数の overmap terrain を1つの概念的な実体としてまとめ、オーバーマップ上に
配置する仕組みです。広大な邸宅、2階建ての家、大きな池、あるいは何の変哲もない書店の地下に秘密基地を
配置したい場合は、**overmap_special** または **city_building** を使用する必要があります。

これらの型は、実質的には構成要素となる overmap terrain の一覧、その相対的な配置、およびオーバーマップ
スペシャルや都市建物の配置を決めるためのデータです (都市からの距離、道路への接続の要否、森林・平原・
河川に配置できるかなど)。

### overmap_connection

道路に関連して、道路、下水道、地下鉄、鉄道、林道などの線状施設は、overmap terrain の属性と
**overmap_connection** という別の型の組み合わせによって管理されます。

overmap connection は、接続を作成できる overmap terrain の種類、接続を作成する「コスト」、接続時に
配置する地形の種類を定義します。たとえば、次のような規則を表現できます:

- 道路は平原、森林、沼地、河川に配置できる
- 平原は森林より優先され、森林は沼地より、沼地は河川より優先される
- 河川を横断する道路は橋になる

**overmap_connection** のデータは、以上の規則を適用し、2地点を結ぶ線状の道路施設を作成するために
使用されます。

### overmap_location

ここまで、オーバーマップスペシャル、都市建物、overmap connection を配置できる overmap terrain の型を
定義することに触れてきました。ただし実際には **overmap_terrain** の値を直接参照するのではなく、
**overmap_location** という別の型を利用します。

簡単に言えば、**overmap_location** は、名前を付けた **overmap_terrain** 値の集合です。

次に2つの単純な定義例を示します。

```json
{
    "type": "overmap_location",
    "id": "forest",
    "terrains": ["forest"]
},
{
    "type": "overmap_location",
    "id": "wilderness",
    "terrains": ["forest", "field"]
}
```

この仕組みにより、1つの overmap terrain を複数の location に所属させられます。また、間接参照の層が
設けられるため、新しい overmap terrain を追加する (または既存のものを削除する) とき、そのグループを
必要とするすべての **overmap_connection**、**overmap_special**、**city_building** ではなく、関連する
**overmap_location** のエントリだけを変更すれば済みます。

たとえば、森林の overmap terrain として `forest_thick` を追加する場合、次のように定義を更新するだけです:

```json
{
    "type": "overmap_location",
    "id": "forest",
    "terrains": ["forest", "forest_thick"]
},
{
    "type": "overmap_location",
    "id": "wilderness",
    "terrains": ["forest", "forest_thick", "field"]
}
```

<a id="overmap-terrain"></a>

## オーバーマップ地形

### 回転

overmap terrain が回転可能 (`NO_ROTATE` フラグを持たない) な場合、ゲームが JSON から定義を読み込む際に、
ここで定義した `id` へ `_north`、`_east`、`_south`、`_west` のいずれかを付加した回転別の定義を
自動生成します。overmap terrain を **overmap_special** または **city_building** の定義で使用する場合は
特に重要です。それらの回転が許可されているなら、参照する overmap terrain の向きを1つに決めて指定する
ことが望ましいためです (たとえば、すべて `_north` 版を指定します)。

### フィールド

| 識別子            | 説明                                                                                                                                                                                  |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `type`            | `overmap_terrain` でなければなりません。                                                                                                                                              |
| `id`              | 一意の ID。                                                                                                                                                                           |
| `name`            | ゲーム内で表示される場所の名前。                                                                                                                                                      |
| `sym`             | 場所の描画に使用するシンボル。`"F"` などを指定します (または `70` のような ASCII 値も使用できます)。                                                                                  |
| `color`           | シンボルの描画色。[COLOR.md](../graphics/color.md)を参照してください。                                                                                                                |
| `looks_like`      | この地形にグラフィックタイルがない場合に使用する、別の overmap terrain の ID。                                                                                                        |
| `connect_group`   | タイルセット側が対応している場合、この overmap terrain を隣接地形とグラフィック上で接続できることを指定します。同じ `connect_group` を持つすべての `overmap_terrain` と接続されます。 |
| `see_cost`        | オーバーマップ上でのプレイヤーの視界に影響します。値が大きいほど視界を強く遮ります。                                                                                                  |
| `travel_cost`     | 経路探索のコストに影響します。値が大きいほど通過しにくくなります (参考: 森林 = 10)。                                                                                                  |
| `extras`          | region_settings で名前を付けた `map_extras` への参照。適用できる map extra を定義します。                                                                                             |
| `mondensity`      | 隣接する overmap terrain の値と合算され、この場所にスポーンするモンスターの密度に影響します。                                                                                         |
| `spawns`          | マップ生成時に一度だけ追加されるスポーン。モンスターグループ、確率 (%)、個体数の範囲 (最小/最大) を指定します。                                                                       |
| `flags`           | [json_flags.md](../json_flags.md)の「オーバーマップ地形」を参照してください。                                                                                                         |
| `mapgen`          | C++ のマップ生成関数を指定します。使用せず、JSON を使ってください。                                                                                                                   |
| `mapgen_straight` | LINEAR 施設の直線形に使う C++ マップ生成関数を指定します。代わりに JSON を推奨します。                                                                                                |
| `mapgen_curved`   | LINEAR 施設の曲線形に使う C++ マップ生成関数を指定します。代わりに JSON を推奨します。                                                                                                |
| `mapgen_end`      | LINEAR 施設の終端形に使う C++ マップ生成関数を指定します。代わりに JSON を推奨します。                                                                                                |
| `mapgen_tee`      | LINEAR 施設の T 字形に使う C++ マップ生成関数を指定します。代わりに JSON を推奨します。                                                                                               |
| `mapgen_four_way` | LINEAR 施設の四方向形に使う C++ マップ生成関数を指定します。代わりに JSON を推奨します。                                                                                              |

### 例

実際の `overmap_terrain` でこれらすべてを同時に定義することはありませんが、網羅的な例として示します:

```json
{
  "type": "overmap_terrain",
  "id": "field",
  "name": "field",
  "sym": ".",
  "color": "brown",
  "looks_like": "forest",
  "see_cost": 2,
  "extras": "field",
  "mondensity": 2,
  "spawns": { "group": "GROUP_FOREST", "population": [0, 1], "chance": 13 },
  "flags": ["NO_ROTATE"],
  "mapgen": [{ "method": "builtin", "name": "bridge" }],
  "mapgen_straight": [{ "method": "builtin", "name": "road_straight" }],
  "mapgen_curved": [{ "method": "builtin", "name": "road_curved" }],
  "mapgen_end": [{ "method": "builtin", "name": "road_end" }],
  "mapgen_tee": [{ "method": "builtin", "name": "road_tee" }],
  "mapgen_four_way": [{ "method": "builtin", "name": "road_four_way" }]
}
```

## オーバーマップスペシャル

オーバーマップスペシャルは、都市の生成処理が完了した後にオーバーマップへ配置される実体であり、
**city_building** 型に対する「都市外」の存在です。通常は複数の overmap terrain で構成されますが、
必ずしもそうとは限りません。道路、下水道、地下鉄などの overmap connection を持つことができ、配置を
制御する規則が JSON で定義されています。

## 配置ルール

マップ生成では、各スペシャルについて、まず `occurrences` の `min` から `max` までの乱数を振り、配置する
インスタンス数を決めます。その数は、設定されたスペシャル密度、地形比率、混雑率という複数の乗数で
調整されます。

「地形比率」は、オーバーマップ上の陸地タイルと湖タイルの比率を表す数値です。30% が水没した
オーバーマップでは、陸地スペシャルの乗数が0.7、湖スペシャルの乗数が0.3になります。

「混雑率」は、オーバーマップの総面積と、現在の範囲および密度設定ですべてのスペシャルを配置するために
必要と見込まれる平均面積との比率を表す数値です。上限があり、通常は1倍のままです。ただし、物理的に
配置可能な数を超えるスペシャルをスポーンさせようとすると低下します。

これらすべてを考慮した結果が2.4のような値になった場合、そのスペシャルは60%の確率で2個、40%の確率で
3個配置されます。最終的な数が `occurrences` の調整前の `min` 値を下回ることはありません。

正確な数が決まると、マップ生成はスペシャルをスポーンできる場所を探します。スペシャルが都市に依存する
場合、そのインスタンスは条件を満たす別々の都市周辺へ分散されます。たとえば3個の配置を試み、必要な
規模の都市がオーバーマップに2つある場合、同じ都市の近くには2個までしか配置されません。

### 固定スペシャルと可変スペシャル

オーバーマップスペシャルには、固定と可変の2つのサブタイプがあります。固定オーバーマップスペシャルは、
複数の OMT にまたがり得る固定レイアウトを持ちます。回転はできますが (後述)、基本的な外観は常に同じです。

可変オーバーマップスペシャルは、より柔軟なレイアウトを持ちます。overmap terrain の集合と、それらを
組み合わせる方法によって定義されます。ピースを複数の方法で組み合わせられるジグソーパズルに似ています。
巨大アリが地下に掘ったトンネルや、形の異なる棟や増築部分が広がる大規模建物など、より有機的な形状を
作成できます。

可変スペシャルをエラーなく確実に配置できるようにするには、設計時に細心の注意が必要です。

### 回転

一般に、オーバーマップスペシャルは回転可能に定義することが望まれます。ゲーム世界に変化が生まれ、
スペシャルの配置時に候補となる有効な場所も増えるためです。オーバーマップスペシャルの回転と、その
構成要素である overmap terrain の回転には関連があるため、スペシャルからは対応する overmap terrain の
特定の回転版を参照する必要があります。通常は、マップ生成 JSON の定義方向に対応する `_north` 版を
使用します。

### 場所

オーバーマップスペシャルには、配置可能な有効場所 (`overmap_location`) を指定する仕組みが2つあります。
`overmaps` の各エントリに有効場所を指定できます。これは、スペシャルの部分ごとに異なる種類の地形へ
配置できる場合 (たとえば、一部を岸に、一部を水上に配置する桟橋) に便利です。すべて同じ値なら、代わりに
最上位の `locations` キーでスペシャル全体の場所を指定できます。個別エントリの値は最上位の値より優先
されるため、最上位に共通値を定義し、異なるエントリだけに個別指定できます。

### フィールド

| 識別子          | 説明                                                                                                |
| --------------- | --------------------------------------------------------------------------------------------------- |
| `type`          | `"overmap_special"` でなければなりません。                                                          |
| `id`            | 一意の ID。                                                                                         |
| `connections`   | overmap connection と、スペシャル内での相対位置 `[ x, y, z ]` の一覧。                              |
| `place_nested`  | このスペシャルを基準とする、ネストしたスペシャルの `{ "point": [x, y, z], "special": id }` 配列。   |
| `subtype`       | `"fixed"` または `"mutable"`。省略時は `"fixed"` です。                                             |
| `locations`     | スペシャルを配置できる `overmap_location` ID の一覧。                                               |
| `city_distance` | スペシャルを配置できる都市からの最小/最大距離。無制限には -1 を使用します。                         |
| `city_sizes`    | スペシャルを近くに配置できる都市規模の最小/最大値。無制限には -1 を使用します。                     |
| `occurrences`   | スペシャル配置時の最小/最大出現数。UNIQUE フラグが設定されている場合は、Y 分の X の確率になります。 |
| `flags`         | [json_flags.md](../json_flags.md)の「オーバーマップのスペシャル」を参照してください。               |
| `rotate`        | スペシャルを回転できるかどうか。省略時は true です。                                                |

サブタイプに応じて、さらに次のフィールドを使用します:

#### 固定オーバーマップスペシャルの追加フィールド

| 識別子               | 説明                                                                                                                                                                                             |
| -------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `overmaps`           | overmap terrain と、スペシャル内での相対位置 `[ x, y, z ]` の一覧。                                                                                                                              |
| `absolute_spawn_loc` | このスペシャルをスポーンする絶対オーバーマップを定義する `{ "x": 0, "y": 0, "do_absolute_spawn_loc": bool }` オブジェクト。`do_absolute_spawn_loc` は省略可能で、上書き用 MOD のためにあります。 |

#### 可変オーバーマップスペシャルの追加フィールド

| 識別子                     | 説明                                                                                              |
| -------------------------- | ------------------------------------------------------------------------------------------------- |
| `check_for_locations`      | 初期配置に必要な場所を定義するペア `[ [ x, y, z ], [ locations, ... ] ]` の一覧。                 |
| `check_for_locations_area` | 明示的な `check_for_locations` ペアに加えて検査する、check_for_locations の領域オブジェクト一覧。 |
| `overmaps`                 | 各種 overmap と、それらを互いに接続する方法の定義。                                               |
| `root`                     | 可変スペシャルを成長させる起点となる最初の overmap。                                              |
| `shared`                   | `"id": value` として定義し、スペシャルの一部をスケーリングするために使用できる乗数の一覧。        |
| `phases`                   | ルート OMT からオーバーマップスペシャルを成長させる方法の指定。                                   |

### 固定スペシャルの例

```json
[
  {
    "type": "overmap_special",
    "id": "campground",
    "overmaps": [
      { "point": [0, 0, 0], "overmap": "campground_1a_north", "locations": ["forest_edge"] },
      { "point": [1, 0, 0], "overmap": "campground_1b_north" },
      { "point": [0, 1, 0], "overmap": "campground_2a_north" },
      { "point": [1, 1, 0], "overmap": "campground_2b_north" }
    ],
    "connections": [{ "point": [1, -1, 0], "connection": "local_road", "from": [1, 0, 0] }],
    "locations": ["forest"],
    "city_distance": [10, -1],
    "city_sizes": [3, 12],
    "occurrences": [0, 5],
    "flags": ["CLASSIC"],
    "rotate": true
  }
]
```

### 固定スペシャルの overmap

| 識別子      | 説明                                                             |
| ----------- | ---------------------------------------------------------------- |
| `point`     | スペシャル内の overmap terrain の `[ x, y, z]`。                 |
| `overmap`   | その場所に配置する `overmap_terrain` の ID。                     |
| `locations` | この overmap terrain を配置できる `overmap_location` ID の一覧。 |

### 接続

| 識別子       | 説明                                                                                   |
| ------------ | -------------------------------------------------------------------------------------- |
| `point`      | 接続端点の `[ x, y, z]`。そのスペシャルの overmap terrain エントリとは重複できません。 |
| `connection` | 構築する `overmap_connection` の ID。                                                  |
| `from`       | 接続の起点として扱う、スペシャル内の省略可能な地点 `[ x, y, z]`。                      |

### 可変スペシャルの例

```json
[
  {
    "type": "overmap_special",
    "id": "anthill",
    "subtype": "mutable",
    "locations": ["subterranean_empty"],
    "city_distance": [25, -1],
    "city_sizes": [0, 20],
    "occurrences": [0, 1],
    "flags": ["CLASSIC", "WILDERNESS"],
    "check_for_locations": [
      [[0, 0, 0], ["land"]],
      [[0, 0, -1], ["subterranean_empty"]],
      [[1, 0, -1], ["subterranean_empty"]],
      [[0, 1, -1], ["subterranean_empty"]],
      [[-1, 0, -1], ["subterranean_empty"]],
      [[0, -1, -1], ["subterranean_empty"]]
    ],
    "//1": "Same as writing out 'check_for_locations' 9 times with different points.",
    "check_for_locations_area": [
        [ { "type": [ "subterranean_empty" ], "from": [ 1, 1, -2 ], "to": [ -1, -1, -2 ] } ]
    ],
    "//2": "The anthill will have 3 possible sizes",
    "shared": { "size": [ 1, 3 ] },
    "joins": ["surface_to_tunnel", "tunnel_to_tunnel"],
    "overmaps": {
      "surface": { "overmap": "anthill", "below": "surface_to_tunnel", "locations": ["land"] },
      "below_entrance": {
        "overmap": "ants_nesw",
        "above": "surface_to_tunnel",
        "north": "tunnel_to_tunnel",
        "east": "tunnel_to_tunnel",
        "south": "tunnel_to_tunnel",
        "west": "tunnel_to_tunnel"
      },
      "crossroads": {
        "overmap": "ants_nesw",
        "north": "tunnel_to_tunnel",
        "east": "tunnel_to_tunnel",
        "south": "tunnel_to_tunnel",
        "west": "tunnel_to_tunnel"
      },
      "tee": {
        "overmap": "ants_nes",
        "north": "tunnel_to_tunnel",
        "east": "tunnel_to_tunnel",
        "south": "tunnel_to_tunnel"
      },
      "straight_tunnel": {
        "overmap": "ants_ns",
        "north": "tunnel_to_tunnel",
        "south": "tunnel_to_tunnel"
      },
      "corner": { "overmap": "ants_ne", "north": "tunnel_to_tunnel", "east": "tunnel_to_tunnel" },
      "dead_end": { "overmap": "ants_end_south", "north": "tunnel_to_tunnel" },
      "queen": { "overmap": "ants_queen", "north": "tunnel_to_tunnel" },
      "larvae": { "overmap": "ants_larvae", "north": "tunnel_to_tunnel" },
      "food": { "overmap": "ants_food", "north": "tunnel_to_tunnel" }
    },
    "root": "surface",
    "phases": [
      [{ "overmap": "below_entrance", "max": 1 }],
      [
        "//1": "Shared multiplier 'size' will affect the size",
        "//2": "of the tunnel system generated in this phase.",
        { "overmap": "straight_tunnel", "max": 10, "scale": "size" },
        { "overmap": "corner", "max": { "poisson": 2.5 }, "scale": "size" },
        { "overmap": "tee", "max": 5, "scale": "size" }
      ],
      [{ "overmap": "queen", "max": 1 }],
      [{ "overmap": "food", "max": 5 }, { "overmap": "larvae", "max": 5 }],
      [
        { "overmap": "dead_end", "weight": 2000 },
        { "overmap": "straight_tunnel", "weight": 100 },
        { "overmap": "corner", "weight": 100 },
        { "overmap": "tee", "weight": 10 },
        { "overmap": "crossroads", "weight": 1 }
      ]
    ]
  }
]
```

### 可変スペシャルの配置方法

#### Overmap と join

注: 以下の文脈における「overmap」は、「overmap level」や、ほぼ無限のワールドマップを分割する OMT の
180x180チャンクとしての「overmap」と混同しないでください。

可変スペシャルは、それを構築する OMT を定義する _overmap_ の集合と、それらを互いに接続できる方法を
定義する _join_ を持ちます。各 overmap では、それぞれの辺 (東西南北の4方向と上下) に join を指定できます。
各 join は、その方向に隣接する overmap の反対側の join と一致しなければなりません。

上の例では、`surface` overmap に `"below": "surface_to_tunnel"` が指定されています。つまり、その下側の
join は `surface_to_tunnel` でなければなりません。したがって、下にある overmap は
`"above": "surface_to_tunnel"` を指定する必要があります。該当するのは `below_entrance` overmap だけなので、
この overmap を `surface` overmap の下に配置する必要があります。

overmap は常に回転できるため、`north` 制約が別の方向に対応する場合もあります。したがって、上記の
`dead_end` overmap はどの方向の行き止まりトンネルも表現できますが、生成されたマップが正しくなるには、
選択した OMT `ants_end_south` と `north` join の向きが一致していることが重要です。

overmap には接続も指定できます。たとえば、次のように overmap を定義できます:

```json
"where_road_connects": {
  "overmap": "road_end_north",
  "west": "parking_lot_to_road",
  "connections": { "north": { "connection": "local_road" } }
}
```

可変スペシャルの配置が完了すると、固定スペシャルの接続と同じ方法で、この overmap の北端から
`local_road` 接続が構築されます (ここでも「北」は相対的な用語で、overmap とともに回転します)。可変
スペシャルは「既存の」接続には対応していません。

#### レイアウトフェーズ

すべての join と overmap を定義した後、`root` と `phases` でスペシャルの配置方法を指定します。

`root` は、このスペシャルの原点に最初に配置する overmap を指定します。

続いて `phases` には、追加の overmap を配置するための成長フェーズ一覧を指定します。これらのフェーズは
厳密に順番どおり処理されます。

各 _フェーズ_ は規則の一覧です。各 _規則_ は overmap と、整数の `max`、`weight`、またはその両方を
指定します。

weight は必ず単純な整数でなければなりませんが、`max` には整数の確率分布を定義するオブジェクトも指定
できます。スペシャルがスポーンするたびに、その分布から値が1つ抽出されます。ポアソン分布は
`{ "poisson": 5 }` のようなオブジェクトで指定し、5が分布の平均 (λ) になります。一様分布は `[min, max]`
のペアで指定できます。また、スペシャル内の別の乗数 `shared` で `max` を `scale` し、複数の異なる
複数の異なる overmap の数を互いに比例させて増減できます。各 `shared` 乗数も `max` 値と同様に確率分布で
定義でき、スペシャルの配置時に一度だけ抽出されます。

各フェーズでは、既存の overmap にある未解決の join を探し、そのフェーズの規則で使用可能な overmap から
join を満たすものを見つけようとします。このスペシャルの join 定義一覧で先に記載された join が優先
されますが、同じ ID の最優先 join が複数ある場合はランダムに1つ選ばれます。

まず、特定の場所の join を満たせる規則だけに絞り込み、その一覧から重み付き抽選を行います。重みには、
規則で指定した `max` と `weight` の小さい方が使用されます。`max` と `weight` の違いは、規則を使用する
たびに `max` が1減ることです。そのため、`max` はその規則を選べる回数を制限します。`weight` だけを
指定した規則は何回でも選択できます。

現在のフェーズのどの規則でも特定の場所の join を満たせない場合、その場所は保留され、後のフェーズで
再試行されます。

すべての join が満たされるか保留されると、そのフェーズは終了し、次のフェーズへ進みます。

すべてのフェーズが完了しても未解決の join が残っている場合はエラーとなり、詳細を示す debugmsg が
表示されます。

#### チャンク

フェーズ内の配置規則では、特定の構成で配置する複数の overmap を指定できます。単一の OMT より大きな
施設を配置したい場合に便利です。次に microlab の例を示します。

```json
{
  "name": "subway_chunk_at_-2",
  "chunk": [
    { "overmap": "microlab_sub_entry", "pos": [0, 0, 0], "rot": "north" },
    { "overmap": "microlab_sub_station", "pos": [0, -1, 0] },
    { "overmap": "microlab_subway", "pos": [0, -2, 0] }
  ],
  "max": 1
}
```

チャンクの `"name"` は、問題が発生したときのデバッグメッセージだけに使用されます。`"max"` と
`"weight"` は前述のとおり処理されます。

新しい機能は `"chunk"` で、overmap とその相対位置および回転の一覧を指定します。overmap には、この
スペシャル用に定義したものを使用します。既定の回転は `"north"` なので、指定しても動作は変わりませんが、
ここでは構文を示すために含めています。

位置と回転は相対値です。すべての overmap を剛体のようにまとめて移動・回転させる限り、チャンクは
任意のオフセットと回転で配置できます。

#### 配置エラーを防ぐ手法

可変スペシャルの配置には、こうしたエラーを防ぐために役立つ追加機能があります。

##### `check_for_locations`

`check_for_locations` は、スペシャルの配置を試みる前に検査する追加制約の一覧を定義します。各制約は、
位置 (ルートからの相対位置) と location 集合のペアです。各位置に既に存在する OMT が指定 location の
いずれかに含まれていなければ、配置試行は中止されます。

`check_for_locations` 制約により、`below_entrance` overmap をルートの下に配置でき、東西南北に隣接する
4つの OMT がすべて `subterranean_empty` であることが保証されます。これは、`below_entrance` の残り4つの
join を満たす overmap を追加するために必要です。

`check_for_locations_area` を使うと、地点ごとに `check_for_locations` を繰り返さずに、検査する領域を
定義できます。

##### `into_locations`

各 join には、関連する location の一覧もあります。既定ではスペシャルの location が使用されますが、
次のように特定の join だけ上書きできます:

```json
"joins": [
  { "id": "surface_to_surface", "into_locations": [ "land" ] },
  "tunnel_to_tunnel"
]
```

未解決の join を満たすために overmap を配置するとき、特定の場所に隣接する既存の join を満たすだけでは
不十分です。その overmap が持つ join のうち、既に一致しているもの以外に残るすべての join は、その join の
location と一致する地形の OMT を指していなければなりません。

上記の蟻塚の例では、これら2つの追加機能により、必ず配置に成功し、満たされない join が残らないことを
確認できます。

続くいくつかの配置フェーズでは、さまざまなトンネルを配置しようとします。join 制約により、満たされて
いない join (トンネルの開いた端) は必ず `subterranean_empty` OMT を指すようになります。

##### 最終フェーズでの完全な網羅を保証する

最終フェーズには、蟻塚をそれ以上成長させずに、満たされていない join をすべて塞ぐための5つの異なる
規則があります。join の少ない overmap を使用する規則ほど、高い重みを与えることが重要です。通常は、
満たされていない join をすべて `dead_end` で単純に閉じます。ただし、2つの未解決 join が同じ OMT を
指す可能性にも対処する必要があります。その場合、`dead_end` は適合せず (候補から除外されます)、
`straight_tunnel` または `corner` が適合します。この2つは重みが最も大きいため、いずれかが選ばれる
可能性が高くなります。`tee` は適合しても、新たな未解決 join が生じてトンネルがさらに成長するため、
選ばせたくありません。ただし、この状況で `tee` がときどき選ばれても大きな問題にはなりません。
新しい join は、おそらく `dead_end` で単純に満たされます。

独自の可変オーバーマップスペシャルを設計するときは、こうした組み合わせを検討し、最終フェーズ終了時に
すべての join が満たされるようにする必要があります。

#### `optional` join

最終フェーズで起こり得るすべての状況を満たす規則を大量に用意する代わりに、`optional` join を使って
簡単にできる場合があります。この機能は他のフェーズでも使用できます。

可変スペシャルの overmap に関連する join を指定するときは、`Crater` オーバーマップスペシャルの次の例の
ように、型を付けて詳細に定義できます:

```json
"overmaps": {
  "crater_core": {
    "overmap": "crater_core",
    "north": "crater_to_crater",
    "east": "crater_to_crater",
    "south": "crater_to_crater",
    "west": "crater_to_crater"
  },
  "crater_edge": {
    "overmap": "crater",
    "north": "crater_to_crater",
    "east": { "id": "crater_to_crater", "type": "available" },
    "south": { "id": "crater_to_crater", "type": "available" },
    "west": { "id": "crater_to_crater", "type": "available" }
  }
},
```

`crater_edge` の定義には、北への `mandatory` join が1つ、その他の方角への `available` join が3つあります。
`available` join は未解決の join とは見なされないため、それが原因で追加の overmap が配置されることは
ありません。ただし、既存の未解決 join を満たすために必要なら、特定のタイルへ向かう別の join を
満たすことができます。

overmap は、できる限り多くの `mandatory` join を満たし、現在 join を必要としない別方向を `available` join が
指すように必ず回転されます。

そのため、この `crater_edge` overmap は、新たな未解決 join を生成せずに `Crater` スペシャルの任意の
未解決 join を満たせます。最終フェーズでスペシャルを仕上げるのに最適です。

3つ目の join 型である `optional` は、上記2つを組み合わせたものです。成長の起点となる新たな未解決 join を
積極的に生成しますが、その join は必須ではなく、未解決のまま残すことができます。

`optional` join と `available` join は解決する必要がないため、望ましくない場所で終端する可能性があります。
これを防ぐには `reject` 型の疑似 join を追加します。これにより、同じ ID の他の join がそこへ接続することを
禁止できます。

#### 非対称 join

2つの異なる OMT を接続する一方で、どちらも同種同士では接続させたくない場合があります。この場合、
両方に同じ join は使用できません。代わりに、一方を他方の反対側として指定し、対になる2つの join を
定義できます。

join の両側で異なる location 制約が必要な場合にも、この状況が発生します。たとえば蟻塚では、地表部分と
地下部分に異なる location が必要です。次のように地表とトンネル間の join を非対称にすれば、その join 定義を
改善できます:

```json
"joins": [
  { "id": "surface_to_tunnel", "opposite": "tunnel_to_surface" },
  { "id": "tunnel_to_surface", "opposite": "surface_to_tunnel", "into_locations": [ "land" ] },
  "tunnel_to_tunnel"
],
```

このとおり、対になった `tunnel_to_surface` 側は地表を指すため、`into_locations` の既定値を上書きする
必要があります。

#### 代替 join

可変スペシャルの次のフェーズで、既存の未解決 join へ接続できるようにしつつ、その型の未解決 join を
新たに生成させたくない場合があります。これにより、以前の部分と新しい部分を明確に分離できます。

たとえば `microlab_mutable` スペシャルが該当します。このスペシャルでは、ある程度規則的に並ぶ `hallway`
OMT の周囲を、ひとかたまりの `microlab` OMT が囲んでいます。廊下には側面の外向きに
`hallway_to_microlab` join があるため、それと一致させるには `microlab` OMT に `microlab_to_hallway` join
(`hallway_to_microlab` の反対側) が必要です。

しかし、`microlab` OMT の未解決の辺すべてで追加の廊下を必要とする状態にはしたくないため、通常は
`microlab_to_microlab` join を使用させます。join 型ごとの数が異なる `microlab` の亜種を大量に作らずに、
一見矛盾するこれらの要件を満たすにはどうすればよいでしょうか。ここで代替 join が役立ちます。

`microlab` overmap の定義は次のようになります:

```json
"microlab": {
  "overmap": "microlab_generic",
  "north": { "id": "microlab_to_microlab", "alternatives": [ "microlab_to_hallway" ] },
  "east": { "id": "microlab_to_microlab", "alternatives": [ "microlab_to_hallway" ] },
  "south": { "id": "microlab_to_microlab", "alternatives": [ "microlab_to_hallway" ] },
  "west": { "id": "microlab_to_microlab", "alternatives": [ "microlab_to_hallway" ] }
},
```

これにより、オーバーマップ上に既に配置されている廊下と接続できますが、新しい未解決 join は追加の
`microlab` だけと一致します。

#### 新しい可変スペシャルをテストする

可変スペシャルの配置エラーを網羅的にテストしたく、ゲームをコンパイルできる環境がある場合は、
`tests/overmap_test.cpp` にある既存のテストを使うのが簡単です。

そのファイルで `TEST_CASE( "mutable_overmap_placement"` を探してください。関数の先頭には、テストで
スポーンを試みる可変スペシャル ID の一覧があります。そのうち1つを新しいスペシャルの ID に置き換え、
再コンパイルしてテストを実行します。

テストはスペシャルの配置を数千回試行するため、配置が失敗する原因のほとんどを検出できるはずです。

### Join

join 定義には単純な文字列を使用でき、その文字列が ID になります。または、次のキーの一部を持つ辞書として
定義できます:

| 識別子           | 説明                                                        |
| ---------------- | ----------------------------------------------------------- |
| `id`             | 定義する join の ID。                                       |
| `opposite`       | 隣接地形からこの join に一致させる必要がある join の ID。   |
| `into_locations` | この join が指すことのできる `overmap_location` ID の一覧。 |

### 可変スペシャルの overmap

overmap は JSON 辞書です。各 overmap には、このスペシャル内でのみ使う ID を JSON 辞書のキーとして設定する
必要があります。値には次のフィールドを指定できます:

| 識別子      | 説明                                                                                                                      |
| ----------- | ------------------------------------------------------------------------------------------------------------------------- |
| `overmap`   | その場所に配置する `overmap_terrain` の ID。                                                                              |
| `locations` | この overmap terrain を配置できる `overmap_location` ID の一覧。省略時は、スペシャル定義の `locations` 値が使用されます。 |
| `north`     | この OMT の北端と揃える必要がある join。                                                                                  |
| `east`      | この OMT の東端と揃える必要がある join。                                                                                  |
| `south`     | この OMT の南端と揃える必要がある join。                                                                                  |
| `west`      | この OMT の西端と揃える必要がある join。                                                                                  |
| `above`     | この OMT を上の OMT と接続する必要がある join。                                                                           |
| `below`     | この OMT を下の OMT と接続する必要がある join。                                                                           |

各方向に関連付ける join には単純な文字列を使用でき、join ID として解釈されます。または、次のキーを持つ
JSON オブジェクトとして指定できます:

| 識別子         | 説明                                                                                                                                                                                  |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `id`           | ここで使用する join の ID。                                                                                                                                                           |
| `type`         | `"mandatory"` または `"available"`。既定値は `"mandatory"` です。                                                                                                                     |
| `alternatives` | `id` に記載したものの代わりに使用できる join ID の一覧。ただし、この overmap を配置するときだけ使用されます。配置によって生成される未解決 join は、プライマリ join の `id` だけです。 |

### 生成規則

| 識別子                   | 説明                                                                                                 |
| ------------------------ | ---------------------------------------------------------------------------------------------------- |
| `overmap` または `chunk` | 配置する `overmap` の ID、またはチャンク構成。                                                       |
| `join`                   | 現在のフェーズ中に解決する必要がある `join` の ID。                                                  |
| `z`                      | このフェーズの Z レベル制限。                                                                        |
| `om_pos`                 | このフェーズの実行を許可する overmap (ワールドの180x180チャンク) の絶対座標 `[ x, y ]`。             |
| `rotate`                 | 現在のピースを回転できるかを表す true または false。スペシャルの回転可能プロパティより優先されます。 |
| `max`                    | この規則を使用できる最大回数。                                                                       |
| `scale`                  | `max` のスケーリングに使用する共有乗数の ID。                                                        |
| `weight`                 | この規則を選択するときの重み。                                                                       |

Z レベル制限は、数値、絶対座標制限を伴う `["min", "mix"]` 範囲、これまでに配置したスペシャルの別タイルの
境界を参照する `"top"` / `"bottom"` 文字列、および `"top"` / `"bottom"` プロパティとその境界からの
オフセットを持つオブジェクトに対応しています。

`max` と `weight` のいずれかを指定する必要があります。`weight` が省略されている場合は、`max` が重みとして
使用されます。

## 都市建物

都市建物は、都市の生成処理中にオーバーマップへ配置される実体であり、**overmap_special** 型に対する
「都市内」の存在です。都市建物の定義はオーバーマップスペシャルの定義の一部と同じなので、ここでは詳細を
繰り返しません。

### 必須オーバーマップスペシャル / 地域設定

都市建物にはオーバーマップスペシャルと同じ数量制限はなく、`occurrences` 属性はまったく適用されません。
代わりに、都市建物の配置は `region_settings` 内でその都市建物に割り当てた頻度によって決まります。詳細は
[REGION_SETTINGS.md](region_settings.md)を参照してください。

### フィールド

| 識別子      | 説明                                                                                                        |
| ----------- | ----------------------------------------------------------------------------------------------------------- |
| `type`      | `"city_building"` でなければなりません。                                                                    |
| `id`        | 一意の ID。                                                                                                 |
| `overmaps`  | `overmap_special` と同じですが、すべての地点の x と y の値が0以上でなければならないという注意点があります。 |
| `locations` | `overmap_special` と同じです。                                                                              |
| `flags`     | `overmap_special` と同じです。                                                                              |
| `rotate`    | `overmap_special` と同じです。                                                                              |

### 例

```json
[
  {
    "type": "city_building",
    "id": "zoo",
    "locations": ["land"],
    "overmaps": [
      { "point": [0, 0, 0], "overmap": "zoo_0_0_north" },
      { "point": [1, 0, 0], "overmap": "zoo_1_0_north" },
      { "point": [2, 0, 0], "overmap": "zoo_2_0_north" },
      { "point": [0, 1, 0], "overmap": "zoo_0_1_north" },
      { "point": [1, 1, 0], "overmap": "zoo_1_1_north" },
      { "point": [2, 1, 0], "overmap": "zoo_2_1_north" },
      { "point": [0, 2, 0], "overmap": "zoo_0_2_north" },
      { "point": [1, 2, 0], "overmap": "zoo_1_2_north" },
      { "point": [2, 2, 0], "overmap": "zoo_2_2_north" }
    ],
    "flags": ["CLASSIC"],
    "rotate": true
  }
]
```

## オーバーマップ接続

### フィールド

| 識別子            | 説明                                                                              |
| ----------------- | --------------------------------------------------------------------------------- |
| `type`            | `"overmap_connection"` でなければなりません。                                     |
| `id`              | 一意の ID。                                                                       |
| `default_terrain` | 無方向接続と存在検査に使用する既定の `overmap_terrain`。                          |
| `subtypes`        | 有効場所、地形コスト、結果となる overmap terrain の決定に使用するエントリの一覧。 |
| `layout`          | 省略可能な接続レイアウト。既定値は `city` です。                                  |

`city` レイアウトでは各接続地点が最寄りの都市中心部へ接続されます。`p2p` レイアウトでは各接続地点が
同じ型の最寄りの接続へ接続されます。

### 例

```json
[
  {
    "type": "overmap_connection",
    "id": "local_road",
    "subtypes": [
      { "terrain": "road", "locations": ["field", "road"] },
      { "terrain": "road", "locations": ["forest_without_trail"], "basic_cost": 20 },
      { "terrain": "road", "locations": ["forest_trail"], "basic_cost": 25 },
      { "terrain": "road", "locations": ["swamp"], "basic_cost": 40 },
      { "terrain": "road_nesw_manhole", "locations": [] },
      { "terrain": "bridge", "locations": ["water"], "basic_cost": 120 }
    ]
  },
  {
    "type": "overmap_connection",
    "id": "subway_tunnel",
    "subtypes": [
      { "terrain": "subway", "locations": ["subterranean_subway"], "flags": ["ORTHOGONAL"] }
    ]
  }
]
```

### サブタイプ

| 識別子       | 説明                                                                                                                              |
| ------------ | --------------------------------------------------------------------------------------------------------------------------------- |
| `terrain`    | 配置場所が `locations` と一致するときに配置する `overmap_terrain`。                                                               |
| `locations`  | このサブタイプを適用する `overmap_location` の一覧。空にすることもでき、その場合は `terrain` がそのまま有効であることを示します。 |
| `basic_cost` | 経路探索時におけるこのサブタイプのコスト。既定値は0です。                                                                         |
| `weight`     | 他の項目より優先して出現するための重み。既定値は0です。                                                                           |
| `flags`      | [json_flags.md](../json_flags.md)の「オーバーマップ接続」を参照してください。                                                     |

## オーバーマップ場所

### フィールド

| 識別子     | 説明                                                      |
| ---------- | --------------------------------------------------------- |
| `type`     | `"overmap_location"` でなければなりません。               |
| `id`       | 一意の ID。                                               |
| `terrains` | この location の一部と見なせる `overmap_terrain` の一覧。 |

### 例

```json
[
  {
    "type": "overmap_location",
    "id": "wilderness",
    "terrains": ["forest", "forest_thick", "field", "forest_trail"]
  }
]
```

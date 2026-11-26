# 地域設定

**region_settings** は、地域全体のマップ生成に適用される属性を定義します。一般設定では、デフォルトのオーバーマップ地形と地表被覆を定義します。その他のセクションは次のとおりです。

| セクション                      | 説明                                                                 |
| ------------------------------- | -------------------------------------------------------------------- |
| `region_terrain_and_furniture`  | 地域用の地形や家具を実際の種類へ解決する方法を定義します。           |
| `field_coverage`                | `field` オーバーマップ地形を覆う植物相を定義します。                 |
| `overmap_lake_settings`         | 地域内に湖を生成するためのパラメーターを定義します。                 |
| `overmap_forest_settings`       | 地域内に森林や沼地を生成するためのパラメーターを定義します。         |
| `forest_mapgen_settings`        | `forest` 地形を覆う植物相（およびその他の「もの」）を定義します。    |
| `forest_trail_settings`         | 林道のオーバーマップ上およびローカルマップ上の構造を定義します。     |
| `city`                          | 都市の構造的な構成を定義します。                                     |
| `map_extras`                    | オーバーマップ地形から参照されるマップ追加要素グループを定義します。 |
| `weather`                       | 地域の基本的な天候属性を定義します。                                 |
| `overmap_feature_flag_settings` | オーバーマップの特徴のフラグに基づいて実行する処理を定義します。     |

デフォルト地域では、すべての属性とセクションが必須であることに注意してください。

### フィールド

| 識別子                | 説明                                                           |
| --------------------- | -------------------------------------------------------------- |
| `type`                | 種類の識別子。"region_settings" でなければなりません。         |
| `id`                  | この地域の一意な識別子。                                       |
| `default_oter`        | この地域のデフォルトのオーバーマップ地形。                     |
| `default_groundcover` | デフォルトの地表被覆として適用される地形の種類と重みのリスト。 |

### 例

```json
{
  "type": "region_settings",
  "id": "default",
  "default_oter": "field",
  "default_groundcover": [
    ["t_grass", 4],
    ["t_dirt", 1]
  ]
}
```

## 地域の地形 / 家具

**region_terrain_and_furniture** セクションは、地域用の地形や家具を、その地域における実際の地形や家具の種類へ解決する方法を定義します。各地形および家具の項目には重み付きリストがあり、マップ生成時に地域用の項目を実際の項目へ解決する際の相対的な重みを定義します。

### フィールド

| 識別子      | 説明                                               |
| ----------- | -------------------------------------------------- |
| `terrain`   | 地域用の地形と、それぞれに対応する重み付きリスト。 |
| `furniture` | 地域用の家具と、それぞれに対応する重み付きリスト。 |

### 例

```json
{
  "region_terrain_and_furniture": {
    "terrain": {
      "t_region_groundcover": {
        "t_grass": 4,
        "t_grass_long": 2,
        "t_dirt": 1
      }
    },
    "furniture": {
      "f_region_flower": {
        "f_black_eyed_susan": 1,
        "f_lily": 1,
        "f_flower_tulip": 1,
        "f_flower_spurge": 1,
        "f_chamomile": 1,
        "f_dandelion": 1,
        "f_datura": 1,
        "f_dahlia": 1,
        "f_bluebell": 1
      }
    }
  }
}
```

## フィールドの被覆

**field_coverage** セクションは、`field` オーバーマップ地形を覆う植物相を構成する家具と地形を定義します。

### フィールド

| 識別子                     | 説明                                                                                                   |
| -------------------------- | ------------------------------------------------------------------------------------------------------ |
| `percent_coverage`         | オーバーマップ地形内で植物が存在するタイルの割合（%）。                                                |
| `default_ter`              | 植物用のデフォルト地形要素。                                                                           |
| `other`                    | `default_ter` が使用されない場合に、指定した確率（%）で配置される要素のリスト。                        |
| `boost_chance`             | 植物の成長が強化されるフィールドオーバーマップ地形の割合（%）。                                        |
| `boosted_percent_coverage` | 成長が強化された場所のうち、植物が存在するタイルの割合（%）。                                          |
| `boosted_other`            | 成長が強化された場所で `default_ter` が使用されない場合に、指定した確率（%）で配置される要素のリスト。 |
| `boosted_other_percent`    | `boosted_percent_coverage` のうち、`boosted_other` で覆われる割合（%）。                               |

### 例

```json
{
  "field_coverage": {
    "percent_coverage": 0.9333,
    "default_ter": "t_shrub",
    "other": {
      "t_shrub_blueberry": 0.4166,
      "t_shrub_strawberry": 0.4166,
      "f_mutpoppy": 8.3333
    },
    "boost_chance": 0.833,
    "boosted_percent_coverage": 2.5,
    "boosted_other": {
      "t_shrub_blueberry": 40.0,
      "f_dandelion": 6.6
    },
    "boosted_other_percent": 50.0
  }
}
```

## オーバーマップ湖設定

**overmap_lake_settings** セクションは、オーバーマップ上に湖を生成する際に使用する属性を定義します。これらの要素の実際の配置は、要素の境界が揃うようにすべてのオーバーマップにわたって全体的に決定されます。これらのパラメーターは主に、そのような全体的な要素をどのように解釈するかを指定します。

### フィールド

| 識別子                                     | 説明                                                                      |
| ------------------------------------------ | ------------------------------------------------------------------------- |
| `noise_threshold_lake`                     | `[0, 1]`。x > 値の場合、`lake_surface` または `lake_shore` を生成します。 |
| `lake_size_min`                            | 湖が実際に生成されるために必要な、オーバーマップ地形単位での最小サイズ。  |
| `lake_depth`                               | Z レベルで表した湖の深さ（例: -1 ～ -10）。                               |
| `shore_extendable_overmap_terrain`         | 隣接している場合に岸辺まで延長できるオーバーマップ地形のリスト。          |
| `shore_extendable_overmap_terrain_aliases` | 岸辺を延長する際に、別のオーバーマップ地形として扱うオーバーマップ地形。  |

### 例

```json
{
  "overmap_lake_settings": {
    "noise_threshold_lake": 0.25,
    "lake_size_min": 20,
    "lake_depth": -5,
    "shore_extendable_overmap_terrain": ["forest_thick", "forest_water", "field"],
    "shore_extendable_overmap_terrain_aliases": [
      { "om_terrain": "forest", "om_terrain_match_type": "TYPE", "alias": "forest_thick" }
    ]
  }
}
```

## オーバーマップ森林設定

**overmap_forest_settings** セクションは、オーバーマップ上に森林や沼地を生成する際に使用する属性を定義します。これらの要素の実際の配置は、要素の境界が揃うようにすべてのオーバーマップにわたって全体的に決定されます。これらのパラメーターは主に、そのような全体的な要素をどのように解釈するかを指定します。

### フィールド

| 識別子                                 | 説明                                                                         |
| -------------------------------------- | ---------------------------------------------------------------------------- |
| `noise_threshold_forest`               | `[0, 1]`。x > 値の場合、`forest` を生成します。                              |
| `noise_threshold_forest_thick`         | `[0, 1]`。x > 値の場合、`forest_thick` を生成します。                        |
| `noise_threshold_swamp_adjacent_water` | `[0, 1]`。水域の近くにある森林で x > 値の場合、`forest_water` を生成します。 |
| `noise_threshold_swamp_isolated`       | `[0, 1]`。水域から離れた森林で x > 値の場合、`forest_water` を生成します。   |
| `river_floodplain_buffer_distance_min` | 河川の氾濫原における、オーバーマップ地形単位での最小緩衝距離。               |
| `river_floodplain_buffer_distance_max` | 河川の氾濫原における、オーバーマップ地形単位での最大緩衝距離。               |

### 例

```json
{
  "overmap_forest_settings": {
    "noise_threshold_forest": 0.25,
    "noise_threshold_forest_thick": 0.3,
    "noise_threshold_swamp_adjacent_water": 0.3,
    "noise_threshold_swamp_isolated": 0.6,
    "river_floodplain_buffer_distance_min": 3,
    "river_floodplain_buffer_distance_max": 15
  }
}
```

## 森林マップ生成設定

**forest_mapgen_settings** セクションは、森林（`forest`、`forest_thick`、`forest_water`）地形を生成する際に使用する属性を定義します。これにはアイテム、地表被覆、地形、家具が含まれます。

### 全体構造

最上位の `forest_mapgen_settings` は、名前付き設定の集合です。各項目は、その設定が適用されるオーバーマップ地形の名前（`forest`、`forest_thick`、`forest_water` など）を持ちます。森林マップ生成の対象ではないオーバーマップ地形用の設定も定義でき、森林地形と他の種類の地形を混合する際に使用されます。

```json
{
  "forest_mapgen_settings": {
    "forest": {},
    "forest_thick": {},
    "forest_water": {}
  }
}
```

各地形には、マップ生成を制御する独立した設定値一式があります。

### フィールド

| 識別子                        | 説明                                                                                     |
| ----------------------------- | ---------------------------------------------------------------------------------------- |
| `sparseness_adjacency_factor` | 隣接地形との相対値によって、このオーバーマップ地形がどの程度まばらになるかを制御します。 |
| `item_group`                  | オーバーマップ地形内にアイテムをランダム配置するために使用するアイテムグループ。         |
| `item_group_chance`           | アイテムが配置される確率（1 ～ 100%）。                                                  |
| `item_spawn_iterations`       | アイテム生成を呼び出す回数。                                                             |
| `clear_groundcover`           | このオーバーマップ地形に対して以前に定義された `groundcover` をすべて消去します。        |
| `groundcover`                 | 基本の地表被覆に使用する地形の重み付きリスト。                                           |
| `clear_components`            | このオーバーマップ地形に対して以前に定義された `components` をすべて消去します。         |
| `components`                  | 配置される地形や家具を構成する構成要素の集合。                                           |
| `clear_terrain_furniture`     | このオーバーマップ地形に対して以前に定義された `terrain_furniture` をすべて消去します。  |
| `terrain_furniture`           | 地形に基づいて条件付きで配置される家具の集合。                                           |

### 例

```json
{
  "forest": {
    "sparseness_adjacency_factor": 3,
    "item_group": "forest",
    "item_group_chance": 60,
    "item_spawn_iterations": 1,
    "clear_groundcover": false,
    "groundcover": {
      "t_grass": 3,
      "t_dirt": 1
    },
    "clear_components": false,
    "components": {},
    "clear_terrain_furniture": false,
    "terrain_furniture": {}
  }
}
```

### 構成要素

構成要素は、名前、処理順、確率、種類の集合を持つオブジェクトの集合です。マップ生成時には、指定された順に抽選され、特定の場所へ配置する要素が選ばれます。構成要素の名前が意味を持つのは、地域オーバーレイで上書きする場合だけです。

### フィールド

| 識別子        | 説明                                                              |
| ------------- | ----------------------------------------------------------------- |
| `sequence`    | 構成要素が処理される順序。                                        |
| `chance`      | この構成要素から何かが配置される確率（X 分の 1）。                |
| `clear_types` | この構成要素に対して以前に定義された `types` をすべて消去します。 |
| `types`       | この構成要素を成す地形と家具の重み付きリスト。                    |

### 例

```json
{
  "trees": {
    "sequence": 0,
    "chance": 12,
    "clear_types": false,
    "types": {
      "t_tree_young": 128,
      "t_tree": 32,
      "t_tree_birch": 32,
      "t_tree_pine": 32,
      "t_tree_maple": 32,
      "t_tree_willow": 32,
      "t_tree_hickory": 32,
      "t_tree_blackjack": 8,
      "t_tree_coffee": 8,
      "t_tree_apple": 2,
      "t_tree_apricot": 2,
      "t_tree_cherry": 2,
      "t_tree_peach": 2,
      "t_tree_pear": 2,
      "t_tree_plum": 2,
      "t_tree_deadpine": 1,
      "t_tree_hickory_dead": 1,
      "t_tree_dead": 1
    }
  },
  "shrubs_and_flowers": {
    "sequence": 1,
    "chance": 10,
    "clear_types": false,
    "types": {
      "t_underbrush": 8,
      "t_shrub_blueberry": 1,
      "t_shrub_strawberry": 1,
      "t_shrub": 1,
      "f_chamomile": 1,
      "f_dandelion": 1,
      "f_datura": 1,
      "f_dahlia": 1,
      "f_bluebell": 1,
      "f_mutpoppy": 1
    }
  }
}
```

### 地形上の家具

地形上の家具は、地形 ID の集合です。通常のマップ生成が完了した後、指定された確率で、該当地形用の重み付きリストから家具を選んでその地形上に配置します。たとえば、沼地の淡水上にガマを配置するために使用されます。ガマを単に `components` セクションへ含め、通常の森林マップ生成中に配置することもできますが、その方法では淡水上だけに配置される保証がありません。この仕組みなら淡水上だけに配置できます。

### フィールド

| 識別子            | 説明                                                              |
| ----------------- | ----------------------------------------------------------------- |
| `chance`          | この構成要素から家具が配置される確率（X 分の 1）。                |
| `clear_furniture` | この地形に対して以前に定義された `furniture` をすべて消去します。 |
| `furniture`       | この地形上に配置される家具の重み付きリスト。                      |

### 例

```json
{
  "t_water_sh": {
    "chance": 2,
    "clear_furniture": false,
    "furniture": {
      "f_cattails": 1
    }
  }
}
```

## 森林道設定

**forest_trail_settings** セクションは、森林内に道を生成する際に使用する属性を定義します。これには、道が生成される確率、道同士の接続、登山口が生成される確率、およびマップ生成時の実際の道幅や位置に関する一般的な調整が含まれます。

### フィールド

| 識別子                     | 説明                                                                                             |
| -------------------------- | ------------------------------------------------------------------------------------------------ |
| `chance`                   | 連続した森林に道網が存在する確率（X 分の 1）。                                                   |
| `border_point_chance`      | 森林の最北端、最南端、最東端、最西端の点が道網の一部になる確率（X 分の 1）。                     |
| `minimum_forest_size`      | 道網を生成できるようになる、連続した森林の最小サイズ。                                           |
| `random_point_min`         | 道網の形成に使用する、連続した森林内のランダムな点の最小数。                                     |
| `random_point_max`         | 道網の形成に使用する、連続した森林内のランダムな点の最大数。                                     |
| `random_point_size_scalar` | 森林のサイズをこの値で除算し、ランダムな点の最小数に加算します。                                 |
| `trailhead_chance`         | フィールド付近の道の終端に登山口が生成される確率（X 分の 1）。                                   |
| `trailhead_road_distance`  | 登山口を生成できる、道路からの最大距離。                                                         |
| `trail_center_variance`    | マップ生成時の道の中心を、X 軸と Y 軸のそれぞれについて +/- この値の範囲でランダムにずらします。 |
| `trail_width_offset_min`   | マップ生成時の道幅を `rng(trail_width_offset_min, trail_width_offset_max)` の値だけずらします。  |
| `trail_width_offset_max`   | マップ生成時の道幅を `rng(trail_width_offset_min, trail_width_offset_max)` の値だけずらします。  |
| `clear_trail_terrain`      | 以前に定義された `trail_terrain` をすべて消去します。                                            |
| `trail_terrain`            | 道に使用する地形の重み付きリスト。                                                               |
| `trailheads`               | 登山口として配置されるオーバーマップスペシャルまたは都市建造物の重み付きリスト。                 |

### 例

```json
{
  "forest_trail_settings": {
    "chance": 2,
    "border_point_chance": 2,
    "minimum_forest_size": 100,
    "random_point_min": 4,
    "random_point_max": 50,
    "random_point_size_scalar": 100,
    "trailhead_chance": 1,
    "trailhead_road_distance": 6,
    "trail_center_variance": 3,
    "trail_width_offset_min": 1,
    "trail_width_offset_max": 3,
    "clear_trail_terrain": false,
    "trail_terrain": {
      "t_dirt": 1
    },
    "trailheads": {
      "trailhead_basic": 50
    }
  }
}
```

## 都市

**city** セクションは、都市内の建造物として使用できるオーバーマップ地形とスペシャル、それぞれが配置される重み付き確率、および各種建造物の相対的な配置を制御する一部の属性を定義します。

### フィールド

| 識別子        | 説明                                                               |
| ------------- | ------------------------------------------------------------------ |
| `type`        | 都市の種類を示す識別子。現在は使用されていません。                 |
| `shop_radius` | 店舗配置の半径方向の頻度。値が小さいほど店舗が多くなります。       |
| `park_radius` | 公園配置の半径方向の頻度。値が小さいほど公園が多くなります。       |
| `houses`      | 住宅として使用するオーバーマップ地形とスペシャルの重み付きリスト。 |
| `parks`       | 公園として使用するオーバーマップ地形とスペシャルの重み付きリスト。 |
| `shops`       | 店舗として使用するオーバーマップ地形とスペシャルの重み付きリスト。 |

### 店舗、公園、住宅の配置

指定された場所に配置する建造物を選ぶ際、ゲームは都市のサイズ、その場所から都市中心部までの距離、最後に地域の `shop_radius` と `park_radius` の値を考慮します。その後、店舗、公園、住宅の順に配置を試みます。店舗または公園を配置する確率は、`rng( 0, 99 ) > X_radius * distance from city center / city size` という式に基づきます。

### 例

```json
{
  "city": {
    "type": "town",
    "shop_radius": 80,
    "park_radius": 90,
    "houses": {
      "house_two_story_basement": 1,
      "house": 1000,
      "house_base": 333,
      "emptyresidentiallot": 20
    },
    "parks": {
      "park": 4,
      "pool": 1
    },
    "shops": {
      "s_gas": 5,
      "s_pharm": 3,
      "s_grocery": 15
    }
  }
}
```

## マップ追加要素

**map_extras** セクションは、オーバーマップ地形の `extras` プロパティから参照できる、名前付きのマップ追加要素の集合を定義します。マップ追加要素とは、定義済みのマップ生成に重ねて適用される特殊なマップ生成イベントです。このセクションには、追加要素が発生する確率と、追加要素の重み付きリストの両方が含まれます。

### フィールド

| 識別子   | 説明                                                             |
| -------- | ---------------------------------------------------------------- |
| `chance` | オーバーマップ地形にマップ追加要素が生成される確率（X 分の 1）。 |
| `extras` | 生成可能なマップ追加要素の重み付きリスト。                       |

### 例

```json
{
  "map_extras": {
    "field": {
      "chance": 90,
      "extras": {
        "mx_helicopter": 40,
        "mx_portal_in": 1
      }
    }
  }
}
```

## 天候

**weather** セクションは、その地域で使用する基本的な天候属性を定義します。

### フィールド

| 識別子                       | 説明                                                                                                                                                                              |
| ---------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `spring_temp`                | 地域の春半ばの気温（摂氏）。                                                                                                                                                      |
| `summer_temp`                | 地域の夏半ばの気温（摂氏）。                                                                                                                                                      |
| `autumn_temp`                | 地域の秋半ばの気温（摂氏）。                                                                                                                                                      |
| `winter_temp`                | 地域の冬半ばの気温（摂氏）。                                                                                                                                                      |
| `base_humidity`              | 地域の基本湿度（相対湿度 %）。                                                                                                                                                    |
| `base_pressure`              | 地域の基本気圧（ミリバール）。                                                                                                                                                    |
| `base_acid`                  | 地域の基本酸性度（? 単位）。1 以上の値は酸性とみなされます。                                                                                                                      |
| `base_wind`                  | 地域の基本風速（mph）。おおよその年間平均です。                                                                                                                                   |
| `base_wind_distrib_peaks`    | 風速のピークがどの程度高くなるかを指定します。値が大きいほど風の強い日が増えます。                                                                                                |
| `base_wind_season_variation` | 季節による風の変動度。値が小さいほど変動が大きくなります。                                                                                                                        |
| `weather_types`              | この地域で許可される天候タイプの ID。最初の値がデフォルトの天候タイプになります。宣言順序は天候の選択に影響します。詳細は [WEATHER_TYPE.md](weather_type.md) を参照してください。 |

### 例

```json
{
	"weather": {
		"spring_temp": 7,
		"summer_temp": 16,
		"autumn_temp": 6,
		"winter_temp": -14,
		"base_humidity": 66.0,
		"base_pressure": 1015.0,
		"base_acid": 0.0,
		"base_wind": 5.7,
		"base_wind_distrib_peaks": 30,
		"base_wind_season_variation": 64,
		"base_acid": 0.0,
		"weather_types": [
			"clear",
			"sunny",
			"cloudy",
			"light_drizzle",
			"drizzle",
			"rain",
			"thunder",
			"lightning",
			"acid_drizzle",
			"acid_rain",
			"flurries",
			"snowing",
			"snowstorm"
		]
	},
	}
}
```

## オーバーマップ特徴フラグ設定

**overmap_feature_flag_settings** セクションは、オーバーマップの特徴に割り当てられたフラグに対して実行する処理を定義します。現在は、地域ごとに場所をホワイトリストまたはブラックリストへ登録する仕組みを提供するために使用されています。

### フィールド

| 識別子            | 説明                                                                             |
| ----------------- | -------------------------------------------------------------------------------- |
| `clear_blacklist` | 以前に定義された `blacklist` をすべて消去します。                                |
| `blacklist`       | フラグのリスト。一致するフラグを持つ場所は、オーバーマップ生成から除外されます。 |
| `clear_whitelist` | 以前に定義された `whitelist` をすべて消去します。                                |
| `whitelist`       | フラグのリスト。一致するフラグを持つ場所だけが、オーバーマップ生成に含まれます。 |

### 例

```json
{
  "overmap_feature_flag_settings": {
    "clear_blacklist": false,
    "blacklist": ["FUNGAL"],
    "clear_whitelist": false,
    "whitelist": []
  }
}
```

# 地域オーバーレイ

**region_overlay** を使用すると、指定した地域に適用する `region_settings` の値を指定し、既存の値と統合または上書きできます。変更する値だけを指定すれば十分です。

### フィールド

| 識別子    | 説明                                                                                   |
| --------- | -------------------------------------------------------------------------------------- |
| `type`    | 種類の識別子。"region_overlay" でなければなりません。                                  |
| `id`      | この地域オーバーレイの一意な識別子。                                                   |
| `regions` | このオーバーレイを適用する地域のリスト。"all" を指定するとすべての地域に適用されます。 |

その他のすべてのフィールドとセクションは、`region_overlay` について定義されているものと同じです。

### 例

```json
[{
  "type": "region_overlay",
  "id": "example_overlay",
  "regions": ["all"],
  "city": {
    "parks": {
      "examplepark": 1
    }
  }
}]
```

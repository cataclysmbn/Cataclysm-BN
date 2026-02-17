# 지역 설정

**region_settings**는 전체 지역에 적용되는 맵 생성 속성을 정의합니다. 일반 설정은 기본 오버맵 지형과
지표 피복을 정의합니다. 추가 섹션은 다음과 같습니다:

| 섹션                            | 설명                                                       |
| ------------------------------- | ---------------------------------------------------------- |
| `region_terrain_and_furniture`  | 지역 지형/가구를 실제 타입으로 해석하는 방법을 정의합니다. |
| `field_coverage`                | `field` 오버맵 지형을 덮는 식생을 정의합니다.              |
| `overmap_lake_settings`         | 지역 내 호수를 생성하기 위한 매개변수를 정의합니다.        |
| `overmap_forest_settings`       | 지역 내 숲과 습지를 생성하기 위한 매개변수를 정의합니다.   |
| `forest_mapgen_settings`        | `forest` 지형 타입을 덮는 식생(및 기타 요소)을 정의합니다. |
| `forest_trail_settings`         | 숲길의 오버맵 및 로컬 구조를 정의합니다.                   |
| `city`                          | 도시의 구조적 구성을 정의합니다.                           |
| `map_extras`                    | 오버맵 지형이 참조하는 맵 엑스트라 그룹을 정의합니다.      |
| `weather`                       | 지역의 기본 날씨 속성을 정의합니다.                        |
| `overmap_feature_flag_settings` | 플래그 기반 오버맵 피처 처리 동작을 정의합니다.            |

기본 지역(default region)의 경우 모든 속성과 섹션이 필요합니다.

### 필드

| 식별자                | 설명                                                     |
| --------------------- | -------------------------------------------------------- |
| `type`                | 타입 식별자입니다. 반드시 "region_settings"여야 합니다.  |
| `id`                  | 이 지역의 고유 식별자입니다.                             |
| `default_oter`        | 이 지역의 기본 오버맵 지형입니다.                        |
| `default_groundcover` | 기본 지표 피복으로 적용할 지형 타입과 가중치 목록입니다. |

### 예시

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

## 지역 지형 / 가구

**region_terrain_and_furniture** 섹션은 지역 지형/가구 항목이 해당 지역의 실제 지형 및 가구 타입으로
해석되는 방식을 정의합니다. 각 항목에는 가중치 목록이 있으며, mapgen이 지역 항목을 실제 항목으로 해석할 때
각 후보의 상대적 비중을 결정합니다.

### 필드

| 식별자      | 설명                                         |
| ----------- | -------------------------------------------- |
| `terrain`   | 지역 지형과 그에 대응하는 가중치 목록입니다. |
| `furniture` | 지역 가구와 그에 대응하는 가중치 목록입니다. |

### 예시

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

## 초원 피복

**field_coverage** 섹션은 `field` 오버맵 지형을 덮는 식생을 구성하는 가구와 지형을 정의합니다.

### 필드

| 식별자                     | 설명                                                                   |
| -------------------------- | ---------------------------------------------------------------------- |
| `percent_coverage`         | 식물이 존재하는 오버맵 지형 타일 비율(%).                              |
| `default_ter`              | 식물용 기본 지형 피처입니다.                                           |
| `other`                    | `default_ter`를 사용하지 않을 때의 피처 목록과 각 % 확률입니다.        |
| `boost_chance`             | 식물 성장이 강화되는 field 오버맵 지형의 비율(%).                      |
| `boosted_percent_coverage` | 강화 상태에서 식물이 존재하는 타일 비율(%).                            |
| `boosted_other`            | 강화 상태에서 `default_ter`를 쓰지 않을 때의 피처 목록과 % 확률입니다. |
| `boosted_other_percent`    | `boosted_percent_coverage` 중 `boosted_other`로 덮이는 비율(%).        |

### 예시

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

## 오버맵 호수 설정

**overmap_lake_settings** 섹션은 오버맵에서 호수를 생성할 때 사용하는 속성을 정의합니다. 실제 피처 배치는
모든 오버맵 전역에서 가장자리가 맞도록 결정되며, 이 매개변수들은 그런 전역 피처를 어떻게 해석할지에 초점이
맞춰져 있습니다.

### 필드

| 식별자                                     | 설명                                                                |
| ------------------------------------------ | ------------------------------------------------------------------- |
| `noise_threshold_lake`                     | `[0, 1]`, x > 값이면 `lake_surface` 또는 `lake_shore`를 생성합니다. |
| `lake_size_min`                            | 실제 생성되기 위한 호수의 최소 크기(오버맵 지형 단위).              |
| `lake_depth`                               | 호수 깊이(Z-레벨 기준, 예: -1 ~ -10).                               |
| `shore_extendable_overmap_terrain`         | 인접 시 해안까지 확장될 수 있는 오버맵 지형 목록입니다.             |
| `shore_extendable_overmap_terrain_aliases` | 해안 확장 시 다른 오버맵 지형으로 취급할 오버맵 지형 목록입니다.    |

### 예시

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

## 오버맵 숲 설정

**overmap_forest_settings** 섹션은 오버맵에서 숲과 습지를 생성할 때 사용하는 속성을 정의합니다. 실제 피처
배치는 모든 오버맵 전역에서 가장자리가 맞도록 결정되며, 이 매개변수들은 그런 전역 피처를 어떻게 해석할지에
초점이 맞춰져 있습니다.

### 필드

| 식별자                                 | 설명                                                                 |
| -------------------------------------- | -------------------------------------------------------------------- |
| `noise_threshold_forest`               | `[0, 1]`, x > 값이면 `forest`를 생성합니다.                          |
| `noise_threshold_forest_thick`         | `[0, 1]`, x > 값이면 `forest_thick`을 생성합니다.                    |
| `noise_threshold_swamp_adjacent_water` | `[0, 1]`, 물가 인접 숲에서 x > 값이면 `forest_water`를 생성합니다.   |
| `noise_threshold_swamp_isolated`       | `[0, 1]`, 물과 떨어진 숲에서 x > 값이면 `forest_water`를 생성합니다. |
| `river_floodplain_buffer_distance_min` | 강 범람원 버퍼의 최소 거리(오버맵 지형 단위).                        |
| `river_floodplain_buffer_distance_max` | 강 범람원 버퍼의 최대 거리(오버맵 지형 단위).                        |

### 예시

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

## 숲 맵 생성 설정

**forest_mapgen_settings** 섹션은 숲(`forest`, `forest_thick`, `forest_water`) 지형 생성에 사용하는 속성을
정의하며, 여기에는 아이템, 지표 피복, 지형, 가구가 포함됩니다.

### 일반 구조

최상위에서 `forest_mapgen_settings`는 이름 있는 설정 모음이며, 각 항목의 이름은 해당 설정이 적용될 오버맵
지형 이름(예: `forest`, `forest_thick`, `forest_water`)입니다. 숲 mapgen에서 렌더링되지 않는 오버맵 지형에
대해서도 설정을 정의할 수 있으며, 이는 숲 지형을 다른 지형 타입과 블렌딩할 때 사용됩니다.

```json
{
  "forest_mapgen_settings": {
    "forest": {},
    "forest_thick": {},
    "forest_water": {}
  }
}
```

각 지형은 mapgen을 제어하는 독립적인 설정값 집합을 가집니다.

### 필드

| 식별자                        | 설명                                                                   |
| ----------------------------- | ---------------------------------------------------------------------- |
| `sparseness_adjacency_factor` | 이웃 지형 대비 값으로 오버맵 지형의 성김 정도를 제어합니다.            |
| `item_group`                  | 오버맵 지형 내부에 아이템을 무작위 배치할 때 사용할 아이템 그룹입니다. |
| `item_group_chance`           | 아이템이 배치될 확률(1~100%).                                          |
| `item_spawn_iterations`       | 아이템 생성 로직을 호출할 횟수입니다.                                  |
| `clear_groundcover`           | 이 오버맵 지형의 기존 `groundcover` 정의를 모두 제거합니다.            |
| `groundcover`                 | 기본 지표 피복에 사용할 지형의 가중치 목록입니다.                      |
| `clear_components`            | 이 오버맵 지형의 기존 `components` 정의를 모두 제거합니다.             |
| `components`                  | 배치될 지형/가구를 구성하는 컴포넌트 모음입니다.                       |
| `clear_terrain_furniture`     | 이 오버맵 지형의 기존 `terrain_furniture` 정의를 모두 제거합니다.      |
| `terrain_furniture`           | 지형에 따라 조건부로 배치되는 가구 모음입니다.                         |

### 예시

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

### 컴포넌트

컴포넌트는 이름 있는 객체들의 모음으로, 각 객체는 sequence, chance, 타입 집합을 가지며 mapgen 중 순서대로
판정되어 해당 위치에 배치할 피처를 결정합니다. 컴포넌트 이름은 region overlay에서 이를 덮어쓸 때에만 의미가
있습니다.

### 필드

| 식별자        | 설명                                                  |
| ------------- | ----------------------------------------------------- |
| `sequence`    | 컴포넌트를 처리하는 순서입니다.                       |
| `chance`      | 이 컴포넌트에서 무언가가 배치될 확률(1/X)입니다.      |
| `clear_types` | 이 컴포넌트의 기존 `types` 정의를 모두 제거합니다.    |
| `types`       | 이 컴포넌트를 구성하는 지형/가구의 가중치 목록입니다. |

### 예시

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

### 지형 가구

지형 가구는 지형 id 모음이며, 일반 mapgen이 끝난 뒤 해당 지형 위에 배치할 가구를 가중치 목록에서 뽑아
적용할 확률을 정의합니다. 예를 들어 습지의 담수 지형 위에 부들을 배치할 때 사용됩니다. 부들을 `components`
섹션에 넣어 일반 숲 mapgen 과정에서 배치할 수도 있지만, 그 경우 담수 지형에만 배치된다는 보장이 없습니다.
이 방식은 그 보장을 제공합니다.

### 필드

| 식별자            | 설명                                               |
| ----------------- | -------------------------------------------------- |
| `chance`          | 이 컴포넌트의 가구가 배치될 확률(1/X)입니다.       |
| `clear_furniture` | 이 지형의 기존 `furniture` 정의를 모두 제거합니다. |
| `furniture`       | 이 지형 위에 배치될 가구의 가중치 목록입니다.      |

### 예시

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

## 숲길 설정

**forest_trail_settings** 섹션은 숲길 생성에 사용하는 속성을 정의합니다. 여기에는 숲길 시스템 생성 확률,
연결성, 들머리(trailhead) 생성 확률, 그리고 mapgen에서 실제 숲길 폭/위치를 조정하는 일반 매개변수가 포함됩니다.

### 필드

| 식별자                     | 설명                                                                                         |
| -------------------------- | -------------------------------------------------------------------------------------------- |
| `chance`                   | 연속된 숲에 숲길 시스템이 생길 확률(1/X)입니다.                                              |
| `border_point_chance`      | 숲의 N/S/E/W 최외곽 지점이 숲길 시스템에 포함될 확률(1/X)입니다.                             |
| `minimum_forest_size`      | 숲길 시스템 생성이 가능한 최소 연속 숲 크기입니다.                                           |
| `random_point_min`         | 숲길 시스템 구성에 사용할 연속 숲 내 무작위 지점의 최소 개수입니다.                          |
| `random_point_max`         | 숲길 시스템 구성에 사용할 연속 숲 내 무작위 지점의 최대 개수입니다.                          |
| `random_point_size_scalar` | 숲 크기를 이 값으로 나눈 값을 무작위 지점 최소 개수에 더합니다.                              |
| `trailhead_chance`         | 초원 근처 숲길 끝에 들머리가 생성될 확률(1/X)입니다.                                         |
| `trailhead_road_distance`  | 들머리가 생성되기 위해 도로와 떨어질 수 있는 최대 거리입니다.                                |
| `trail_center_variance`    | mapgen에서 숲길 중심점을 X/Y 축 각각 +/- 분산 범위 내 임의 오프셋합니다.                     |
| `trail_width_offset_min`   | mapgen 숲길 폭 오프셋의 최소값입니다(`rng(trail_width_offset_min, trail_width_offset_max)`). |
| `trail_width_offset_max`   | mapgen 숲길 폭 오프셋의 최대값입니다(`rng(trail_width_offset_min, trail_width_offset_max)`). |
| `clear_trail_terrain`      | 기존 `trail_terrain` 정의를 모두 제거합니다.                                                 |
| `trail_terrain`            | 숲길에 사용할 지형의 가중치 목록입니다.                                                      |
| `trailheads`               | 들머리로 배치할 오버맵 스페셜/도시 건물의 가중치 목록입니다.                                 |

### 예시

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

## 도시

**city** 섹션은 도시 건물로 사용할 수 있는 오버맵 지형과 스페셜, 이들의 가중치 기반 배치 확률, 그리고
건물 종류별 상대 배치를 제어하는 일부 속성을 정의합니다.

### 필드

| 식별자        | 설명                                                          |
| ------------- | ------------------------------------------------------------- |
| `type`        | 도시 타입 식별자(현재는 사용되지 않음).                       |
| `shop_radius` | 상점 배치의 반경 빈도입니다. 값이 작을수록 상점이 많아집니다. |
| `park_radius` | 공원 배치의 반경 빈도입니다. 값이 작을수록 공원이 많아집니다. |
| `houses`      | 주택으로 사용할 오버맵 지형/스페셜의 가중치 목록입니다.       |
| `parks`       | 공원으로 사용할 오버맵 지형/스페셜의 가중치 목록입니다.       |
| `shops`       | 상점으로 사용할 오버맵 지형/스페셜의 가중치 목록입니다.       |

### 상점, 공원, 주택 배치

특정 위치에 배치할 건물을 고를 때, 게임은 도시 크기, 해당 위치의 도심 거리, 그리고 지역의 `shop_radius`와
`park_radius` 값을 고려합니다. 이후 상점, 공원, 주택 순서로 배치를 시도하며, 상점/공원 배치 확률은
`rng( 0, 99 ) > X_radius * distance from city center / city size` 공식을 기반으로 계산됩니다.

### 예시

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

## 맵 엑스트라

**map_extras** 섹션은 오버맵 지형의 `extras` 속성이 참조할 수 있는, 이름 있는 맵 엑스트라 모음을
정의합니다. 맵 엑스트라는 정의된 mapgen 위에 추가 적용되는 특수 mapgen 이벤트를 의미합니다. 여기에는
엑스트라 발생 확률과 엑스트라 가중치 목록이 모두 포함됩니다.

### 필드

| 식별자   | 설명                                                       |
| -------- | ---------------------------------------------------------- |
| `chance` | 해당 오버맵 지형에서 맵 엑스트라가 생성될 확률(1/X)입니다. |
| `extras` | 생성 가능한 맵 엑스트라의 가중치 목록입니다.               |

### 예시

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

## 날씨

**weather** 섹션은 지역에 사용되는 기본 날씨 속성을 정의합니다.

### 필드

| 식별자                       | 설명                                                                                                                                                                         |
| ---------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `spring_temp`                | 지역의 봄 중반 기온(섭씨)                                                                                                                                                    |
| `summer_temp`                | 지역의 여름 중반 기온(섭씨)                                                                                                                                                  |
| `autumn_temp`                | 지역의 가을 중반 기온(섭씨)                                                                                                                                                  |
| `winter_temp`                | 지역의 겨울 중반 기온(섭씨)                                                                                                                                                  |
| `base_humidity`              | 지역의 기본 습도(상대습도 %)                                                                                                                                                 |
| `base_pressure`              | 지역의 기본 기압(밀리바 단위)                                                                                                                                                |
| `base_acid`                  | 지역의 기본 산성도(? 단위). 값이 1 이상이면 산성으로 간주됩니다.                                                                                                             |
| `base_wind`                  | 지역의 기본 풍속(mph 단위). 대략적인 연평균 값입니다.                                                                                                                        |
| `base_wind_distrib_peaks`    | 풍속 피크가 얼마나 높아질 수 있는지입니다. 값이 높을수록 바람이 강한 날이 많아집니다.                                                                                        |
| `base_wind_season_variation` | 계절에 따른 바람 변화량입니다. 값이 낮을수록 변동이 커집니다.                                                                                                                |
| `weather_types`              | 이 지역에서 허용되는 날씨 타입 id입니다. 첫 값은 기본 날씨 타입이 됩니다. 선언 순서는 날씨 선택에 영향을 줍니다. 자세한 내용은 [WEATHER_TYPE.md](weather_type)를 참고하세요. |

### 예시

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

## 오버맵 피처 플래그 설정

**overmap_feature_flag_settings** 섹션은 오버맵 피처에 지정된 플래그를 대상으로 수행되는 동작을 정의합니다.
현재는 지역별 허용 목록/차단 목록을 지정하는 메커니즘으로 사용됩니다.

### 필드

| 식별자            | 설명                                                                         |
| ----------------- | ---------------------------------------------------------------------------- |
| `clear_blacklist` | 기존 `blacklist` 정의를 모두 제거합니다.                                     |
| `blacklist`       | 플래그 목록입니다. 일치하는 플래그가 있는 위치는 오버맵 생성에서 제외됩니다. |
| `clear_whitelist` | 기존 `whitelist` 정의를 모두 제거합니다.                                     |
| `whitelist`       | 플래그 목록입니다. 일치하는 플래그가 있는 위치만 오버맵 생성에 포함됩니다.   |

### 예시

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

# 지역 오버레이

**region_overlay**는 지정한 지역에 적용할 `region_settings` 값을 정의할 수 있게 해주며, 기존 값을 병합하거나
덮어씁니다. 변경할 값만 지정하면 됩니다.

### 필드

| 식별자    | 설명                                                                    |
| --------- | ----------------------------------------------------------------------- |
| `type`    | 타입 식별자입니다. 반드시 "region_overlay"여야 합니다.                  |
| `id`      | 이 지역 오버레이의 고유 식별자입니다.                                   |
| `regions` | 이 오버레이를 적용할 지역 목록입니다. "all"이면 모든 지역에 적용됩니다. |

그 외 추가 필드와 섹션은 `region_overlay`에 대해 정의된 것과 동일합니다.

### 예시

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

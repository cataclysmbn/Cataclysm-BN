# JSON 지붕 추가 가이드

건물에 JSON 지붕을 추가하려면 맵젠 중에 지붕과 건물을 연결하기 위해 몇 가지 파일을 더 사용해야 합니다.

편집할 파일:

`data/json/mapgen/[name of building].json` : 건물과 지붕의 맵.

`data/json/overmap_terrain.json` : 건물의 오버맵 특성.

`data/json/regional_map_settings.json` : 건물의 오버맵 생성 설정.

`data/json/overmap/multietile_city_buildings.json` : 건물 층 연결.

## 지붕 맵 만들기

맵을 처음 만든다면 [MAPGEN](../reference/map/mapgen.md)을 참고하세요.

건물 맵이 들어 있는 `data/json/mapgen/[name of building].json` 파일을 열고 지붕 항목을 새로 추가합니다. 지붕도 같은 기초 면적을 사용하므로 기존 건물 항목을 복사할 수 있습니다.

지붕에 고유한 `om_terrain` ID를 지정합니다. 본층과 지붕의 `om_terrain` ID 예시는 다음과 같습니다.

```json
"om_terrain": [ "abstorefront" ],
"om_terrain": [ "abstorefront_roof" ],
```

참고: 다른 맵과 공통 `om_terrain` ID를 공유하는 기존 건물에 지붕을 추가한다면 기존 층의 `om_terrain` ID도 고유하게 바꿔야 합니다.

원래 층의 벽 외곽선을 유지하고, 건물 바깥에는 `t_open_air`, 건물 면적 위에는 `t_flat_roof`를 추가합니다. `terrain.json`에서 선택할 수 있는 평평한 지붕 지형을 확인할 수 있습니다.

홈통, 굴뚝, 지붕 터빈 환풍구를 포함한 지붕 지형과 가구가 있습니다. 아이디어를 얻으려면 `json/terrain.json`과 `furniture.json`을 살펴보세요. 지붕에 접근할 방법도 고려해야 합니다. 사다리, 계단, 홈통을 사용할 수 있으며 일부 가구는 올라갈 수 있습니다.

`data/json/mapgen/nested_chunks_roof.json`에는 선택적으로 사용할 수 있는 중첩 맵 청크가 있습니다. 새 지붕 청크도 이 파일에 추가하세요.

지붕 항목 예시:

```json
{
  "type": "mapgen",
  "method": "json",
  "om_terrain": "abstorefront_roof",
  "weight": 200,
  "object": {
    "fill_ter": "t_flat_roof",
    "rows": [
      "                        ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |....................3 ",
      " |......&.............3 ",
      " |....................3 ",
      " |....................3 ",
      " |-----------------5--3 ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        ",
      "                        "
    ],
    "terrain": {
      ".": "t_flat_roof",
      " ": "t_open_air",
      "|": "t_gutter_west",
      "-": "t_gutter_south",
      "3": "t_gutter_east",
      "5": "t_gutter_drop"
    },
    "furniture": { "&": "f_roof_turbine_vent" },
    "place_items": [
      { "item": "roof_trash", "x": [2, 21], "y": [3, 14], "chance": 50, "repeat": [1, 3] }
    ],
    "place_nested": [
      {
        "chunks": [
          ["null", 50],
          ["roof_4x4_party", 15],
          ["roof_4x4_holdout", 5],
          ["roof_4x4_utility", 40],
          ["roof_5x5_coop", 5]
        ],
        "x": [3, 15],
        "y": [3, 7]
      }
    ]
  }
}
```

## 본층과 지붕 연결

한 z 레벨에서 오버맵 타일을 하나보다 많이 차지하는 건물(학교, 저택)은 `json/overmap/multitile_city_buildings.json` 또는 `json/overmap/multitile_buildings_terrain.json`으로 이동합니다. 본층 항목을 추가합니다. `point` 좌표는 건물의 x, y, z 위치를 정의합니다. 1은 지붕을 지상층보다 한 z 레벨 위에 둡니다. 건물을 회전시키려면 방향을 나타내는 `north`를 덧붙입니다.

```json
{
  "type": "city_building",
  "id": "abstorefront",
  "locations": ["land"],
  "overmaps": [
    { "point": [0, 0, 0], "overmap": "abstorefront_north" },
    { "point": [0, 0, 1], "overmap": "abstorefront_roof_north" }
  ]
}
```

## 오버맵 스페셜

오버맵 스페셜은 조금 다르게 처리합니다. z 레벨 연결과 오버맵 생성을 모두 `json/overmap/specials.json`에서 처리하므로 `data/json/regional_map_settings.json` 항목이 필요하지 않습니다.

오버맵 스페셜 예시:

```json
{
  "type": "overmap_special",
  "id": "Evac Shelter",
  "overmaps": [
    { "point": [0, 0, 0], "overmap": "shelter" },
    { "point": [0, 0, -1], "overmap": "shelter_under" },
    { "point": [0, 0, 1], "overmap": "shelter_roof" }
  ],
  "connections": [
    { "point": [0, -1, 0], "terrain": "road" }
  ],
  "locations": ["wilderness"],
  "city_distance": [5, 10],
  "city_sizes": [4, 12],
  "occurrences": [1, 3],
  "rotate": false,
  "flags": ["CLASSIC"]
}
```

## overmap_terrain 항목 추가

`data/json/overmap_terrain.json`으로 이동합니다. 각 z 레벨에는 오버맵에 표시되는 방식을 정의하는 항목이 필요합니다. `name` 필드가 게임 오버맵에 표시되는 이름을 결정합니다. 항목은 같은 색과 기호를 공유해야 합니다.

```json
  {
    "type": "overmap_terrain",
    "id": "abandonedwarehouse",
    "copy-from": "generic_city_building",
    "name": "abandoned warehouse",
    "sym": 119,
    "color": "brown"
  },
  {
    "type": "overmap_terrain",
    "id": "abandonedwarehouse_roof",
    "copy-from": "generic_city_building",
    "name": "abandoned warehouse roof",
    "sym": 119,
    "color": "brown"
}
```

## regional_map_settings에 추가

`data/json/regional_map_settings.json`으로 이동합니다.

이 파일은 스페셜이 아닌 건물의 생성 빈도와 위치를 결정합니다. 건물에 맞는 범주를 찾아 오버맵 스페셜 ID 또는 도시 건물 ID를 추가하고 생성 가중치를 지정합니다.

```json
"abandonedwarehouse": 200,
```

테스트할 때 자연 생성을 확인하고 싶다면 생성률을 높여도 됩니다.

마지막으로 제출하기 전에 항상 추가한 내용을 [검사](http://dev.narc.ro/cataclysm/format.html)하세요.

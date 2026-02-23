# JSON 지붕 맵젠

이 가이드는 맵젠에서 지붕을 정의하는 방법을 설명합니다.

## 개요

지붕은 건물 위에 나타나는 특수 레이어입니다. 3D 오버레이를 제공하고 비와 햇빛을 차단할 수 있습니다.

## 기본 지붕 구문

```json
{
  "type": "mapgen",
  "om_terrain": "house",
  "object": {
    "fill_ter": "t_floor",
    "rows": [
      "######################",
      "#....................#",
      "#....................#"
    ],
    "palettes": ["standard_domestic_palette"],
    "roof": "t_flat_roof"
  }
}
```

## 지붕 타입

### 단일 지붕 타입

건물 전체에 하나의 지붕 타일 사용:

```json
{
  "roof": "t_flat_roof"
}
```

### 패턴 지붕

`rows`처럼 지붕 패턴 정의:

```json
{
  "roof": [
    "rrrrrrrrrrrrrrrrrrrrrr",
    "rrrrrrrrrrrrrrrrrrrrrr",
    "rrrrrrrrrrrrrrrrrrrrrr"
  ],
  "palettes": ["roof_palette"]
}
```

팔레트에서:

```json
{
  "type": "palette",
  "id": "roof_palette",
  "terrain": {
    "r": "t_shingle_flat_roof"
  }
}
```

## 일반적인 지붕 타일

| 타일 ID               | 설명        |
| --------------------- | ----------- |
| `t_flat_roof`         | 평평한 지붕 |
| `t_shingle_flat_roof` | 판자 지붕   |
| `t_tar_flat_roof`     | 타르 지붕   |
| `t_metal_flat_roof`   | 금속 지붕   |
| `t_glass_roof`        | 유리 지붕   |
| `t_open_air`          | 지붕 없음   |

## 부분 지붕

부분 지붕이 있는 건물의 경우 `t_open_air` 사용:

```json
{
  "roof": [
    "rrrrrrrrrrrrrrrrrrrrrr",
    "rrrr..............rrrr",
    "rrrr..............rrrr",
    "rrrrrrrrrrrrrrrrrrrrrr"
  ],
  "palettes": ["roof_palette"]
}
```

팔레트에서:

```json
{
  "terrain": {
    "r": "t_flat_roof",
    ".": "t_open_air"
  }
}
```

## 다층 건물

### 첫 번째 층

```json
{
  "type": "mapgen",
  "om_terrain": "house_1",
  "object": {
    "fill_ter": "t_floor",
    "rows": ["..."],
    "roof": "t_floor_above" // 위층은 이 층의 지붕
  }
}
```

### 두 번째 층

```json
{
  "type": "mapgen",
  "om_terrain": "house_2",
  "object": {
    "fill_ter": "t_floor",
    "rows": ["..."],
    "roof": "t_flat_roof" // 실제 지붕
  }
}
```

## 지붕 속성

### 날씨 보호

지붕은 비와 햇빛을 차단합니다:

- `t_flat_roof` - 완전히 차단
- `t_glass_roof` - 빛은 통과, 비는 차단
- `t_open_air` - 차단 없음

### 시야

일부 지붕은 보는 것에 영향을 줍니다:

- 불투명한 지붕 - 아래에서 위를 볼 수 없음
- 투명한 지붕 - 보기 허용
- 지붕 없음 - 완전한 시야

### 이동

플레이어는 일반적으로 지붕 위를 걸을 수 없습니다 (특별한 경우 제외).

## 고급 지붕

### 중첩 객체

객체를 사용하여 복잡한 지붕 정의:

```json
{
  "roof": {
    "rows": [
      "rrrrrrrrrr",
      "rrrrrrrrrr"
    ],
    "terrain": {
      "r": "t_flat_roof"
    }
  }
}
```

### 조건부 지붕

맵젠 매개변수에 따라 다른 지붕:

```json
{
  "object": {
    "switch": "season",
    "cases": [
      {
        "case": "winter",
        "roof": "t_snow_flat_roof"
      },
      {
        "default": true,
        "roof": "t_flat_roof"
      }
    ]
  }
}
```

## 팔레트 통합

팔레트는 지붕 정의를 공유할 수 있습니다:

```json
{
  "type": "palette",
  "id": "standard_building_palette",
  "terrain": {
    "r": "t_flat_roof",
    "g": "t_glass_roof"
  }
}
```

맵젠에서 사용:

```json
{
  "palettes": ["standard_building_palette"],
  "roof": [
    "rrrrrrrr",
    "rggggggr",
    "rrrrrrrr"
  ]
}
```

## 테스트

### 게임 내 테스트

1. 맵젠 위치 생성
2. 디버그 메뉴로 텔레포트
3. 지붕 확인:
   - 외관
   - 날씨 보호
   - 시야 차단

### 일반적인 문제

**지붕이 나타나지 않음**

- 지붕 타일 ID 확인
- 팔레트 정의 확인
- 행 크기가 일치하는지 확인

**지붕이 잘못 정렬됨**

- 지붕 행이 맵 행과 같은 크기인지 확인
- 팔레트 문자 확인

**z-레벨 문제**

- 위층은 `t_floor_above` 사용
- 최상층은 실제 지붕 타일 사용

## 모범 사례

1. **일관된 지붕**: 유사한 건물에 유사한 지붕 사용
2. **적절한 타입**: 건물 유형에 맞는 지붕 재질
3. **중첩 팔레트**: 공통 지붕 정의를 팔레트로 재사용
4. **테스트 z-레벨**: 다층 건물에서 항상 테스트
5. **문서화**: 비표준 지붕 패턴 문서화

## 예제

### 간단한 집

```json
{
  "type": "mapgen",
  "om_terrain": "house_simple",
  "object": {
    "fill_ter": "t_floor",
    "rows": [
      "########################",
      "#......................#",
      "#......................#",
      "########################"
    ],
    "roof": "t_shingle_flat_roof"
  }
}
```

### 일부 유리가 있는 건물

```json
{
  "type": "mapgen",
  "om_terrain": "building_glass",
  "object": {
    "fill_ter": "t_floor",
    "rows": ["..."],
    "roof": [
      "rrrrrrrrrrrrrrrrrrrrrrrr",
      "r......................r",
      "r.....ggggggggg........r",
      "r.....ggggggggg........r",
      "r......................r",
      "rrrrrrrrrrrrrrrrrrrrrrrr"
    ],
    "palettes": ["roof_palette"]
  }
}
```

### 2층 건물

```json
[
  {
    "type": "mapgen",
    "om_terrain": "building_1",
    "object": {
      "fill_ter": "t_floor",
      "rows": ["..."],
      "roof": "t_floor_above"
    }
  },
  {
    "type": "mapgen",
    "om_terrain": "building_2",
    "object": {
      "fill_ter": "t_floor",
      "rows": ["..."],
      "roof": "t_flat_roof"
    }
  }
]
```

## 관련 문서

- [맵젠](map/mapgen.md)
- [팔레트](../../reference/map/palettes.md)
- [지형](../../reference/map/terrain.md)

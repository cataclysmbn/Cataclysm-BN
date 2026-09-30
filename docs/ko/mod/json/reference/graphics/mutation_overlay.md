# 돌연변이 오버레이 순서

`mutation_ordering.json` 파일은 캐릭터에서 시각적 돌연변이 및 바이오닉 오버레이를 렌더링하는 순서를 정의합니다. 0(아래)부터 9999(위)까지의 레이어 값으로 순서를 지정합니다.

예시:

```json
[
  {
    "type": "overlay_order",
    "overlay_ordering": [
      {
        "id": [
          "BEAUTIFUL",
          "BEAUTIFUL2",
          "BEAUTIFUL3",
          "LARGE",
          "PRETTY",
          "RADIOACTIVE1",
          "RADIOACTIVE2",
          "RADIOACTIVE3",
          "REGEN"
        ],
        "order": 1000
      },
      {
        "id": ["HOOVES", "ROOTS1", "ROOTS2", "ROOTS3", "TALONS"],
        "order": 4500
      },
      {
        "id": "FLOWERS",
        "order": 5000
      },
      {
        "id": [
          "PROF_CYBERCOP",
          "PROF_FED",
          "PROF_PD_DET",
          "PROF_POLICE",
          "PROF_SWAT",
          "PHEROMONE_INSECT"
        ],
        "order": 8500
      },
      {
        "id": [
          "bio_armor_arms",
          "bio_armor_legs",
          "bio_armor_torso",
          "bio_armor_head",
          "bio_armor_eyes"
        ],
        "order": 500
      }
    ]
  }
]
```

## `id`

(string)

돌연변이의 내부 ID입니다. 문자열 하나 또는 문자열 배열로 지정할 수 있습니다. 배열의 모든 항목에 지정한 순서 값이 적용됩니다.

## `order`

(integer)

돌연변이 오버레이의 순서 값입니다. 값의 범위는 0~9999이며, 9999가 가장 위에 그려지는 레이어입니다. 어떤 목록에도 없는 돌연변이는 기본값 9999를 사용합니다.

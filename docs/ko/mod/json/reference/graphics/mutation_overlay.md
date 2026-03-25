# 돌연변이 오버레이 순서 지정

`mutation_ordering.json` 파일은 게임 내에서 캐릭터 오버레이가 렌더링되는 순서를 정의합니다.
돌연변이, 바이오닉, 효과, 착용한 아이템, 손에 든 아이템의 순서를 재배치할 수 있습니다. 레이어 값은
0 (하단) - 9999 (상단) 범위로 설정되며, 착용 및 손에 든 오버레이는 재정의하지 않는 한 더 높은 기본값을 사용합니다.

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
        "id": "worn_backpack",
        "order": 5400
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

(문자열)

오버레이 ID입니다. 단일 문자열로 제공하거나 문자열 배열로 제공할 수 있습니다. 제공된 순서 값은
배열의 모든 항목에 적용됩니다.

`ELFA_EARS` 및 `bio_armor_head`와 같은 레거시 돌연변이 및 바이오닉 ID는 여전히 작동합니다.
다른 오버레이의 순서를 재배치하려면 `worn_backpack`, `wielded_katana`,
`mutation_ELFA_EARS` 또는 `effect_onfire`와 같은 전체 오버레이 ID를 사용하세요.

## `order`

(정수)

돌연변이 오버레이의 순서 값입니다. 값의 범위는 0 - 9999이며, 9999가 가장 위에 그려지는
레이어입니다. 어떤 목록에도 없는 돌연변이는 기본적으로 9999로 설정됩니다.

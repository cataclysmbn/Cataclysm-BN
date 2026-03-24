---
title: 식물
---

> [!NOTE]
>
> 이 문서는 최근 `JSON INFO`에서 분리되었으며 추가 작업이 필요할 수 있습니다

### `plant_data`

```json
{
  "transform": "f_planter_harvest",
  "base": "f_planter",
  "growth_multiplier": 1.2,
  "harvest_multiplier": 0.8
}
```

#### `transform`

`PLANT` 가구가 단계를 성장할 때 무엇으로 변하는지, 또는 `PLANTABLE` 가구가 심어졌을 때 무엇으로 변하는지.

#### `base`

`PLANT` 가구의 '기본' 가구는 무엇인지 - 식물이 자라고 있지 않다면 무엇이 될지. 몬스터가 식물을 '먹을' 때 무슨 가구인지 보존하는 데 사용됩니다.

#### `growth_multiplier`

식물의 성장 속도에 대한 평면 승수. 1보다 큰 숫자의 경우 성장하는 데 더 오래 걸리고, 1보다 작은 숫자의 경우 성장하는 데 더 적은 시간이 걸립니다.

#### `harvest_multiplier`

식물의 수확 횟수에 대한 평면 승수. 1보다 큰 숫자의 경우 식물은 수확에서 더 많은 생산물을 제공하고, 1보다 작은 숫자의 경우 더 적은 생산물을 제공합니다.

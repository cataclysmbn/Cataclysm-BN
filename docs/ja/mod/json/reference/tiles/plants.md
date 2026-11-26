---
title: Plants
---

> [!NOTE]
>
> この記事は最近 `JSON INFO` から分割されたもので、追加の作業が必要になる可能性があります。

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

`PLANT` 家具が成長して段階を進めるときに変化する家具、または `PLANTABLE` 家具に植えたときに変化する家具です。

#### `base`

`PLANT` 家具の「基礎」となる家具、つまり植物が生えていない場合の家具です。モンスターが植物を「食べた」際に家具の種類を維持するために使用されます。

#### `growth_multiplier`

植物の成長速度に対する固定倍率です。1より大きい値では成長に時間がかかり、1より小さい値では成長が早くなります。

#### `harvest_multiplier`

植物の収穫数に対する固定倍率です。1より大きい値では収穫物が増え、1より小さい値では収穫物が減ります。

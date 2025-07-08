---
title: Plants
---

:::note
This article was recently split off from `JSON INFO` and likely could use additional work
:::

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

What the `PLANT` furniture turn into when it grows a stage, or what a `PLANTABLE` furniture turns
into when it is planted on.

#### `base`

What the 'base' furniture of the `PLANT` furniture is - what it would be if there was not a plant
growing there. Used when monsters 'eat' the plant to preserve what furniture it is.

#### `growth_multiplier`

A flat multiplier on the growth speed on the plant. For numbers greater than one, it will take
longer to grow, and for numbers less than one it will take less time to grow.

#### `harvest_multiplier`

A flat multiplier on the harvest count of the plant. For numbers greater than one, the plant will
give more produce from harvest, for numbers less than one it will give less produce from harvest.

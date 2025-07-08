---
title: Monster Spawning System
---

:::note
This article was recently split off from `JSON INFO` and likely could use additional work
:::

### Monster Groups

#### Group definition

| Identifier  | Description                                                                                                                                                                                                                                                             |
| ----------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `name`      | Unique ID. Must be one continuous word, use underscores if necessary.                                                                                                                                                                                                   |
| `default`   | Default monster, automatically fills in any remaining spawn chances.                                                                                                                                                                                                    |
| `monsters`  | To choose a monster for spawning, the game creates 1000 entries and picks one. Each monster will have a number of entries equal to it's "freq" and the default monster will fill in the remaining. See the table below for how to build the single monster definitions. |
| `is_safe`   | (bool) Check to not trigger safe-mode warning.                                                                                                                                                                                                                          |
| `is_animal` | (bool) Check if that group has only normal animals.                                                                                                                                                                                                                     |

#### Monster definition

| Identifier        | Description                                                                                                                                                                                                                                                                                                                                                                                                                         |
| ----------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `monster`         | The monster's unique ID, eg. `"mon_zombie"`.                                                                                                                                                                                                                                                                                                                                                                                        |
| `freq`            | Chance of occurrence, x/1000.                                                                                                                                                                                                                                                                                                                                                                                                       |
| `cost_multiplier` | How many monsters each monster in this definition should count as, if spawning a limited number of monsters.                                                                                                                                                                                                                                                                                                                        |
| `pack_size`       | (_optional_) The minimum and maximum number of monsters in this group that should spawn together. (default: `[1,1]`)                                                                                                                                                                                                                                                                                                                |
| `conditions`      | Conditions limit when monsters spawn. Valid options: `SUMMER`, `WINTER`, `AUTUMN`, `SPRING`, `DAY`, `NIGHT`, `DUSK`, `DAWN`. Multiple Time-of-day conditions (`DAY`, `NIGHT`, `DUSK`, `DAWN`) will be combined together so that any of those conditions makes the spawn valid. Multiple Season conditions (`SUMMER`, `WINTER`, `AUTUMN`, `SPRING`) will be combined together so that any of those conditions makes the spawn valid. |
| `starts`          | (_optional_) This entry becomes active after this time. (Measured in hours)                                                                                                                                                                                                                                                                                                                                                         |
| `ends`            | (_optional_) This entry becomes inactive after this time. (Measured in hours)                                                                                                                                                                                                                                                                                                                                                       |

```json
{
  "name": "GROUP_ANT",
  "default": "mon_ant",
  "monsters": [
    { "monster": "mon_ant_larva", "freq": 40, "multiplier": 0 },
    { "monster": "mon_ant_soldier", "freq": 90, "multiplier": 5 },
    { "monster": "mon_ant_queen", "freq": 0, "multiplier": 0 },
    {
      "monster": "mon_thing",
      "freq": 100,
      "multiplier": 0,
      "pack_size": [3, 5],
      "conditions": ["DUSK", "DAWN", "SUMMER"]
    }
  ]
}
```

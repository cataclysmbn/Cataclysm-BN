---
title: Monster Spawning System
---

> [!NOTE]
>
> This article was recently split off from `JSON INFO` and likely could use additional work

### Monster Groups

#### Group definition

| Identifier             | Description                                                                                                                                                                                                                                                             |
| ---------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `name`                 | Unique ID. Must be one continuous word, use underscores if necessary.                                                                                                                                                                                                   |
| `default`              | Default monster, automatically fills in any remaining spawn chances.                                                                                                                                                                                                    |
| `monsters`             | To choose a monster for spawning, the game creates 1000 entries and picks one. Each monster will have a number of entries equal to it's "freq" and the default monster will fill in the remaining. See the table below for how to build the single monster definitions. |
| `is_safe`              | (bool) Check to not trigger safe-mode warning.                                                                                                                                                                                                                          |
| `is_animal`            | (bool) Check if that group has only normal animals.                                                                                                                                                                                                                     |
| `evolve_repeat`        | (int) Number of attempts for extra default evolutions. 0 means off                                                                                                                                                                                                      |
| `evolve_chance`        | (int) Chance for extra evolutions from 0-100                                                                                                                                                                                                                            |
| `replacement_time`     | (time_duration) Time until it upgrades to `new_monster_group_id`                                                                                                                                                                                                        |
| `new_monster_group_id` | (mongroup_id) What group this upgrades to over time                                                                                                                                                                                                                     |
| `auto_total`           | (bool) Force `freq_total` to be the sum of all frequencies, no matter how it was defined                                                                                                                                                                                |

#### Monster definition

| Identifier        | Description                                                                                                                                                                                                                                                                                                                                                                                                                         |
| ----------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `monster`         | The monster's unique ID, eg. `"mon_zombie"`. Either this or `monster_group` is required                                                                                                                                                                                                                                                                                                                                             |
| `monster_group`   | The monster groups unique ID to pull from, eg. `"GROUP_NETHER"`                                                                                                                                                                                                                                                                                                                                                                     |
| `freq`            | Chance of occurrence, x/`freq_total`.                                                                                                                                                                                                                                                                                                                                                                                               |
| `weight`          | Chance of occurrence, weighted. If used without `freq_total` being defined ( I.E. `freq_total` == 1000 ), their sum is `freq_total`, otherwise it adds to freq total.                                                                                                                                                                                                                                                               |
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

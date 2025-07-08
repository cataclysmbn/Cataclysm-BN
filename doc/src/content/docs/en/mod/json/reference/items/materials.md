---
title: Material Definitions
---

:::note
This article was recently split off from `JSON INFO` and likely could use additional work
:::

### Materials

| Identifier             | Description                                                                                                                                                                                                |
| ---------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `id`                   | Unique ID. Lowercase snake_case. Must be one continuous word, use underscores if necessary.                                                                                                                |
| `name`                 | In-game name displayed.                                                                                                                                                                                    |
| `bash_resist`          | How well a material resists bashing damage.                                                                                                                                                                |
| `cut_resist`           | How well a material resists cutting damage.                                                                                                                                                                |
| `bullet_resist`        | How well a material resists bullet damage.                                                                                                                                                                 |
| `acid_resist`          | Ability of a material to resist acid.                                                                                                                                                                      |
| `elec_resist`          | Ability of a material to resist electricity.                                                                                                                                                               |
| `fire_resist`          | Ability of a material to resist fire.                                                                                                                                                                      |
| `chip_resist`          | Returns resistance to being damaged by attacks against the item itself.                                                                                                                                    |
| `bash_dmg_verb`        | Verb used when material takes bashing damage.                                                                                                                                                              |
| `cut_dmg_verb`         | Verb used when material takes cutting damage.                                                                                                                                                              |
| `dmg_adj`              | Description added to damaged item in ascending severity.                                                                                                                                                   |
| `dmg_adj`              | Adjectives used to describe damage states of a material.                                                                                                                                                   |
| `density`              | Density of a material.                                                                                                                                                                                     |
| `vitamins`             | Vitamins in a material. Usually overridden by item specific values.                                                                                                                                        |
| `wind_resist`          | Percentage 0-100. How effective this material is at stopping wind from getting through. Higher values are better. If none of the materials an item is made of specify a value, a default of 99 is assumed. |
| `warmth_when_wet`      | Percentage of warmth retained when fully drenched. Default is 0.2.                                                                                                                                         |
| `specific_heat_liquid` | Specific heat of a material when not frozen (J/(g K)). Default 4.186.                                                                                                                                      |
| `specific_heat_solid`  | Specific heat of a material when frozen (J/(g K)). Default 2.108.                                                                                                                                          |
| `latent_heat`          | Latent heat of fusion for a material (J/g). Default 334.                                                                                                                                                   |
| `freeze_point`         | Freezing point of this material (F). Default 32 F ( 0 C ).                                                                                                                                                 |
| `edible`               | Optional boolean. Default is false.                                                                                                                                                                        |
| `rotting`              | Optional boolean. Default is false.                                                                                                                                                                        |
| `soft`                 | Optional boolean. Default is false.                                                                                                                                                                        |
| `reinforces`           | Optional boolean. Default is false.                                                                                                                                                                        |

There are six -resist parameters: acid, bash, chip, cut, elec, and fire. These are integer values;
the default is 0 and they can be negative to take more damage.

```json
{
  "type": "material",
  "id": "hflesh",
  "name": "Human Flesh",
  "density": 5,
  "specific_heat_liquid": 3.7,
  "specific_heat_solid": 2.15,
  "latent_heat": 260,
  "edible": true,
  "rotting": true,
  "bash_resist": 1,
  "cut_resist": 1,
  "bullet_resist": 1,
  "acid_resist": 1,
  "fire_resist": 1,
  "elec_resist": 1,
  "chip_resist": 2,
  "dmg_adj": ["bruised", "mutilated", "badly mutilated", "thoroughly mutilated"],
  "bash_dmg_verb": "bruised",
  "cut_dmg_verb": "sliced",
  "vitamins": [["calcium", 0.1], ["vitB", 1], ["iron", 1.3]],
  "burn_data": [
    { "fuel": 1, "smoke": 1, "burn": 1, "volume_per_turn": "2500_ml" },
    { "fuel": 2, "smoke": 3, "burn": 2, "volume_per_turn": "10000_ml" },
    { "fuel": 3, "smoke": 10, "burn": 3 }
  ]
}
```

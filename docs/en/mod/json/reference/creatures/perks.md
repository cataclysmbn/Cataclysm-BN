# Perks

Perks are categorized bonuses given to the player.
Their only affects are either given via enchantments or lua based id hardcode

## Fields

| Identifier   | Description                                                                                                      |
| ------------ | ---------------------------------------------------------------------------------------------------------------- |
| id           | (_mandatory_) Unique string id                                                                                   |
| name         | (_mandatory_) In game name of the perk                                                                           |
| description  | (_mandatory_) In game description of the perk                                                                    |
| category     | (_mandatory_) Category of the enchantment, used as the in game perk menu name and for matching with other perks. |
| hidden       | (_optional_) Weather the enchantment is hidden in the perk menu, default false                                   |
| enchantments | (_optional_) Array of enchantments defined in either of the inline methods, what enchantments this perk provides |

## Example

```jsonc
{
  "type": "perk",
  "id": "duck_perk_eagle_eyes",
  "name": "Eagle Eyes",
  "description": "Your sharp gazes misses little. +25% to unenhanced sight range",
  "category": "Perk",
  "hidden": false,
  "enchantments": [
    { "values": [ { "value": "SIGHT_RANGE", "multiply": 0.25 } ] },
    { "id": "ENCH_INVISIBILITY" }
  ],
},
```

# Explosion Data

This is a piece of inline data reused across many different files
Kept here for global reference.

> [!CAUTION]
> This data is not standalone, it is always within another object

### Fields

> [!NOTE]
> This doesn't list the legacy fields and what they do

| Identifier      | Description                                                                  |
| --------------- | ---------------------------------------------------------------------------- |
| damage          | (_optional_) The amount of damage the explosion does                         |
| radius          | (_optional_) The radius of the explosion                                     |
| fragment        | (_optional_) See [projectiles](/mod/json/reference/misc/projectiles/#fields) |
| fragment_effect | (_optional_) Array of [fragment effects](#fragment-effect)                   |
| fire            | (_optional_) Also spawns fire fields                                         |

#### Fragment Effect

| Identifier | Description                                     |
| ---------- | ----------------------------------------------- |
| effect     | (_optional_) Id of effect to apply              |
| odds       | (_optional_) One in odds chance to apply effect |
| min_turns  | (_optional_) Minimum turns applied              |
| max_turns  | (_optional_) Maxiumum turns applied             |

### Example

```json
{
  "damage": 50,
  "radius": 2,
  "fire": false,
  "fragment_effect": [{ "effect": "onfire", "odds": 2, "min_turns": 4, "max_turns": 8 }],
  "fragment": {
    "impact": { "damage_type": "bullet", "amount": 70, "armor_multiplier": 1 },
    "range": 5
  }
}
```

# Relicgen

# Relicgen

Relicgen is a method of creating _static_ items with _dynamic_ effects and _dynamic_ names.

This is all defined via nested items in the `relicgen` type

## Relicgen

Relicgen is the high level id of what is spawned.
It selects name changes, effects, and the item type

| Identifier   | Description                                                                                                   |
| ------------ | ------------------------------------------------------------------------------------------------------------- |
| id           | (_mandatory_) Unique string id                                                                                |
| itypes       | (_mandatory_) Array of weighted itypes in the form `{ "itype": "relic_baseline_flesh_glob", "weight": 100 }`  |
| effects      | (_mandatory_) Array of [relicgen_sets](#relicgen-set)                                                         |
| min_power    | (_optional_) Minimum preset cost capacity it has; Default 1                                                   |
| max_power    | (_optional_) Maximum preset cost capacity it has; Default 1                                                   |
| name_prefix  | (_optional_) Prefixes to apply to the name; In the form of an `{ "prefix": "prefix", "weight": 1 }` object    |
| name_postfix | (_optional_) Postfixes to apply to the name; In the form of an `{ "postfix": "postfix", "weight": 1 }` object |

## Relicgen Set

Relicgen sets are categories of relicgen effects to applied in order.

Each one gets an attempt, if one effect is added it is recorded.

If nothing is applied in a whole loop through, artifact generation is cancelled.

Lack of application can occur when:

- Point count is too high
- Maximum number of the type has been reached

| Identifier | Description                                              |
| ---------- | -------------------------------------------------------- |
| min_count  | (_mandatory_) Minimum count of effects from this to draw |
| max_count  | (_mandatory_) Maximum count of effects from this to draw |
| effects    | (_mandatory_) Array of [relicgen parts](#relicgen-part)  |

## Relicgen Part

These are weighted values that actually provide the effects

| Identifier      | Description                                                                                                                     |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| weight          | (_mandatory_) Weight of this relicgen part                                                                                      |
| cost            | (_mandatory_) Relicgen point cost for this value                                                                                |
| recharge_scheme | (_optional_) Array of [recharge schemes](/docs/en/mod/json/reference/items/relics.md#relic-recharge) to apply                   |
| active_effects  | (_optional_) Array of active effects; Same format as [hit_you_effect](/docs/en/mod/json/reference/enchantments.md#hityoueffect) |
| enchantments    | (_optional_) Array of inline enchantments using either the `id` method, or the standard inline method                           |

## Example

```json
{
  "type": "relicgen",
  "id": "wriggling",
  "//": "These artifacts are not power capped in any sense",
  "min_power": 1,
  "max_power": 1,
  "effects": [
    {
      "//": "Passive Good",
      "min_count": 1,
      "max_count": 1,
      "effects": [
        { "weight": 1, "cost": 0, "enchantments": [ { "id": "AEP_SPEED_UP" } ] },
        { "weight": 1, "cost": 0, "enchantments": [ { "id": "AEP_SNAKES" } ] },
        { "weight": 1, "cost": 0, "enchantments": [ { "id": "AEP_FUN" } ] },
        { "weight": 1, "cost": 0, "enchantments": [ { "id": "AEP_NULL" } ] }
      ]
    },
    {
      "//": "Passive Bad",
      "min_count": 1,
      "max_count": 1,
      "effects": [
        { "weight": 1, "cost": 0, "enchantments": [ { "id": "AEP_DEX_DOWN" } ] },
        { "weight": 1, "cost": 0, "enchantments": [ { "id": "AEP_FORCE_TELEPORT" } ] },
        { "weight": 1, "cost": 0, "enchantments": [ { "id": "AEP_SICK" } ] },
        { "weight": 1, "cost": 0, "enchantments": [ { "id": "AEP_NULL" } ] }
      ]
    },
    {
      "//": "Active Good",
      "min_count": 1,
      "max_count": 1,
      "effects": [
        { "weight": 1, "cost": 0, "active_effects": [ { "id": "AEA_TELEPORT" } ] },
        { "weight": 1, "cost": 0, "active_effects": [ { "id": "AEA_ADRENALINE" } ] },
        { "weight": 1, "cost": 0, "active_effects": [ { "id": "AEA_FUN" } ] },
        { "weight": 1, "cost": 0, "enchantments": [ { "id": "AEP_NULL" } ] }
      ]
    },
    {
      "//": "Active Bad",
      "min_count": 1,
      "max_count": 1,
      "effects": [
        { "weight": 1, "cost": 0, "active_effects": [ { "id": "AEA_MUTATE" } ] },
        { "weight": 1, "cost": 0, "active_effects": [ { "id": "AEA_ATTENTION" } ] },
        { "weight": 1, "cost": 0, "active_effects": [ { "id": "AEA_VOMIT" } ] },
        { "weight": 1, "cost": 0, "enchantments": [ { "id": "AEP_NULL" } ] }
      ]
    }
  ],
  "name_prefix": [ { "prefix": "wriggling", "weight": 1 } ],
  "itypes": [ { "itype": "relic_baseline_flesh_glob", "weight": 100 } ]
},
```

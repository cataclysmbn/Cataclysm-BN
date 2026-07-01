# Damage

Damage instances and simmilar inline values that are commonly reused.

Kept here for global reference.

> [!CAUTION]
> This data is not standalone, it is always within another object

## Damage Instance

There are two ways to define a damage insance, one as if it was a [damage unit](#damage-unit)
The other is below

### Fields

| Identifier | Description                                           |
| ---------- | ----------------------------------------------------- |
| values     | (_mandatory_) A array of [damage units](#damage-unit) |

### Example

```json
{
  "values": [
    {
      "damage_type": "acid",
      "amount": 10,
      "armor_penetration": 4,
      "armor_multiplier": 2.5
    }
  ]
}
```

## Damage Unit

### Fields

| Identifier        | Description                                                                    |
| ----------------- | ------------------------------------------------------------------------------ |
| damage_type       | (_mandatory_) A [type of damage](#damage-types)                                |
| amount            | (_mandatory_) How much damage to do                                            |
| armor_penetration | (_mandatory_) How much armor to penetrate per armor piece                      |
| armor_multiplier  | (_mandatory_) Multiplier on remaining armor after armor_penetration is applied |

### Example

```json
{
  "damage_type": "acid",
  "amount": 10,
  "armor_penetration": 4,
  "armor_multiplier": 2.5
}
```

## Damage Types

The below is a list of all damage types in the game

- true
- biological
- bash
- cut
- acid
- stab
- bullet
- heat
- cold
- dark
- light
- psi
- electric

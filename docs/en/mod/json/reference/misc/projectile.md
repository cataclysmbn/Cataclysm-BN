# Projectile

This is a piece of inline data reused across many different files
Kept here for global reference.

> [!CAUTION]
> This data is not standalone, it is always within another object

### Fields

| Identifier        | Description                                                                                             |
| ----------------- | ------------------------------------------------------------------------------------------------------- |
| impact            | (_mandatory_) A [damage instance](/mod/json/reference/misc/damage/#damage-instance) for each projectile |
| range             | (_mandatory_) The range of the projectile                                                               |
| speed             | (_optional_) Speed of the projectile in meters per second                                               |
| aimedcritbonus    | (_optional_) The minimum bonus from a crit                                                              |
| aimedcritmaxbonus | (_optional_) The maximum bonus from a crit                                                              |

### Example

```json
{
  "impact": { "damage_type": "bullet", "amount": 70, "armor_multiplier": 1 },
  "range": 5,
  "speed": 1000,
  "aimedcritbonus": 0.0,
  "aimedcritmaxbonus": 0.0
}
```

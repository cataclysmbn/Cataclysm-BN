# Relics

# Relics

Relics are regular items with special relic data attached to them.

Relic data is defined in JSON in `relic_data` field of corresponding item type definition, and
whenever the item is spawned a copy of relic data is attached to the item instance.

Relic data object can contain the following fields:

```json
{
"name": "Boots of Haste",       // Overrides default item name
"moves": 100,                   // (optional) Activation move cost (default 100)
"charges_per_activation": 1,    // (optional) Charges per activation (default 1)
"active_effects": [ {}, ... ],  // (optional) Spells executed on activation (identical to `hit_you_effect`, see [MAGIC.md](MAGIC.md/#hit_you_effect))
"passive_effects": [ {}, ... ], // (optional) List of passive effects (enchantments), see [MAGIC.md](MAGIC.md/#enchantments)
"recharge_scheme": [ {}, ... ], // (optional) List of recharge methods, see below
}
```

## Relic recharge

Relics can recharge under certain conditions. Recharge method is defined as follows (all fields
optional):

Only works for guns, tools and magazines. Won't work while an item is loaded into another item.

```json
{
  "type": "time", // Defines what resource is consumed. Default: time
  "req": "none", // Defines under what conditions recharge works. Default: none (no special requirements)
  "field": "fd_blood", // Field type to be consumed with 'field' recharge type
  "trap": "tr_portal", // Trap type to be consumed with 'trap' recharge type
  "interval": "5 minutes", // Interval at which the recharge check is done. Default: 1 second
  "int_min": 1, // Min intensity of related 'type' effect. Default: 0
  "int_min": 5, // Max intensity of related 'type' effect. Default: 0
  "rate": 2, // Amount of charges restored when recharge operation succeeds. Default: 0
  "message": "Your body decays!" // Optional message to print on success
}
```

### Recharge type

| ID        | Description                                                                                                                                    |
| --------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| `time`    | Needs no additional resources                                                                                                                  |
| `solar`   | Consumes sunlight (character must be in sunlight)                                                                                              |
| `pain`    | Causes pain to recharge. Intensity controlled by `int_min` and `int_max`                                                                       |
| `hp`      | Causes damage to all body parts. Intensity controlled by `int_min` and `int_max`                                                               |
| `fatigue` | Causes fatigue and drains stamina. Fatigue drain controlled by `int_min` and `int_max`, stamina drain rolled as `[ int_min*100, int_max*100 ]` |
| `field`   | Consumes adjacent field. Allowed field intensity controlled by `int_min` and `int_max`                                                         |
| `trap`    | Consumes adjacent trap.                                                                                                                        |

### Recharge requirements

| ID              | Description                                                               |
| --------------- | ------------------------------------------------------------------------- |
| `none`          | No additional requirements (always works)                                 |
| `equipped`      | Must be worn if armor, wielded if weapon.                                 |
| `close_to_skin` | Must be worn underneath all other clothing, or be wielded with bare hands |
| `sleep`         | Character must be asleep                                                  |
| `rad`           | Character or map tile must be irradiated                                  |
| `wet`           | Character must be wet, or it's raining                                    |
| `sky`           | Character must be above z=0                                               |

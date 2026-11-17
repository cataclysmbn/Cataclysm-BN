# Character Resources

## Character Resource

The jsonized value for character wide drainable resources

### Fields

| Identifier     | Description                                                                          |
| -------------- | ------------------------------------------------------------------------------------ |
| id             | (_mandatory_) Unique string id                                                       |
| need_info      | (_optional_) Info to display when you need more charges                              |
| item_info      | (_optional_) Info to display on items with this resource                             |
| drain_flag     | (_optional_) Items with this flag will have the charges drained from ( I.E. IS_UPS ) |
| drain_resource | (_optional_) Hardcode character resource to drain ( see below )                      |
| charge_scalar  | (_optional_) Multiplier to use when draining via this resource                       |

```jsonc
{
  "type": "character_resource",
  "id": "ups",
  "drain_flag": "IS_UPS",
  "drain_resource": "RAD",
  "item_info": "*  This tool drains charges from a <info>universal power supply</info>",
  "need_info": "universal power supply",
  "charge_scalar": 1.0,
}
```

## Hardcode Character Resource

These are valid values for `drain_resource` above

| Identifier    | Description                                                          |
| ------------- | -------------------------------------------------------------------- |
| health_points | Drains health points equal to the value, all from a single body part |
| mana          | Drains mana points equal to the value                                |
| stamina       | Drains stamina equal to the value                                    |
| bionic        | Drains bionic storage power equal to the value                       |
| fatigue       | Causes increased fatigue equal to the value                          |
| pain          | Causes pain equal to the value                                       |
| radiation     | Causes radiation equal to the value                                  |
| health        | Lowers health to equal to the value                                  |
| hunger        | Causes hunger equal to the value                                     |
| none          | Default value, does nothing                                          |

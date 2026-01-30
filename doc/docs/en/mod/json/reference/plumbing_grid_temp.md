# Plumbing Grid

This document describes the JSON plumbing grid support for furniture.

## Plumbing object (furniture)

Furniture can define a `plumbing` object to mark it as a plumbing grid tank or fixture and control
what liquids it can accept.

### Fields

| Identifier | Description |
| --- | --- |
| role | (_mandatory_) Either `tank` or `fixture`. Tanks contribute storage capacity to the plumbing grid. Fixtures are grid endpoints without storage. |
| allow_input | (_mandatory_) Whether the grid can accept liquids into this furniture. |
| allow_output | (_mandatory_) Whether the grid can output liquids from this furniture. |
| allowed_liquids | (_mandatory_) Array of liquid item ids allowed for this furniture (for example `water`, `water_clean`). |
| capacity | (_optional_) Integer capacity in the game's standard volume unit. If set, this overrides `use_keg_capacity`. |
| use_keg_capacity | (_optional_) If `true`, use the furniture `keg_capacity` as its tank capacity. |
| plumbed_variant | (_optional_) Furniture id to switch to when connecting this tank to the plumbing grid. |
| unplumbed_variant | (_optional_) Furniture id to switch to when disconnecting this tank from the plumbing grid. |

### Notes

- For `role: "tank"`, you must set either `capacity` or `use_keg_capacity`. If both are set,
  `capacity` is used.
- For `role: "tank"`, you must define at least one of `plumbed_variant` or `unplumbed_variant`.
  Defining one side lets the engine infer the other during load.
- For `role: "fixture"`, capacity and variant fields are ignored.
- Typically, only the plumbed variant needs the `plumbing` object. Define
  `unplumbed_variant` on the plumbed furniture and leave the unplumbed furniture without
  `plumbing` so it does not contribute to grid storage until connected.
- Plumbing grids store only one water quality at a time. Adding any dirty water contaminates the
  entire stored supply, and clean water added to a dirty grid becomes dirty. A grid becomes clean
  only after all dirty water is drained and clean water is added.

### Examples

Tank using `keg_capacity`:

```json
{
  "type": "furniture",
  "id": "f_standing_tank_plumbed",
  "copy-from": "f_standing_tank",
  "name": "standing tank (plumbed)",
  "keg_capacity": 1200,
  "plumbing": {
    "role": "tank",
    "allow_input": true,
    "allow_output": true,
    "allowed_liquids": [ "water", "water_clean" ],
    "unplumbed_variant": "f_standing_tank",
    "use_keg_capacity": true
  }
}
```

Tank with explicit capacity:

```json
{
  "type": "furniture",
  "id": "f_custom_plumbed_tank",
  "name": "custom plumbed tank",
  "plumbing": {
    "role": "tank",
    "allow_input": true,
    "allow_output": true,
    "allowed_liquids": [ "water", "water_clean" ],
    "capacity": 2000
  }
}
```

Fixture (no storage):

```json
{
  "type": "furniture",
  "id": "f_plumbed_sink",
  "name": "plumbed sink",
  "examine_action": "plumbing_fixture",
  "plumbing": {
    "role": "fixture",
    "allow_input": true,
    "allow_output": true,
    "allowed_liquids": [ "water", "water_clean" ]
  }
}
```

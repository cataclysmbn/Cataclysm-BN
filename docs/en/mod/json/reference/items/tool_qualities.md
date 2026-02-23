### Tool Qualities

> [!NOTE]
>
> This article is new and may need more examples and links.

Tool qualities define named capability tiers used by recipes and other systems. They are loaded from
`data/json/tool_qualities.json`.

```json
{
  "type": "tool_quality", // Tool quality definition
  "id": "SEW", // Unique quality id
  "name": { "str": "sewing" }, // Display name for the quality
  "crafting_speed_bonus_per_level": 1.1, // Optional per-level multiplier (see below)
  "crafting_speed_level_offset": 2 // Optional per-level offset (see below)
}
```

#### Fields

- `id`: Unique quality id.
- `name`: Display name for the quality.
- `crafting_speed_bonus_per_level`: Optional (default = 0.0). Crafting speed multiplier per
  quality level above the recipe requirement (e.g., `1.1` for +10% per extra level), applied only
  when the item does not define its own `crafting_speed_modifier`.
- `crafting_speed_level_offset`: Optional (default = 0). Minimum quality level at which
  `crafting_speed_bonus_per_level` starts to apply, even if recipes require lower levels.

#### Crafting Usage

Tool qualities are used in recipes to group many equivalent items under a single capability. Any
item that lists a quality in its `qualities` array can satisfy a recipe `qualities` requirement at
or above the requested level. This lets you avoid listing every acceptable tool explicitly.

Example recipe requirement:

```json
"qualities": [ { "id": "SEW", "level": 2 } ]
```

Example items:

```json
"qualities": [ [ "SEW", 2 ] ]   // Basic sewing tools
"qualities": [ [ "SEW", 4 ] ]   // Higher-quality tailoring kit
```

When a recipe requires `SEW` 2, any item with `SEW` 2 or higher can be used. If the quality defines
`crafting_speed_bonus_per_level`, higher levels above the requirement increase crafting speed
unless the item provides its own `crafting_speed_modifier`. If
`crafting_speed_level_offset` is set, the speed bonus starts at the higher of the recipe level and
the offset.

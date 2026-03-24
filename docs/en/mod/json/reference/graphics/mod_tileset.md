# Mod Tileset

MOD tileset defines additional sprite sheets. It is specified as JSON object with `type` member set
to `mod_tileset`.

Example:

```json
[
  {
    "type": "mod_tileset",
    "compatibility": ["MshockXottoplus"],
    "tiles-new": [
      {
        "file": "test_tile.png",
        "tiles": [
          {
            "id": "player_female",
            "fg": 1,
            "bg": 0
          },
          {
            "id": "player_male",
            "fg": 2,
            "bg": 0
          }
        ]
      }
    ]
  }
]
```

## `compatibility`

(string)

The internal ID of the compatible tilesets. MOD tileset is only applied when base tileset's ID
exists in this field.

## `tiles-new`

Setting of sprite sheets. Same as `tiles-new` field in `tile_config`. Sprite files are loaded from
the same folder json file exists.

## `state-modifiers`

State modifiers allow mod tilesets to define or override UV-based sprite modifications for character states. When a mod tileset defines a state modifier group with the same `id` as the base tileset, the mod's definition replaces the base tileset's.

```json
{
  "type": "mod_tileset",
  "compatibility": ["UndeadPeopleTileset"],
  "tiles-new": [
    {
      "file": "uv-tiles.png",
      "tiles": [],
      "state-modifiers": [
        {
          "id": "movement_mode",
          "override": false,
          "use_offset": false,
          "tiles": [
            { "id": "walk", "fg": null },
            { "id": "crouch", "fg": 1 },
            { "id": "run", "fg": 2 }
          ]
        }
      ]
    }
  ]
}
```

The `state-modifiers` array goes inside a `tiles-new` entry alongside `tiles`. Each modifier group requires:

| Field        | Type   | Description                                                 |
| ------------ | ------ | ----------------------------------------------------------- |
| `id`         | string | Group identifier (`movement_mode`, `downed`, `lying_down`). |
| `override`   | bool   | Skip lower-priority groups when this state is active.       |
| `use_offset` | bool   | `true` for offset mode, `false` for normalized UV mode.     |
| `tiles`      | array  | State-to-sprite mappings.                                   |

See the [State Modifiers section in Tilesets](./tileset.md#state-modifiers) for full documentation on UV mapping modes and creating modifier sprites.

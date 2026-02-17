# Tilesets

A tileset provides graphic images for the game. Each tileset has one or more tilesheets of image
sprites and a `tile_config.json` file that describes how to map the contents of the sprite sheets to
various entities in the game. It also has a `tileset.txt` file that provides metadata.

## Compositing Tilesets

Prior October 2019, tilesets had to be submitted to the repo with each tilesheet fully composited
and the sprite indices in `tile_config.json` calculated by hand. After October 2019, tilesets can be
submitted to repos as directories of individual sprite files and tile entry JSON files that used
sprite file names, and a Python script that runs at compile time would merge the sprite images into
tilesheets, convert the files names into sprite indices for the tile entries, and merge the tile
entries into a `tile_config.json`.

For the rest of this document, tilesets that are submitted as fully composited tilesheets are called
legacy tilesets, and tilesets that submitted as individual sprite image files are compositing
tilesets.

### tools/gfx_tools/decompose.py

This is a Python script that will convert a legacy tileset into a compositing tileset. It reads the
`tile_config.json` and assigns semi-arbitrary file names to each sprite index. Then it changes all
the sprite indexes references to the file names. Then it breaks up `tile_config.json` into many
small tile_entry JSON files with arbitrary file names, and pulls out each sprite and writes it to
aseparate file.

It requires pyvips to do the image processing.

It takes a single mandatory argument, which is the path to the tileset directory. For example:
`python tools/gfx_tools/decompose.py gfx/ChestHole16Tileset` will convert the legacy ChestHole16
tileset to a compositing tileset.

decompose.py creates a sufficient directory hierarchy and file names for a tileset to be
compositing, but it is machine generated and badly organized. New compositing tilesets should use
more sensible file names and a better organization.

It shouldn't be necessary to run decompose.py very often. Legacy tilesets should only need to be
converted to composite tilesets one time.

### tools/gfx_tools/compose.py

This is a Python script that creates the tilesheets for a compositing tileset. It reads all of the
directories in a tileset's directory with names that start with `pngs_` for sprite files and
`tile_entry` JSON files, creates mappings of sprite file names to indices, merges the sprite files
into tilesheets, changes all of the sprite file name references in the `tile_entries` to indices,
and merges the `tile_entries` into a `tile_config.json`.

Like decompose.py, it requires pyvips to the image processing.

The original sprite files and `tile_entry` JSON files are preserved.

### directory structure

Each compositing tileset has one or more directories in it with a name that starts with `pngs_`,
such as `pngs_tree_32x40` or `pngs_overlay`. These are the image directories. All of the sprites in
an image directory must have the same height and width and will be merged into a single tilesheet.

It is recommended that tileset developers include the sprite dimensions in the image directory name,
but this is not required. `pngs_overlay_24x24` is preferred over `pngs_overlay` but both are
allowed. As each image directory creates its own tilesheet, and tilesheets should be as large as
possible for performance reasons, tileset developers are strongly encouraged to minimize the number
of image directories.

Each image directory contains a hierarchy of subdirectories, `tile_entry` JSON files, and sprite
files. There is no restriction on the arrangement or names of these files, except for `tile_entry`
JSON files for expansion tilesheets must be at the top level of the image directory. Subdirectories
are not required but are recommended to keep things manageable.

#### `tile_entry` JSON

Each `tile_entry` JSON is a dictionary that describes how to map one or more game entities to one or
more sprites. The simplest version has a single game entity, a single foreground sprite, an
_optional_ background sprite, and a rotation value. For instance:

```cpp
{                                           // this is an object and doesn't require a list
    "id": "mon_cat",                        // the game entity represented by this sprite
    "fg": "mon_cat_black",                  // some sprite name
    "bg": "shadow_bg_1",                    // some sprite name; always a single value
    "rotates": false                        // true for things that rotate like vehicle parts
}
```

The values in `"id"`, `"fg"`, and `"bg"` can be repeated within an image directory or in different
image directories. `"fg"` and `"bg"` sprite images can be referenced across image directories, but
the sprites must be stored in an image directory with other sprites of the same height and width.

`"id"` can also be a list of multiple game entities sharing the same sprite, like
`"id": ["vp_door"], ["vp_hddoor"]`. `"id"` can be any vehicle part, terrain, furniture, item, or
monster in the game. The special ids `"player_female", "player_male", "npc_female", "npc_male"` are
used to identify the sprites for the player avatar and NPCs. The special id `"unknown"` provides a
sprite that is displayed when an entity has no other sprite.

The special suffixes `_season_spring`, `_season_summer`, `_season_autumn`, and `_season_winter` can
be applied to any entity id to create a seasonal variant for that entity that will be displayed in
the appropriate season like this `"id": "mon_wolf_season_winter"`.

The special prefixes `overlay_mutation_`, `overlay_female_mutation_`, `overlay_male_mutation_` can
prefix any trait or bionic in the game to specify an overlay image that will be laid over the player
and NPC sprites to indicate they have that mutation or bionic.

The special prefixes `overlay_worn_`, `overlay_female_worn_`, `overlay_male_worn_` can prefix any
item in the game to specify an overlay image that will be laid over the player and NPC sprites to
indicate they are wearing that item.

The special prefixes `overlay_wielded_`, `overlay_female_wielded_`, `overlay_male_wielded_` can
prefix any item in the game to specify an overlay image that will be laid over the player and NPC
sprites to indicate they are holding that item.

`"fg"` and `"bg"` can also be a list of 2 or 4 pre-rotated rotational variants, like
`"bg": ["t_wall_n", "t_wall_e", "t_wall_s", "t_wall_w"]` or
`"fg": ["mon_dog_left", "mon_dog_right"]`.

`"fg"` and `"bg"` can also be a list of dictionaries of weighted, randomly chosen options, any of
which can also be a rotated list:

```cpp
"fg": [
    { "weight": 50, "sprite": "t_dirt_brown"},       // appears in 50 of 53 tiles
    { "weight": 1, "sprite": "t_dirt_black_specks"}, // appears 1 in 53 tiles
    { "weight": 1, "sprite": "t_dirt_specks_gray"},
    { "weight": 1, "sprite": "t_patchy_grass"}       // file names are arbitrary
],
```

`"multitle"` is an _optional_ field. If it is present and `true`, there must be an
`additional_tiles` list with 1 or more dictionaries for entities and sprites associated with this
tile, such as broken versions of an item or wall connections. Each dictionary in the list has an
`"id`" field, as above, and a `"fg"` field, which can be a single filename, a list of filenames, or
a list of dictionaries as above.

Each `tile_entry.json` file can have a single object in it, or a list of 1 or more objects like so:

```cpp
[
    { "id": "mon_zombie", "fg": "mon_zombie", "bg": "mon_zombie_bg", "rotates": false },
    { "id": "corpse_mon_zombie", "fg": "mon_zombie_corpse", "bg": "mon_zombie_bg", "rotates": false },
    { "id": "overlay_wielding_corse_mon_zombie", "fg": "wielded_mon_zombie_corpse", "bg": [], "rotates": false }
]
```

Having a list of tile entries in a file may be useful for organization, but completely unrelated
entries may all exist in the same file without any complications.

#### expansion `tile_entry` JSON

Tilesheets can have expansion tilesheets, which are tilesheets from mods. Each expansion tilesheet
is a single `"id"` value, `"rotates": false"`, and `"fg": 0`. Expansion `tile_entry` JSON are the
only `tile_entry` JSONs that use an integer value for `"fg"` and that value must be 0. Expansion
`tile_entry` JSONs must be located at the top layer of each image directory.

#### Sprite Images

Every sprite inside an image directory must have the same height and width as every other sprite in
the image directory.

Sprites can be organized into subdirectories within the image directory however the tileset
developer prefers. Sprite filenames are completely arbitrary and should be chosen using a scheme
that makes sense to the tileset developer.

After loading a tileset, config/debug.log will contain a space separated list of every entity
missing a sprite in the tileset. Entities that have sprites because of a `"looks_like"` definition
will not show up in the list.

### `tile_info.json`

Each compositing tileset _must_ have a `tile_info.json`, laid out like so:

```
[
  {
    "width": 32,
    "pixelscale": 1,
    "height": 32
  },
  {
    "1_tiles_32x32_0-5199.png": {}
  },
  {
    "2_expan_32x32_5200-5391.png": {}
  },
  {
    "3_tree_64x80_5392-5471.png": {
      "sprite_offset_x": -16,
      "sprite_offset_y": -48,
      "sprite_height": 80,
      "sprite_width": 64
    }
  },
  {
    "4_fallback_5472-9567.png": { "fallback": true }
  }
]
```

The first dictionary is mandatory, and gives the default sprite width and sprite height for all
tilesheets in the tileset. Each of the image directories must have a separate dictionary, containing
the tilesheet png name as its key. If the tilesheet has the default sprite dimensions and no special
offsets, it can have an empty dictionary as the value for the tilesheet name key. Otherwise, it
should have a dictionary of the sprite offsets, height, and width.

A special key is `"fallback"` which should be `true` if present. If a tilesheet is designated as
fallback, it will be treated as a tilesheet of fallback ASCII characters. `compose.py` will also
compose the fallback tilesheet to the end of the tileset, and will add a "fallback.png" to
`tile_config.json` if there is no `"fallback"` entry in `tile_info.json`.

A special is `"filler"` which should be `true` if present. If a tilesheet is designated as filler,
entries from its directory will be ignored if an entry from a non-filler directory has already
defined the same id. Entries will also be ignored if the id was already defined by in the filler
directory. Also, pngs from a filler directory will be ignored if they share a name with a png from a
non-filler directory. A filler tilesheet is useful when upgrading the art in a tileset: old,
low-quality art can be placed on filler tilesheet and will be automatically replaced as better
images are added to the non-filler tilesheets.

## Legacy tilesets

### tilesheets

Each tilesheet contains 1 or more sprites with the same width and height. Each tilesheet contains
one or more rows of exactly 16 sprites. Sprite index 0 is special and the first sprite of the first
tilesheet in a tileset should be blank. Indices run sequentially through each sheet and continue
incrementing for each new sheet without reseting, so index 32 is the first sprite in the third row
of the first sheet. If the first sheet has 320 sprites in it, index 352 would be the first sprite of
the third row of the second sheet.

### `tile_config`

Each legacy tileset has a `tile_config.json` describing how to map the contents of a sprite sheet to
various tile identifiers, different orientations, etc. The ordering of the overlays used for
displaying mutations can be controlled as well. The ordering can be used to override the default
ordering provided in `mutation_ordering.json`. Example:

```json
{ // whole file is a single object
  "tile_info": [ // tile_info is mandatory
    {
      "height": 32,
      "width": 32,
      "iso": true, //  Optional. Indicates an isometric tileset. Defaults to false.
      "pixelscale": 2 //  Optional. Sets a multiplier for resizing a tileset. Defaults to 1.
    }
  ],
  "tiles-new": [ // tiles-new is an array of sprite sheets
    { //   alternately, just one "tiles" array
      "file": "tiles.png", // file containing sprites in a grid
      "tiles": [ // array with one entry per tile
        {
          "id": "10mm", // id is how the game maps things to sprites
          "fg": 1, //   lack of prefix mostly indicates items
          "bg": 632, // fg and bg can be sprite indexes in the image
          "rotates": false
        },
        {
          "id": "t_wall", // "t_" indicates terrain
          "fg": [2918, 2919, 2918, 2919], // 2 or 4 sprite numbers indicates pre-rotated
          "bg": 633,
          "rotates": true,
          "multitile": true,
          "additional_tiles": [ // connected/combined versions of sprite
            { //   or variations, see below
              "id": "center",
              "fg": [2919, 2918, 2919, 2918]
            },
            {
              "id": "corner",
              "fg": [2924, 2922, 2922, 2923]
            },
            {
              "id": "end_piece",
              "fg": [2918, 2919, 2918, 2919]
            },
            {
              "id": "t_connection",
              "fg": [2919, 2918, 2919, 2918]
            },
            {
              "id": "unconnected",
              "fg": 2235
            }
          ]
        },
        {
          "id": "vp_atomic_lamp", // "vp_" vehicle part
          "fg": 3019,
          "bg": 632,
          "rotates": false,
          "multitile": true,
          "additional_tiles": [
            {
              "id": "broken", // variant sprite
              "fg": 3021
            }
          ]
        },
        {
          "id": "t_dirt",
          "rotates": false,
          "fg": [
            { "weight": 50, "sprite": 640 }, // weighted random variants
            { "weight": 1, "sprite": 3620 },
            { "weight": 1, "sprite": 3621 },
            { "weight": 1, "sprite": 3622 }
          ]
        },
        {
          "id": [
            "overlay_mutation_GOURMAND", // character overlay for mutation
            "overlay_mutation_male_GOURMAND", // overlay for specified gender
            "overlay_mutation_active_GOURMAND" // overlay for activated mutation
          ],
          "fg": 4040
        }
      ]
    },
    { // second entry in tiles-new
      "file": "moretiles.png", // another sprite sheet
      "tiles": [
        {
          "id": ["xxx", "yyy"], // define two ids at once
          "fg": 1,
          "bg": 234
        }
      ]
    }
  ],
  "overlay_ordering": [
    {
      "id": "WINGS_BAT", // mutation name, in a string or array of strings
      "order": 1000 // range from 0 - 9999, 9999 being the topmost layer
    },
    {
      "id": ["PLANTSKIN", "BARK"], // mutation name, in a string or array of strings
      "order": 3500 // order is applied to all items in the array
    },
    {
      "id": "bio_armor_torso", // Overlay order of bionics is controlled in the same way
      "order": 500
    }
  ]
}
```

## Projectile Sprites

Custom sprites can be defined for projectiles (bullets and thrown items) using specific naming conventions:

> [!NOTE]
> included sprite must face upwards (0 degrees) for correct orientation.

### Bullets (fired from guns)

Use `animation_bullet_{ammo_type}` where `{ammo_type}` is the ammo's item ID:

```json
{ "id": "animation_bullet_9mm", "fg": 123, "rotates": true }
{ "id": "animation_bullet_556", "fg": 124, "rotates": true }
{ "id": "animation_bullet_762", "fg": 125, "rotates": true }
```

The system follows the ammo's `looks_like` chain. If `animation_bullet_556` doesn't exist but the `556` ammo has `looks_like: "223"`, it will automatically use `animation_bullet_223` if available.

### Thrown Items

Use `animation_bullet_{item_type}` where `{item_type}` is the thrown item's ID:

```json
{ "id": "animation_bullet_javelin", "fg": 126, "rotates": true }
{ "id": "animation_bullet_throwing_axe", "fg": 127, "rotates": true }
{ "id": "animation_bullet_throwing_knife", "fg": 128, "rotates": true }
```

The system also follows the item's `looks_like` chain for thrown items.

### Fallback Behavior

If no custom projectile sprite is found:

1. **Thrown items**: Falls back to the item's own sprite (e.g., `javelin`)
2. **Bullets**: Falls back to `animation_bullet_normal_0deg`

### Rotation

- Items with the `FLY_STRAIGHT` flag (like javelins and spears) maintain their orientation during flight
- Other thrown items (axes, knives, etc.) will rotate during flight
- Set `"rotates": true` in the tile definition to enable directional sprite support

## State Modifiers

State modifiers allow tilesets to dynamically adjust character sprites based on game state (crouching, downed, etc.) without requiring separate artwork for each state. This is achieved through UV mapping, where a modifier image controls how pixels are displaced.

### How UV Mapping Works

UV mapping is a technique from 3D graphics where a 2D image controls how another image is sampled. In this context:

- Each pixel in a UV modifier image encodes a displacement using its red (X) and green (Y) channels
- When rendering, instead of drawing pixel (x, y) directly, the system reads the modifier at (x, y) to determine where to sample from the source sprite
- This allows effects like squishing, stretching, or shifting parts of a sprite

### Offset Mode vs Normalized Mode

State modifiers support two interpretation modes for the UV data:

**Offset Mode** (`"use_offset": true`, default):

- Red/Green values encode displacement relative to neutral (127, 127)
- Value 127 = no movement, 0 = -127 pixels, 255 = +128 pixels
- Easy to understand: paint gray (127,127) where no change is needed
- Displacements from multiple modifiers stack additively

**Normalized Mode** (`"use_offset": false`):

- Red/Green values encode absolute UV coordinates normalized to tile dimensions
- (0,0) samples bottom-left, (255,255) samples top-right
- More precise for complex remapping but harder to intuit
- Easier to modify quickly: rotating the uv results in a rotated result
- Modifiers chain by re-sampling through each other
- Marginally more computationally expensive

### JSON Structure

State modifiers are defined in the `"state-modifiers"` array within a tileset's tile configuration:

```json
"state-modifiers": [
  {
    "id": "movement_mode",
    "override": false,
    "use_offset": true,
    "tiles": [
      { "id": "walk", "fg": null },
      { "id": "crouch", "fg": 100 },
      { "id": "run", "fg": 101 }
    ]
  },
  {
    "id": "downed",
    "override": true,
    "use_offset": true,
    "tiles": [
      { "id": "normal", "fg": null },
      { "id": "downed", "fg": 102 }
    ]
  }
]
```

### Fields

| Field        | Type   | Description                                                              |
| ------------ | ------ | ------------------------------------------------------------------------ |
| `id`         | string | Modifier group identifier. Must match a supported group (see below).     |
| `override`   | bool   | If `true`, when this state is active, lower-priority groups are skipped. |
| `use_offset` | bool   | `true` for offset mode, `false` for normalized mode. Default: `true`.    |
| `tiles`      | array  | State-to-sprite mappings for this group.                                 |
| `whitelist`  | array  | Optional. Only apply to overlays matching these prefixes.                |
| `blacklist`  | array  | Optional. Never apply to overlays matching these prefixes.               |

Each entry in `tiles`:

| Field    | Type     | Description                                                                      |
| -------- | -------- | -------------------------------------------------------------------------------- |
| `id`     | string   | State identifier within the group.                                               |
| `fg`     | int/null | Sprite index for the UV modifier image. `null` means identity (no modification). |
| `offset` | object   | Optional `{"x": n, "y": n}` for oversized modifier sprites.                      |

### Supported Modifier Groups

| Group ID        | States                                     | Description                                      |
| --------------- | ------------------------------------------ | ------------------------------------------------ |
| `movement_mode` | `walk`, `run`, `crouch`                    | Character movement stance                        |
| `downed`        | `normal`, `downed`                         | Whether character is knocked down                |
| `lying_down`    | `normal`, `lying`                          | Whether character is lying down (sleeping, etc.) |
| `activity`      | `none`, activity IDs                       | Current activity (e.g., `ACT_CRAFT`, `ACT_READ`) |
| `body_size`     | `tiny`, `small`, `medium`, `large`, `huge` | Size of character (changed by mutations)         |

### Priority and Overrides

Modifier groups are processed in array order (index 0 = highest priority). When `"override": true` is set on a group and its state has an active modifier (non-null `fg`), all lower-priority groups are skipped. This allows, for example, a "downed" state to completely replace movement-based modifications.

### Overlay Filtering

Per-group `whitelist` and `blacklist` arrays control which overlays a modifier affects. Overlays are matched by prefix (e.g., `"wielded_"` matches `"wielded_katana"`). If a group specifies either filter, it overrides global filters. Common prefixes include `wielded_`, `worn_`, `mutation_`, `effect_`, and `bionic_`.

```json
{
  "id": "movement_mode",
  "blacklist": ["wielded_"],
  "tiles": [...]
}
```

Multiple groups can share the same `id` if they have different filters, allowing different UV modifiers for different overlay types. When doing this, **filters must be mutually exclusive**—each overlay should match at most one group per ID. Overlapping filters cause duplicate rendering artifacts.

```json
{
  "id": "movement_mode",
  "blacklist": ["wielded_"],
  "tiles": [...]
},
{
  "id": "movement_mode",
  "whitelist": ["wielded_"],
  "tiles": [...]
}
```

**Base sprite behavior:** The base character sprite (skin, eyes, hair, etc.) is not an overlay and has no prefix. Groups with a `whitelist` never apply to the base sprite since it cannot match any prefix. Groups with only a `blacklist` (or no filters) apply to the base sprite normally. To apply different modifiers to the base sprite vs specific overlays, use a blacklist group for the base and a whitelist group for the overlays.

### Creating UV Modifier Sprites

#### Method 1; use_offset = true

1. Start with a neutral gray image (RGBA 127, 127, 0, 255)
2. Paint red channel to shift pixels horizontally (< 127 = left, > 127 = right)
3. Paint green channel to shift pixels vertically (< 127 = up, > 127 = down)

For a crouch effect, you might paint the lower portion of the modifier with green values < 127 to pull pixels downward, compressing the sprite vertically.

---

#### Method 2; use_offset = false

1. Start with a base UV Identity image:

<img src=".\img\uv_identity.png" width="128" height="128">

2. Shift pixels around based on translation, rotation, scale, etc. to cause that effect to the result
3. Paint green channel to shift pixels vertically (< 127 = up, > 127 = down)

As this method may be harder to intuit, here's some examples:

<details><summary>Standing</summary>
As you can see, "standing" is the normal state, so the UV image is not edited.

<img src=".\img\uv_identity.png" width="256" height="256">
<img src=".\img\uv_identity_result.png" width="256" height="256">
</details>

<details><summary>Crouching</summary>
It may be hard to notice, but the pixels just below the character have been adjusted, and the topmost pixels are missing. Most of the UVs have been lowered.

<img src=".\img\uv_crouch.png" width="256" height="256">
<img src=".\img\uv_crouch_result.png" width="256" height="256">
</details>
</details>

<details><summary>Lying Down</summary>
This is dead simple. There are some small adjustements to the back to lie flatter, but this largely boils down to rotating the entire UV image and moving it down slightly.

<img src=".\img\uv_lying_down.png" width="256" height="256">
<img src=".\img\uv_lying_down_result.png" width="256" height="256">
</details>

---

The blue channel is ignored, and alpha 0 makes the pixel transparent.

### Sprite Size

When defining the tileset, you specify sprite_width and sprite_height, alongside sprite_offset_x and sprite_offset_y. For UVs, this defines the area they effect. Pixels outside these bounds remain unaffected.
If everything were the same resolution, this wouldn't be of note. But overlays can also be offset / different sizes.
This feature assumes consistent pixel scaling, but otherwise supports different sized sprites.
It's recommended for state-modifiers to be authored in 64x64 with an offset of (-16,-16) in order to account for overlays that extend past the avatar, such as weapons and status indicators.
In theory, you could reduce memory footprint for a state-modifier.
As an example, if one were to implement facial expressions as a new modifier group, you could limit the modifier to just the facial region, using extremely small UVs. You would likely want to use offset mode in that case to avoid issues with rounding.

### Sprite Bounds

Pixels moved outside the original sprite bounds are supported, but there is a rendering limitation: tiles are drawn row by row from top to bottom, so if a warped sprite extends downward into the row below, that portion will be overwritten when the next row's terrain is drawn. Sprites extending upward, left, or right render correctly. This is a fundamental limitation of the tile rendering order.

### Performance

State modifiers are processed at render time and cached per unique state combination. The feature can be disabled in graphics options via the "State Modifiers" toggle if performance is a concern.

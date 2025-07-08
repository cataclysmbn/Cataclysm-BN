---
title: Furniture and Terrain
---

### Furniture

```json
{
  "type": "furniture",
  "id": "f_toilet",
  "name": "toilet",
  "symbol": "&",
  "looks_like": "chair",
  "color": "white",
  "move_cost_mod": 2,
  "light_emitted": 5,
  "required_str": 18,
  "flags": ["TRANSPARENT", "BASHABLE", "FLAMMABLE_HARD"],
  "crafting_pseudo_item": "anvil",
  "examine_action": "toilet",
  "close": "f_foo_closed",
  "open": "f_foo_open",
  "lockpick_result": "f_safe_open",
  "lockpick_message": "With a click, you unlock the safe.",
  "provides_liquids": "beer",
  "bash": "TODO",
  "deconstruct": "TODO",
  "max_volume": "1000 L",
  "examine_action": "workbench",
  "workbench": { "multiplier": 1.1, "mass": 10000, "volume": "50L" },
  "boltcut": {
    "result": "f_safe_open",
    "duration": "1 seconds",
    "message": "The safe opens.",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "count": 3 }]
  },
  "hacksaw": {
    "result": "f_safe_open",
    "duration": "12 seconds",
    "message": "The safe is hacksawed open!",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "count": 13 }]
  }
}
```

#### `type`

Fixed string, must be `furniture` to identify the JSON object as such.

`"id", "name", "symbol", "looks_like", "color", "bgcolor", "max_volume", "open", "close", "bash", "deconstruct", "examine_action", "flgs`

Same as for terrain, see below in the chapter "Common to furniture and terrain".

#### `move_cost_mod`

Movement cost modifier (`-10` = impassable, `0` = no change). This is added to the movecost of the
underlying terrain.

#### `lockpick_result`

(Optional) When the furniture is successfully lockpicked, this is the furniture it will turn into.

#### `lockpick_message`

(Optional) When the furniture is successfully lockpicked, this is the message that will be printed
to the player. When it is missing, a generic `"The lock opens…"` message will be printed instead.

#### `oxytorch`

(Optional) Data for using with an oxytorch.

```cpp
oxytorch: {
    "result": "furniture_id", // (optional) furniture it will become when done, defaults to f_null
    "duration": "1 seconds", // ( optional ) time required for oxytorching, default is 1 second
    "message": "You quickly cut the metal", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `light_emitted`

How much light the furniture produces. 10 will light the tile it's on brightly, 15 will light that
tile and the tiles around it brightly, as well as slightly lighting the tiles two tiles away from
the source. For examples: An overhead light is 120, a utility light, 240, and a console, 10.

#### `boltcut`

(Optional) Data for using with an bolt cutter.

```cpp
"boltcut": {
    "result": "furniture_id", // (optional) furniture it will become when done, defaults to f_null
    "duration": "1 seconds", // ( optional ) time required for bolt cutting, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "sound": "Gachunk!", // ( optional ) description of the sound when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `hacksaw`

(Optional) Data for using with an hacksaw.

```cpp
"hacksaw": {
    "result": "furniture_id", // (optional) furniture it will become when done, defaults to f_null
    "duration": "1 seconds", // ( optional ) time required for hacksawing, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `required_str`

Strength required to move the furniture around. Negative values indicate an unmovable furniture.

#### `crafting_pseudo_item`

(Optional) Id of an item (tool) that will be available for crafting when this furniture is range
(the furniture acts as an item of that type). Can be made into an array. Example:
`"crafting_pseudo_item": [ "fake_gridwelder", "fake_gridsolderingiron" ],`

#### `workbench`

(Optional) Can craft here. Must specify a speed multiplier, allowed mass, and allowed volume.
Mass/volume over these limits incur a speed penalty. Must be paired with a `"workbench"`
`examine_action` to function.

#### `plant_data`

(Optional) This is a plant. Must specify a plant transform, and a base depending on context. You can
also add a harvest or growth multiplier if it has the `GROWTH_HARVEST` flag.

#### `surgery_skill_multiplier`

(Optional) Surgery skill multiplier (float) applied by this furniture to survivor standing next to
it for the purpose of surgery.

#### `provides_liquids`

(Optional) Dispenses infinite amounts of specified liquid item when interacted. Must be used with
`"examine_action": "liquid_source"` to work.

### Terrain

```json
{
  "type": "terrain",
  "id": "t_spiked_pit",
  "name": "spiked pit",
  "symbol": "0",
  "looks_like": "pit",
  "color": "ltred",
  "move_cost": 10,
  "light_emitted": 10,
  "trap": "spike_pit",
  "fill_result": "t_dirt", // Terrain result from filling this terrain in
  "fill_minutes": 10, // Number of minutes to fill terrain in
  "max_volume": "1000 L",
  "flags": ["TRANSPARENT"],
  "digging_results": {
    "digging_min": 1,
    "result_ter": "t_pit_shallow",
    "num_minutes": 60,
    "items": "digging_sand_50L"
  },
  "connects_to": "WALL",
  "close": "t_foo_closed",
  "open": "t_foo_open",
  "lockpick_result": "t_door_unlocked",
  "lockpick_message": "With a click, you unlock the door.",
  "nail_pull_result": "t_fence_post", // terrain ID to transform into when you use a hammer to pull the nails out
  "nail_pull_items": [8, 5], // nails and planks (respectively) to drop as a result of pulling nails with a hammer. Defaults to 0,0 and thus technically optional even if you add a nail_pull_result.
  "bash": "TODO",
  "deconstruct": "TODO",
  "harvestable": "blueberries",
  "transforms_into": "t_tree_harvested",
  "harvest_season": "WINTER",
  "roof": "t_roof",
  "examine_action": "pit",
  "boltcut": {
    "result": "t_door_unlocked",
    "duration": "1 seconds",
    "message": "The door opens.",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "2x4": 3 }]
  },
  "hacksaw": {
    "result": "t_door_unlocked",
    "duration": "12 seconds",
    "message": "The door is hacksawed open!",
    "sound": "Gachunk!",
    "byproducts": [{ "item": "scrap", "2x4": 13 }]
  }
}
```

#### `type`

Fixed string, must be "terrain" to identify the JSON object as such.

`"id", "name", "symbol", "looks_like", "color", "bgcolor", "max_volume", "open", "close", "bash", "deconstruct", "examine_action", "flgs`

Same as for furniture, see below in the chapter "Common to furniture and terrain".

#### `move_cost`

Move cost to move through. A value of 0 means it's impassable (e.g. wall). You should not use
negative values. The positive value is multiple of 50 move points, e.g. value 2 means the player
uses `2 * 50 = 100` move points when moving across the terrain.

#### `light_emitted`

How much light the terrain emits. 10 will light the tile it's on brightly, 15 will light that tile
and the tiles around it brightly, as well as slightly lighting the tiles two tiles away from the
source. For examples: An overhead light is 120, a utility light, 240, and a console, 10.

#### `digging_results`

(Optional) Fields relating to digging terrain using a shovel

- `"digging_min"` - minimum digging quality required for digging this terrain
- `"result_ter"` - resulting terrain ID from digging
- `"num_minutes"` - number of minutes spent digging
- `"items"` - (Optional) either an array of items, or an existing itemgroup ID. Defaults to `digging_soil_loam_200L`

#### `lockpick_result`

(Optional) When the terrain is successfully lockpicked, this is the terrain it will turn into.

#### `lockpick_message`

(Optional) When the terrain is successfully lockpicked, this is the message that will be printed to
the player. When it is missing, a generic `"The lock opens…"` message will be printed instead.

#### `oxytorch`

(Optional) Data for using with an oxytorch.

```cpp
oxytorch: {
    "result": "terrain_id", // terrain it will become when done
    "duration": "1 seconds", // ( optional ) time required for oxytorching, default is 1 second
    "message": "You quickly cut the bars", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `trap`

(Optional) Id of the build-in trap of that terrain.

For example the terrain `t_pit` has the built-in trap `tr_pit`. Every tile in the game that has the
terrain `t_pit` also has, therefore, an implicit trap `tr_pit` on it. The two are inseparable (the
player can not deactivate the built-in trap, and changing the terrain will also deactivate the
built-in trap).

A built-in trap prevents adding any other trap explicitly (by the player and through mapgen).

#### `harvestable`

(Optional) If defined, the terrain is harvestable. This entry defines the item type of the harvested
fruits (or similar). To make this work, you also have to set one of the `harvest_*` `examine_action`
functions.

#### `boltcut`

(Optional) Data for using with an bolt cutter.

```cpp
"boltcut": {
    "result": "terrain_id", // terrain it will become when done
    "duration": "1 seconds", // ( optional ) time required for bolt cutting, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "sound": "Gachunk!", // ( optional ) description of the sound when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `hacksaw`

(Optional) Data for using with an hacksaw.

```cpp
"hacksaw": {
    "result": "terrain_id", // terrain it will become when done
    "duration": "1 seconds", // ( optional ) time required for hacksawing, default is 1 second
    "message": "You finish cutting the metal.", // ( optional ) message that will be displayed when finished
    "byproducts": [ // ( optional ) list of items that will be spawned when finished
        {
            "item": "item_id",
            "count": 100 // exact amount
        },
        {
            "item": "item_id",
            "count": [ 10, 100 ] // random number in range ( inclusive )
        }
    ]
}
```

#### `transforms_into`

(Optional) Used for various transformation of the terrain. If defined, it must be a valid terrain
id. Used for example:

- When harvesting fruits (to change into the harvested form of the terrain).
- In combination with the `HARVESTED` flag and `harvest_season` to change the harvested terrain back
  into a terrain with fruits.

#### `harvest_season`

(Optional) On of "SUMMER", "AUTUMN", "WINTER", "SPRING", used in combination with the "HARVESTED"
flag to revert the terrain back into a terrain that can be harvested.

#### `roof`

(Optional) The terrain of the terrain on top of this (the roof).

### Common To Furniture And Terrain

Some values can/must be set for terrain and furniture. They have the same meaning in each case.

#### `id`

Id of the object, this should be unique among all object of that type (all terrain or all furniture
types). By convention (but technically not needed), the id should have the "f_" prefix for furniture
and the "t_" prefix for terrain. This is not translated. It must not be changed later as that would
break save compatibility.

#### `name`

Displayed name of the object. This will be translated.

#### `flags`

(Optional) Various additional flags, see "doc/JSON_FLAGS.md".

#### `connects_to`

(Optional) The group of terrains to which this terrain connects. This affects tile rotation and
connections, and the ASCII symbol drawn by terrain with the flag "AUTO_WALL_SYMBOL".

Current values:

- `CHAINFENCE`
- `RAILING`
- `WALL`
- `WATER`
- `WOODFENCE`
- `GUTTER`

Example: `-` , `|` , `X` and `Y` are terrain which share the same `connects_to` value. `O` does not
have it. `X` and `Y` also have the `AUTO_WALL_SYMBOL` flag. `X` will be drawn as a T-intersection
(connected to west, south and east), `Y` will be drawn as a horizontal line (going from west to
east, no connection to south).

```
-X-    -Y-
 |      O
```

#### `symbol`

ASCII symbol of the object as it appears in the game. The symbol string must be exactly one
character long. This can also be an array of 4 strings, which define the symbol during the different
seasons. The first entry defines the symbol during spring. If it's not an array, the same symbol is
used all year round.

#### `comfort`

How comfortable this terrain/furniture is. Impact ability to fall asleep on it. uncomfortable =
-999, neutral = 0, slightly_comfortable = 3, comfortable = 5, very_comfortable = 10

#### `floor_bedding_warmth`

Bonus warmth offered by this terrain/furniture when used to sleep.

#### `bonus_fire_warmth_feet`

Increase warmth received on feet from nearby fire (default = 300)

#### `looks_like`

id of a similar item that this item looks like. The tileset loader will try to load the tile for
that item if this item doesn't have a tile. looks_like entries are implicitly chained, so if
'throne' has looks_like 'big_chair' and 'big_chair' has looks_like 'chair', a throne will be
displayed using the chair tile if tiles for throne and big_chair do not exist. If a tileset can't
find a tile for any item in the looks_like chain, it will default to the ascii symbol.

#### `color` or `bgcolor`

Color of the object as it appears in the game. "color" defines the foreground color (no background
color), "bgcolor" defines a solid background color. As with the "symbol" value, this can be an array
with 4 entries, each entry being the color during the different seasons.

> **NOTE**: You must use ONLY ONE of "color" or "bgcolor"

#### `max_volume`

(Optional) Maximal volume that can be used to store items here. Volume in ml and L can be used - "50
ml" or "2 L"

#### `examine_action`

(Optional) The json function that is called when the object is examined. See "src/iexamine.h".

#### `close" And "open`

(Optional) The value should be a terrain id (inside a terrain entry) or a furniture id (in a
furniture entry). If either is defined, the player can open / close the object. Opening / closing
will change the object at the affected tile to the given one. For example one could have object
"safe_c", which "open"s to "safe_o" and "safe_o" in turn "close"s to "safe_c". Here "safe_c" and
"safe_o" are two different terrain (or furniture) types that have different properties.

#### `bash`

(Optional) Defines whether the object can be bashed and if so, what happens. See "map_bash_info".

#### `deconstruct`

(Optional) Defines whether the object can be deconstructed and if so, what the results shall be. See
"map_deconstruct_info".

#### `pry`

(Optional) Defines whether the object can be pried open and if so, what happens. See
"prying_result".

#### `map_bash_info`

Defines the various things that happen when the player or something else bashes terrain or
furniture.

```json
{
  "str_min": 80,
  "str_max": 180,
  "str_min_blocked": 15,
  "str_max_blocked": 100,
  "str_min_supported": 15,
  "str_max_supported": 100,
  "sound": "crunch!",
  "sound_vol": 2,
  "sound_fail": "whack!",
  "sound_fail_vol": 2,
  "ter_set": "t_dirt",
  "furn_set": "f_rubble",
  "explosive": 1,
  "collapse_radius": 2,
  "destroy_only": true,
  "bash_below": true,
  "tent_centers": ["f_groundsheet", "f_fema_groundsheet", "f_skin_groundsheet"],
  "items": "bashed_item_result_group"
}
```

#### `str_min`, `str_max`, `str_min_blocked`, `str_max_blocked`, `str_min_supported`, `str_max_supported`

TODO

#### `sound`, `sound_fail`, `sound_vol`, `sound_fail_vol`

(Optional) Sound and volume of the sound that appears upon destroying the bashed object or upon
unsuccessfully bashing it (failing). The sound strings are translated (and displayed to the player).

#### `furn_set`, `ter_set`

The terrain / furniture that will be set when the original is destroyed. This is mandatory for bash
entries in terrain, but optional for entries in furniture (it defaults to no furniture).

#### `explosive`

(Optional) If greater than 0, destroying the object causes an explosion with this strength (see
`game::explosion`).

#### `destroy_only`

TODO

#### `bash_below`

TODO

#### `tent_centers`, `collapse_radius`

(Optional) For furniture that is part of tents, this defines the id of the center part, which will
be destroyed as well when other parts of the tent get bashed. The center is searched for in the
given "collapse_radius" radius, it should match the size of the tent.

#### `items`

(Optional) An item group (inline) or an id of an item group, see doc/ITEM_SPAWN.md. The default
subtype is "collection". Upon successful bashing, items from that group will be spawned.

#### `map_deconstruct_info`

```json
{
  "furn_set": "f_safe",
  "ter_set": "t_dirt",
  "items": "deconstructed_item_result_group"
}
```

#### `furn_set`, `ter_set`

The terrain / furniture that will be set after the original has been deconstructed. "furn_set" is
optional (it defaults to no furniture), "ter_set" is only used upon "deconstruct" entries in terrain
and is mandatory there.

#### `items`

(Optional) An item group (inline) or an id of an item group, see doc/ITEM_SPAWN.md. The default
subtype is "collection". Upon deconstruction the object, items from that group will be spawned.

#### `prying_result`

```json
{
  "success_message": "You pry open the door.",
  "fail_message": "You pry, but cannot pry open the door.",
  "break_message": "You damage the door!",
  "pry_quality": 2,
  "pry_bonus_mult": 3,
  "noise": 12,
  "break_noise": 10,
  "sound": "crunch!",
  "break_sound": "crack!",
  "breakable": true,
  "difficulty": 8,
  "new_ter_type": "t_door_o",
  "new_furn_type": "f_crate_o",
  "break_ter_type": "t_door_b",
  "break_furn_type": "f_null",
  "break_items": [
    { "item": "2x4", "prob": 25 },
    { "item": "wood_panel", "prob": 10 },
    { "item": "splinter", "count": [1, 2] },
    { "item": "nail", "charges": [0, 2] }
  ]
}
```

#### `new_ter_type`, `new_furn_type`

The terrain / furniture that will be set after the original has been pried open. "furn_set" is
optional (it defaults to no furniture), "ter_set" is only used upon "pry" entries in terrain and is
mandatory there.

#### `success_message`, `fail_message`, `break_message`

Messages displayed on successfully prying open the terrain / furniture, on failure, or should the
terrain / furniture break into something else from a failed pry attempt. `break_message` is only
required if `breakable` is set to true and `break_ter_type` is defined.

#### `pry_quality`, `pry_bonus_mult`, `difficulty`

This determines the minimum prying quality needed to attempt to pry open the terrain / furniture,
and the chance of successfully prying it open. From iuse.cpp:

```cpp
int diff = pry->difficulty;
diff -= ( ( pry_level - pry->pry_quality ) * pry->pry_bonus_mult );
```

```cpp
if( dice( 4, diff ) < dice( 4, p->str_cur ) ) {
    p->add_msg_if_player( m_good, pry->success_message );
```

`difficulty` is compared to the character's current strength during the prying attempt. If the
available prying quality of your tool is above the required `pry_quality`, effective difficulty is
reduced at a rate determined by `pry_bonus_mult`. `pry_bonus_mult` is optional, and defaults to 1
(meaning for every level of prying quality your tool has beyond the minimum, `diff` will be reduced
by 1).

#### `noise`, `break_noise`, 'sound', 'break_sound'

If `noise` is specified, successfully prying the terrain / furniture open will play `sound` at the
specified volume. If `breakable` is true and `break_noise` is specified, breaking the terrain /
furniture on a failed prying attempt will play `break_noise` at the specified volume.

If `noise` or `break_noise` are not specified then prying or breaking the terrain / furniture in
question will simply be silent and make no sound, making them optional. `sound` and `break_sound`
are also optional, with default messages of "crunch!" and "crack!" respectively.

#### `breakable`, `break_ter_type`, `break_furn_type`

If `breakable` is set to true, then failed pry attempts have a chance of breaking the terrain /
furniture instead. For terrain, `break_ter_type` is mandatory if `breakable` is set to true,
`break_furn_type` is optional and defaults to null.

#### `break_items`

(Optional) An item group (inline) or an id of an item group, see doc/ITEM_SPAWN.md. The default
subtype is "collection". If `breakable` is set to true, breaking the object from a failed pry
attempt will spawn items from that group.

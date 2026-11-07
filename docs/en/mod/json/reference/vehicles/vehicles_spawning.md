# Spawning vehicles

Vehicle prototypes are used to spawn stock vehicles. After a vehicle has been spawned, it is saved
in a different format.

Vehicle prototypes do not currently accept copy-from

## vehicle prototypes

```json
"type": "vehicle",
"id": "sample_vehicle",                    // Unique ID. Must be one continuous word,
                                           // use underscores if necessary.
"name": "Sample Vehicle",                  // In-game name displayed
"blueprint": [                             // Preview of vehicle; Can be used as documentation
    "o#o",                                 // Or with palette to be a way to define parts
    "o#o"                                  // Partial definition with palette is also fine
],
"blueprint_origin": { "x": 10, "y": 3 },   // Used by palettes to locate the offset
"palette": {                               // Palette used by blueprint to set parts on a tile
  "O": [ "airship_balloon_external" ]      // Can only be an array not one string
  "J": [ { "part": "tank", "fuel": "gasoline" }, "battery_car" ], // All part types supported
},
"color_palette": "car_standard",           // Vehicle Color Palette used by this car
"parts": [                                 // Parts list
    { "x": 0, "y": 0, "part": "frame" },   // Part definition, positive x direction is up,
    { "x": 0, "y": 0, "part": "seat" },    // positive y is to the right
    { "x": 0, "y": 0, "part": "controls"}, // See vehicle_parts.json for part ids
    { "x": 0, "y": 1, "parts: [ "frame", "seat" ] }, // Arrays of parts on the same space
    { "x": 0, "y": 1, "parts: [ { "part": "tank", "fuel": "gasoline" }, "battery_car" },
    { "x": 0, "y": 1, "part": "stereo" },  // parts arrays and part may be mixed on the same space
    { "x": 1, "y": 0, "parts: [ "frame, "wheel" ] },
    { "x": 1, "y": 1, "parts: [ "frame, "wheel" ] },
    { "x": -1, "y": 0, "parts: [ "frame, "wheel" ] },
    { "x": -1, "y": 1, "parts: [ "frame, "wheel" ] }
],
"items": [                                 // Item spawn list
    { "x": 0, "y": 0, "items": "helmet_army" },   // individual item
    { "x": 0, "y": 0, "item_groups": "army_uniform" }, // item or items from an item_group
    { "x": 0, "y": 1, "items": [ "matchbook", "two_by_four" ] }, // all items in the list spawn
    { "x": 0, "y": 0, "item_groups": [ "army_uniform", "rare_guns" ] } all item_groups are processed
]
```

.* Important! *. Vehicle parts must be defined in the same order you would install them in the game
(ie, frames and mount points first). You also cannot break the normal rules of installation (you
can't stack non-stackable part flags).

### Parts list

The part list contains an arbitary number of lines. Each line is of the form:
`{ "x": X, "y": Y, "part": PARTID, ... }` or `{ "x": X, "y": Y, "parts": [ PARTID1, ... ] }`.
Both forms accept an optional integer `z`, defaulting to zero. These are vehicle-local coordinates,
not world coordinates. Different decks can contain separate assemblies at the same X and Y.

In the first form, the line defines a single part at location X,Y of vehicle part type PARTID. It
can have the optional "ammo", "ammo_types", "ammo_qty", or "fuel" keys with an appropriate value
following.

In the second form, the line defines several parts at location X, Y. Each part is either defined by
its PARTID string, or can be an object of the form { "part": PARTID, ... } with any of the optional
keys "ammo", "ammo_types", "ammo_qty", or "fuel" as above.

Several different lines can have the same X, Y, Z co-ordinates and each one adds additional parts to
that location. Parts must be added in the correct order ie: A wheel hub must be added prior to the
wheel, but after the frame.

### Blueprints

With an inline `palette`, `blueprint` provides an ASCII layout for installing parts. Existing flat
blueprints remain valid without modification:

```json
{
  "blueprint_origin": { "x": 1, "y": 0 },
  "palette": {
    "a": ["frame_vertical", "aisle_horizontal"],
    "s": ["frame_vertical", "seat"]
  },
  "blueprint": ["aa", "ss"]
}
```

This fragment expands at local z=0. A character at column `c` and row `r` has local coordinates
`x = c - blueprint_origin.x`, `y = r - blueprint_origin.y`. Missing origin coordinates default to
zero. Row lengths may differ; spaces and other characters absent from the palette generate no parts.
Each palette value is an ordered array of part IDs or part objects with the same fuel/ammunition
fields supported by explicit parts. The inline palette is unrelated to the separate
`color_palette` paint setting.

For multiple decks, use an array of layer objects instead of an array of row strings:

```json
{
  "id": "two_deck_blueprint_example",
  "type": "vehicle",
  "name": "Two-deck blueprint example",
  "blueprint_origin": { "x": 1, "y": 0 },
  "palette": {
    "a": ["frame_vertical", "aisle_horizontal"],
    "s": ["frame_vertical", "seat"],
    "<": ["frame_vertical", "vehicle_stairs"],
    "A": ["frame_vertical", "aisle_horizontal", "roof"],
    "S": ["frame_vertical", "seat", "roof"],
    ">": ["frame_vertical", "vehicle_stair_landing", "roof"]
  },
  "blueprint": [
    { "z": 0, "rows": ["a<", "ss"] },
    { "z": 1, "rows": ["A>", "SS"] }
  ],
  "parts": [{ "x": -1, "y": 0, "z": 1, "part": "solar_panel" }]
}
```

Each layer requires `z` and a `rows` array. All layers share the inline palette and XY origin;
row/column numbering restarts for each layer. Negative local heights are supported. Layer order is
not required to be sorted by height; multiple layers may add parts at the same z.
Do not mix layer objects and flat row strings. Empty `rows` arrays are allowed and generate no parts.

Layer heights use the ordinary JSON integer reader; a spawned vehicle must fit the world's actual
height limits. The origin affects only XY: each layer's `z` is already its vehicle-local height.

Blueprint parts are added in layer, row, column, and palette-entry order. Explicit `parts` entries
are then added normally, so partial palettes and explicit additions can be combined. Explicit parts
without `z` still use zero; they do not inherit the last blueprint layer's height. Prototypes are
force-assembled in this order. During gameplay, a roof or `on_roof` component cannot be installed
below an occupied local layer in the same XY column, nor can parts be added above such a component.
A roof at a passage's lower endpoint blocks traversal.

Without an inline `palette`, `blueprint` remains display/documentation-only and generates no parts.
Existing flat, nested-array, and empty display-only blueprints keep their old behavior. There is no
required migration of existing vehicle definitions.

Repeated definitions retain the existing override rules for a prototype that already has parts:
a definition with `name` replaces it; a definition without `name` appends parts. Each definition
supplies its own palette and origin. Appending another definition may add parts on an existing deck.

### Items list

The items list contains an arbitrary number of lines. Each line is of the form: { "x": X, "y": Y,
TYPE: DATA }, and describes the items that may spawn at that location. TYPE and DATA may be one of:

```json
"items": "itemid"                              // single item of that type
"items": [ "itemid1", "itemid2", ... ]         // all the items in the array
"item_groups": "groupid"                       // one or more items in the group, depending on
                                               // whether the group is a collection or distribution
"item_groups": [ "groupid1", "groupid2" ... ]  // one or more items for each group
```

the optional keyword "chance" provides an X in 100 chance that a particular item definition will
spawn.

Multiple lines of items may share the same X and Y values.

Item-spawn entries also accept optional vehicle-local `z`, defaulting to zero. Set it explicitly to
spawn cargo on another deck; blueprint layers do not change the default for items.
The exact X, Y, Z mount must contain a `CARGO` part (including a seat with storage). For example,
`{ "x": -2, "y": 0, "z": 1, "chance": 35, "item_groups": [ "bus_common" ] }` places loot on the
upper deck independently of a lower-deck entry at the same X and Y.

### Vehicle Groups

```json
"id":"city_parked",            // Unique ID. Must be one continuous word, use underscores if necessary
"vehicles":[                 // List of potential vehicle ID's. Chance of a vehicle spawning is X/T, where
  ["suv", 600],           //    X is the value linked to the specific vehicle and T is the total of all
  ["pickup", 400],          //    vehicle values in a group
  ["car", 4700],
  ["road_roller", 300]
]
```

### Vehicle Placement

```json
"id":"road_straight_wrecks",  // Unique ID. Must be one continuous word, use underscores if necessary
"locations":[ {               // List of potential vehicle locations. When this placement is used, one of those locations will be chosen at random.
  "x" : [0,19],               // The x placement. Can be a single value or a range of possibilities.
  "y" : 8,                    // The y placement. Can be a single value or a range of possibilities.
  "facing" : [90,270]         // The facing of the vehicle. Can be a single value or an array of possible values.
} ]
```

### Vehicle Spawn

```json
"id":"default_city",            // Unique ID. Must be one continuous word, use underscores if necessary
"spawn_types":[ {       // List of spawntypes. When this vehicle_spawn is applied, it will choose from one of the spawntypes randomly, based on the weight.
  "description" : "Clear section of road",           //    A description of this spawntype
  "weight" : 33,          //    The chance of this spawn type being used.
  "vehicle_function" : "jack-knifed_semi", // This is only needed if the spawntype uses a built-in json function.
  "vehicle_json" : {      // This is only needed for a json-specified spawntype.
  "vehicle" : "car",      // The vehicle or vehicle_group to spawn.
  "placement" : "%t_parked",  // The vehicle_placement to use when spawning the vehicle. This is not needed if the x, y, and facing are specified.
  "x" : [0,19],     // The x placement. Can be a single value or a range of possibilities. Not needed if placement is specified.
  "y" : 8,   // The y placement. Can be a single value or a range of possibilities. Not needed if placement is specified.
  "facing" : [90,270], // The facing of the vehicle. Can be a single value or an array of possible values. Not needed if placement is specified.
  "number" : 1, // The number of vehicles to spawn.
  "fuel" : -1, // The fuel of the new vehicles.
  "status" : 1  // The status of the new vehicles.
} } ]
```

## Vehicle Color Palette

```json
"type": "vehicle_color_palette",
"id": "car_standard",                                      // Unique ID
"palette": [
  {
    "fuzzy_ids": [ "board", "windshield", "door", "roo" ], // List of fuzzily matched ids for tinting the below selected color
    "colors": [
      { "color": "White aluminium", "weight": 8 },         // Color & Weight, This uses named color
      { "color": "#622625", "weight": 8 },                 // It can also be a hex code
      { "color": "Grey", "weight": 8 },                    // Or a fuzzy matched named color
    ]
  }
]
```

## Vehicle Blacklist

```jsonc
{
  "type": "vehicle_blacklist", // Mandatory type
  "vehicles": [ // List of Vehicle Prototype Ids to blacklist
    "car", // They can still be debug spawned and such
    "4x4_car", // They are just prevented from spawning via vehicle groups
    "beetle",
  ],
}
```

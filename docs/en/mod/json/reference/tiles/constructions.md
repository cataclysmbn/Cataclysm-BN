---
title: Construction Info
---

> [!NOTE]
>
> This article was recently split off from `JSON INFO` and likely could use additional work

### Constructions

```json
"id": "constr_pit_spiked",                                          // Identifier of the construction
"group": "spike_pit",                                               // Construction group, used to provide description and group related constructions in UI (e.g. different stages of some construction).
"category": "DIG",                                                  // Construction category
"required_skills": [ [ "survival", 1 ] ],                           // Skill levels required to undertake construction
"time": "30 m",                                                     // Time required to complete construction. Integers will be read as minutes or a time string can be used.
"components": [ [ [ "spear_wood", 4 ], [ "pointy_stick", 4 ] ] ],   // Items used in construction
"using": [ [ "welding_standard", 5 ] ],                             // (Optional) External requirements to use
"pre_note": "I am a dwarf and I'm digging a hole!",                 // (Optional) Annotation
"pre_flags": [ "FLAT" ],                                // (Optional) Flags the terrain must have to be built on
"needs_diggable": true,                                          // (Optional) Whether the pre-terrain must be diggable
"pre_terrain": "t_pit",                                             // (Optional) Required terrain to build on
"pre_furniture": "f_sandbag_half",                                  // (Optional) Required furniture to build on
"pre_special": "check_down_OK",                                     // (Optional) Hardcoded tile validity checks, refer to construction.cpp 
"post_terrain": "t_pit_spiked",                                     // (Optional) Terrain type after construction is complete
"post_furniture": "f_sandbag_wall",                                 // (Optional) Furniture type after construction is complete
"post_special": "done_dig_stair",                                   // (Optional) Hardcoded completion function, refer to construction.cpp
"post_flags": [ "keep_items" ],                                     // (Optional) Additional hardcoded effects, refer to construction.cpp. The only one available as of September 2022 is `keep_items`
"byproducts": [ { "item": "pebble", "charges": [ 3, 6 ] } ],        // (Optional) construction byproducts
"vehicle_start": false,                                             // (Optional, default false) Whether it will create a vehicle (for hardcode purposes)
"on_display": false,                                                // (Optional, default true) Whether it will show in player's UI
"dark_craftable": true,                                             // (Optional, default false) Whether it can be constructed in dark
```

### Construction groups

```json
"id": "build_wooden_door",            // Group identifier
"name": "Build Wooden Door",          // Description string displayed in the construction menu
```

### Construction sequences

Constuction sequences are needed for automatic calculation of basecamp blueprint requirements. Each
construction sequence describes steps needed to construct specified terrain or furniture on an empty
dirt tile. At any moment, only 1 construction sequence may be defined for any terrain or furniture.
For convenience, each non-blacklisted construction recipe that starts from an empty dirt tile
automatically generates a one-element-long construction sequence, but only as long as there are no
other such construction recipes that would produce the same sequence and there isn't an explicitly
defined sequence with same results.

```json
"id": "f_workbench",                // Sequence identifier
"blacklisted": false,               // (Optional) Whether this sequence is blacklisted
"post_furniture": "f_workbench",    // (Optional) Identifier of resulting furniture
"post_terrain": "t_rootcellar",     // (Optional) Identifier of resulting terrain
"elems": [                          // Array of construction identifiers.
  "constr_pit",                     //   First construction must start from an empty dirt tile,
  "constr_rootcellar"               //   and last construction must result in either post_furniture
]                                   //   or post_terrain, whichever one has been specified.
                                    //   Leave it empty to exclude the terrain/furniture from blueprint autocalc.
```

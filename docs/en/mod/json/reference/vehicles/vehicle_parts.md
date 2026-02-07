# Vehicles

> [!NOTE]
>
> This page is currently under-construction and was recently split off from `JSON INFO`

### Vehicle Parts

Vehicle components when installed on a vehicle.

```json
"id": "wheel",                // Unique identifier
"name": "wheel",              // Displayed name
"symbol": "0",                // ASCII character displayed when part is working
"looks_like": "small_wheel",  // hint to tilesets if this part has no tile, use the looks_like tile
"color": "dark_gray",         // Color used when part is working
"broken_symbol": "x",         // ASCII character displayed when part is broken
"broken_color": "light_gray", // Color used when part is broken
"damage_modifier": 50,        // (Optional, default = 100) Dealt damage multiplier when this part hits something, as a percentage. Higher = more damage to creature struck
"durability": 200,            // How much damage the part can take before breaking
"description": "A wheel."     // A description of this vehicle part when installing it
"size": 2000                  // If vehicle part has flag "FLUIDTANK" then capacity in mLs, else divide by 4 for volume on space
"wheel_width": 9,             /* (Optional, default = 0)
                               * SPECIAL: A part may have at most ONE of the following fields:
                               *    wheel_width = base wheel width in inches
                               *    size        = trunk/box storage volume capacity
                               *    power       = base engine power in watts
                               *    bonus       = bonus granted; muffler = noise reduction%, seatbelt = bonus to not being thrown from vehicle
                               *    par1        = generic value used for unique bonuses, like the headlight's light intensity */
"wheel_type":                 // (Optional: standard, off-road)
"contact_area":               // (Optional) Affects vehicle ground pressure
"cargo_weight_modifier": 33,  // (Optional, default = 100) Modifies cargo weight by set percentage
"weight_modifier": 33,        // (Optional, default = 100) Modifies base part weight by set percentage
"fuel_type": "NULL",          // (Optional, default = "NULL") Type of fuel/ammo the part consumes, as an item id

"item": "wheel",              // The item used to install this part, and the item obtained when removing this part
"difficulty": 4,              // Your mechanics skill must be at least this level to install this part
"breaks_into" : [             // When the vehicle part is destroyed, items from this item group (see ITEM_SPAWN.md) will be spawned around the part on the ground.
  {"item": "scrap", "count": [0,5]} // instead of an array, this can be an inline item group,
],
"breaks_into" : "some_item_group", // or just the id of an item group.
"flags": [                    // Flags associated with the part
     "EXTERNAL", "MOUNT_OVER", "WHEEL", "MOUNT_POINT", "VARIABLE_SIZE"
],
"damage_reduction" : {        // Flat reduction of damage, as described below. If not specified, set to zero
    "all" : 10,
    "physical" : 5
},
                              // The following optional fields are specific to ENGINEs.
"m2c": 50,                    // Mandatory field for parts with the ENGINE flag, indicates ratio of cruise power to maximum power
"backfire_threshold": 0.5,    // Optional field, defaults to 0. Indicates maximum ratio of damaged HP to max HP to trigger backfires
"backfire_freq": 20,          // Optional field unless backfire threshold > 0, then mandatory, defaults to 0. One in X chance of a backfire.
"noise_factor": 15,           // Optional field, defaults to 0. Multiple engine power by this number to declare noise.
"damaged_power_factor": 0.5,  // Optional field, defaults to 0. If more than 0, power when damaged is scaled to power * ( damaged_power_factor + ( 1 - damaged_power_factor ) * ( damaged HP / max HP )
"muscle_power_factor": 0,     // Optional field, defaults to 0. If more than 0, each point of the survivor's Strength over 8 adds this much power to the engine, and each point less than 8 removes this much power.
"exclusions": [ "souls" ]     // Optional field, defaults to empty. A list of words. A new engine can't be installed on the vehicle if any engine on the vehicle shares a word from exclusions.
"fuel_options": [ "soul", "black_soul" ] // Optional field, defaults to fuel_type.  A list of words. An engine can be fueled by any fuel type in its fuel_options.  If provided, it overrides fuel_type and should include the fuel in fuel_type.
"comfort": 3,                 // Optional field, defaults to 0. How comfortable this terrain/furniture is. Impact ability to fall asleep on it. (uncomfortable = -999, neutral = 0, slightly_comfortable = 3, comfortable = 5, very_comfortable = 10)
"floor_bedding_warmth": 300,  // Optional field, defaults to 0. Bonus warmth offered by this terrain/furniture when used to sleep.
"bonus_fire_warmth_feet": 200,// Optional field, defaults to 300. Increase warmth received on feet from nearby fire.
"height": 5,                  // Optional field, height of balloons in meters ( aka multiplie of their lift )
"lift_coff": 0.5,             // Optional field, multiplier of wing effectiveness
"propeller_diameter": 0.5,    // Optional field, diameter of propeller
"length": 3,                  // Optional field, z-level length of ladder
```

### Integrated Tools

```json
"integrated_tools": [ "foo" ],
```

An option array of tools that this vehiclepart will provide for crafting purposes, compare and contrast `crafting_pseudo_item` for furniture. Requires the vehiclepart to have the `CRAFTING` flag to function.

Most legacy crafting vehiclepart flags have been removed and should be replaced with equivalent tools. The `WATER_PURIFIER`, `FAUCET` and the `WATER_FAUCET` flags, which provide specific functions on examine, have been retained.

```json
"integrated_tools": [ "pot", "pan", "hotplate" ],  // Replaces the `KITCHEN` flag
"integrated_tools": [ "dehydrator", "vac_sealer", "food_processor", "press" ],  // Replaces the `CRAFTRIG` flag
"integrated_tools": [ "chemistry_set", "electrolysis_kit" ],  // Replaces the `CHEMLAB` flag
"integrated_tools": [ "forge" ],  // Replaces the `FORGE` flag
"integrated_tools": [ "fake_adv_butchery" ],  // Needed with the `BUTCHER_EQ` flag
"integrated_tools": [ "kiln" ],  // Replaces the `KILN` flag
"integrated_tools": [ "soldering_iron", "welder" ],  // Replaces the tools
"integrated_tools": [ "water_purifier" ],  // Replaces the tools, but not the ability to purify water in vehicle tanks, of the `WATER_PURIFIER` flag
```

### Part Resistance

```json
"all" : 0.0f,        // Initial value of all resistances, overridden by more specific types
"physical" : 10,     // Initial value for bash, cut and stab
"non_physical" : 10, // Initial value for acid, heat, cold, electricity and biological
"biological" : 0.2f, // Resistances to specific types. Those override the general ones.
"bash" : 3,
"cut" : 3,
"acid" : 3,
"stab" : 3,
"heat" : 3,
"cold" : 3,
"electric" : 3
```

### Vehicles

See also `vehicles_spawning.md`

```json
"id": "shopping_cart",                     // Internally-used name.
"name": "Shopping Cart",                   // Display name, subject to i18n.
"blueprint": "#",                          // Preview of vehicle - ignored by the code, so use only as documentation
"parts": [                                 // Parts list
    {"x": 0, "y": 0, "part": "box"},       // Part definition, positive x direction is to the left, positive y is to the right
    {"x": 0, "y": 0, "part": "casters"}    // See vehicle_parts.json for part ids
]
                                           /* Important! Vehicle parts must be defined in the same order you would install
                                            * them in the game (ie, frames and mount points first).
                                            * You also cannot break the normal rules of installation
                                            * (you can't stack non-stackable part flags). */
```

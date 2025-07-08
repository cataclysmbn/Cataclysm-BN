---
title: Recipes
---

:::note
This article was recently split off from `JSON INFO` and likely could use additional work
:::

### Recipes

Recipes represent both craft and uncraft (disassembly) recipes.

```json
"type": "recipe",            // Recipe type. Possible values: 'recipe' (craft recipe) and 'uncraft' (uncraft recipe).
"reversible": false,         // Generate an uncraft recipe that is a reverse of this craft recipe.
"result": "javelin",         // ID of resulting item. By default, also used as the ID of the recipe.
"id_suffix": "",             // Optional (default: empty string). Some suffix to make the ID of the recipe unique. The ID of the recipe is "<result><id_suffix>".
"category": "CC_WEAPON",     // Category of crafting recipe. CC_NONCRAFT used for disassembly recipes
"subcategory": "",           // Subcategory of crafting recipe. 
"override": false,           // Optional (default: false). If false and the ID of the recipe is already used by another recipe, loading of recipes fails. If true and a recipe with the ID is already defined, the existing recipe is replaced by the new recipe.
"obsolete": false,           // Optional (default: false). Marks this recipe as obsolete. Use this instead of outright deleting it to preserve save compatibility.
"delete_flags": [ "CANNIBALISM" ], // Optional (default: empty list). Flags specified here will be removed from the resultant item upon crafting. This will override flag inheritance, but *will not* delete flags that are part of the item type itself.
"contained": false,          // Optional (default: false). Spawn produced item in a container.
"container": "can_medium",   // Optional. Override container to spawn item in (by default uses item's default container)
"charges": 1,                // Optional (default: null). Override default charges for count-by-charges items (ammo, comestibles, etc.)
"result_mult": 1,            // Optional (default: 1). Produce multiple items (or stacks for count-by-charges items) from single craft
"skill_used": "fabrication", // Skill trained and used for success checks
"skills_required": [["survival", 1], ["throw", 2]], // Skills required to unlock recipe
"book_learn": [              // (optional) Array of books that this recipe can be learned from. Each entry contains the id of the book and the skill level at which it can be learned.
    [ "textbook_anarch", 7, "something" ], // The optional third entry defines a name for the recipe as it should appear in the books description (default is the name of resulting item of the recipe)
    [ "textbook_gaswarfare", 8, "" ] // If the name is empty, the recipe is hidden, it will not be shown in the description of the book.
],
"difficulty": 3,             // Difficulty of success check
"time": "5 m",               // Time to perform the recipe specified as time string (can use minutes, hours, etc.)
"time": 5000,                // Legacy time format, in moves. Won't be supported in the future. Here, 1000 ~= 10 turns ~= 10 seconds of game time
"autolearn": true,           // Automatically learned upon gaining required skills
"autolearn" : [              // Automatically learned upon gaining listed skills
    [ "survival", 2 ],
    [ "fabrication", 3 ]
],
"decomp_learn" : 4,          // Can be learned by disassembling an item of same type as result at this level of the skill_used
"decomp_learn" : [           // Can be learned by disassembling an item of same type as result at specified levels of skills
    [ "survival", 1 ],
    [ "fabrication", 2 ]
],
"never_learn": false,        // Optional (default: false). Prevents this recipe from being learned by the player. For debug or NPC purposes.
"batch_time_factors": [25, 15], // Optional factors for batch crafting time reduction. First number specifies maximum crafting time reduction as percentage, and the second number the minimal batch size to reach that number. In this example given batch size of 20 the last 6 crafts will take only 3750 time units.
"flags": [                   // A set of strings describing boolean features of the recipe
  "BLIND_EASY",
  "ANOTHERFLAG"
],
"construction_blueprint": "camp", // an optional string containing an update_mapgen_id.  Used by faction camps to upgrade their buildings. If non-empty, the function must exist.
"on_display": false,         // this is a hidden construction item, used by faction camps to calculate construction times but not available to the player
"using": [                   // Optional, string or array. Additional external requirements to use.
  "butter_resealable_containers"
],
"qualities": [               // Generic qualities of tools needed to craft
  {"id":"CUT","level":1,"amount":1}
],
"tools": [                   // Specific tools needed to craft
[
    [ "fire", -1 ]           // Charges consumed when tool is used, -1 means no charges are consumed
]],
"components": [              // Equivalent tools or components are surrounded by a single set of brackets
[
  [ "spear_wood", 1 ],       // Number of charges/items required
  [ "pointy_stick", 1 ]
],
[
  [ "rag", 1 ],
  [ "leather", 1 ],
  [ "fur", 1 ]
],
[
  [ "plant_fibre", 20, false ], // Optional flag for recoverability, default is true.
  [ "sinew", 20, false ],
  [ "thread", 20, false ],
  [ "duct_tape", 20 ]  // Certain items are flagged as unrecoverable at the item definition level.
]
]
```

#### Overlapping recipe component requirements

:::note
This section was inherited from DDA, and has not been proven to still apply yet. Take the below with a grain of salt.
:::

If recipes have requirements which overlap, this makes it more difficult for the game to calculate
whether it is possible to craft a recipe at all.

For example, the survivor telescope recipe has the following requirements (amongst others):

```
1 high-quality lens
AND
1 high-quality lens OR 1 small high-quality lens
```

These overlap because both list the high-quality lens.

A small amount of overlap (such as the above) can be handled, but if you have too many component
lists which overlap in too many ways, then you may see an error during recipe finalization that your
recipe is too complex. In this case, the game may not be able to corectly predict whether it can be
crafted.

To work around this issue, if you do not wish to simplify the recipe requirements, then you can
split your recipe into multiple steps. For example, if we wanted to simplify the above survivor
telescope recipe we could introduce an intermediate item "survivor eyepiece", which requires one of
either lens, and then the telescope would require a high-quality lens and an eyepiece. Overall, the
requirements are the same, but neither recipe has any overlap.

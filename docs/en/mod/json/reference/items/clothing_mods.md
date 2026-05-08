---
title: Clothing Modifications
---

### clothing_mod

```json
"type": "clothing_mod",
"id": "leather_padded",   // Unique ID.
"flag": "leather_padded", // flag to add to clothing.
"item": "leather",        // item to consume.
"implement_prompt": "Pad with leather",      // prompt to show when implement mod.
"destroy_prompt": "Destroy leather padding", // prompt to show when destroy mod.
"restricted": true,       // (optional) If true, clothing must list this mod's flag in "valid_mods" list to use it. Defaults to false.
"mod_value": [            // List of mod effect.
    {
        "type": "bash",   // "bash", "cut", "bullet", "fire", "acid", "warmth", "storage", and "encumbrance" is available.
        "value": 1,       // value of effect.
        "round_up": false // (optional) round up value of effect. defaults to false.
        "proportion": [   // (optional) value of effect propotions to clothing's parameter.
            "thickness",  //            Add value for every layer of "material_thickness" the item has.
            "volume",     //            Add value for every liter of baseline volume the item has.
            "coverage"    //            Reduce value by percentage of average coverage the item has.
        ]
    }
]
```

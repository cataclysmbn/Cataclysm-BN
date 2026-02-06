# Item Creation / Types

> [!NOTE]
>
> This page is currently under-construction and was recently split off from `JSON INFO`

### Generic Items

```json
"type": "GENERIC",                // Defines this as some generic item
"id": "socks",                    // Unique ID. Must be one continuous word, use underscores if necessary
"name": {
    "ctxt": "clothing",           // Optional translation context. Useful when a string has multiple meanings that need to be translated differently in other languages.
    "str": "pair of socks",       // The name appearing in the examine box.  Can be more than one word separated by spaces
    "str_pl": "pairs of socks"    // Optional. If a name has an irregular plural form (i.e. cannot be formed by simply appending "s" to the singular form), then this should be specified. "str_sp" can be used if the singular and plural forms are the same
},
"conditional_names": [ {          // Optional list of names that will be applied in specified conditions (see Conditional Naming section for more details).
    "type": "COMPONENT_ID",       // The condition type.
    "condition": "leather",       // The condition to check for.
    "name": { "str": "pair of leather socks", "str_pl": "pairs of leather socks" } // Name field, same rules as above.
} ],
"container": "null",             // What container (if any) this item should spawn within
"repairs_like": "scarf",          // If this item does not have recipe, what item to look for a recipe for when repairing it.
"color": "blue",                 // Color of the item symbol.
"symbol": "[",                   // The item symbol as it appears on the map. Must be a Unicode string exactly 1 console cell width.
"looks_like": "rag",              // hint to tilesets if this item has no tile, use the looks_like tile
"description": "Socks. Put 'em on your feet.", // Description of the item
"ascii_picture": "ascii_socks", // Id of the asci_art used for this item
"phase": "solid",                            // (Optional, default = "solid") What phase it is
"weight": "350 g",                           // Weight, weight in grams, mg and kg can be used - "50 mg", "5 g" or "5 kg". For stackable items (ammo, comestibles) this is the weight per charge.
"volume": "250 ml",                          // Volume, volume in ml and L can be used - "50 ml" or "2 L". For stackable items (ammo, comestibles) this is the volume of stack_size charges.
"integral_volume": 0,                        // Volume added to base item when item is integrated into another (eg. a gunmod integrated to a gun). Volume in ml and L can be used - "50 ml" or "2 L".
"integral_weight": 0,                        // Weight added to base item when item is integrated into another (eg. a gunmod integrated to a gun)
"rigid": false,                              // For non-rigid items volume (and for worn items encumbrance) increases proportional to contents
"insulation": 1,                             // (Optional, default = 1) If container or vehicle part, how much insulation should it provide to the contents
"price": 100,                                // Used when bartering with NPCs. For stackable items (ammo, comestibles) this is the price for stack_size charges. Can use string "cent" "USD" or "kUSD".
"price_postapoc": "1 USD",                   // Same as price but represent value post cataclysm. Can use string "cent" "USD" or "kUSD".
"material": ["COTTON"],                      // Material types, can be as many as you want.  See materials.json for possible options
"weapon_category": [ "WEAPON_CAT1" ],        // (Optional) Weapon categories this item is in for martial arts.
"cutting": 0,                                // (Optional, default = 0) Cutting damage caused by using it as a melee weapon.  This value cannot be negative.
"bashing": 0,                                // (Optional, default = 0) Bashing damage caused by using it as a melee weapon.  This value cannot be negative.
"to_hit": 0,                                 // (Optional, default = 0) To-hit bonus if using it as a melee weapon (whatever for?)
"attacks": [                                 // (Optional) New attack statblock, WIP feature
  { "id": "BASH",                            // ID of the attack.  Attack with ID "DEFAULT" will be replaced by calculated data (this can be used to remove custom attacks on "copy-from" item)
    "to_hit": 1,                             // To-hit bonus of this attack
    "damage": { "values": [ { "damage_type": "bash", "amount": 50 } ] } }, // Damage of this attack, using `damage_instance` syntax (see below)
  { "id": "THRUST", "damage": { "values": [ { "damage_type": "stab", "amount": 45 } ] } }
],
"flags": ["VARSIZE"],                        // Indicates special effects, see json_flags.md
"environmental_protection_with_filter": 6,   // the resistance to environmental effects if an item (for example a gas mask) requires a filter to operate and this filter is installed. Used in combination with use_action 'GASMASK' and 'DIVE_TANK'
"magazine_well": 0,                          // Volume above which the magazine starts to protrude from the item and add extra volume
"magazines": [                               // Magazines types for each ammo type (if any) which can be used to reload this item
    [ "9mm", [ "glockmag" ] ]                // The first magazine specified for each ammo type is the default
    [ "45", [ "m1911mag", "m1911bigmag" ] ],
],
"milling": {                                 // Optional. If given, the item can be milled in a water/wind mill.
  "into": "flour",                           // The item id of the product. Product MUST be something that uses charges.
  "conversion_rate": 4                       // Number of products per item consumed. At a conversion_rate of 4, 1 item is milled into 4 product. Only accepts integers.
},
"item_vars": {                               // Prepopulate Item Vars; Currently used by nothing other then mods and in game logic
  "test": "test"                             // Predominately lua mods will find this useful
}
"explode_in_fire": true,                     // Should the item explode if set on fire
"explosion": {                               // Physical explosion data
  "damage": 10,                              // Damage the explosion deals to player at epicenter. Damage is halved above 50% radius.
  "radius": 8,                               // Radius of the explosion. 0 means only the epicenter is affected.
  "fire": true,                              // Should the explosion leave fire
  "fragment": {                              // Projectile data of "shrapnel". This projectile will hit every target in its range and field of view exactly once.
    "damage": {                              // Damage data of the shrapnel projectile.  Uses damage_instance syntax (see below)
      "damage_type": "acid",                 // Type of damage dealt.
      "amount": 10                           // Amount of damage dealt.
      "armor_penetration": 4                 // Amount of armor ignored. Applied per armor piece, not in total.
      "armor_multiplier": 2.5                // Multiplies remaining damage reduction from armor, applied after armor penetration (if present). Higher numbers make armor stop fragments from this explosion more effectively, lower numbers act as a percentage reduction in remaining armor.
    }
  }
},
"repair_difficulty": 2                       // Overrites recipe difficulty being used for repair difficulty
```

#### damage_instance

```json
{
  "damage_type": "acid", // Type of damage dealt.
  "amount": 10, // Amount of damage dealt.
  "armor_penetration": 4, // Amount of armor ignored. Applied per armor piece, not in total.
  "armor_multiplier": 2.5 // Multiplies remaining damage reduction from armor, applied after armor penetration (if present). Higher numbers make armor more effective at protecting from this attack, lower numbers act as a percentage reduction in remaining armor.
}
```

### Ammo

```json
"type" : "AMMO",            // Defines this as ammo
...                         // same entries as above for the generic item.
                            // additional some ammo specific entries:
"ammo_type" : "shot",       // Determines what it can be loaded in
"damage" : 18,              // Ranged damage when fired
"prop_damage": 2,           // Multiplies the damage of weapon by amount (overrides damage field)
"pierce" : 0,               // Armor piercing ability when fired
"range" : 5,                // Range when fired
"dispersion" : 0,           // Inaccuracy of ammo, measured in quarter-degrees
"recoil" : 18,              // Recoil caused when firing
"count" : 25,               // Number of rounds that spawn together
"stack_size" : 50,          // (Optional) How many rounds are in the above-defined volume. If omitted, is the same as 'count'
"show_stats" : true,        // (Optional) Force stat display for combat ammo. (for projectiles lacking both damage and prop_damage)
"dont_recover_one_in": 1    // (Optional) 1 in x chance of not recovering the ammo (100 means you have a 99% chance of getting it back)
"drop": "nail",             // (Optional) Defines an object that drops at the projectile location at a 100% chance.
"drop_active": false        // (Optional) Whether the object starts active. Default is true.
"drop_count": 1,            // (Optional) Number of items to drop. For tools, this sets their charges. 
                            // If omitted, the drop amount defaults to the 'count' defined in its itype.
"effects" : ["COOKOFF", "SHOT"]
```

### Magazine

```json
"type": "MAGAZINE",              // Defines this as a MAGAZINE
...                              // same entries as above for the generic item.
                                 // additional some magazine specific entries:
"ammo_type": [ "40", "357sig" ], // What types of ammo this magazine can be loaded with
"capacity" : 15,                 // Capacity of magazine (in equivalent units to ammo charges)
"count" : 0,                     // Default amount of ammo contained by a magazine (set this for ammo belts)
"default_ammo": "556",           // If specified override the default ammo (optionally set this for ammo belts)
"reliability" : 8,               // How reliable this this magazine on a range of 0 to 10? (see GAME_BALANCE.md)
"reload_time" : 100,             // How long it takes to load each unit of ammo into the magazine
"linkage" : "ammolink"           // If set one linkage (of given type) is dropped for each unit of ammo consumed (set for disintegrating ammo belts)
```

### Armor

Armor can be defined like this:

```json
"type" : "ARMOR",     // Defines this as armor
...                   // same entries as above for the generic item.
                      // additional some armor specific entries:
"covers" : ["FEET"],  // Where it covers.  Possible options are TORSO, HEAD, EYES, MOUTH, ARMS, HANDS, LEGS, FEET
"storage" : 0,        //  (Optional, default = 0) How many volume storage slots it adds
"warmth" : 10,        //  (Optional, default = 0) How much warmth clothing provides
"environmental_protection" : 0,  //  (Optional, default = 0) How much environmental protection it affords
"encumbrance" : 0,    // Base encumbrance (unfitted value)
"max_encumbrance" : 0,    // When a character is completely full of volume, the encumbrance of a non-rigid storage container will be set to this. Otherwise it'll be between the encumbrance and max_encumbrance following the equation: encumbrance + (max_encumbrance - encumbrance) * character volume.
"weight_capacity_bonus": "20 kg",    // (Optional, default = 0) Bonus to weight carrying capacity, can be negative. Strings must be used - "5000 g" or "5 kg"
"weight_capacity_modifier": 1.5, // (Optional, default = 1) Factor modifying base weight carrying capacity.
"coverage" : 80,      // What percentage of body part
"material_thickness" : 1,  // Thickness of material, in millimeter units (approximately).  Generally ranges between 1 - 5, more unusual armor types go up to 10 or more
"power_armor" : false, // If this is a power armor item (those are special).
"valid_mods" : ["steel_padded"] // List of valid clothing mods. Note that if the clothing mod doesn't have "restricted" listed, this isn't needed.
"resistance": { "cut": 0, "bullet": 1000 } // If set, overrides usual resistance calculation. Values are for undamaged item, thickness affects scaling with damage - 1 thickness means no reduction from damage, 2 means it's halved on first damage, 10 means each level of damage decreases armor by 10%
```

Alternately, every item (book, tool, gun, even food) can be used as armor if it has armor_data:

```json
"type" : "TOOL",      // Or any other item type
...                   // same entries as for the type (e.g. same entries as for any tool),
"armor_data" : {      // additionally the same armor data like above
    "covers" : ["FEET"],
    "storage" : 0,
    "warmth" : 10,
    "environmental_protection" : 0,
    "encumbrance" : 0,
    "coverage" : 80,
    "material_thickness" : 1,
    "power_armor" : false
}
```

### Pet Armor

Pet armor can be defined like this:

```json
"type" : "PET_ARMOR",     // Defines this as armor
...                   // same entries as above for the generic item.
                      // additional some armor specific entries:
"storage" : 0,        //  (Optional, default = 0) How many volume storage slots it adds
"environmental_protection" : 0,  //  (Optional, default = 0) How much environmental protection it affords
"material_thickness" : 1,  // Thickness of material, in millimeter units (approximately).  Generally ranges between 1 - 5, more unusual armor types go up to 10 or more
"pet_bodytype": "dog",       // the body type of the pet that this monster will fit.  See MONSTERS.md
"max_pet_vol": "50 ml",   // the maximum volume of the pet that will fit into this armor. Volume in ml and L can be used - "50 ml" or "2 L".
"min_pet_vol": "50 ml",   // the minimum volume of the pet that will fit into this armor. Volume in ml and L can be used - "50 ml" or "2 L".
"power_armor" : false, // If this is a power armor item (those are special).
```

Alternately, every item (book, tool, gun, even food) can be used as armor if it has armor_data:

```json
"type" : "TOOL",      // Or any other item type
...                   // same entries as for the type (e.g. same entries as for any tool),
"pet_armor_data" : {      // additionally the same armor data like above
    "storage" : 0,
    "environmental_protection" : 0,
    "pet_bodytype": "dog",
    "max_pet_vol": "35000 ml",
    "min_pet_vol": "25000 ml",
    "material_thickness" : 1,
    "power_armor" : false
}
```

### Books

Books can be defined like this:

```json
"type" : "BOOK",      // Defines this as a BOOK
...                   // same entries as above for the generic item.
                      // additional some book specific entries:
"max_level" : 5,      // Maximum skill level this book will train to
"intelligence" : 11,  // Intelligence required to read this book without penalty
"time" : "35 m",          // Time a single read session takes. An integer will be read in minutes or a time string can be used.
"fun" : -2,           // Morale bonus/penalty for reading
"skill" : "computer", // Skill raised
"chapters" : 4,       // Number of chapters (for fun only books), each reading "consumes" a chapter. Books with no chapters left are less fun (because the content is already known to the character).
"required_level" : 2  // Minimum skill level required to learn
```

Alternately, every item (tool, gun, even food) can be used as book if it has book_data:

```json
"type" : "TOOL",      // Or any other item type
...                   // same entries as for the type (e.g. same entries as for any tool),
"book_data" : {       // additionally the same book data like above
    "max_level" : 5,
    "intelligence" : 11,
    "time" : "35 m",
    "fun" : -2,
    "skill" : "computer",
    "chapters" : 4,
    "use_action" : "MA_MANUAL", // The book_data can have use functions (see USE ACTIONS) that are triggered when the books has been read. These functions are not triggered by simply activating the item (like tools would).
    "required_level" : 2
}
```

Since many book names are proper names, it's often necessary to explicitly specify the plural forms.
The following is the game's convention on plural names of books:

1. For non-periodical books (textbooks, manuals, spellbooks, etc.),
   1. If the book's singular name is a proper name, then the plural name is
      `copies of (singular name)`. For example, the plural name of
      `Lessons for the Novice Bowhunter` is `copies of Lessons for the Novice Bowhunter`.
   2. Otherwise, the plural name is the usual plural of the singular name. For example, the plural
      name of `tactical baton defense manual` is `tactical baton defense manuals`
2. For periodicals (magazines and journals),
   1. If the periodical's singular name is a proper name, and doesn't end with "Magazine", "Weekly",
      "Monthly", etc., the plural name is `issues of (singular name)`. For example, the plural name
      of `Archery for Kids` is `issues of Archery for Kids`.
   2. Otherwise, the periodical's plural name is the usual plural of the singular name. For example,
      the plural name of `Crafty Crafter's Quarterly` is `Crafty Crafter's Quarterlies`.
3. For board games (represented internally as book items),
   1. If the board game's singular name is a proper name, the plural is `sets of (singular name)`.
      For example, the plural name of `Picturesque` is `sets of Picturesque`.
   2. Otherwise the plural name is the usual plural. For example, the plural of `deck of cards` is
      `decks of cards`.

#### Conditional Naming

The `conditional_names` field allows defining alternate names for items that will be displayed
instead of (or in addition to) the default name, when specific conditions are met. Take the
following (incomplete) definition for `sausage` as an example of the syntax:

```json
{
  "name": "sausage",
  "conditional_names": [
    {
      "type": "FLAG",
      "condition": "CANNIBALISM",
      "name": "Mannwurst"
    },
    {
      "type": "COMPONENT_ID",
      "condition": "mutant",
      "name": { "str_sp": "sinister %s" }
    }
  ]
}
```

You can list as many conditional names for a given item as you want. Each conditional name must
consist of 3 elements:

1. The condition type:
   - `COMPONENT_ID` searches all the components of the item (and all of _their_ components, and so
     on) for an item with the condition string in their ID. The ID only needs to _contain_ the
     condition, not match it perfectly (though it is case sensitive). For example, supplying a
     condition `mutant` would match `mutant_meat`.
   - `FLAG` which checks if an item has the specified flag (exact match).
2. The condition you want to look for.
3. The name to use if a match is found. Follows all the rules of a standard `name` field, with valid
   keys being `str`, `str_pl`, and `ctxt`. You may use %s here, which will be replaced by the name
   of the item. Conditional names defined prior to this one are taken into account.

So, in the above example, if the sausage is made from mutant humanoid meat, and therefore both has
the `CANNIBALISM` flag, _and_ has a component with `mutant` in its ID:

1. First, the item name is entirely replaced with "Mannwurst" if singular, or "Mannwursts" if
   plural.
2. Next, it is replaced by "sinister %s", but %s is replaced with the name as it was before this
   step, resulting in "sinister Mannwurst" or "sinister Mannwursts".

NB: If `"str": "sinister %s"` was specified instead of `"str_sp": "sinister %s"`, the plural form
would be automatically created as "sinister %ss", which would become "sinister Mannwurstss" which is
of course one S too far. Rule of thumb: If you are using %s in the name, always specify an identical
plural form unless you know exactly what you're doing!

#### Color Key

When adding a new book, please use this color key:

- Magazines: `pink`
- “Paperbacks” Short enjoyment books (including novels): `light_cyan`
- “Hardbacks” Long enjoyment books (including novels): `light_blue`
- “Small textbook” Beginner level textbooks, guides and martial arts books: `green`
- “Large textbook” Advanced level textbooks and advanced guides: `blue`
- Religious books: `dark_gray`
- “Printouts” (including spiral-bound, binders, and similar) Technical documents, (technical?)
  protocols, (lab) journals, personal diaries: `light_green`
- Other reading material/non-books (use only if every other category does not apply): `light_gray`

A few exceptions to this color key may apply, for example for books that don’t are what they seem to
be. Never use `yellow` and `red`, those colors are reserved for sounds and infrared vision.

#### CBMs

CBMs can be defined like this:

```json
"type" : "BIONIC_ITEM",         // Defines this as a CBM
...                             // same entries as above for the generic item.
                                // additional some CBM specific entries:
"bionic_id" : "bio_advreactor", // ID of the installed bionic if not equivalent to "id"
"difficulty" : 11,              // Difficulty of installing CBM
"is_upgrade" : true             // Whether the CBM is an upgrade of another bionic.
"installation_data" : "AID_bio_advreactor" // ID of the item which allows for almost guaranteed installation of corresponding bionic.
```

### Comestibles

```json
"type" : "COMESTIBLE",      // Defines this as a COMESTIBLE
...                         // same entries as above for the generic item.
                            // additional some comestible specific entries:
"addiction_type" : "crack", // Addiction type
"spoils_in" : 0,            // A time duration: how long a comestible is good for. 0 = no spoilage.
"use_action" : "CRACK",     // What effects a comestible has when used, see special definitions below
"stim" : 40,                // Stimulant effect
"fatigue_mod": 3,           // How much fatigue this comestible removes. (Negative values add fatigue)
"radiation": 8,             // How much radiation you get from this comestible.
"comestible_type" : "MED",  // Comestible type, used for inventory sorting
"quench" : 0,               // Thirst quenched
"heal" : -2,                // Health effects (used for sickness chances)
"addiction_potential" : 80, // Ability to cause addictions
"monotony_penalty" : 0,     // (Optional, default: 2) Fun is reduced by this number for each one you've consumed in the last 48 hours.
                            // Can't drop fun below 0, unless the comestible also has the "NEGATIVE_MONOTONY_OK" flag.
"calories" : 0,             // Hunger satisfied (in kcal)
"nutrition" : 0,            // Hunger satisfied (OBSOLETE)
"tool" : "apparatus",       // Tool required to be eaten/drank
"charges" : 4,              // Number of uses when spawned
"stack_size" : 8,           // (Optional) How many uses are in the above-defined volume. If omitted, is the same as 'charges'
"fun" : 50                  // Morale effects when used
"cooks_like": "meat_cooked" // (Optional) If the item is used in a recipe, replaces it with its cooks_like
"parasites": 10,            // (Optional) Probability of becoming parasitised when eating
"contamination": [ { "disease": "bad_food", "probability": 5 } ],         // (Optional) List of diseases carried by this comestible and their associated probability. Values must be in the [0, 100] range.
```

### Containers

```json
"type": "CONTAINER",  // Defines this as a container
...                   // same data as for the generic item (see above).
"contains": 200,      // How much volume this container can hold
"seals": false,       // Can be resealed, this is a required for it to be used for liquids. (optional, default: false)
"watertight": false,  // Can hold liquids, this is a required for it to be used for liquids. (optional, default: false)
"preserves": false,   // Contents do not spoil. (optional, default: false)
```

Alternately, every item can be used as container:

```json
"type": "ARMOR",      // Any type is allowed here
...                   // same data as for the type
"container_data" : {  // The container specific data goes here.
    "contains": 200,
}
```

This defines a armor (you need to add all the armor specific entries), but makes it usable as
container. It could also be written as a generic item ("type": "GENERIC") with "armor_data" and
"container_data" entries.

### Melee

```json
"id": "hatchet",       // Unique ID. Must be one continuous word, use underscores if necessary
"symbol": ";",         // ASCII character used in-game
"color": "light_gray", // ASCII character color
"name": "hatchet",     // In-game name displayed
"description": "A one-handed hatchet. Makes a great melee weapon, and is useful both for cutting wood, and for use as a hammer.", // In-game description
"price": 95,           // Used when bartering with NPCs.  Can use string "cent" "USD" or "kUSD".
"material": ["iron", "wood"], // Material types.  See materials.json for possible options
"weight": 907,         // Weight, measured in grams
"volume": "1500 ml",   // Volume, volume in ml and L can be used - "50 ml" or "2 L"
"bashing": 12,         // Bashing damage caused by using it as a melee weapon
"cutting": 12,         // Cutting damage caused by using it as a melee weapon
"flags" : ["CHOP"],    // Indicates special effects
"to_hit": 1            // To-hit bonus if using it as a melee weapon
```

#### Melee `Weapon_category`

| Weapon Category | Description                                                                              |
| --------------- | ---------------------------------------------------------------------------------------- |
| FIST_WEAPONS    | Handheld weapons used to supplement fists in martial arts.                               |
| ---             | ---                                                                                      |
| STILETTOS       | Short, tapering 'blade' fixed onto a handle, has no cutting edge                         |
| KNIVES          | Short blade fixed onto a handle, for cutting or as weapon.                               |
| SHORT_SWORDS    | One handed sword of length between a large knife and a 'proper' sword.                   |
| 1H_SWORDS       | Sword meant to be wielded with one hand.                                                 |
| 2H_SWORDS       | Sword meant to be wielded with both hands.                                               |
| DUELING_SWORDS  | Swords with thin profiles typically meant for stabbing.                                  |
| BIONIC_SWORDS   | Swords integrated into the body via bionics.                                             |
| ---             | ---                                                                                      |
| SAPS            | Very short length of typically flexible material, with a weighted tip.                   |
| BATONS          | Thin, balanced rod of strong material.                                                   |
| TONFAS          | Unique looking T-shaped baton believed to originate from China.                          |
| CLUBS           | Rod with a thicker striking head. May be one or two-handed.                              |
| QUARTERSTAVES   | Long pole wielded with both hands.                                                       |
| MACES           | Short one-handed weapon with a striking head firmly attached to a short handle.          |
| MORNINGSTARS    | Typically two-handed weapon with a striking head firmly attached to a long handle.       |
| FLAILS          | Striking head attached to handle by flexible rope/chain.                                 |
| 1H_HAMMERS      | Hammers meant to be wielded with a single hand.                                          |
| 2H_HAMMERS      | Hammers meant to be wielded with both hands.                                             |
| ---             | ---                                                                                      |
| HAND_AXES       | Axe with a short handle, typically wielded in one hand, ocassionally thrown.             |
| 1H_AXES         | Axes meant to be wielded with one hand, typically with a handle longer than the handaxe. |
| 2H_AXES         | Axes meant to be wielded with two hands.                                                 |
| ---             | ---                                                                                      |
| WHIPS           | Flexible tool used to strike at range.                                                   |
| ---             | ---                                                                                      |
| 1H_HOOKED       | One handed weapon with hooking capability. (and isn't an axe or hammer)                  |
| ---             | ---                                                                                      |
| HOOKED_POLES    | Polearm with hooked end (Like a shepherd's crook)                                        |
| SPEARS          | Polearm with a long shaft and a sharp tip made of hard material.                         |
| PIKES           | Very long spear that can only be wielded in two hands, very unwieldy.                    |
| GLAIVES         | Polearm with a single-edged blade mounted on the end.                                    |

| Weapon Styles   | Description                                   |
| --------------- | --------------------------------------------- |
| MEDIEVAL_SWORDS | Swords associated with European culture.      |
| JAPANESE_SWORDS | Swords associated with Japanese culture.      |
| BIONIC_WEAPONRY | Weapons integrated into the body via bionics. |

### Gun

Guns can be defined like this:

```json
"type": "GUN",             // Defines this as a GUN
...                        // same entries as above for the generic item.
                           // additional some gun specific entries:
"skill": "pistol",         // Skill used for firing
"ammo": [ "357", "38" ],   // Ammo types accepted for reloading
"ranged_damage": 0,        // Ranged damage when fired
"range": 0,                // Range when fired
"dispersion": 32,          // Inaccuracy of gun, measured in quarter-degrees
// When sight_dispersion and aim_speed are present in a gun mod, the aiming system picks the "best"
// sight to use for each aim action, which is the fastest sight with a dispersion under the current
// aim threshold.
"sight_dispersion": 10,    // Inaccuracy of gun derived from the sight mechanism, also in quarter-degrees
"aim_speed": 3,            // A measure of how quickly the player can aim, in moves per point of dispersion.
"recoil": 0,               // Recoil caused when firing, in quarter-degrees of dispersion.
"durability": 8,           // Resistance to damage/rusting, also determines misfire chance
"blackpowder_tolerance": 8,// One in X chance to get clogged up (per shot) when firing blackpowder ammunition (higher is better). Optional, default is 8.
"min_cycle_recoil": 0,     // Minimum ammo recoil for gun to be able to fire more than once per attack.
"burst": 5,                // Number of shots fired in burst mode
"handling": 15,            // Modifier in how quick the gun recovers from recoil. 20 is baseline, below that is worse and above is better. Scales with '2 - 2.0 => 0' by default, for example 15 handling would be '2 - 1.5 => 0.5' increase to recoil and 30 would be '2 - 3.0 => -1' reduction to recoil
"clip_size": 100,          // Maximum amount of ammo that can be loaded
"ups_charges": 0,          // Additionally to the normal ammo (if any), a gun can require some charges from an UPS. This also works on mods. Attaching a mod with ups_charges will add/increase ups drain on the weapon.
"ammo_to_fire" 1,          // Amount of ammo used per shot, separate from any UPS cost that may be given to the weapon.
// The legacy item flags `FIRE_20`, `FIRE_50`, and `FIRE_100` are still permitted and will override `ammo_to_fire` if present.
"reload": 450,             // Amount of time to reload, 100 = 1 second = 1 "turn". Default 100.
"built_in_mods": ["m203"], // An array of mods that will be integrated in the weapon using the IRREMOVABLE tag.
"default_mods": ["m203"]   // An array of mods that will be added to a weapon on spawn.
"barrel_volume": "30 mL",  // Amount of volume lost when the barrel is sawn. Approximately 250 ml per IRL inch is a decent approximation.
"barrel_length": "30 mL",  // Depreciated alias of barrel_volume, which should be used instead for clarity.
"valid_mod_locations": [ [ "accessories", 4 ], [ "grip", 1 ] ],  // The valid locations for gunmods and the mount of slots for that location.
```

Alternately, every item (book, tool, armor, even food) can be used as gun if it has gun_data:

```json
"type": "TOOL",      // Or any other item type
...                   // same entries as for the type (e.g. same entries as for any tool),
"gun_data" : {        // additionally the same gun data like above
    "skill": ...,
    "recoil": ...,
    ...
}
```

#### Ranged `Weapon_category`

| Weapon Category   | Description                                                                                                                 |
| ----------------- | --------------------------------------------------------------------------------------------------------------------------- |
| BOWS              | Elastic launching device for long-shafted projectiles.                                                                      |
| S_XBOWS           | Elastic launching device mounted on a frame to be triggered, pistol sized.                                                  |
| M_XBOWS           | Elastic launching device mounted on a frame to be triggered.                                                                |
| SLINGSHOTS        | Elastic, handheld launching device typically used for small round projectiles.                                              |
| SLINGS            | Projectile weapon using a cradle connected to two retention cords, used to fling blunt projectiles.                         |
| ---               | ---                                                                                                                         |
| PISTOLS           | Handgun with a chamber integral to the gun barrel. In-game, any handgun that isn't a revolver goes here.                    |
| REVOLVERS         | Repeating handgun with a revolving cylinder containing multiple chambers.                                                   |
| SUBMACHINE_GUNS   | Magazine fed automatic carbine designed to fire handgun cartridges.                                                         |
| RIFLES            | Long barrelled firearms designed for more accurate shooting.                                                                |
| MACHINE_GUNS      | Fully automatic autoloading firearm designed for sustained fire.                                                            |
| GATLING_GUNS      | Rapid firing multi barrel firearm.                                                                                          |
| SHOTGUNS          | Long barreled firearm generally designed to fire shotshells.                                                                |
| ---               | ---                                                                                                                         |
| GRENADE_LAUNCHERS | Firearm designed to propel large caliber projectile typically loaded with warhead of some kind (smoke, gas, explosive, etc) |
| ROCKET_LAUNCHERS  | Firearm that propels unguided, rocket-propelled projectile.                                                                 |
| ---               | ---                                                                                                                         |
| FLAMETHROWERS     | Ranged incendiary device designed to propel a controllable jet of fire.                                                     |
| WATER_CANNONS     | It fires water at your enemies.                                                                                             |
| SPRAY_GUNS        | It spews chemicals at your enemies.                                                                                         |

| Action category | Description                                                                                                       |
| --------------- | ----------------------------------------------------------------------------------------------------------------- |
| 1SHOT           | Ranged weapon with at least one barrel but no loading system/magazine.                                            |
| AUTOLOADING     | Ranged weapon with autoloading mechanisms like blowback, gas-operated, or recoil operated systems.                |
| MANUAL_ACTION   | Ranged weapon using manual actions like bolt/pump/lever.                                                          |
| ENERGY_WEAPONS  | Weapon designed to utilize focused energy (sonic, electromagnetic waves, particle beams, etc). Both Ranged/Melee. |
| MAGNETIC        | Weapon that propels payload via electromagnetism.                                                                 |
| PNEUMATIC       | Ranged weapon that propels payload via compressed air.                                                            |
| ELASTIC         | Ranged weapon that propels payload via elastic band.                                                              |

### Gunmod

Gun mods can be defined like this:

```json
"type": "GUNMOD",              // Defines this as a GUNMOD
...                            // Same entries as above for the generic item.
                               // Additionally some gunmod specific entries:
"location": "stock",           // Mandatory. Where is this gunmod is installed?
"mod_targets": [ "crossbow" ], // Optional. What specific weapons can this gunmod be used with?
"mod_target_category": [ [ "BOWS" ] ], // Optional. What specific weapon categories can this gunmod be used with?
"mod_exclusions": [ "laser_rifle" ], // Optional. What specific weapons can't this gunmod be used with?
"mod_exclusion_category": [ [ "ENERGY_WEAPONS" ] ], // Optional. What specific weapon categories can't this gunmod be used with?
"acceptable_ammo": [ "9mm" ],  // Optional filter restricting mod to guns with those base (before modifiers) ammo types
"install_time": "30 s",        // Optional time installation takes. Installation is instantaneous if unspecified. An integer will be read as moves or a time string can be used.
"ammo_modifier": [ "57" ],     // Optional field which if specified modifies parent gun to use these ammo types
"magazine_adaptor": [ [ "223", [ "stanag30" ] ] ], // Optional field which changes the types of magazines the parent gun accepts
"mode_modifier": [ [ "AUTO", "auto", 5 ] ]         // Optional field which adds new firing modes to a weapon
"damage_modifier": -1,         // Optional field increasing or decreasing base gun damage
"dispersion_modifier": 15,     // Optional field increasing or decreasing base gun dispersion
"loudness_modifier": 4,        // Optional field increasing or decreasing base guns loudness
"range_modifier": 2,           // Optional field increasing or decreasing base gun range
"recoil_modifier": -100,       // Optional field increasing or decreasing base gun recoil
"ups_charges_modifier": 200,   // Optional field increasing or decreasing base gun UPS consumption (per shot) by adding given value
"ups_charges_multiplier": 2.5, // Optional field increasing or decreasing base gun UPS consumption (per shot) by multiplying by given value
"ammo_to_fire_modifier": 200,   // Optional field increasing or decreasing amount of main ammo consumed per shot by adding given value
"ammo_to_fire_multiplier": 2.5, // Optional field increasing or decreasing main ammo consumed per shot by multiplying by given value
"reload_modifier": -10,        // Optional field increasing or decreasing base gun reload time in percent
"min_str_required_mod": 14,    // Optional field increasing or decreasing minimum strength required to use gun
```

Alternately, every item (book, tool, armor, even food) can be used as a gunmod if it has
gunmod_data:

```json
"type": "TOOL",       // Or any other item type
...                   // same entries as for the type (e.g. same entries as for any tool),
"gunmod_data" : {
    "location": ...,
    "mod_targets": ...,
    ...
}
```

### Batteries

```json
"type": "BATTERY",    // Defines this as a BATTERY
...                   // Same entries as above for the generic item
                      // Additionally some battery specific entries:
"max_energy": "30 kJ" // Mandatory. Maximum energy quantity the battery can hold
```

### Tools

```json
"id": "torch_lit",    // Unique ID. Must be one continuous word, use underscores if necessary
"type": "TOOL",       // Defines this as a TOOL
"symbol": "/",        // ASCII character used in-game
"color": "brown",     // ASCII character color
"name": "torch (lit)", // In-game name displayed
"description": "A large stick, wrapped in gasoline soaked rags. This is burning, producing plenty of light", // In-game description
"price": 0,           // Used when bartering with NPCs.  Can use string "cent" "USD" or "kUSD".
"material": "wood",   // Material types.  See materials.json for possible options
"techniques": [ "FLAMING" ], // Combat techniques used by this tool
"flags": "FIRE",      // Indicates special effects
"weight": 831,        // Weight, measured in grams
"volume": "1500 ml",  // Volume, volume in ml and L can be used - "50 ml" or "2 L"
"bashing": 12,        // Bashing damage caused by using it as a melee weapon
"cutting": 0,         // Cutting damage caused by using it as a melee weapon
"to_hit": 3,          // To-hit bonus if using it as a melee weapon
"max_charges": 75,    // Maximum charges tool can hold
"initial_charges": 75, // Charges when spawned
"rand_charges": [10, 15, 25], // Randomize the charges when spawned. This example has a 50% chance of rng(10, 15) charges and a 50% chance of rng(15, 25) (The endpoints are included)
"sub": "hotplate",    // optional; this tool has the same functions as another tool
"charge_factor": 5,   // this tool uses charge_factor charges for every charge required in a recipe; intended for tools that have a "sub" field but use a different ammo that the original tool
"charges_per_use": 1, // Charges consumed per tool use
"turns_per_charge": 20, // Charges consumed over time, deprecated in favor of power_draw
"power_draw": 50,       // Energy consumption rate in mW
"ups_eff_mult": 2,       // Multiplier for UPS efficiency
"ups_recharge_rate": 10,       // The power output for UPS recharging of items
"ammo": [ "NULL" ],       // Ammo types used for reloading
"revert_to": "torch_done", // Transforms into item when charges are expended
"use_action": "firestarter" // Action performed when tool is used, see special definition below
```

### Seed Data

Every item type can have optional seed data, if the item has seed data, it's considered a seed and
can be planted:

```json
"seed_data" : {
    "fruit": "weed", // The item id of the fruits that this seed will produce.
    "seeds": false, // (optional, default is true). If true, harvesting the plant will spawn seeds (the same type as the item used to plant). If false only the fruits are spawned, no seeds.
    "fruit_div": 2, // (optional, default is 1). Final amount of fruit charges produced is divided by this number. Works only if fruit item is counted by charges.
    "byproducts": ["withered", "straw_pile"], // A list of further items that should spawn upon harvest.
    "plant_name": "sunflower", // The name of the plant that grows from this seed. This is only used as information displayed to the user.
    "grow" : 91 // A time duration: how long it takes for a plant to fully mature. Based around a 91 day season length (roughly a real world season) to give better accuracy for longer season lengths
                // Note that growing time is later converted based upon the season_length option, basing it around 91 is just for accuracy purposes
                // A value 91 means 3 full seasons, a value of 30 would mean 1 season.
    "required_terrain_flag": "PLANTABLE" // A tag that terrain and furniture would need to have in order for the seed to be plantable there.
					 // Default is "PLANTABLE", and using this will cause any terain the plant is wrown on to turn into dirt once the plant is planted, unless furniture is used.
					 // Using any other tag will not turn the terrain into dirt.
}
```

### Brewing Data

Every item type can have optional brewing data, if the item has brewing data, it can be placed in a
vat and will ferment into a different item type.

Currently only vats can only accept and produce liquid items.

```json
"brewable" : {
    "time": 3600, // A time duration: how long the fermentation will take.
    "result": "beer" // The id of the result of the fermentation.
}
```

### Relic Data

Defines various (semi-)magical properties of items. See RELICS.md for

### Artifact Data

Artifacts are getting deprecated in favor of relic, avoid using them.

#### `Effects_carried`

(optional, default: empty list)

Effects of the artifact when it's in the inventory (main inventory, wielded, or worn) of the player.

Possible values (see src/enums.h for an up-to-date list):

- `AEP_STR_UP` Strength + 4
- `AEP_DEX_UP` Dexterity + 4
- `AEP_PER_UP` Perception + 4
- `AEP_INT_UP` Intelligence + 4
- `AEP_ALL_UP` All stats + 2
- `AEP_SPEED_UP` +20 speed
- `AEP_IODINE` Reduces radiation
- `AEP_SNAKES` Summons friendly snakes when you're hit
- `AEP_INVISIBLE` Makes you invisible
- `AEP_CLAIRVOYANCE` See through walls
- `AEP_SUPER_CLAIRVOYANCE` See through walls to a great distance
- `AEP_STEALTH` Your steps are quieted
- `AEP_EXTINGUISH` May extinguish nearby flames
- `AEP_GLOW` Four-tile light source
- `AEP_PSYSHIELD` Protection from fear paralyze attack
- `AEP_RESIST_ELECTRICITY` Protection from electricity
- `AEP_CARRY_MORE` Increases carrying capacity by 200
- `AEP_SAP_LIFE` Killing non-zombie monsters may heal you
- `AEP_HUNGER` Increases hunger
- `AEP_THIRST` Increases thirst
- `AEP_SMOKE` Emits smoke occasionally
- `AEP_EVIL` Addiction to the power
- `AEP_SCHIZO` Mimicks schizophrenia
- `AEP_RADIOACTIVE` Increases your radiation
- `AEP_MUTAGENIC` Mutates you slowly
- `AEP_ATTENTION` Draws netherworld attention slowly
- `AEP_STR_DOWN` Strength - 3
- `AEP_DEX_DOWN` Dex - 3
- `AEP_PER_DOWN` Per - 3
- `AEP_INT_DOWN` Int - 3
- `AEP_ALL_DOWN` All stats - 2
- `AEP_SPEED_DOWN` -20 speed
- `AEP_FORCE_TELEPORT` Occasionally force a teleport
- `AEP_MOVEMENT_NOISE` Makes noise when you move
- `AEP_BAD_WEATHER` More likely to experience bad weather
- `AEP_SICK` Decreases health over time

#### `effects_worn`

(optional, default: empty list)

Effects of the artifact when it's worn (it must be an armor item to be worn).

Possible values are the same as for effects_carried.

#### `effects_wielded`

(optional, default: empty list)

Effects of the artifact when it's wielded.

Possible values are the same as for effects_carried.

#### `effects_activated`

(optional, default: empty list)

Effects of the artifact when it's activated (which require it to have a `"use_action": "ARTIFACT"`
and it must have a non-zero max_charges value).

Possible values (see src/artifact.h for an up-to-date list):

- `AEA_STORM` Emits shock fields
- `AEA_FIREBALL` Targeted
- `AEA_ADRENALINE` Adrenaline rush
- `AEA_MAP` Maps the area around you
- `AEA_BLOOD` Shoots blood all over
- `AEA_FATIGUE` Creates interdimensional fatigue
- `AEA_ACIDBALL` Targeted acid
- `AEA_PULSE` Destroys adjacent terrain
- `AEA_HEAL` Heals minor damage
- `AEA_CONFUSED` Confuses all monsters in view
- `AEA_ENTRANCE` Chance to make nearby monsters friendly
- `AEA_BUGS` Chance to summon friendly insects
- `AEA_TELEPORT` Teleports you
- `AEA_LIGHT` Temporary light source
- `AEA_GROWTH` Grow plants, a la triffid queen
- `AEA_HURTALL` Hurts all monsters!
- `AEA_FUN` Temporary morale bonus
- `AEA_SPLIT` Split between good and bad
- `AEA_RADIATION` Spew radioactive gas
- `AEA_PAIN` Increases player pain
- `AEA_MUTATE` Chance of mutation
- `AEA_PARALYZE` You lose several turns
- `AEA_FIRESTORM` Spreads minor fire all around you
- `AEA_ATTENTION` Attention from sub-prime denizens
- `AEA_TELEGLOW` Teleglow disease
- `AEA_NOISE` Loud noise
- `AEA_SCREAM` Noise & morale penalty
- `AEA_DIM` Darkens the sky slowly
- `AEA_FLASH` Flashbang
- `AEA_VOMIT` User vomits
- `AEA_SHADOWS` Summon shadow creatures
- `AEA_STAMINA_EMPTY` Empties most of the player's stamina gauge

### Software Data

Every item type can have software data, it does not have any behavior:

```json
"software_data" : {
    "type": "USELESS", // unused
    "power" : 91 // unused
}
```

### Fuel data

Every item type can have fuel data that determines how much horse power it produces per unit
consumed. Currently, gasses and plasmas cannot really be fuels.

If a fuel has the PERPETUAL flag, engines powered by it never use any fuel. This is primarily
intended for the muscle pseudo-fuel, but mods may take advantage of it to make perpetual motion
machines.

```json
"fuel" : {
    "energy": 34.2,               // battery charges per mL of fuel. batteries have energy 1
                                 // is also MJ/L from https://en.wikipedia.org/wiki/Energy_density
                                 // assumes stacksize 250 per volume 1 (250mL). Multiply
                                 // by 250 / stacksize * volume for other stack sizes and
                                 // volumes
   "pump_terrain": "t_gas_pump", // optional. terrain id for the fuel's pump, if any.
   "explosion_data": {           // optional for fuels that can cause explosions
        "chance_hot": 2,         // 1 in chance_hot of explosion when attacked by HEAT weapons
        "chance_cold": 5,        // 1 in chance_cold of explosion when attacked by other weapons
        "factor": 1.0,           // explosion factor - larger numbers create more powerful explosions
        "fiery": true,           // true for fiery explosions
        "size_factor": 0.1       // size factor - larger numbers make the remaining fuel increase explosion power more
    }
}
```

### Use Actions

The contents of use_action fields can either be a string indicating a built-in function to call when
the item is activated (defined in iuse.cpp), or one of several special definitions that invoke a
more structured function.

```json
"use_action": {
    "type": "transform",  // The type of method, in this case one that transforms the item.
    "target": "gasoline_lantern_on", // The item to transform to.
    "active": true,       // Whether the item is active once transformed.
    "msg": "You turn the lamp on.", // Message to display when activated.
    "need_fire": 1,                 // Whether fire is needed to activate.
    "need_fire_msg": "You need a lighter!", // Message to display if there is no fire.
    "transform_charges": 1,         // Number of charges used by item when it transforms.
    "need_charges": 1,                      // Number of charges the item needs to transform. Just a check, nothing is consumed.
    "need_charges_msg": "The lamp is empty.", // Message to display if there aren't enough charges.
    "need_worn": true,                        // Whether the item needs to be worn to be transformed, is false by default.
    "need_wielding": false,             // Whether the item needs to be wielded to be transformed, false by default.
    "need_dry": false,                  // Whether the item cannot transform while submerged in water, false by default.
    "target_charges" : 3, // Number of charges the transformed item has.
    "rand_target_charges": [10, 15, 25], // Randomize the charges the transformed item has. This example has a 50% chance of rng(10, 15) charges and a 50% chance of rng(15, 25) (The endpoints are included)
    "container" : "jar",  // Container holding the target item.
    "moves" : 500         // Moves required to transform the item in excess of a normal action.
},
"use_action": {
    "type": "explosion", // An item that explodes when it runs out of charges.
    "sound_volume": 0, // Volume of a sound the item makes every turn.
    "sound_msg": "Tick.", // Message describing sound the item makes every turn.
    "no_deactivate_msg": "You've already pulled the %s's pin, try throwing it instead.", // Message to display if the player tries to activate the item, prevents activation from succeeding if defined.
    "explosion": { // Optional: physical explosion data
        // Specified like `"explosion"` field in generic items
    },
    "draw_explosion_radius" : 5, // How large to draw the radius of the explosion.
    "draw_explosion_color" : "ltblue", // The color to use when drawing the explosion.
    "do_flashbang" : true, // Whether to do the flashbang effect.
    "flashbang_player_immune" : true, // Whether the player is immune to the flashbang effect.
    "fields_radius": 3, // The radius of spread for fields produced.
    "fields_type": "fd_tear_gas", // The type of fields produced.
    "fields_min_density": 3,
    "fields_max_density": 3,
    "emp_blast_radius": 4,
    "scrambler_blast_radius": 4
},
"use_action": {
    "type": "change_scent", // Change the scent type of the user.
    "scent_typeid": "sc_fetid", // The scenttype_id of the new scent.
    "charges_to_use": 2, // Charges consumed when the item is used.  (Default: 1)
    "scent_mod": 150, // Modifier added to the scent intensity.  (Default: 0)
    "duration": "6 m", // How long does the effect last.
    "effects": [ { "id": "fetid_goop", "duration": 360, "bp": "TORSO" } ], // List of effects with their id, duration, and bodyparts
    "waterproof": true, // Is the effect waterproof.  (Default: false)
    "moves": 500 // Number of moves required in the process.
},
"use_action": {
    "type": "unfold_vehicle", // Transforms the item into a vehicle.
    "vehicle_name": "bicycle", // Vehicle name to create.
    "unfold_msg": "You painstakingly unfold the bicycle and make it ready to ride.", // Message to display when transforming.
    "moves": 500 // Number of moves required in the process.
},
"use_action" : {
    "type" : "consume_drug", // A drug the player can consume.
    "activation_message" : "You smoke your crack rocks.  Mother would be proud.", // Message, ayup.
    "effects" : { "high": 15 }, // Effects and their duration.
    "stat_adjustments": {"hunger" : -10}, // Adjustment to make to player stats.
    "fields_produced" : {"cracksmoke" : 2}, // Fields to produce, mostly used for smoke.
    "charges_needed" : { "fire" : 1 }, // Charges to use in the process of consuming the drug.
    "tools_needed" : { "apparatus" : -1 }, // Tool needed to use the drug.
    "fake_item" : "fake_ecig", // Item used to fake addiction and copy other consumption effects from.
    "lightweight_mod" : 1.2, // Drug duration modifier if the user has LIGHTWEIGHT trait.
    "tolerance_mod" : .8, // Drug duration modifier if the user has a tolerance.
    "tolerance_lightweight_effected": true, // Should this drugs time duration be effected by tolerance/lightweight?
    "too_much_threshold": 10, // Number of minutes to use for the too much calculation.
    "addiction_type_too_much": [["cig", "nicotine"]], // Link an effect to an addiction. Used for 'you feel gross... too much' message
    "lit_item": "cigar_lit", // Defines an item to turn into for smoking_duration minutes.
    "smoking_duration": 12, // Number of minutes the lit_item will remain lit, afterwards it will turn into the extinguished item.
    "used_up_item": "", // Item to give once used, alternative to lit_item which should be used in tandem with smoking_duration.
    "do_weed_msg": false, // Should weed_msg(player) be called after using the item?
    "snippet_category": "killer_withdrawal", // Chance (1 in 5) to say a random snippet from this category upon use. Like do_weed_msg, but unhardcoded.
    "snippet_chance": 5, // Chance (1 in snippet_chance) to make the player think a snippet.
    "moves": 50 // Number of moves required in the process, default value is 100.
},
"use_action": {
    "type": "place_monster", // place a turret / manhack / whatever monster on the map
    "monster_id": "mon_manhack", // monster id, see monsters.json
    "difficulty": 4, // difficulty for programming it (manhacks have 4, turrets 6, ...)
    "hostile_msg": "It's hostile!", // (optional) message when programming the monster failed and it's hostile.
    "friendly_msg": "Good!", // (optional) message when the monster is programmed properly and it's friendly.
    "place_randomly": true, // if true: places the monster randomly around the player, if false: let the player decide where to put it (default: false)
    "skill1": "throw", // Id of a skill, higher skill level means more likely to place a friendly monster.
    "skill2": "unarmed", // Another id, just like the skill1. Both entries are optional.
    "moves": 60 // how many move points the action takes.
},
"use_action": {
    "type": "place_npc", // place npc of specific class on the map
    "npc_class_id": "true_foodperson", // npc class id, see npcs/classes.json
    "summon_msg": "You summon a food hero!", // (optional) message when summoning the npc.
    "place_randomly": true, // if true: places npc randomly around the player, if false: let the player decide where to put it (default: false)
    "moves": 50 // how many move points the action takes.
},
"use_action": {
    "type": "ups_based_armor", // Armor that can be activated and uses power from an UPS, needs additional json code to work
    "activate_msg": "You activate your foo.", // Message when the player activates the item.
    "deactive_msg": "You deactivate your foo.", // Message when the player deactivates the item.
    "out_of_power_msg": "Your foo runs out of power and deactivates itself." // Message when the UPS runs out of power and the item is deactivated automatically.
}
"use_action" : {
    "type" : "delayed_transform", // Like transform, but it will only transform when the item has a certain age
    "transform_age" : 600, // The minimal age of the item. Items that are younger wont transform. In turns (60 turns = 1 minute)
    "not_ready_msg" : "The yeast has not been done The yeast isn't done culturing yet." // A message, shown when the item is not old enough
},
"use_action": {
    "type": "firestarter", // Start a fire, like with a lighter.
    "moves_cost": 15 // Number of moves it takes to start the fire.
},
"use_action": {
    "type": "unpack", // unpack this item
    "group": "gobag_contents", // itemgroup this unpacks into
    "items_fit": true, // Do the armor items in this fit? Defaults to false.
},
"use_action": {
    "type": "extended_firestarter", // Start a fire (like with magnifying glasses or a fire drill). This action can take many turns, not just some moves like firestarter, it can also be canceled (firestarter can't).
    "need_sunlight": true // Whether the character needs to be in direct sunlight, e.g. to use magnifying glasses.
},
"use_action": {
    "type": "inscribe", // Inscribe a message on an item or on the ground.
    "on_items": true, // Whether the item can inscribe on an item.
    "on_terrain": false, // Whether the item can inscribe on the ground.
    "material_restricted": true, // Whether the item can only inscribe on certain item materials. Not used when inscribing on the ground.
    "material_whitelist": [ // List of material ids (not item ids!) that can be inscribed on.
        "wood",             // Only used when inscribing on an item, and only when material_restricted is true.
        "plastic",          // The list here is the default that is used when no explicit list is given.
        "glass",
        "chitin",
        "iron",
        "steel",
        "silver"
    ]
},
"use_action": {
    "type": "cauterize", // Cauterize the character.
    "flame": true // If true, the character needs 4 charges of fire (e.g. from a lighter) to do this action, if false, the charges of the item itself are used.
},
"use_action": {
    "type": "enzlave" // Make a zlave.
},
"use_action": {
    "type": "fireweapon_off", // Activate a fire based weapon.
    "target_id": "firemachete_on", // The item type to transform this item into.
    "success_message": "Your No. 9 glows!", // A message that is shows if the action succeeds.
    "failure_message": "", // A message that is shown if the action fails, for whatever reason. (Optional, if not given, no message will be printed.)
    "lacks_fuel_message": "Out of fuel", // Message that is shown if the item has no charges.
    "noise": 0, // The noise it makes to active the item, Optional, 0 means no sound at all.
    "moves": 0, // The number of moves it takes the character to even try this action (independent of the result).
    "success_chance": 0 // How likely it is to succeed the action. Default is to always succeed. Try numbers in the range of 0-10.
},
"use_action": {
    "type": "fireweapon_on", // Function for active (burning) fire based weapons.
    "noise_chance": 1, // The chance (one in X) that the item will make a noise, rolled on each turn.
    "noise": 0, // The sound volume it makes, if it makes a noise at all. If 0, no sound is made, but the noise message is still printed.
    "noise_message": "Your No. 9 hisses.", // The message / sound description (if noise is > 0), that appears when the item makes a sound.
    "voluntary_extinguish_message": "Your No. 9 goes dark.", // Message that appears when the item is turned of by player.
    "charges_extinguish_message": "Out of ammo!", // Message that appears when the item runs out of charges.
    "water_extinguish_message": "Your No. 9 hisses in the water and goes out.", // Message that appears if the character walks into water and the fire of the item is extinguished.
    "auto_extinguish_chance": 0, // If > 0, this is the (one in X) chance that the item goes out on its own.
    "auto_extinguish_message": "Your No. 9 cuts out!" // Message that appears if the item goes out on its own (only required if auto_extinguish_chance is > 0).
},
"use_action": {
    "type": "musical_instrument", // The character plays an instrument (this item) while walking around.
    "speed_penalty": 10, // This is subtracted from the characters speed.
    "volume": 12, // Volume of the sound of the instrument.
    "fun": -5, // Together with fun_bonus, this defines how much morale the character gets from playing the instrument. They get `fun + fun_bonus * <character-perception>` morale points out of it. Both values and the result may be negative.
    "fun_bonus": 2,
    "description_frequency": 20, // Once every Nth turn, a randomly chosen description (from the that array) is displayed.
    "player_descriptions": [
        "You play a little tune on your flute.",
        "You play a beautiful piece on your flute.",
        "You play a piece on your flute that sounds harmonious with nature."
    ]
},
"use_action": {
    "type": "holster", // Holster or draw a weapon
    "holster_prompt": "Holster item", // Prompt to use when selecting an item
    "holster_msg": "You holster your %s", // Message to show when holstering an item
    "max_volume": "1500 ml", // Maximum volume of each item that can be holstered. Volume in ml and L can be used - "50 ml" or "2 L".
    "min_volume": "750 ml",  // Minimum volume of each item that can be holstered or 1/3 max_volume if unspecified. volume in ml and L can be used - "50 ml" or "2 L".
    "max_weight": 2000, // Maximum weight of each item. If unspecified no weight limit is imposed
    "multi": 1, // Total number of items that holster can contain
    "draw_cost": 10, // Base move cost per unit volume when wielding the contained item
    "skills": ["pistol", "shotgun"], // Guns using any of these skills can be holstered
    "flags": ["SHEATH_KNIFE", "SHEATH_SWORD"] // Items with any of these flags set can be holstered
},
"use_action": {
    "type": "bandolier", // Store ammo and later reload using it
    "capacity": 10, // Total number of rounds that can be stored
    "ammo": [ "shot", "9mm" ], // What types of ammo can be stored?
},
"use_action": {
    "type": "reveal_map", // reveal specific terrains on the overmap
    "radius": 180, // radius around the player where things are revealed. A single overmap is 180x180 tiles.
    "terrain": ["s_gun", "hiway", "road"], // ids of overmap terrain types that should be revealed (as many as you want).
    "terrain_view": ["s_gun"], // (optional) ids of overmap terrain types that should be shown on a prompt after using the map (as many as you want, ideally a subset of terrain).
    "terrain_view_exclude": ["hiway", "road"], // (optional) ids of overmap terrain types that should NOT be shown on the prompt after using the map (as many as you want, ideally a subset of terrain_view).
    "message": "You add roads and tourist attractions to your map." // Displayed after the revelation.
},
"use_action": {
    "type" : "heal",        // Heal damage, possibly some statuses
    "limb_power" : 10,      // How much hp to restore when healing limbs? Mandatory value
    "head_power" : 7,       // How much hp to restore when healing head? If unset, defaults to 0.8 * limb_power.
    "torso_power" : 15,     // How much hp to restore when healing torso? If unset, defaults to 1.5 * limb_power.
    "bleed" : 0.4,          // Chance to remove bleed effect.
    "bite" : 0.95,          // Chance to remove bite effect.
    "infect" : 0.1,         // Chance to remove infected effect.
    "move_cost" : 250,      // Cost in moves to use the item.
    "long_action" : true,   // Is using this item a long action. Setting this to true will divide move cost by (first aid skill + 1).
    "limb_scaling" : 1.2,   // How much extra limb hp should be healed per first aid level. Defaults to 0.25 * limb_power.
    "head_scaling" : 1.0,   // How much extra limb hp should be healed per first aid level. Defaults to (limb_scaling / limb_power) * head_power.
    "torso_scaling" : 2.0,  // How much extra limb hp should be healed per first aid level. Defaults to (limb_scaling / limb_power) * torso_power.
    "effects" : [           // Effects to apply to patient on finished healing. Same syntax as in consume_drug effects.
        { "id" : "pkill1", "duration" : 120 }
    ],
    "used_up_item" : "rag_bloody" // Item produced on successful healing. If the healing item is a tool, it is turned into the new type. Otherwise a new item is produced.
},
"use_action": {
    "type": "place_trap", // places a trap
    "allow_underwater": false, // (optional) allow placing this trap when the player character is underwater
    "allow_under_player": false, // (optional) allow placing the trap on the same square as the player character (e.g. for benign traps)
    "needs_solid_neighbor": false, // (optional) trap must be placed between two solid tiles (e.g. for tripwire).
    "needs_neighbor_terrain": "t_tree", // (optional, default is empty) if non-empty: a terrain id, the trap must be placed adjacent to that terrain.
    "outer_layer_trap": "tr_blade", // (optional, default is empty) if non-empty: a trap id, makes the game place a 3x3 field of traps. The center trap is the one defined by "trap", the 8 surrounding traps are defined by this (e.g. tr_blade for blade traps).
    "bury_question": "", // (optional) if non-empty: a question that will be asked if the player character has a shoveling tool and the target location is diggable. It allows to place a buried trap. If the player answers the question (e.g. "Bury the X-trap?") with yes, the data from the "bury" object will be used.
    "bury": { // If the bury_question was answered with yes, data from this entry will be used instead of outer data.
         // This json object should contain "trap", "done_message", "practice" and (optional) "moves", with the same meaning as below.
    },
    "trap": "tr_engine", // The trap to place.
    "done_message": "Place the beartrap on the %s.", // The message that appears after the trap has been placed. %s is replaced with the terrain name of the place where the trap has been put.
    "practice": 4, // How much practice to the "traps" skill placing the trap gives.
    "moves": 10 // (optional, default is 100): the move points that are used by placing the trap.
},
"use_action": {
  {
    "type": "repair_item",                // Repair items. Skill, tool quality, and dexterity is checked against how hard that item is to craft/dissemble (which may be modified with repairs_like).
    "item_action_type": "repair_fabric",  // Points to an item_action JSON entry that determines the action's name in the use menu. Vanilla examples include repair_fabric and repair_metal.
    "materials": [                        // What materials can be repaired by this item. Materials.json defines what item is consumed when repairing items of that material.
      "cotton",
      "leather",
      "nylon",
      "wool",
      "fur",
      "faux_fur",
      "nomex",
      "kevlar",
      "neoprene",
      "gutskin"
    ],
    "skill": "tailor",    // What skill determines chance of success vs. risk of damaging the item further.
    "tool_quality": 3,    // Bonus from tool, 1.<X> times multiplier on skill.  With 8 Dex, 10 skill plus 2 or more tool_quality allows any item in the game to be fully reinforced.
    "cost_scaling": 0.1,  // Reduces or increases how much raw material is needed per successful repair action, also affected by the item's volume.
    "move_cost": 800      // How long between each roll for success or failure, 100 moves is 1 turn.
  }
},
"use_action": {
    "type": "sew_advanced",  // Modify clothing
    "materials": [           // materials to deal with.
        "cotton",
        "leather"
    ],
    "skill": "tailor",       // Skill used.
    "clothing_mods": [       // Clothing mods to deal with.
        "leather_padded",
        "kevlar_padded"
    ]
},
"use_action": {
    "type": "multicooker",  // Crafts an item over time without requiring the player to sit there and watch
    "do_hallu": false, //Enables multicooker hallucinations and spawning of the evil multicooker when interacting
    "charges_to_start": 50, // Static cost per item crafted
    "charges_per_minute": 1, // Cost per minute of craft time
    "time_mult": 1, //Multiplier for craft time, 2 is twice as long, 0.5 is half as long, 0 is instant
    "recipes": [ "water_clean" ], //Any specific item itype in this list can be crafted
    "subcategories": [ "CSC_FOOD_MEAT", "CSC_FOOD_VEGGI", "CSC_FOOD_PASTA" ], //Any item in these subcategories can be crafted
    "temporary_tools": [ "hotplate", "tongs", "toolset", "pot" ] //Temporary items spawned when crafting to allow for innate qualities
},
"use_action": { 
    "type": "cast_spell",       // Casts a spell based on ID using item charges or, if the flag USE_PLAYER_ENERGY is on the item, the energy source defined in the spell
    "spell_id": "magus_escape", // The ID of the spell to be casted
    "no_fail": true,            // Whether you can fail the cast
    "level": 10,                // The level its cast at
    "need_worn": true,           // if you need to wear it to cast the spell
    "need_wielding": true       // if you need to wield it to cast the spell
}
```

### Random Descriptions

Any item with a "snippet_category" entry will have random descriptions, based on that snippet
category:

```json
"snippet_category": "newspaper",
```

The item descriptions are taken from snippets, which can be specified like this (the value of
category must match the snippet_category in the item definition):

```json
{
  "type": "snippet",
  "category": "newspaper",
  "id": "snippet-id", // id is optional, it's used when the snippet is referenced in the item list of professions
  "text": "your flavor text"
}
```

or several snippets at once:

```json
{
  "type": "snippet",
  "category": "newspaper",
  "text": [
    "your flavor text",
    "more flavor",
    // entries can also bo of this form to have a id to reference that specific snippet.
    { "id": "snippet-id", "text": "another flavor text" }
  ],
  "text": ["your flavor text", "another flavor text", "more flavor"]
}
```

Multiple snippets for the same category are possible and actually recommended. The game will select
a random one for each item of that type.

One can also put the snippets directly in the item definition:

```json
"snippet_category": [ "text 1", "text 2", "text 3" ],
```

This will automatically create a snippet category specific to that item and populate that category
with the given snippets. The format also support snippet ids like above.

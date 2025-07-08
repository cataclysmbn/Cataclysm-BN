---
title: Professions
---

:::danger
This article was recently split off from `JSON INFO`, and needs to be updated due to referring to ancient methods of mods extending and deleting base game professions.
:::

### Profession item substitution

Defines item replacements that are applied to the starting items based upon the starting traits.
This allows for example to replace wool items with non-wool items when the characters starts with
the wool allergy trait.

If the JSON objects contains a "item" member, it defines a replacement for the given item, like
this:

```json
{
  "type": "profession_item_substitutions",
  "item": "sunglasses",
  "sub": [
    { "present": [ "HYPEROPIC" ], "new": [ "fitover_sunglasses" ] },
    { "present": [ "MYOPIC" ], "new": [ { "fitover_sunglasses", "ratio": 2 } ] }
  ]
}
```

This defines each item of type "sunglasses" shall be replaced with:

- an item "fitover_sunglasses" if the character has the "HYPEROPIC" trait,
- two items "fitover_sunglasses" if the character has the "MYOPIC" trait.

If the JSON objects contains a "trait" member, it defines a replacement for multiple items that
applies when the character has the given trait:

````json
{
  "type": "profession_item_substitutions",
  "trait": "WOOLALLERGY",
  "sub": [
    { "item": "blazer", "new": [ "jacket_leather_red" ] },
    { "item": "hat_hunting", "new": [ { "item": "hat_cotton", "ratio": 2 } ] }
  ]
}
```json
This defines characters with the WOOLALLERGY trait get some items replaced:
- "blazer" is converted into "jacket_leather_red",
- each "hat_hunting" is converted into *two* "hat_cotton" items.

### Professions

Professions are specified as JSON object with "type" member set to "profession":

```json
{
    "type": "profession",
    "id": "hunter",
    ...
}
````

The id member should be the unique id of the profession.

The following properties (mandatory, except if noted otherwise) are supported:

#### `description`

(string)

The in-game description.

#### `name`

(string or object with members "male" and "female")

The in-game name, either one gender-neutral string, or an object with gender specific names.
Example:

```json
"name": {
    "male": "Groom",
    "female": "Bride"
}
```

#### `points`

(integer)

Point cost of profession. Positive values cost points and negative values grant points.

#### `addictions`

(optional, array of addictions)

List of starting addictions. Each entry in the list should be an object with the following members:

- "type": the string id of the addiction (see JSON_FLAGS.md),
- "intensity": intensity (integer) of the addiction.

Example:

```json
"addictions": [
    { "type": "nicotine", "intensity": 10 }
]
```

Mods can modify this list (requires `"edit-mode": "modify"`, see example) via "add:addictions" and
"remove:addictions", removing requires only the addiction type. Example:

```json
{
  "type": "profession",
  "id": "hunter",
  "edit-mode": "modify",
  "remove:addictions": [
    "nicotine"
  ],
  "add:addictions": [
    { "type": "alcohol", "intensity": 10 }
  ]
}
```

#### `skills`

(optional, array of skill levels)

List of starting skills. Each entry in the list should be an object with the following members:

- "name": the string id of the skill (see skills.json),
- "level": level (integer) of the skill. This is added to the skill level that can be chosen in the
  character creation.

Example:

```json
"skills": [
    { "name": "archery", "level": 2 }
]
```

Mods can modify this list (requires `"edit-mode": "modify"`, see example) via "add:skills" and
"remove:skills", removing requires only the skill id. Example:

```json
{
  "type": "profession",
  "id": "hunter",
  "edit-mode": "modify",
  "remove:skills": [
    "archery"
  ],
  "add:skills": [
    { "name": "computer", "level": 2 }
  ]
}
```

#### `items`

(optional, object with optional members "both", "male" and "female")

Items the player starts with when selecting this profession. One can specify different items based
on the gender of the character. Each lists of items should be an array of items ids, or pairs of
item ids and snippet ids. Item ids may appear multiple times, in which case the item is created
multiple times. The syntax for each of the three lists is identical.

Example:

```json
"items": {
    "both": [
        "pants",
        "rock",
        "rock",
        ["tshirt_text", "allyourbase"],
        "socks"
    ],
    "male": [
        "briefs"
    ],
    "female": [
        "panties"
    ]
}
```

This gives the player pants, two rocks, a t-shirt with the snippet id "allyourbase" (giving it a
special description), socks and (depending on the gender) briefs or panties.

Mods can modify the lists of existing professions. This requires the "edit-mode" member with value
"modify" (see example). Adding items to the lists can be done with via "add:both" / "add:male" /
"add:female". It allows the same content (it allows adding items with snippet ids). Removing items
is done via "remove:both" / "remove:male" / "remove:female", which may only contain items ids.

Example for mods:

```json
{
  "type": "profession",
  "id": "hunter",
  "edit-mode": "modify",
  "items": {
    "remove:both": [
      "rock",
      "tshirt_text"
    ],
    "add:both": ["2x4"],
    "add:female": [
      ["tshirt_text", "allyourbase"]
    ]
  }
}
```

This mod removes one of the rocks (the other rock is still created), the t-shirt, adds a 2x4 item
and gives female characters a t-shirt with the special snippet id.

#### `pets`

(optional, array of string mtype_ids )

A list of strings, each is the same as a monster id player will start with these as tamed pets.

#### `vehicle`

(optional, string vproto_id )

A string, which is the same as a vehicle ( vproto_id ) player will start with this as a nearby
vehicle. ( it will find the nearest road and place it there, then mark it as "remembered" on the
overmap )

#### `flags`

(optional, array of strings)

A list of flags. TODO: document those flags here.

Mods can modify this via `add:flags` and `remove:flags`.

#### `cbms`

(optional, array of strings)

A list of CBM ids that are implanted in the character.

Mods can modify this via `add:CBMs` and `remove:CBMs`.

#### `traits`

(optional, array of strings)

A list of trait/mutation ids that are applied to the character.

Mods can modify this via `add:traits` and `remove:traits`.

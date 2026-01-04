# Scenarios

Scenarios are specified as JSON object with `type` member set to `scenario`.

```json
{
    "type": "scenario",
    "id": "schools_out",
    ...
}
```

The id member should be the unique id of the scenario.

The following properties (mandatory, except if noted otherwise) are supported:

## `description`

(string)

The in-game description.

## `name`

(string or object with members "male" and "female")

The in-game name, either one gender-neutral string, or an object with gender specific names.
Example:

```json
"name": {
    "male": "Runaway groom",
    "female": "Runaway bride"
}
```

## `points`

(integer)

Point cost of scenario. Positive values cost points and negative values grant points.

## `items`

(optional, object with optional members "both", "male" and "female")

Items the player starts with when selecting this scenario. One can specify different items based on
the gender of the character. Each lists of items should be an array of items ids. Ids may appear
multiple times, in which case the item is created multiple times.

Example:

```json
"items": {
    "both": [
        "pants",
        "rock",
        "rock"
    ],
    "male": [ "briefs" ],
    "female": [ "panties" ]
}
```

This gives the player pants, two rocks and (depending on the gender) briefs or panties.

Mods can modify the lists of an existing scenario via "add:both" / "add:male" / "add:female" and
"remove:both" / "remove:male" / "remove:female".

Example for mods:

```json
{
  "type": "scenario",
  "id": "schools_out",
  "edit-mode": "modify",
  "items": {
    "remove:both": ["rock"],
    "add:female": ["2x4"]
  }
}
```

## `surround_groups`

(optional, array with group and density number)

This replaces the `SUR_START` flag for scenarios, specifies groups of monsters which spawn in the
area surrounding the scenario's staritng location.

```json
"surround_groups": [ [ "GROUP_BLACK_ROAD", 70.0 ] ],
```

The string defines the ID of which monstergroup will be spawned in the surrounding area, while the
number is the density of spawns. 70.0 replicates the value of the original `SUR_START` behavior.

## `flags`

(optional, array of strings)

A list of flags. TODO: document those flags here.

Mods can modify this via "add:flags" and "remove:flags".

## `cbms`

(optional, array of strings)

A list of CBM ids that are implanted in the character.

Mods can modify this via "add:CBMs" and "remove:CBMs".

## `traits", "forced_traits", "forbidden_traits`

(optional, array of strings)

Lists of trait/mutation ids. Traits in "forbidden_traits" are forbidden and can't be selected during
the character creation. Traits in "forced_traits" are automatically added to character. Traits in
"traits" enables them to be chosen, even if they are not starting traits.

Mods can modify this via "add:traits" / "add:forced_traits" / "add:forbidden_traits" and
"remove:traits" / "remove:forced_traits" / "remove:forbidden_traits".

## `bionics", "forced_bionics", "forbidden_bionics`

(optional, array of strings)

Lists of trait/mutation ids. Bionics in "forbidden_bionics" are forbidden and can't be selected during
the character creation. bionics in "forced_bionics" are automatically added to character. Bionics in
"bionics" enables them to be chosen, even if they are not starting bionics.

Mods can modify this via "add:bionics" / "add:forced_bionics" / "add:forbidden_bionics" and
"remove:bionics" / "remove:forced_bionics" / "remove:forbidden_bionics".

## `forbids_bionics`

(optional, bool)

Prohibits players from adding bionics through the bionics chargen tab

## `allowed_locs`

(optional, array of strings)

A list of starting location ids (see start_locations.json) that can be chosen when using this
scenario.

## `start_name`

(string)

The name that is shown for the starting location. This is useful if the scenario allows several
starting locations, but the game can not list them all at once in the scenario description. Example:
if the scenario allows to start somewhere in the wilderness, the starting locations would contain
forest and fields, but its "start_name" may simply be "wilderness".

## `professions`

(optional, array of strings)

A list of allowed professions that can be chosen when using this scenario. The first entry is the
default profession. If this is empty, all professions are allowed.

## `map_special`

(optional, string)

Add a map special to the starting location, see json_flags for the possible specials.

## `missions`

(optional, array of strings)

A list of mission ids that will be started and assigned to the player at the start of the game. Only
missions with the ORIGIN_GAME_START origin are allowed. The last mission in the list will be the
active mission, if multiple missions are assigned.

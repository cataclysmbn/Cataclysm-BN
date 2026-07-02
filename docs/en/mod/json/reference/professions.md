# Professions

## Profession

### Fields

| Identifier        | Description                                                                                                                                                                                                     |
| ----------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| id                | (_mandatory_) Unique id of the profession                                                                                                                                                                       |
| name              | (_mandatory_) Object with `male` and `female` names, or a string for both genders to be set to it                                                                                                               |
| description       | (_mandatory_) Description of the profession, same across both genders                                                                                                                                           |
| vehicle           | (_optional_) Vehicle Prototype Id for the profession to start with                                                                                                                                              |
| pets              | (_optional_) Array of `name` ( monster ids ) and `amount` ( count of monster id ) objects for starting pets                                                                                                     |
| spells            | (_optional_) Array of `id` ( spell ids ) and `level` ( level of spell ) objects for starting spells.                                                                                                            |
| points            | (_mandatory_) Point cost of getting the profession                                                                                                                                                              |
| items             | (_mandatory_) Three different [item groups](/mod/json/reference/items/item_spawn/#format) or arrays of string ids ( technically legacy ). `male` for male only `female` for female only `both` for both genders |
| no_bonus          | (_optional_) Array prevents gaining these items from profession item substitution bonuses                                                                                                                       |
| starting_cash     | (_optional_) Amount of cash the profession starts with                                                                                                                                                          |
| skills            | (_optional_) Array of `level` ( skill level ) and `name` ( skill id ) objects                                                                                                                                   |
| addictions        | (_optional_) Array of `type` ( addiction type ) and `intensity` ( integer addiction intensity ) objects                                                                                                         |
| CBMs              | (_optional_) Array of bionic ids for bionics the profession gains                                                                                                                                               |
| traits            | (_optional_) Array of trait ids for traits the profession gains                                                                                                                                                 |
| forbidden_traits  | (_optional_) Array of trait ids that cannot be chosen on start                                                                                                                                                  |
| allowed_traits    | (_optional_) Array of trait ids for non starting traits which can be chosen on start                                                                                                                            |
| forbidden_bionics | (_optional_) Array of bionic ids that cannot be chosen on start                                                                                                                                                 |
| allowed_bionics   | (_optional_) Array of bionic ids for non starting bionics which can be chosen on start                                                                                                                          |
| forbids_bionics   | (_optional_) Boolean to prevent choosing any bionic on start                                                                                                                                                    |
| flags             | (_optional_) Array of flags which are [profession compatible](/mod/json/reference/json_flags/#scenarios)                                                                                                        |
| missions          | (_optional_) Array of mission ids ( and thus missions ) to start with                                                                                                                                           |
| npcs              | (_optional_) Array of npc class ids ( and thus npcs of that class ) to start with                                                                                                                               |
| age               | (_optional_) Object of `min` and `max` for starting age range, or just an int for only one possible age                                                                                                         |

### Example

```json
{
  "type": "profession",
  "id": "wolfpack_mutant",
  "name": "Mutant Pack Leader",
  "description": "They treated you like an animal in that lab.  Now that you're free, you wonder if they had the right idea.  Your new friends don't seem afraid of you like they are of humans, after all.",
  "vehicle": "covered_wagon",
  "pets": [ { "name": "mon_wolf", "amount": 2 } ],
  "spells": [ { "id": "summon_zombie", "level": 1 }, { "id": "necrotic_gaze", "level": 1 } ],
  "points": 4,
  "items": { "both": [ "subsuit_xl" ], "male": [ "briefs" ], "female": [ "bra", "panties" ] },
  "no_bonus": [ "glasses_eye" ]
  "starting_cash": 0,
  "skills": [ { "level": 1, "name": "survival" } ],
  "addictions": [ { "intensity": 10, "type": "nicotine" } ],
  "CBMs": [ "bio_batteries", "bio_lockpick", "bio_fingerhack", "bio_power_storage_mkII" ],
  "traits": [ "LUPINE_FUR", "TAIL_FLUFFY", "LUPINE_EARS", "THRESH_LUPINE" ],
  "forbidden_traits": [ "CARRY_PERMIT" ],
  "allowed_traits": [ "MUZZLE" ],
  "forbidden_bionics": [ "bio_power_storage" ],
  "allowed_bionics": [ "bio_razors" ],
  "flags": [ "SCEN_ONLY" ],
  "missions": [ "MISSION_HEIST_DRIVER" ],
  "npcs": [ "NC_PROF_ESCAPIST_LAB", "NC_PROF_ESCAPIST_LAB", "NC_PROF_ESCAPIST_LAB" ],
  "age": { "min": 17, "max": 35 }
},
```

## Profession Item Substitutions

### Substitutions

#### Trait Based

```jsonc
{
  "type": "profession_item_substitutions", // Mandatory type
  "trait": "WOOLALLERGY", // Trait that is required
  "sub": [
    {
      "item": "blazer", // Item to replace
      "new": ["jacket_leather_red"], // Items that replace it. One for one ratio
    },
    {
      "item": "hat_hunting", // Item to replace
      "new": [{
        "item": "hat_cotton", // Item that replaces it
        "ratio": 2, // For every item, give 2 to replace
      }],
    },
  ],
}
```

In the above example. With `WOOLALLERGY` trait. `blazer` is replaced with 1 `jacket_leather_red`.
And `hat_hunting` is replaced with 2 `hat_cotton` items.

#### Item based

```jsonc
{
  "type": "profession_item_substitutions", // Mandatory type
  "item": "sunglasses", // Item to replace
  "sub": [
    {
      "present": [ "HYPEROPIC" ], // Must have all of these traits
      "absent": [ "MYOPIC" ], // Cannot have any of these traits
      "new": [ "fitover_sunglasses" ] // Replaced with these
    },
    {
      "present": [ "MYOPIC" ], // Must have all these traits
      "new": [ { "fitover_sunglasses", "ratio": 2 } ] // Also supports ratio replaces
    }
  ]
}
```

Thus if you have `HYPEROPIC` you get one fitover sunglasses
And `MYOPIC` gives 2 fitover sunglasses in all cases even with `HYPEROPIC`

### Bonuses

#### Itemgroup

```jsonc
{
  "type": "profession_item_substitutions", // Needed type
  "item_group": "profession_carry_bonus",  // Itemgroup to give
  "bonus": { "present": [ "CARRY_PERMIT" ] } // Works with both present and absent as above
},
```

This grants a free spawn from itemgroup `profession_carry_bonus` to characters with the `CARRY_PERMIT` trait.

#### Item

```jsonc
{
  "type": "profession_item_substitutions", // Needed type
  "item": "glasses_eye",                   // Item to give
  "bonus": { "present": [ "MYOPIC" ], "absent": [ "HYPEROPIC" ] } // As above present and absent works
},
```

This grants a free item `glasses_eye` to characters with the `MYOPIC` trait, but not `HYPEROPIC`.

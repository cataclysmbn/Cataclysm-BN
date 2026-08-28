# Enchantments

Enchantments make it possible to specify custom effects provided by item, bionic or mutation.

### Fields

#### id

(string) Unique identifier for this enchantment.

#### conditions

(string array) How an enchantment determines if it should be active

All conditions must pass for it to be valid, if there are no conditions it is automatically true

For all basegame values see [here](#Basegame-Enchantment-Condition-ID-List)

#### emitter

(string) Identifier of an emitter that's active as long as this enchantment is active. Default: no
emitter.

#### ench_effects

(array) Grants effects of specified intensity as long as this enchantment is active.

Syntax for single entry:

```json
{
  // (required) Identifier of the effect
  "effect": "effect_identifier",

  // (required) Intensity. Setting to 1 works for effects that do not actually have intensities.
  "intensity": 2
}
```

#### hit_you_effect

(array) List of spells that may be cast when enchantment is active and character melee attacks a
creature.

Syntax for single entry:

```json
{
  // (required) Identifier of the spell
  "id": "spell_identifier",

  // If true, the spell is centered on the character's location.
  // If false, the spell is centered on the attacking creature.
  // Default: false
  "hit_self": false,

  // Chance to trigger, one in X.
  // Default: 1
  "once_in": 1,

  // Message for when the spell is triggered for you.
  // %1$s is your name, %2$s is creature's
  // Default: no message
  "message": "You pierce %2$s with Magic Piercing!",

  // Message for when the spell in triggered for an NPC.
  // %1$s is their name, %2$s is creature's
  // Default: no message
  "npc_message": "%1$s pierces %2$s with Magic Piercing!",

  // TODO: broken?
  "min_level": 1,

  // TODO: broken?
  "max_level": 2
}
```

#### hit_me_effect

(array) List of spells that may be cast when enchantment is active and character gets melee attacked
by a creature.

Same syntax as for `hit_you_effect`.

#### mutations

(array) List of mutations temporarily granted while enchantment is active.

#### intermittent_activation

(object) Rules that specify random effects which occur while enchantment is active.

Syntax:

```json
{
  // List of checks to run on every turn while enchantment is active.
  "effects": [
    {
      // Average activation frequency.
      // The exact chance to pass is "one in (X converted to turns)" per turn.
      "frequency": "5 minutes",

      // List of spells to cast if the check passed.
      "spell_effects": [
        {
          // (required) Identifier of the spell
          "id": "nasty_random_effect",

          // TODO: broken?
          "min_level": 1,

          // TODO: broken?
          "max_level": 5
          // TODO: other fields appear to be loaded, but unused
        }
      ]
    }
  ]
}
```

#### values

(array) List of miscellaneous character/item values to modify.

Syntax for single entry:

```json
{
  // (required) Value ID to modify, refer to list below.
  "value": "VALUE_ID_STRING",

  // Additive bonus. Optional integer number, default is 0.
  // Ignored for:
  // METABOLISM, MANA_REGEN, STAMINA_CAP, STAMINA_REGEN, THIRST, FATIGUE
  "add": 13,

  // Multiplicative bonus. Optional, default is 0.
  "multiply": -0.3
}
```

Additive bonus is applied separately from multiplicative, like so:

```json
bonus = add + base_value * multiply
```

Thus, a `multiply` value of -0.8 is -80%, and a `multiply` of 2.5 is +250%. When modifying integer
values, final bonus is rounded towards 0 (truncated).

When multiple enchantments (e.g. one from an item and one from a bionic) modify the same value,
their bonuses are added together without rounding, then the sum is rounded (if necessary) before
being applied to the base value.

Since there's no limit on number of enchantments the character can have at a time, the final
calculated values have hardcoded bounds to prevent unintended behavior.

For all basegame values see [here](#Basegame-Enchantment-Value-ID-List)

#### Flags

(array) of enchantment_flag_id values

For all basegame values see [here](#Basegame-Enchantment-Flag-ID-List)

#### Immune Effects

(array) of effect_type_id values

Prevents recieving these effects, but any present effects will persist

#### Immune Fields

(array) of field_type_id values

Prevents environmental effects of fields from being applied

### Examples

```json
[
  {
    "//": "On-hit effect for ink glands mutation, implemented via enchantment.",
    "type": "enchantment",
    "id": "MEP_INK_GLAND_SPRAY",
    "hit_me_effect": [
      {
       "id": "generic_blinding_spray_1",
        "hit_self": false,
        "once_in": 15,
        "message": "Your ink glands spray some ink into %2$s's eyes.",
        "npc_message": "%1$s's ink glands spay some ink into %2$s's eyes."
      }
    ]
  },
  {
    "//": "This one would look good on a katana for an anime mod.",
    "type": "enchantment",
    "id": "ENCH_ULTIMATE_ASSKICK",
    "has": "WIELD",
    "condition": "ALWAYS",
    "ench_effects": [{ "effect": "invisibility", "intensity": 1 }],
    "hit_you_effect": [{ "id": "AEA_FIREBALL" }],
    "hit_me_effect": [{ "id": "AEA_HEAL" }],
    "mutations": ["KILLER", "PARKOUR"],
    "values": [{ "value": "STRENGTH", "multiply": 1.1, "add": -5 }],
    "intermittent_activation": {
      "effects": [
        {
          "frequency": "1 hour",
          "spell_effects": [
            { "id": "AEA_ADRENALINE" }
          ]
        }
      ]
    },
    "flags": ["FOOD_POISON_IMMUNE"],
    "immune_fields": ["fd_fire"],
    "immune_effects": ["poison"]
    }
  }
]
```

## Enchantment Values

```jsonc
{
  "id": "CLIMATE_CONTROL", // Id of enchantment
  "type": "enchantment_value", // Needed type
  "can_add": true, // Weather adding to the enchantment value will do anything; Default true
  "can_mult": true, // Weather multiplying to the enchantment value will do anything; Default true
  "can_max": false, // Weather getting the maximum value of this type will do anything; Default false
  "suffixes": [ // All of the suffixes this is quite literally the most complicated part
    [
      { "suffix": "COOLING", "desc_insert": [ "hot", "" ] }, // These show up as `CLIMATE_CONTROL_XXX`
      { "suffix": "HEATING", "desc_insert": [ "cold", "" ] } // Desc insert overwrites the below `desc_insert`, desc can also be replaced
    ],
    [ // Then you can define a second set, there can be as many sets as you want
      { "suffix": "TORSO", "replace": { "desc_insert": { "idx": 1, "val": "Torse" } } }, // Replace replaces based off starting at 0 index, so this replaces "" with "Torso"
      {
        "suffix": "ARM", 
        "replace": { "desc_insert": { "idx": 1, "val": "Arm" } },
        "suffixes": [ // These are a list of further suffix specific suffixes
          { "suffix": "L", "replace": { "desc_insert": { "idx": 1, "val": "Left Arm" } } }, // These suffixes could have additional nested suffixes too
          { "suffix": "R", "replace": { "desc_insert": { "idx": 1, "val": "Right Arm" } } } // These only show up as `CLIMATE_CONTROL_XXX_ARM_XXX`, never will they be applied to another type
        ]
      },
    ] // See below for further explination on why there are multiple sets
  ],
  "desc": "Keeps you comfortable against %1$s temperatures ( %2$s )", // Description, with support for formatting in additional strings
  "desc_insert": [ "all bodyparts", "" ], // The additional strings to format in
  "unsupported_conditions": ["character", "item_and_character"] // These values are called where these conditions cannot ever be used
},
```

### Suffixes Explination

Start by examining the above tree
There are two groups:

- COOLING and HEATING
- TORSO and ARM ( With Arm's children )

Order matters here, because COOLING came first, whenever you need the COOLING specifier, it must be first
I.E.
`CLIMATE_CONTROL_COOLING_ARM` works, but `CLIMATE_CONTROL_ARM_COOLING` is invalid.

The chain produced from the above json is as follows

- CLIMATE_CONTROL
  - CLIMATE_CONTROL_COOLING
    - CLIMATE_CONTROL_COOLING_ARM
      - CLIMATE_CONTROL_COOLING_ARM_L -> References CLIMATE_CONTROL_ARM_L
      - CLIMATE_CONTROL_COOLING_ARM_R -> References CLIMATE_CONTROL_ARM_R
  - CLIMATE_CONTROL_HEATING
    - CLIMATE_CONTROL_HEATING_ARM
      - CLIMATE_CONTROL_HEATING_ARM_L -> References CLIMATE_CONTROL_ARM_L
      - CLIMATE_CONTROL_HEATING_ARM_R -> References CLIMATE_CONTROL_ARM_R
  - CLIMATE_CONTROL_ARM -> Does not reference CLIMATE_CONTROL to prevent stacking effects
    - CLIMATE_CONTROL_ARM_L
    - CLIMATE_CONTROL_ARM_R

This can be done with as many chains as wanted, but it gets increasingly complicated as more chains are added

In the docs, only the chain groups will be referenced, not the full list of enchantment values, due to the increasing number of enchantment children

When referencing these values in c++ or lua you always want to use the most specific value, the lower tiers are autofilled.
I.E. `CLIMATE_CONTROL_COOLING_ARM_R` should be what you reference, never `CLIMATE_CONTROL_ARM_R`, or `CLIMATE_CONTROL_ARM` when getting a value

### Standard suffixes

#### Bodyparts

This is a standard set of suffixes for all bodyparts
Includes

- HEAD
- TORSO
- EYES
- MOUTH
- ARM
  - L
  - R
- LEG
  - L
  - R
- HAND
  - L
  - R
- FOOT
  - L
  - R

#### Skills

Used for all skill levels

- `BARTER`
- `SPEECH`
- `COMPUTER`
- `FIRSTAID`
- `MECHANICS`
- `TRAPS`
- `DRIVING`
- `SWIMMING`
- `FABRICATION`
- `COOKING`
- `TAILOR`
- `SURVIVAL`
- `ELECTRONICS`
- `ARCHERY`
- `GUN`
- `LAUNCHER`
- `PISTOL`
- `RIFLE`
- `SHOTGUN`
- `SMG`
- `THROW`
- `MELEE`
- `BASHING`
- `CUTTING`
- `DODGE`
- `STABBING`
- `UNARMED`

#### Damage Types

### Basegame Enchantment Value ID List

#### Character values

##### STRENGTH

Strength stat. `base_value` here is the base stat value. The final value cannot go below 0.

##### DEXTERITY

Dexterity stat. `base_value` here is the base stat value. The final value cannot go below 0.

##### PERCEPTION

Perception stat. `base_value` here is the base stat value. The final value cannot go below 0.

##### INTELLIGENCE

Intelligence stat. `base_value` here is the base stat value. The final value cannot go below 0.

##### HEALTH_POINTS

Hit points stat. `base_value` here is the base health value, The final value cannot go below 1.

These are the children.

| Set       | Values                                  | Use                                    |
| --------- | --------------------------------------- | -------------------------------------- |
| Bodyparts | TORSO, HEAD, ARM ( L, R ), LEG ( L, R ) | The specific bodypart that is affected |

##### SPEED

Character speed. `base_value` here is character speed including pain/hunger/weight penalties. Final
speed value cannot go below 25% of base speed.

##### ATTACK_COST

Melee attack cost. The lower, the better. `base_value` here is attack cost for given weapon
including modifiers from stats and skills. The final value cannot go below 25.

##### MOVE_COST

Movement cost. `base_value` here is tile movement cost including modifiers from clothing and traits.
The final value cannot go below 20.

##### FLAT_MOVE_COST

Movement cost effect on flat ground. `base_value` here is movement cost partially through processing.
The final value cannot go below 20 like MOVE_COST.
This stacks with MOVE_COST

##### OBSTACLE_MOVE_COST

Movement cost effect on obstacles. `base_value` here is initial move cost.
The final value cannot go below 100.
Stacks with MOVE_COST, done before it.

##### SWIM_MOVE_COST

Movement cost effect while swimming
The fianl value cannot go below 30.
This does NOT stack with MOVE_COST

##### READING_SPEED

Speed of reading books. `base_value` is final reading speed in moves.
The final value cannot go below 1 second.

##### CRAFTING_SPEED

Crafting speed. `base_value` is a multiplier of crafting speed.
Calculated after all other multipliers

##### CONSTRUCTION_SPEED

Construction speed. `base_value` is a multiplier of construction speed for vehicles and furniture/terrain.
Calculated after all other multipliers

| Set               | Values   | Use                                                            |
| ----------------- | -------- | -------------------------------------------------------------- |
| Construction Type | CON, VEH | Weather construction or vehicle construction speed is affected |

##### METABOLISM

Metabolic rate. This modifier ignores `add` field. `base_value` here is `PLAYER_HUNGER_RATE`
modified by traits. The final value cannot go below 0.

##### MANA_CAP

Mana capacity. `base_value` here is character's base mana capacity modified by traits. The final
value cannot go below 0.

##### MANA_REGEN

Mana regeneration rate. This modifier ignores `add` field. `base_value` here is character's base
mana gain rate modified by traits. The final value cannot go below 0.

##### STAMINA_CAP

Stamina capacity. This modifier ignores `add` field. `base_value` here is character's base stamina
capacity modified by traits. The final value cannot go below 10% of `PLAYER_MAX_STAMINA`.

##### STAMINA_REGEN

Stamina regeneration rate. This modifier ignores `add` field. `base_value` here is character's base
stamina gain rate modified by mouth encumbrance. The final value cannot go below 0.

##### THIRST

Thirst gain rate. `base_value` here is character's base thirst
gain rate. The final value cannot go below 0.

##### FATIGUE

Fatigue gain rate. `base_value` here is character's base fatigue
gain rate. The final value cannot go below 0.

##### MENDING_MULT

Edits the multiplier to broken limb mending rate. `base_value` is regen mod post mutations ( default 0.25 )
Final value range is 0.0 to 1.0

##### HEARING

Multiplier to hearing. `base_value` is the final multiplier to hearing.
Final value cannot go below 0.

##### NOISE

Footstep noise value. `base_value` is the post-mutaiton multiplier to noise.
Final value cannot go below 0.

##### SCENT

Scent value. `base_value` is the post-mutation scent value
Final value cannot go below 0

##### STEALTH

Stealth modifier value higher value increases stealth, lower value decreases it. `base_value` is the post mutation value
Clamped between 20 and 160. With 160 being 60% more visible and 20 being 80% less visible

##### MOTION_ALARM

From how far away will you be notified of approaching creatures. `base_value` is always 0.
This is a maxed value for tile range of notification.

##### BODYTEMP_X

Modifier to bodytemp accepted ranges
Appropriate values are
`BODYTEMP_MIN`: minimum temp for comfort
`BODYTEMP_MAX`: maximum temp for comfort

##### BODYTEMP_SLEEP

Additional bodytemp given during sleep
`base_value` is the mutation/previous enchantment values
There is no limit

##### BODYTEMP_SPEED

Additional speed change for COLDBLOOD4 characters
`base_value` is the mutation value or 0
There is no current limit

##### SLEEP_PAIN_THRESHOLD

Additional pain required for being woken up
`base_value` is the base sleep pain value
Minimum value is 1

##### SLEEP_DB_RESIST

Modifier to the amount of noise above environemental required to be woken up
`base_value` is 20
There is no minimum nor maximum value

##### CLIMATE_CONTROL

Moves temperature felt by the player towards a point.
`base_value` is the current temperature felt by the player
It will increase or decrease based off if it is below or above normal temperature ( including mutations )
It has two children:

| Set         | Values                                               | Use                                                                        |
| ----------- | ---------------------------------------------------- | -------------------------------------------------------------------------- |
| Temperature | COOLING, HEATING                                     | Weather it applies increase in heat or decrease in heat towards ideal temp |
| Bodypart    | See general bodypart enchantments [here](#bodyparts) | Bodypart that it applies to                                                |

That would only heat or cool respectively

##### LIE

Modifier to Lie chance `base_value` is post skill effects
Under 0 and above 100 has no change

##### PERSUADE

Same as LIE but for persuasion

##### INTIMIDATE

Same as LIE but for intimidation

##### HEALTHY_MULT

Edit to healthy. `base_value` is 1

##### FALL_DAMAGE_MULT

Modifier to fall damage multiplier, `base_value` is post mutations and other modifiers
Cannot go below 0

##### CARRY_STORAGE

Modifier to carryable storage. `base_value` is current storage in mililiters
Cannot go below 0

##### CARRY_WEIGHT

Modifier to carryable weight. `base_value` is current storage in mililiters
Cannot go below 0

##### WEIGHTMOD

Modifier to the current weight of the player. `base_value` is the respective weight of one of the categories listed below.
Minimum is 0, maximum is uncapped

| Set      | Values                                 | Use                                         |
| -------- | -------------------------------------- | ------------------------------------------- |
| Category | BIONICS, WEAPON, INVENTORY, BODY, WORN | What part of player weight it is applied to |

##### OVERMAP_SIGHT

Modifier to overmap sight. `base_value` is the best mutation value.
Maximum is 3

##### EFFECTIVE_FOCUS

Modifier to focus. `base_value` is current focus
There is no limit

##### MELEE_HIT

Modifier to melee hit rating. `base_value` is the value after all modifiers.
There are no limits.

##### OVERKILL

Modifier to after death damage on a zombie. `base_value` is the current overkill damage
Minimum value of 0.

##### FOOD_FUN

Modifier to food morale. `base_value` is current food morale
There is no limit.

##### VOMIT_MOD

Modifier to chance to vomit. `base_value` is always 1.
Minimum value of 0.

##### PAIN

Modifier to shifts in pain. `base_value` is the pain mod.

| Set  | Values     | Use                                           |
| ---- | ---------- | --------------------------------------------- |
| Type | GAIN, LOSS | Apply specifically to increases or decreases. |

##### PAIN_MOD

Modifier to current amount of pain, constant shift. `base_value` is current pain.
Minimum value of 0

##### CHRONIC_PAIN_MOD

Modifier to pain from the `CHRONIC_PAIN` option. `base_value` is the chronic pain.
Minimum value of 0 after being added to value after `PAIN_MOD`

##### PERCEIVED_PAIN_MOD

Modifier to percieved pain. `base_value` is current pain after `PAIN_MOD`.
Minimum value of 0

| Set  | Values                  | Use                                                                                 |
| ---- | ----------------------- | ----------------------------------------------------------------------------------- |
| Stat | SPD, STR, DEX, INT, PER | Apply only to speed, strength, dexterity, intellegence and perception respectively. |

##### PAIN_PENALTY

Affects the stat penalty from high amounts of pain. `base_value` is the current penalty.
Maximum value of 0 ( no effect )

##### ADDICTION_STRENGTH

Modifier to likelyhood to gain another addiction intensity.
`base_value` is the strength of the added addiction.
There is no limit.

##### ADDICTION_TIME_PER_ADDITION

Modifier to how long addiction applications increase addition time. `base_value` is base time added each time additions are applied in seconds.
There is no limit.

##### ADDICTION_TIME_PER_INTENSITY

Modifier to how long addictions last. `base_value` is time to remove one addiction stack.
Reducing _increases_ addiction time, adding _reduces_ addiction time
There is no limit.

##### UNCANNY_DODGE

0-1 chance to dodge bullets. `base_value` is 0.
Each uncanny dodge takes 100 stamina.
Any values below 0 or above 1 will have the same effect as at 0 and at 1 respectively

##### BONUS_DODGE

Additional dodges per turn before dodge penalty kicks in. `base_value` here is character's base
dodges per turn before penalty (usually 1). The final value can go below 0, which results in penalty
to dodge roll.

##### FORCEFIELD

0-1 chance to block damage of a damage type. `base_value` is 0.

| Set         | Values                                                 | Use                            |
| ----------- | ------------------------------------------------------ | ------------------------------ |
| Damage Type | See general damage type suffixes [here](#damage-types) | Damage Type that it applies to |

##### CROWD_CRUSH_RESIST

Modifier to how likely it is to get crowd crushed `base_value` is 5, it is always 5.
Increasing it reduces the chance to get crowd crushed
From 0 ( no chance to resist ) to 95 ( 5% chance to not resist )

##### BLISTER_COUNT

Effective heat armor modifier to gaining the blister effect. `base_value` is the number of blisters.
The final value can go below 0, which would never blister the character. Or it could go higher and always blister the character.

##### LUMINATION

Lumination around the player when active. You cannot add nor multiply this enchantment, only max value works.
The final value will not go below 0, and the maximum value is uncapped.

##### NIGHT_VISION

Night vision value for the player. `EFFECT_NIGHT_VISION` or `GNV_EFFECT` is 10.0 while `GNVE_EFFECT` is 18.0
Only `max` works, and it will take the highest of enchantment and other night vision effects

##### CLAIRVOYANCE

Clairvoyance value for the player. `CLAIRVOYANCE_SUPER` is 40.0 while `CLAIRVOYANCE_PLUS` is 8.0
And `CLAIRVOYANCE` is 3
Only `max` works, and it will take the highest of enchantment and other clairvoyance effects

##### FLASH_PROTECTION

Flash protection value for the player. Item and effect flags give 3.
Only `max` works, and it will take the highest of enchantment, item and effect effects.

##### GROUNDED_CREATURE_SIGHT

Sight that passes through walls of grounded creatures in the form of infrared.
Number of tiles it works on. Only `max` works.

##### REACH_RANGE

Value that adds reach to all melee attacks ( unarmed or armed ).
`base_value` is always 0. Only `max` works

| Set  | Values         | Use                                     |
| ---- | -------------- | --------------------------------------- |
| Type | UNARMED, ARMED | Apply only to armed or unarmed attacks. |

##### ARMOR_X

Incoming damage modifier. Applied after Active Defense System bionic but before the damage is
absorbed by items. Note that `base_value` here is incoming damage value of corresponding type, so
positive `add` and greater than 1 `mul` will **increase** damage received by the character.

| Set         | Values                                                 | Use                            |
| ----------- | ------------------------------------------------------ | ------------------------------ |
| Damage Type | See general damage type suffixes [here](#damage-types) | Damage Type that it applies to |

##### SKILL_LEVEL

Character wide skill level modifier.
`base_value` is the current skill level of the player

| Set        | Values                                     | Use                      |
| ---------- | ------------------------------------------ | ------------------------ |
| Skill Type | See general skill suffixes [here](#skills) | Skill that it applies to |

##### SKILL_EXP

Character wide skill exp gain modifier.
`base_value` is the exp gained by whatever is being done
Warning: this value can only be multiplied, not added

| Set        | Values                                     | Use                      |
| ---------- | ------------------------------------------ | ------------------------ |
| Skill Type | See general skill suffixes [here](#skills) | Skill that it applies to |

##### Encumbrance

Character wide encumbrance modifier, children modify certain bodyparts.

| Set      | Values                                               | Use                         |
| -------- | ---------------------------------------------------- | --------------------------- |
| Bodypart | See general bodypart enchantments [here](#bodyparts) | Bodypart that it applies to |

##### MELEE_DAMAGE

Character wide melee damage modifier ( reach attacks included ), children modify only certain damage types.

| Set         | Values                                                 | Use                            |
| ----------- | ------------------------------------------------------ | ------------------------------ |
| Damage Type | See general damage type suffixes [here](#damage-types) | Damage Type that it applies to |

##### MELEE_ARMOR_PENETRATION

Character wide melee armor penetration modifier. Children modify certain damage types.

| Set         | Values                                                 | Use                            |
| ----------- | ------------------------------------------------------ | ------------------------------ |
| Damage Type | See general damage type suffixes [here](#damage-types) | Damage Type that it applies to |

#### Item values

##### ITEM_ATTACK_COST

Attack cost (melee or throwing) for this item. Ignores condition / location, and is always active.
`base_value` here is base item attack cost. Note that the final value cannot go below 0.

##### ITEM_DAMAGE_X

Melee damage of this item. Ignores condition / location, and is always active. `base_value` here is
base item damage of corresponding type. Note that the final value cannot go below 0.
There is the global damage modifier `ITEM_DAMAGE` in addition to the supported damage types:

| Set         | Values                                                 | Use                            |
| ----------- | ------------------------------------------------------ | ------------------------------ |
| Damage Type | See general damage type suffixes [here](#damage-types) | Damage Type that it applies to |

##### ITEM_ARMOR_PENETRATION_X

Armor penetration of this item. `base_value` here is base armor penetration of corresponding type.
Note that the final value cannot go below 0.
There is the global modifier `ITEM_ARMOR_PENTRATION` in addition to the supported damage types:

| Set         | Values                                                 | Use                            |
| ----------- | ------------------------------------------------------ | ------------------------------ |
| Damage Type | See general damage type suffixes [here](#damage-types) | Damage Type that it applies to |

##### ITEM_ARMOR_X

Incoming damage modifier for this item, applied before the damage is absorbed by the item. Note that
`base_value` here is incoming damage value of corresponding type, so positive `add` and greater than
1 `mul` will **increase** damage received by the character. Each damage type has its own enchant
value, in addition to the global `ITEM_ARMOR`:

| Set         | Values                                                 | Use                            |
| ----------- | ------------------------------------------------------ | ------------------------------ |
| Damage Type | See general damage type suffixes [here](#damage-types) | Damage Type that it applies to |

##### ITEM_REACH_RANGE

Modifier to item reach range, `max` value between this and any item flags.

## Enchantment Flag

```jsonc
{
  "id": "NEARSIGHTED",               // Id of the enchantment flag
  "type": "enchantment_flag",        // Needed type
  "parents": [ "BLIND" ],            // Array of other enchantment_flags it also gives
  "conflicts": [ "FIX_NEARSIGHTED" ] // Array of other enchantment_flags of which it cancels
  "info": "<bad>Causes nearsightedness</bad>" // Info string showed in enchantment info
},
```

All noted effects apply to the character in possession of the enchantment granting thing

### Basegame Enchantment Flag ID List

#### Sight

##### UNDERWATER_SIGHT

Makes sight underwater uninhibited

##### SLEEP_SIGHT

Allows sight while sleeping

##### NEARSIGHTED

Restricts vision greatly, solved by some glasses

##### FIX_NEARSIGHTED

Conflict to NEARSIGHTED, cures and removes it

##### BLIND

Prevents seeing any tile, bumping into walls does reveal them

##### FIX_BLIND

Conflict to BLIND, cures and removes it

##### INFRARED_VISION

Gain infrared vision

##### ELECTROSENSE

Can see robots and electrical creatures through walls

##### SONAR

Can see burrowing creatures with the INFRARED_VISION sprite

##### ANTIGLARE

Prevents glare effects from sunlight and such

#### Consumption

##### EAT_ROTTEN

Gives the ability to eat rotten food safely.

##### ONLY_EAT_ROTTEN

Gives significant penalty to eating fresh food, still allows drinking fresh liquids

##### EAT_ROTTEN_MORALE

Gives no morale penalty to eating rotten food

##### CONSUME_UNCLEAN

Gives the ability to drink unclean liquids and eat unclean foods

##### FOOD_PARASITE_IMMUNE

Prevents gaining parasites from consuming food

##### FOOD_POISON_IMMUNE

Prevents gaining poison from consuming food

#### Miscellaneous

##### ALARMCLOCK

Gives the ability to set an alarm while sleeping

##### INTENAL_ALARMCLOCK

Has the effects of `ALARMCLOCK`, but does not produce sound
It also should prevent sleeping through it.

##### VIEW_DRONE_CAM

Allows viewing any creature with `effect_drone_marker`, generally applied by `PHOTOGRAPH` robots

##### RADIO

Gives the effects of having a radio

##### THERMOMETER

Gives the effects of having a themometer

##### WATCH

Gives the ability to see the precise time

##### FIRE_FIELD_IMMUNE

Provides immunity to fire fields.

##### SILENT

Prevents player sound from movement

##### NO_THERMAL_WAKE

Extreme temperatures will not wake up the player

##### NO_DAMAGE_WAKE

Taking damage will not wake up the player

##### NO_LIGHT_WAKE

Lights will not wake up the player

## Enchantment Condition

```jsonc
{
  "id": "WORN", // Id of condition
  "type": "enchantment_condition", // Mandatory Type
  "condition_type": "item_and_character", // Type of condition, `global`, `item`, `character` and `item_and_character` are possible values
  "condition_function": "worn", // What function to use, generally references a hardcode or lua function
  "condition_info": "While worn", // Enchantment condition info to display on items
}
```

### Basegame Enchantment Condition ID List

#### Item and Character

##### HELD

When in your inventory

##### WIELD

When wielded in your hand

##### WORN

When worn as armor

#### Global

##### ALWAYS

Always active ( Obsolete but supported: Comes out to be true, thus no condition is needed )

##### NIGHT

When it is night time

##### DUSK

When it is dusk

##### DAY

When it is day time

##### DAWN

When it is dawn

##### ACTIVE

Whenever the item, mutation, bionic, or whatever the enchantment is attached to is active.

##### INACTIVE

The opposite of ACTIVE

#### Character

##### INSIDE

When the owner of the item is inside

##### OUTSIDE

When the owner of the item is outside

##### UNDERGROUND

When the owner of the item is below Z-level 0

##### ABOVEGROUND

When the owner of the item is at or above Z-level 0

##### UNDERWATER

When the owner is in swimmable terrain

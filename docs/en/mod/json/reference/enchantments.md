# Enchantments

Enchantments make it possible to specify custom effects provided by item, bionic or mutation.

## Fields

### id

(string) Unique identifier for this enchantment.

### has

(string) How an enchantment determines if it is in the right location in order to qualify for being
active.

This field is relevant only for items.

Values:

- `HELD` (default) - when in your inventory
- `WIELD` - when wielded in your hand
- `WORN` - when worn as armor

### condition

(string) How an enchantment determines if you are in the right environments in order for the
enchantment to qualify for being active.

Values:

- `ALWAYS` (default) - Always active
- `UNDERGROUND` - When the owner of the item is below Z-level 0
- `ABOVEGROUND` - When the owner of the item is at or above Z-level 0
- `UNDERWATER` - When the owner is in swimmable terrain
- `NIGHT` - When it is night time
- `DUSK` - When it is dusk
- `DAY` - When it is day time
- `DAWN` - When it is dawn
- `ACTIVE` - whenever the item, mutation, bionic, or whatever the enchantment is attached to is
  active.
- `INACTIVE` - the opposite of `ACTIVE`

### emitter

(string) Identifier of an emitter that's active as long as this enchantment is active. Default: no
emitter.

### ench_effects

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

### hit_you_effect

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

### hit_me_effect

(array) List of spells that may be cast when enchantment is active and character gets melee attacked
by a creature.

Same syntax as for `hit_you_effect`.

### mutations

(array) List of mutations temporarily granted while enchantment is active.

### intermittent_activation

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

### values

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

#### IDs of modifiable values

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

#### OBSTACLE_MOVE_COST

Movement cost effect on obstacles. `base_value` here is initial move cost.
The final value cannot go below 100.
Stacks with MOVE_COST, done before it.

#### SWIM_MOVE_COST

Movement cost effect while swimming
The fianl value cannot go below 30.
This does NOT stack with MOVE_COST

#### READING_SPEED

Speed of reading books. `base_value` is final reading speed in moves.
The final value cannot go below 1 second.

##### CRAFTING_SPEED

Crafting speed. `base_value` is a multiplier of crafting speed.
Calculated after all other multipliers

##### CONSTRUCTION_SPEED

Construction speed. `base_value` is a multiplier of construction speed.
Calculated after all other multipliers

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

Thirst gain rate. This modifier ignores `add` field. `base_value` here is character's base thirst
gain rate. The final value cannot go below 0.

##### FATIGUE

Fatigue gain rate. This modifier ignores `add` field. `base_value` here is character's base fatigue
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
NOTE: There is no limit, this is subject to future changes

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

##### OVERMAP_SIGHT

Modifier to overmap sight. `base_value` is the best mutation value.
Maximum is 3

##### EFFECTIVE_FOCUS

Modifier to focus. `base_value` is current focus
There is no limit

##### BONUS_DODGE

Additional dodges per turn before dodge penalty kicks in. `base_value` here is character's base
dodges per turn before penalty (usually 1). The final value can go below 0, which results in penalty
to dodge roll.

##### ARMOR_X

Incoming damage modifier. Applied after Active Defense System bionic but before the damage is
absorbed by items. Note that `base_value` here is incoming damage value of corresponding type, so
positive `add` and greater than 1 `mul` will **increase** damage received by the character. Each
damage type has its own enchant value in addition to the globally applied value `ARMOR`:

- `ARMOR_ACID`
- `ARMOR_BASH`
- `ARMOR_BIOLOGICAL`
- `ARMOR_BULLET`
- `ARMOR_COLD`
- `ARMOR_CUT`
- `ARMOR_LIGHT`
- `ARMOR_DARK`
- `ARMOR_PSI`
- `ARMOR_ELECTRIC`
- `ARMOR_HEAT`
- `ARMOR_STAB`
- `ARMOR_TRUE`

#### Item values

##### ITEM_ATTACK_COST

Attack cost (melee or throwing) for this item. Ignores condition / location, and is always active.
`base_value` here is base item attack cost. Note that the final value cannot go below 0.

##### ITEM_DAMAGE_X

Melee damage of this item. Ignores condition / location, and is always active. `base_value` here is
base item damage of corresponding type. Note that the final value cannot go below 0.
There is the global damage modifier `ITEM_DAMAGE` in addition to the supported damage types:

- `ITEM_DAMAGE_BASH`
- `ITEM_DAMAGE_CUT`
- `ITEM_DAMAGE_STAB`
- `ITEM_DAMAGE_BULLET`
- `ITEM_DAMAGE_ACID`
- `ITEM_DAMAGE_BIOLOGICAL`
- `ITEM_DAMAGE_COLD`
- `ITEM_DAMAGE_DARK`
- `ITEM_DAMAGE_ELECTRIC`
- `ITEM_DAMAGE_FIRE`
- `ITEM_DAMAGE_LIGHT`
- `ITEM_DAMAGE_PSI`
- `ITEM_DAMAGE_TRUE`

##### ITEM_ARMOR_X

Incoming damage modifier for this item, applied before the damage is absorbed by the item. Note that
`base_value` here is incoming damage value of corresponding type, so positive `add` and greater than
1 `mul` will **increase** damage received by the character. Each damage type has its own enchant
value, in addition to the global `ITEM_ARMOR`:

- `ITEM_ARMOR_ACID`
- `ITEM_ARMOR_BASH`
- `ITEM_ARMOR_BIOLOGICAL`
- `ITEM_ARMOR_BULLET`
- `ITEM_ARMOR_COLD`
- `ITEM_ARMOR_CUT`
- `ITEM_ARMOR_LIGHT`
- `ITEM_ARMOR_DARK`
- `ITEM_ARMOR_PSI`
- `ITEM_ARMOR_ELECTRIC`
- `ITEM_ARMOR_HEAT`
- `ITEM_ARMOR_STAB`
- `ITEM_ARMOR_TRUE`

## Examples

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
    }
  }
]
```

# Enchantment Values

```jsonc
{
  "id": "RANGED_DAMAGE", // Id of the enchantment
  "type": "enchantment_value", // Needed Type
  "can_add": true, // Weather adding to the enchantment value will do anything
  "can_mult": true, // Weather multiplying to the enchantment value will do anything
  "suffixes": [ // All the suffixes. These appear as in this case RANGED_DAMAGE_XXX
    "BASH",     // In addition suffixes will also reference the parent type when in use
    "CUT",
    "DARK",
    "LIGHT",
    "PSI",
    "STAB",
    "BULLET",
    "HEAT",
    "COLD",
    "ELECTRIC",
    "ACID",
    "BIOLOGICAL"
  ]
},
```

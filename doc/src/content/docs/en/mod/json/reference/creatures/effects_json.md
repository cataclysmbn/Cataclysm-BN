---
title: Effects
---

## How to give effects in-game?

### Comestibles

The first way to give a player an effect in-game is through the drug system. To do this your item
must have a use_action of type "consume_drug".

```json
"use_action" : {
    "type" : "consume_drug",
    "activation_message" : "You take some oxycodone.",
    "effects" : [
        {
            "id": "pkill3",
            "duration": 20
        },
        {
            "id": "pkill2",
            "duration": 200
        }
    ]
},
```

Notice the "effects" field. Each effect has four potential fields:

```json
"id" - Required
"duration" - Required
"bp" - This will cause the effect to target this body part specifically
```

Valid "bp" entries are (no entry means the effect is untargeted):

```json
"torso"
"head"
"eyes"
"mouth"
"arm_l"
"arm_r"
"hand_l"
"hand_r"
"leg_l"
"leg_r"
"foot_l"
"foot_r"
```

### Creature attacks

Creatures have an effect field similar to the "consume_drug" entry for items. You can make a
creature's attacks apply effects by adding an "attack_effs" entry for the creature.

```json
"attack_effs": [
    {
        "//": "applying this multiple times makes intensity go up by 3 instead of 1",
        "id": "paralyzepoison",
        "duration": 33
    },
    {
        "id": "paralyzepoison",
        "duration": 33
    },
    {
        "id": "paralyzepoison",
        "duration": 33
    }
],
```

The fields for "attack_effs" function identically to the ones for "consume_drug". However, creatures
have an additional field:

```json
"chance" - The percentage chance of the effect being applied on a good hit, defaults to 100%
```

If a creature successfully damages the player and their chance roll succeeds they will apply all of
the listed effects to the player. The effects are added one after another.

### Mutations

Mutations can give effects with the field "enchantments", the following mutation grants a special
attack. The enchant provided after the ink gland mutation permanently affects armor values on the
player.

```json
{
    "type": "mutation",
    "id": "INK_GLANDS",
    "name": { "str": "Ink glands" },
    "points": 1,
    "visibility": 1,
    "ugliness": 1,
    "description": "Several ink glands have grown onto your torso.  They can be used to spray defensive ink and blind an attacker in an emergency, as long as the torso isn't covered.",
    "enchantments": [ "MEP_INK_GLAND_SPRAY" ],
    "category": [ "CEPHALOPOD" ]
  },
  {
    "type": "enchantment",
    "id": "ENCH_BIO_CARBON",
    "condition": "ALWAYS",
    "values": [
      { "value": "ARMOR_BASH", "multiply": -0.05 },
      { "value": "ARMOR_CUT", "multiply": -0.1 },
      { "value": "ARMOR_STAB", "multiply": -0.08 },
      { "value": "ARMOR_BULLET", "multiply": -0.15 }
    ]
  }
```

## Required fields

```json
"type": "effect_type",      - Required
"id": "xxxx"                - Must be unique
```

## Optional fields

### Max intensity

```json
"max_intensity": 3          - Used for many later fields, defaults to 1
"max_effective_intensity"   - How many intensity levels will apply effects.
                              Other intensity levels will only increase duration.
```

### Name

```json
"name": ["XYZ"]
or
"name": [
    "ABC",
    "XYZ",
    "123"
]
```

If `"max_intensity" > 1` and the number of entries in `"name" >= "max_intensity"` then it will
attempt to use the proper intensity name. In this case that means an intensity of 1 would give the
name "ABC", 2 would give "XYZ", and 3 would give "123". If `"max_intensity" == 1` or the number of
entries in "name" is less than "max_intensity", it will use the first entry followed by the
intensity in brackets if the current intensity > 1, i.e.`"ABC", "ABC [2]", "ABC [3]"`. If the
desired entry of "name" is the empty string ("") or "name" is missing then the effect will not
display to the player in the status screen.

Each entry in "name" can also have an optional context:

```json
"name": [ { "ctxt": "ECIG", "str": "Smoke" } ]
```

In this case, the game will translate the name with the given context "ECIG", which makes it
possible to distinguish the verb "Smoke" from the noun "Smoke" in other languages.

```json
"speed_name" : "XYZ"        - Defaults to the first name value
```

This is the value used in the list of modifiers on a player's speed. It will default to the first
entry in "name" if it doesn't exist, and if neither one exists or if "speed_name" is the empty
string (""), then it will not appear in the list of modifiers on the players speed (though the
effect might still have an effect).

### Descriptions

```json
"desc": ["XYZ"]
or
"desc": [
    "ABC",
    "XYZ",
    "123"
]
```

Descriptions operate identically to the name field when picking which one to use. In general,
descriptions should be only 1 line. Stats and effects do not need to be included, and will be
automatically generated from the other effect data. Should a description line be the empty string
("") it will only display the stat changes in the effect description.

Descriptions also have a second field that can act as a modifier:

```json
"part_descs": true      - Defaults to false if not present
```

If "part_descs" == true then descriptions are preceded by "Your X", where X is the body part name,
meaning the prior descriptions would appear as "Your left arm ABC".

Descriptions can also have a reduced form:

```json
"reduced_desc": ["XYZ"]
or
"reduced_desc": [
    "ABC",
    "XYZ",
    "123"
]
```

This is the description that will be used if an effect is reduced. By default this will use the
normal description if it doesn't exist.

### Rating

```json
"rating": "good"        - Defaults to "neutral" if missing
```

This is used for how the messages when the effect is applied and removed are displayed. Also this
affects "blood_analysis_description" (see below) field: effects with "good" rating will be colored
green, effects with any other rating will be colored red when character conducts a blood analysis
through some means. Valid entries are:

```json
"good"
"neutral"
"bad"
"mixed"
```

### Looks_like

```json
"looks_like": "drunk"
```

If the "looks_like" field exists, the effect will visually resemble the specified tag (for example, "drunk").
This only affects the appearance of the effect over a NPC or character, not its mechanical behavior.

### Messages

```json
"apply_message": "message",
"remove_message": "message"
```

If the "apply_message" or "remove_message" fields exist, the respective message will be displayed
upon the addition or removal of the effect. Note: "apply_message" will only display if the effect is
being added, not if it is simply incrementing a current effect (so only new bites, etc.).

### Memorial Log

```json
"apply_memorial_log": "log",
"remove_memorial_log": "log"
```

If the "apply_memorial_log" or "remove_memorial_log" fields exist, the game will add the respective
message to the memorial log on addition or removal of the effect. Similar to the message fields the
"apply_memorial_log" will only be added to the log for new effect additions.

### Resistances

```json
"resist_trait": "NOPAIN",
"resist_effect": "flumed"
```

These fields are used to determine if an effect is being resisted or not. If the player has the
matching trait or effect then they are "resisting" the effect, which changes its effects and
description. Effects can only have one "resist_trait" and one "resist_effect" at a time.

### Removes effects

```json
"removes_effects": ["bite", "flu"]
```

This field will cause an effect to automatically remove any other copies of the listed effects if
they are present. In the example above the placed effect would automatically cure any bite wounds or
flu the player had. Any values here automatically count for "blocks_effects" as well, no need to
duplicate them there.

### Blocks effects

```json
"blocks_effects": ["cold", "flu"]
```

This field will cause an effect to prevent the placement of the listed effects. In the example above
the effect would prevent the player from catching the cold or the flu (BUT WOULD NOT CURE ANY
ONGOING COLDS OR FLUS). Any effects present in "removes_effects" are automatically added to
"blocks_effects", no need for manual duplication.

### Effect limiters

```json
"max_duration": 100,
"dur_add_perc": 150     - Defaults to 100%
```

These are utilized when adding to currently existing effects. "max_duration" limits the overall
duration of the effect. "dur_add_perc" is the percentage value of the normal duration for adding to
an existing. An example:

1. I add effect A to the player for 100 ticks.
2. I add effect A to the player again for 100 ticks. Because the "dur_add_perc" = 150 in the example
   above, the second addition adds a total of 100 * 150% = 150 ticks, for a total value of 250 ticks
   from the two. This can also be below 100%, and should be able to even be negative, leading to
   future applications decreasing the overall time left.

### Intensities

Intensities are used to control effect effects, names, and descriptions. They are defined with:

```json
"int_add_val": 2        - Defaults to 0! This means future applications will not increase intensity unless changed!
and/or
"int_decay_step": -2,    - Defaults to -1
"int_decay_tick": 10
or
"int_dur_factor": 700
```

The first value is the amount an intensity will be incremented if adding to an already existing
effect. As an example:

1. I add effect A to the player
2. I add effect A to the player again Because "int_add_val" = 2, the second addition will change the
   effect intensity from 1 to 1 + 2 = 3. NOTE: You must have at least one of the 3 intensity data
   sets for intensity to do anything!

"int_decay_step" and "int_decay_tick" require one another to do anything. If both exist then the
game will automatically increment the current effect intensities by "int_decay_step" every
"int_decay_tick" ticks, capping the result at `[1, "max_intensity"]`. This can be used to make
effects automatically increase or decrease in intensity over time.

"int_dur_factor" overrides the other three intensities fields, and forces the intensity to be a
number defined as intensity = duration / "int_dur_factor" rounded up (so from 0 to "int_dur_factor"
is intensity 1).

### Permanence

An effect that is permanent does not lose duration with time. That is, even if its duration is 1
turn, it will last until removed.

```json
"permanent": true
```

### Miss messages

```json
"miss_messages": [["Your blisters distract you", 1]]
or
"miss_messages": [
    ["Your blisters distract you", 1],
    ["Your blisters don't like you", 10],
]
```

This will add the following miss messages at the given chances while the effect is in effect.

### Decay messages

```json
"decay_messages": [["The jet injector's chemicals wear off.  You feel AWFUL!", "bad"]]
or
"decay_messages": [
    ["The jet injector's chemicals wear off.  You feel AWFUL!", "bad"],
    ["OOGA-BOOGA.  You feel AWFUL!", "bad"],
]
```

The messages are matched to intensities, so the first message is for intensity 1, the second for
intensity 2, and so on. The messages will print whenever the intensity decreases to their matching
intensity from a higher intensity, whether through decay ticks or through "int_dur_factor". So if it
decayed to intensity 2 from 3+ it would display "OOGA-BOOGA. You feel AWFUL!" as a bad message to
the player.

### Targeting modifiers

```json
"main_parts_only": true     - Defaults to false
```

This automatically retargets any effect on a non-main part (hands, eyes, feet, etc.) to the matching
main part (arms, head, legs, etc.).

### Effect modifiers

```json
"pkill_addict_reduces": true,   - Defaults to false
"pain_sizing": true,            - Defaults to false
"hurt_sizing": true,            - Defaults to false
"harmful_cough": true           - Defaults to false
```

"pkill_addict_reduces" makes a player's addiction to painkillers reduce the chance of the effect
giving them more pkill. "pain_sizing" and "hurt_sizing" cause large/huge mutations to affect the
chance of pain and hurt effects triggering. "harmful_cough" means that the coughs caused by this
effect can hurt the player.

### Morale

```json
"morale": "morale_high"
```

Type of morale effect provided. Mandatory if there is a morale effect, must not be specified
otherwise.

### Other effects on removal

```json
"effects_on_remove": [
    {
        "intensity_requirement": 0, - Defaults to 0
        "effect_type": "cold",      - (Mandatory) Effect that will be applied
        "allow_on_decay": false,    - Defaults to true
        "allow_on_remove" true,     - Defaults to false
        "intensity": 5,             - Defaults to 0
        "inherit_intensity": false, - Defaults to false
        "duration": "10 s",         - Defaults to 0
        "inherit_duration": true,   - Defaults to true
        "body_part": "hand_r,       - Defaults to null
        "inherit_body_part": false  - Defaults to true
    }
]
```

"intensity_requirement" will prevent adding the new effect if current effect has lower intensity.
"allow_on_decay" enables adding the effect if parent decayed (was removed due to 0 duration).
"allow_on_remove" enables adding the effect if parent was removed before 0 duration.
"inherit_duration", "inherit_intensity" and "inherit_body_part" cause the relevant variable to be
copied from parent effect.

### Effect effects

```json
"base_mods" : {
    arguments
},
"scaling_mods": {
    arguments
}
```

This is where the real meat of the effect JSON definition lies. Each one can take a variety of
arguments. Decimals are valid but must be formatted as "0.X" or "-0.X". The game will round towards
zero at the end when calculating actually applied values

Basic definitions:

```json
"X_amount"      - Amount applied of X when effect is placed. Like apply messages it will only trigger on new effects
"X_min"         - Minimum amount of X applied when roll triggers
"X_max"         - Maximum amount of X applied when roll triggers (no entry means it will give exactly X_min each time instead of rng(min, max)
"X_min_val"     - Minimum value the effect will push you to, 0 means uncapped! Doesn't exist for some X's!
"X_max_val"     - Maximum value the effect will push you to, 0 means uncapped! Doesn't exist for some X's!
"X_chance"      - Basic chance of X triggering each time, depends on "X_chance_bot" for exact formula
"X_chance_bot"  - If this doesn't exist then the trigger chance is (1 in "X_chance"). If this does exist then the chance is ("X_chance" in "X_chance_bot")
"X_tick"        - Effect rolls for X triggering every Y ticks
```

Valid arguments:

```json
"str_mod"           - Positive values raises stat, negative values lowers stat
"dex_mod"           - Positive values raises stat, negative values lowers stat
"per_mod"           - Positive values raises stat, negative values lowers stat
"int_mod"           - Positive values raises stat, negative values lowers stat
"speed_mod"         - Positive values raises stat, negative values lowers stat

"pain_amount"       - Positives raise pain, negatives don't make anything. Don't make it too high.
"pain_min"          - Minimal amount of pain, certain effect will give/take
"pain_max"          - if 0 or missing value will be exactly "pain_min"
"pain_max_val"      - Defaults to 0, which means uncapped
"pain_chance"       - Chance to get more pain
"pain_chance_bot"
"pain_tick"         - Defaults to every tick.

"hurt_amount"       - Positives will give damage, negatives will heal instead. Don't make it too high.
"hurt_min"          - Minimal amount of damage, certain effect will give/take
"hurt_max"          - if 0 or missing value will be exactly "hurt_min"
"hurt_chance"       - Chance to cause damage
"hurt_chance_bot"
"hurt_tick"         - Defaults to every tick

"sleep_amount"      - Amount of turns spent sleeping.
"sleep_min"         - Minimal amount of sleep in turns, certain effect can give
"sleep_max"         - if 0 or missing value will be exactly "sleep_min"
"sleep_chance"      - Chance to fall asleep
"sleep_chance_bot"
"sleep_tick"        - Defaults to every tick

"pkill_amount"      - Amount of painkiller effect. Don't go too high with it.
"pkill_min"         - Minimal amount of painkiller, certain effect will give
"pkill_max"         - if 0 or missing value will be exactly "pkill_min"
"pkill_max_val"     - Defaults to 0, which means uncapped
"pkill_chance"      - Chance to cause painkiller effect(lowers pain)
"pkill_chance_bot"
"pkill_tick"        - Defaults to every tick

"stim_amount"       - Negatives cause depressants effect and positives cause stimulants effect.
"stim_min"          - Minimal amount of stimulant, certain effect will give.
"stim_max"          - if 0 or missing value will be exactly "stim_min"
"stim_min_val"      - Defaults to 0, which means uncapped
"stim_max_val"      - Defaults to 0, which means uncapped
"stim_chance"       - Chance to cause one of two stimulant effects
"stim_chance_bot"
"stim_tick"         - Defaults to every tick

"health_amount"     - Negatives decrease health and positives increase it. It's semi-hidden stat, which affects healing.
"health_min"        - Minimal amount of health, certain effect will give/take.
"health_max"        - if 0 or missing value will be exactly "health_min"
"health_min_val"    - Defaults to 0, which means uncapped
"health_max_val"    - Defaults to 0, which means uncapped
"health_chance"     - Chance to change health
"health_chance_bot"
"health_tick"       - Defaults to every tick

"h_mod_amount"      - Affects health stat growth, positives increase it and negatives decrease it
"h_mod_min"         - Minimal amount of health_modifier, certain effect will give/take
"h_mod_max"         - if 0 or missing value will be exactly "h_mod_min"
"h_mod_min_val"     - Defaults to 0, which means uncapped
"h_mod_max_val"     - Defaults to 0, which means uncapped
"h_mod_chance"      - Chance to change health_modifier
"h_mod_chance_bot"
"h_mod_tick"        - Defaults to every tick

"rad_amount"        - Amount of radiation it can give/take. Just be aware that anything above [50] is fatal.
"rad_min"           - Minimal amount of radiation, certain effect will give/take
"rad_max"           - if 0 or missing value will be exactly "rad_min"
"rad_max_val"       - Defaults to 0, which means uncapped
"rad_chance"        - Chance to get more radiation
"rad_chance_bot"
"rad_tick"          - Defaults to every tick

"hunger_amount"     - Amount of hunger it can give/take.
"hunger_min"        - Minimal amount of hunger, certain effect will give/take
"hunger_max"        - if 0 or missing value will be exactly "hunger_min"
"hunger_min_val"    - Defaults to 0, which means uncapped
"hunger_max_val"    - Defaults to 0, which means uncapped
"hunger_chance"     - Chance to become more hungry
"hunger_chance_bot"
"hunger_tick"       - Defaults to every tick

"thirst_amount"     - Amount of thirst it can give/take.
"thirst_min"        - Minimal amount of thirst, certain effect will give/take
"thirst_max"        - if 0 or missing value will be exactly "thirst_min"
"thirst_min_val"    - Defaults to 0, which means uncapped
"thirst_max_val"    - Defaults to 0, which means uncapped
"thirst_chance"     - Chance to become more thirsty
"thirst_chance_bot"
"thirst_tick"       - Defaults to every tick

"sleepdebt_amount"     - Amount of sleep debt it can give/take.
"sleepdebt_min"        - Minimal amount of sleep, certain effect will give/take
"sleepdebt_max"        - if 0 or missing value will be exactly "sleepdebt_min"
"sleepdebt_min_val"    - Defaults to 0, which means uncapped
"sleepdebt_max_val"    - Defaults to 0, which means uncapped
"sleepdebt_chance"     - Chance to give more sleep
"sleepdebt_chance_bot" - Min chance, unsure, needs auditing
"sleepdebt_tick"       - Defaults to every tick

"fatigue_amount"    - Amount of fatigue it can give/take. After certain amount character will need to sleep.
"fatigue_min"       - Minimal amount of fatigue, certain effect will give/take
"fatigue_max"       - if 0 or missing value will be exactly "fatigue_min"
"fatigue_min_val"   - Defaults to 0, which means uncapped
"fatigue_max_val"   - Defaults to 0, which means uncapped
"fatigue_chance"    - Chance to get more tired
"fatigue_chance_bot"
"fatigue_tick"      - Defaults to every tick

"stamina_amount"    - Amount of stamina it can give/take.
"stamina_min"       - Minimal amount of stamina, certain effect will give/take
"stamina_max"       - if 0 or missing value will be exactly "stamina_min"
"stamina_min_val"   - Defaults to 0, which means uncapped
"stamina_max_val"   - Defaults to 0, which means uncapped
"stamina_chance"    - Chance to get stamina changes
"stamina_chance_bot"
"stamina_tick"      - Defaults to every tick

"cough_chance"      - Chance to cause cough
"cough_chance_bot"
"cough_tick"        - Defaults to every tick

"vomit_chance"      - Chance to cause vomiting
"vomit_chance_bot"
"vomit_tick"        - Defaults to every tick

"healing_rate"      - Healed rate per day
"healing_head"      - Percentage of healing value for head
"healing_torso"     - Percentage of healing value for torso

"morale"            - Amount of morale provided. Must be a single number (resistance not supported).

This following effects only apply to monsters:

"hit_mod"           - Raises or lowers monster melee_skill
"dodge_mod"         - Raises or lowers monster dodge rating
"bash_mod"          - Raises or lowers bashing damage dealt by basic attacks and melee special attacks, lowering it to zero will make it deal no damage
"cut_mod"           - Raises or lowers cutting damage dealt by basic attacks and melee special attacks, lowering it to zero will make it deal no damage
"size_mod"          - Shrinks or grows a monster's size, can't make a monster smaller than tiny or larger than huge
```

Each argument can also take either one or two values.

```json
"thirst_min": [1]
or
"thirst_min": [1, 2]
```

If an effect is "resisted" (either through "resist_effect" or "resist_trait") then it will use the
second value. If there is only one value given it will always use that amount.

Base mods and Scaling mods: While on intensity = 1 an effect will only have the basic effects of its
"base_mods". However for each additional intensity it gains it adds the value of each of its
"scaling_mods" to the calculations. So:

```json
Intensity 1 values = base_mods values
Intensity 2 values = base_mods values + scaling_mods values
Intensity 3 values = base_mods values + 2 * scaling_mods values
Intensity 4 values = base_mods values + 3 * scaling_mods values
```

and so on.

Special case: The only special case is if base_mods' "X_chance_bot" + intensity * scaling_mods'
"X_chance_bot" = 0 then it treats it as if it were equal to 1 (i.e. trigger every time)

## Example Effect

```json
"type": "effect_type",
"id": "drunk",
"name": [
    "Tipsy",
    "Drunk",
    "Trashed",
    "Wasted"
],
"looks_like": "drunk",
"max_intensity": 4,
"apply_message": "You feel lightheaded.",
"int_dur_factor": 1000,
"miss_messages": [["You feel woozy.", 1]],
"morale": "morale_drunk",
"base_mods": {
    "str_mod": [1],
    "vomit_chance": [-43],
    "sleep_chance": [-1003],
    "sleep_min": [2500],
    "sleep_max": [3500],
    "morale": [ 5 ]
},
"scaling_mods": {
    "str_mod": [-0.67],
    "per_mod": [-1],
    "dex_mod": [-1],
    "int_mod": [-1.42],
    "vomit_chance": [21],
    "sleep_chance": [501],
    "morale": [ 10 ]
}
```

First when "drunk" is applied to the player if they aren't already drunk it prints the message, "You
feel lightheaded". It also adds the "You feel woozy" miss message for as long as it is applied. It
has "int_dur_factor": 1000, meaning that its intensity will always be equal to its duration / 1000
rounded up, and it has "max_intensity": 4 meaning the highest its intensity will go is 4 at a
duration of 3000 or higher. As it moves up through the different intensities, its name will change.
Its description will simply display the stat changes, with no additional description added.

As it moves up through the intensity levels its effects will be:

```json
Intensity 1
    +1 STR
    +5 morale
    No other effects (since both "X_chance"s are negative)
Intensity 2
    1 - .67 = .33 =         0 STR (Round towards zero)
    0 - 1 =                 -1 PER
    0 - 1 =                 -1 DEX
    0 -1.42 =               -1 INT
    -43 + 21 =              still negative, so no vomiting
    -1003 + 501 =           still negative, so no passing out
    5 + 10 =                15 morale
Intensity 3
    1 - 2 * .67 = -.34 =    0 STR (round towards zero)
    0 - 2 * 1 =             -2 PER
    0 - 2 * 1 =             -2 DEX
    0 - 2 * 1.43 =          -2 INT
    -43 + 2 * 21 = -1       still negative, no vomiting
    -1003 + 2 * 501 = -1    still negative, no passing out
    5 + 2 * 10 =            25 morale
Intensity 4
    1 - 3 * .67 = - 1.01 =  -1 STR
    0 - 3 * 1 =             -3 PER
    0 - 3 * 1 =             -3 DEX
    0 - 3 * 1.43 =          -4 INT
    -43 + 3 * 21 = 20       "vomit_chance_bot" doesn't exist, so a 1 in 20 chance of vomiting. "vomit_tick" doesn't exist, so it rolls every turn.
    -1003 + 3 * 501 = 500   "sleep_chance_bot" doesn't exist, so a 1 in 500 chance of passing out for rng(2500, 3500) turns. "sleep_tick" doesn't exist, so it rolls every turn.
    5 + 3 * 10 =            35 morale
```

### Blood analysis description

```json
"blood_analysis_description": "Minor Painkiller"
```

This description will be displayed for every effect which has this field when character conducts a
blood analysis (for example, through Blood Analysis CBM).

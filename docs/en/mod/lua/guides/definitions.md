# Lua Data Definitions

This document describes how to define game data (effects, mutations, bionics, and dialogue) using Lua instead of JSON in Cataclysm: Bright Nights.

## Overview

Lua data definitions allow mods to programmatically create game content. This is useful for:
- Conditional data based on other loaded mods
- Computed values that would be tedious in JSON
- Reusing patterns across multiple definitions

## Loading Phases

Data definitions are processed in two phases:

### Preload Phase (`game.define.*`)

Tables in the `game.define` namespace are processed **before** JSON data is loaded. Use this for:
- Defining new content that doesn't depend on existing JSON data
- Content that other mods may want to extend via `copy_from`

```lua
-- In preload.lua
game.define.effect_type["my_new_effect"] = {
    name = {"My Effect"},
    desc = {"A custom effect"},
    rating = "good",
    max_duration = "1 hour"
}
```

### Finalize Phase (`game.define.finalize_*`)

Tables in the `game.define.finalize_*` namespace are processed **after** all JSON data is loaded. Use this for:
- Extending existing JSON-defined content via `copy_from`
- Content that references JSON-defined data

```lua
-- In finalize.lua
game.define.finalize_mutation["my_variant"] = {
    copy_from = "LIGHTEATER",  -- Copy from JSON-defined trait
    name = "Enhanced Photosynthesis",
    points = 4
}
```

## Type Reference

### effect_type

Define effects that can be applied to characters.

```lua
game.define.effect_type["my_effect"] = {
    -- Basic properties
    name = {"Effect Name"},           -- Array for intensity-based names
    desc = {"Description"},           -- Array for intensity-based descriptions
    rating = "good",                  -- "good", "bad", "mixed", or "neutral"

    -- Messages
    apply_message = "You feel energized.",
    remove_message = "The effect fades.",

    -- Duration and intensity
    max_duration = "2 hours",         -- String or TimeDuration object
    max_intensity = 3,
    int_dur_factor = "30 minutes",

    -- Boolean flags
    permanent = false,
    show_in_info = true,
    main_parts_only = false,

    -- Resistances
    resist_traits = {"TOUGH"},        -- Traits that resist this effect
    resist_effects = {"effect_immunity"},

    -- Stat modifiers (base_mods and scaling_mods)
    base_mods = {
        str_mod = {-2, -1},           -- {normal, reduced} values
        pain_amount = {1},
        pain_chance = {10, 5},
        speed_mod = {-10}
    },
    scaling_mods = {
        pain_amount = {1, 0}          -- Scales with intensity
    },

    -- Message arrays
    miss_messages = {
        {"You stumble", 1},           -- {message, chance}
        {"You trip", 2}
    },
    decay_messages = {
        {"The effect weakens", "good"},
        {"It's fading", "neutral"}
    },

    -- Effects triggered on removal
    effects_on_remove = {
        {
            type = "afterglow",
            duration = "30 minutes",
            intensity = 1
        }
    }
}
```

### mutation (Trait)

Define character mutations/traits.

```lua
game.define.mutation["my_trait"] = {
    -- Basic info
    name = "My Trait",
    description = "A custom trait",
    points = 2,

    -- Visibility
    visibility = 0,
    ugliness = 0,

    -- Classification
    valid = true,                     -- Available via generic mutagen
    purifiable = true,                -- Can be removed by Purifier
    threshold = false,                -- Is a threshold mutation
    profession = false,               -- Professional trait only
    startingtrait = true,             -- Available at character creation

    -- Activation
    activated = false,                -- Has active ability
    starts_active = false,
    cost = 0,                         -- Activation cost
    cooldown = 0,                     -- Turns between activations

    -- Stat modifiers
    hp_modifier = 0.1,                -- +10% HP
    str_modifier = 1.0,
    dodge_modifier = 0.5,
    speed_modifier = 1.1,             -- +10% speed

    -- Categories and flags
    category = {"BEAST", "LIZARD"},
    types = {"LEGS"},                 -- Mutation types
    flags = {"PRED1"},                -- Trait flags

    -- Prerequisites and conflicts
    prereqs = {"TRAIT_A"},
    prereqs2 = {"TRAIT_B", "TRAIT_C"},  -- Need one from each array
    cancels = {"CONFLICTING_TRAIT"},
    changes_to = {"UPGRADED_TRAIT"},
    leads_to = {"ADDITIONAL_TRAIT"},

    -- Body modifications
    lumination = {head = 2.0},        -- Glow intensity by body part
    encumbrance_always = {torso = 5},
    encumbrance_covered = {legs = 3},
    restricts_gear = {"torso"},       -- Body parts needing OVERSIZE gear
    allowed_items = {"OVERSIZE"},     -- Item flags allowed on restricted parts

    -- Armor
    armor = {
        {parts = {"torso", "arm_l", "arm_r"}, bash = 2, cut = 3},
        {parts = {"ALL"}, acid = 5}
    },

    -- Abilities
    enchantments = {"ench_strength_boost"},
    spells_learned = {spell_lightning = 1},  -- spell_id = level
    initial_ma_styles = {"style_karate"},

    -- Other modifiers
    vitamin_rates = {vitC = "1 hour"},
    craft_skill_bonus = {fabrication = 2},
    social_modifiers = {persuade = 10, intimidate = -5}
}
```

### bionic

Define cybernetic bionics (CBMs).

```lua
game.define.bionic["my_cbm"] = {
    -- Basic info
    name = "Enhanced Muscles",
    description = "Synthetic muscle fibers grant increased strength.",

    -- Power costs
    power_activate = "50 kJ",         -- String or Energy object
    power_deactivate = "10 kJ",
    power_over_time = "5 kJ",
    power_trigger = "20 kJ",
    capacity = "500 kJ",              -- Power bank size

    -- Activation
    activated = true,
    charge_time = 10,                 -- Turns or time_duration

    -- Stats
    stat_bonus = {STR = 3, DEX = 1},
    weight_capacity_modifier = 1.25,
    weight_capacity_bonus = "10 kg",

    -- Fuel
    fuel_opts = {"gasoline", "diesel"},
    fuel_capacity = 500,
    fuel_efficiency = 0.8,

    -- Body part effects
    occupied_bodyparts = {torso = 10, arm_l = 5, arm_r = 5},
    encumbrance = {torso = 2},
    env_protec = {eyes = 5},
    bash_protec = {head = 3},
    cut_protec = {head = 2},

    -- Relationships
    canceled_mutations = {"HUGE"},
    included_bionics = {"bio_power_storage"},
    upgraded_bionic = "bio_str_enhancer",
    required_bionics = {"bio_power_storage_mkII"},

    -- Magic integration
    enchantments = {"ench_bionic_power"},
    learned_spells = {spell_shock = 1},

    -- Flags
    flags = {"BIONIC_POWER_SOURCE"}
}
```

### talk_topic

Define NPC dialogue topics.

```lua
game.define.talk_topic["TALK_MY_TOPIC"] = {
    -- Whether to replace built-in responses
    replace_built_in_responses = false,

    -- Dynamic line - what the NPC says
    -- Can be a simple string:
    dynamic_line = "Hello there, traveler!",

    -- Or an array for random selection:
    dynamic_line = {
        "Hello!",
        "Greetings!",
        "Welcome!"
    },

    -- Or conditional:
    dynamic_line = {
        u_male = "Hello, sir!",
        ["no"] = "Hello, ma'am!"
    },

    -- More complex conditionals:
    dynamic_line = {
        u_has_trait = "PRETTY",
        yes = "My, aren't you lovely!",
        ["no"] = "Greetings, traveler."
    },

    -- Player response options
    responses = {
        {
            text = "Tell me about yourself.",
            topic = "TALK_MY_TOPIC_ABOUT"
        },
        {
            text = "I need supplies.",
            topic = "TALK_MY_TOPIC_TRADE",
            condition = {npc_has_trait = "TRADER"}
        },
        {
            text = "[Intimidate] Give me what you have!",
            topic = "TALK_MY_TOPIC_THREATEN",
            trial = {type = "INTIMIDATE", difficulty = 5}
        },
        {
            text = "Goodbye.",
            topic = "TALK_DONE"
        }
    },

    -- Effects when this topic is spoken
    speaker_effect = {
        effect = {npc_add_effect = "relaxed", duration = "1 hour"}
    }
}
```

## Common Patterns

### copy_from Inheritance

You can extend existing definitions using `copy_from`:

```lua
game.define.finalize_mutation["BETTER_NIGHT_VISION"] = {
    copy_from = "NIGHTVISION",        -- Base trait to copy from
    name = "Enhanced Night Vision",   -- Override specific fields
    points = 3,
    night_vision_range = 8.0          -- Better than original
}
```

### Native Types vs String Fallbacks

Most fields accept either native Lua types or string representations:

```lua
-- These are equivalent:
power_activate = Energy.from_kilojoule(50)
power_activate = "50 kJ"

-- These are equivalent:
max_duration = TimeDuration.from_hours(2)
max_duration = "2 hours"

-- These are equivalent:
upgraded_bionic = BionicId.new("bio_str")
upgraded_bionic = "bio_str"
```

### ID References

IDs can be specified as native ID objects or strings:

```lua
-- Both work:
resist_traits = {TraitId.new("TOUGH"), "RESILIENT"}
resist_traits = {"TOUGH", "RESILIENT"}
```

## Complete Example

Here's a complete example mod demonstrating Lua data definitions:

**modinfo.json:**
```json
{
    "type": "MOD_INFO",
    "id": "lua_example",
    "name": "Lua Example Mod"
}
```

**preload.lua:**
```lua
-- Define a new effect
game.define.effect_type["lua_energized"] = {
    name = {"Energized"},
    desc = {"You feel full of energy!"},
    rating = "good",
    max_duration = "30 minutes",
    max_intensity = 3,
    base_mods = {
        speed_mod = {10, 5}
    }
}

-- Define a new trait
game.define.mutation["LUA_QUICK_LEARNER"] = {
    name = "Quick Learner (Lua)",
    description = "You pick up new skills faster than most.",
    points = 3,
    startingtrait = true,
    valid = true,
    purifiable = true,
    reading_speed_multiplier = 0.8,
    skill_rust_multiplier = 0.5
}
```

**finalize.lua:**
```lua
-- Extend an existing bionic
game.define.finalize_bionic["bio_power_storage_mkIII"] = {
    copy_from = "bio_power_storage_mkII",
    name = "Mk. III Power Storage",
    description = "An even more advanced power storage unit.",
    capacity = "1000 kJ"
}
```

## Debugging Tips

1. **Check the debug log**: Errors during Lua data registration are logged with `debugmsg`.

2. **Validate your Lua syntax**: Run your scripts through a Lua interpreter first.

3. **Start simple**: Begin with minimal definitions and add fields incrementally.

4. **Use the correct phase**: If `copy_from` fails, ensure you're using `finalize_*` tables for JSON-defined bases.

5. **Check field names**: Field names must match exactly - they're case-sensitive.

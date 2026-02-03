# Lua Data Definitions

This document describes how to define and modify game data (effects, mutations, bionics, and dialogue) using Lua.

## WARNING

Modifying/Creating values typically created in JSON bypass typical validation checks, thus allowing you to shoot yourself in the foot that much easier.
Utilize the JSON dumping utility, and double check things.
Recommendation: Define as much as possible in JSON, replace as little as possible in Lua to accomplish the effect you're going for.

## Overview

Lua data definitions allow mods to programmatically create and modify game content. This is useful for:

- Dynamic content generation
- Conditional data based on other loaded mods
- Computed values that would be tedious in JSON
- Reusing patterns across multiple definitions
- Runtime modifications from hooks and callbacks

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

## Runtime Modifications

The `game.modify` API allows modifying existing data at any time during gameplay. These are **function calls**, not table assignments, and changes take effect immediately.

### Usage

```lua
-- Modify an effect_type
game.modify.effect_type("poison", {
    max_intensity = 20,
    proportional = { max_duration = 1.5 }
})

-- Modify a mutation
game.modify.mutation("NIGHTVISION", {
    night_vision_range = 10.0
})

-- Modify a bionic
game.modify.bionic("bio_power_storage", {
    capacity = "1000 kJ"
})
```

### When to Use

Runtime modifications are ideal for:

- Hooks that need to adjust game balance dynamically
- Callbacks that modify content based on game state
- Conditional changes based on player choices or world state

```lua
-- Example: In an on_game_load hook, adjust difficulty
game.add_hook("on_game_load", function()
    if some_difficulty_condition then
        game.modify.effect_type("poison", {
            base_mods = {
                pain_amount = {2, 1}  -- Increased pain
            }
        })
    end
end)
```

### How It Works

When you call `game.modify.effect_type("id", {...})`:

1. The existing object with that ID is looked up
2. Your modification table is treated as if it had `copy_from = "id"`
3. A new object is created inheriting from the original
4. Your changes are applied on top
5. The modified object replaces the original

This means you get full support for:

- `proportional` - multiply numeric values
- `relative` - add to numeric values
- `extend` - add to arrays/sets
- `delete` - remove from arrays/sets

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
    encumbrance_always = {torso = 5},
    encumbrance_covered = {legs = 3},
    restricts_gear = {"torso"},       -- Body parts needing OVERSIZE gear

    -- Armor
    armor = {
        {parts = {"torso", "arm_l", "arm_r"}, bash = 2, cut = 3}
    }
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
    capacity = "500 kJ",              -- Power bank size

    -- Activation
    activated = true,
    charge_time = 10,

    -- Stats
    stat_bonus = {STR = 3, DEX = 1},

    -- Body part effects
    occupied_bodyparts = {torso = 10, arm_l = 5, arm_r = 5},
    encumbrance = {torso = 2},

    -- Relationships
    canceled_mutations = {"HUGE"},
    included_bionics = {"bio_power_storage"},

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
    dynamic_line = "Hello there, traveler!",

    -- Or conditional:
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
            text = "Goodbye.",
            topic = "TALK_DONE"
        }
    }
}
```

## Common Patterns

### copy_from Inheritance

Extend existing definitions using `copy_from`:

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
```

### Proportional and Relative Modifiers

When using `copy_from` or `game.modify`, you can use proportional and relative modifiers:

```lua
game.define.finalize_effect_type["stronger_poison"] = {
    copy_from = "poison",
    proportional = {
        max_duration = 2.0,           -- Double the duration
        max_intensity = 1.5           -- 50% more intensity levels
    },
    relative = {
        int_decay_tick = 5            -- Add 5 to decay tick
    }
}
```

## Complete Example

**modinfo.json:**

```json
{
  "type": "MOD_INFO",
  "id": "lua_example",
  "name": "Lua Example Mod",
  "lua_api_version": 1
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
    purifiable = true
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

**main.lua:**

```lua
-- Runtime modification example
game.add_hook("on_game_load", function()
    -- Make night vision slightly better after game loads
    game.modify.mutation("NIGHTVISION", {
        night_vision_range = 6.0
    })
end)
```

## Debugging Tips

1. **Check the debug log**: Errors during Lua data registration are logged with file and line information.

2. **Validate your Lua syntax**: Run your scripts through a Lua interpreter first.

3. **Start simple**: Begin with minimal definitions and add fields incrementally.

4. **Use the correct phase**:
   - Use `game.define.*` in preload.lua for new content
   - Use `game.define.finalize_*` in finalize.lua for content that extends JSON
   - Use `game.modify.*` in main.lua or hooks for runtime changes

5. **Check field names**: Field names must match exactly - they're case-sensitive.

## Data Dumping

For tooling compatibility, Lua-defined data can be exported as JSON:

```bash
./cataclysm-bn --dump-lua-data [types...]
```

This outputs all Lua definitions as JSON to stdout, enabling tools like the HHG extractor or migration scripts to process Lua-defined content.

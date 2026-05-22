# Sky Islands Port to Cataclysm BN - Technical Documentation

## Project Overview

Porting the **CDDA Sky Islands** mod to **Cataclysm: Bright Nights**.

**Main Challenge**: Sky Islands uses DDA's Effect-on-Condition (EOC) system extensively (115 EOCs total). BN does not have EOCs - it uses Lua scripting instead.

**Source Directories**:
- CDDA Original: `/Users/gchao/code/CDDA-Sky-Islands`
- BN Port (in progress): `/Users/gchao/code/CBN-Sky-Islands`
- BN Game: `/Users/gchao/code/Cataclysm-BN`

---

## Sky Islands Mod Summary

### Gameplay Concept
A raid-based gameplay overhaul inspired by Escape from Tarkov and Dark & Darker. Players:
1. Start on a safe "Sky Island" hub
2. Use warp obelisks to teleport to dangerous locations for timed raids
3. Complete missions, collect loot
4. Return home before "warp sickness" kills them
5. Upgrade island facilities and unlock new content

### Scale of EOC Usage
- **115 unique Effect-on-Conditions** across 8 JSON files
- Complex state management with **30+ global variables**
- Sophisticated control flow including weighted selection, switch statements, conditional chains

### EOC File Distribution

| File | EOC Count | Purpose |
|------|-----------|---------|
| `EOCs.json` | 47 | Core mechanics (difficulty, healing, death, status) |
| `missions_and_mapgen.json` | 38 | Mission generation, location selection, enemy placement |
| `obelisk_selector.json` | 11 | Expedition type selection, warp location choices |
| `sickness_checks.json` | 10 | Warp sickness progression system |
| `island_upgrades.json` | 6 | Island construction and upgrades |
| `furniture_and_terrain.json` | 3 | Furniture interaction triggers |
| `dialog_statue.json` | 1 | Heart of Island dialog trigger |
| `recipes.json` | 1 | Artifact crafting |

### Core Game Mechanics Implemented via EOCs

#### 1. Teleportation Flow
```
Warp Obelisk → Raid Type Menu → Location Selection →
Actual Teleport → Mission Generation → Sickness Timer Starts
```

**Key EOCs**: `EOC_warp_statue`, `EOC_raidbegin_*`, `EOC_initiate_randomport`, `EOC_safely_landed`

#### 2. Mission Generation Flow
```
Landing Event → Create Exit Portal → Create Slaughter Mission →
Create Bonus Missions → Location Selection → Mission Assignment
```

**Key EOCs**: `EOC_safely_landed`, `EOC_create_extract`, `EOC_create_slaughter`, `EOC_create_bonus`, 38 location/mission assignment EOCs

#### 3. Warp Sickness System
Recurring timer increments sickness counter → Cascading checks apply escalating penalties → Final stage causes disintegration damage

**Key EOCs**: `EOC_constantticking`, `EOC_sicknesscheck` through `EOC_sicknesscheck6`

#### 4. Difficulty Customization
4-tier difficulty selection → Return mode choice → Emergency return method → State stored in global variables

**Key EOCs**: `EOC_difficultycheck`, `EOC_difficulty0-3`, `EOC_warphome_adjuster_*`, `EOC_warpspell_adjuster_*`

#### 5. Death & Resurrection
Player death intercepted → Check for Lifeshield Mote → If present: teleport home with healing → If not: corpse run mechanics

**Key EOCs**: `EOC_youdied` (PREVENT_DEATH type), `EOC_deathconfirmed`, `EOC_death_heal`

#### 6. Healing & Restoration
Home base healing available → Cost varies by rank → Full restoration of HP/hunger/thirst/pain/radiation/diseases

**Key EOCs**: `EOC_HEAL_NEWBIE`, `EOC_healall`, `EOC_healextras`

#### 7. Progression System
Raid completion tracking → Unlock gates at 10/20 victories → Rank progression → Feature unlocks

**Key EOCs**: `EOC_progressgate1`, `EOC_progressgate2`, `EOC_award_material_tokens`

### Critical Global Variables

The mod tracks extensive state via global math variables:

**Sickness & Timing**:
- `timeawayfromhome` - Warp pulse counter (0-12+)
- `sicknessintervals` - Pulse interval in minutes (5/10/15/30)
- `currentpulselength` - Current raid duration

**Progression**:
- `raidstotal`, `raidswon`, `raidslost` - Raid statistics
- `islandrank` - Progression tier (0-4)
- `missionswon`, `slaughterswon` - Mission completion counts
- `hardmissions`, `hardermissions`, `hardestmissions` - Difficulty unlocks

**Unlocks**:
- `longerraids` - Medium/long raids (0/1/2)
- `bonusexits`, `bonusmissions`, `bonuspulses` - Raid enhancements
- `slaughterunlocked` - Slaughter missions available
- `basementsunlocked`, `roofsunlocked`, `labsunlocked` - Location unlocks

**Configuration**:
- `roomteleportselector` - Return mode (0=whole room, 1=cost, 2=self)
- `has_set_difficulty` - Difficulty selected flag
- `goingtolabs`, `lengthofthisraid` - Current raid configuration
- `invistime` - Warpcloak duration
- `scoutingdistancelanding`, `scoutingdistancetargets` - Reveal radii

**Construction**:
- `skyisland_build_base`, `skyisland_build_bigroom` - Building state

### EOC Trigger Patterns

**Furniture Triggers**:
- `f_exitportal` (Warp Obelisk) → `EOC_warp_statue`
- `f_islandstatue` (Island's Heart) → `EOC_statue_startdialog`
- `f_returnportal` (Return Obelisk) → `EOC_statue_return`

**Item Triggers**:
- `warphome` (Lifeshield Mote) → Used by `EOC_youdied` (death check)
- `warptoken` (Warp Shards) → Consumed by `EOC_warped_return_shard_check`
- `warp_extender` (Food item) → `EOC_warpextender` (reduces sickness)

**Spell Triggers**:
- `warp_home` spell → `EOC_return_OM_teleport_check`
- `warped_return` spell → `EOC_warped_return_shard_check`

**Event Triggers**:
- Game start (scenario) → `scenario_warp_begins` initialization
- Game load → `EOC_skyisland_versioncheck`

### Advanced EOC Patterns Used

1. **Weighted EOC Selection** - Difficulty-scaled random mission selection
2. **Switch Statements** - Multi-way branching based on variable values
3. **Conditional Chains** - Complex if-then-else trees
4. **Delayed Execution** - `queue_eocs` with time delays
5. **Global Location Storage** - `u_location_variable` for coordinate tracking
6. **Map Modification** - Spawn monsters, update mapgen during raids
7. **Menu-Based Selection** - `run_eoc_selector` for player choices

---

## Cataclysm BN Lua System Summary

### Architecture

**Lua Version**: Lua 5.3.6 (bundled in `src/lua/`)
**Binding Framework**: Sol2 v3.3.0 (header-only C++ library)
**Documentation**: `docs/en/mod/lua/` (tutorial, guides, API reference)

### Mod Structure

```
data/mods/<mod_id>/
├── modinfo.json          # Mod metadata
├── preload.lua           # Hook & iuse registration (loaded first)
├── main.lua              # Main logic (hot-reloadable!)
├── finalize.lua          # Post-JSON-loading finalization
└── *.json                # Item defs, traits, furniture, etc.
```

**Load Sequence**:
1. World initialization
2. Mod list retrieved
3. **preload.lua** - Register hooks and item use functions
4. All JSON loaded
5. **finalize.lua** - Post-load modifications
6. Data validation
7. **main.lua** - Main implementation (hot-reloadable!)

### Available Hooks

BN provides 12 game event hooks (defined in `src/catalua_hooks.cpp`):

| Hook Name | Trigger Point | Parameters |
|-----------|---------------|------------|
| `on_game_load` | Save loaded | none |
| `on_game_save` | Saving | none |
| `on_game_started` | New game | none |
| `on_character_reset_stats` | Stats reset | character |
| `on_mon_death` | Monster dies | mon, killer |
| `on_char_death` | Character dies | none |
| `on_character_death` | Avatar dies | none |
| `on_creature_dodged` | Dodge | character, attacker |
| `on_creature_blocked` | Block | character, attacker |
| `on_creature_performed_technique` | Technique used | char, tech, target, dmg, cost |
| `on_creature_melee_attacked` | Melee hit | char, attacker, tech, attack, dealt_dam |
| `on_mapgen_postprocess` | Map generated | map, omt, when |

**Hook Registration** (in preload.lua):
```lua
local mod = game.mod_runtime[game.current_mod]

table.insert(game.hooks.on_game_started, function(...)
  return mod.my_hook(...)
end)
```

### Core API Namespaces

#### `gapi` - Game API
```lua
gapi.get_avatar()                    -- Get player character
gapi.get_map()                       -- Get current map
gapi.add_msg(type, message)          -- Display message
gapi.current_turn()                  -- Get current time_point
gapi.rng(min, max)                   -- Random number
gapi.create_item(itype_id, count)    -- Create item
gapi.get_creature_at(pos)            -- Get creature at position
gapi.place_monster_at(mtype, pos)    -- Spawn monster
gapi.place_player_overmap_at(pos)    -- Teleport player (overmap coords)
gapi.place_player_local_at(pos)      -- Teleport player (local map)
gapi.choose_adjacent(message)        -- Get adjacent tile from player
gapi.add_on_every_x_hook(interval, function) -- Periodic hook
```

#### `gdebug` - Debug Functions
```lua
gdebug.log_info(message)
gdebug.log_error(message)
gdebug.reload_lua_code()             -- Hot reload!
```

#### `locale` - Translation
```lua
locale.gettext(string)
locale.pgettext(context, string)
```

### Character/Creature/Monster API

**Character Methods**:
```lua
char:get_str(), :get_dex(), :get_per(), :get_int()
char:set_str_bonus(amount), :mod_str_bonus(amount)
char:get_pos_ms()                    -- Position in map squares
char:get_skill_level(skill_id)
char:get_hp(), :get_hp_max()
char:has_trait(trait_id)
char:set_mutation(mutation_id), :unset_mutation(mutation_id)
char:add_bionic(bionic_id), :remove_bionic(bionic_id)
char:add_item_with_id(itype_id, count)
char:mod_healthy(amount), :mod_fatigue(amount), :mod_pain(amount)
char:all_items(filter)               -- Inventory items
char:get_visible_creatures(), :get_hostile_creatures()
```

**Monster Methods**:
```lua
mon:get_type(), :get_name()
mon:get_pos_ms()
mon:set_hp(amount), :heal(amount)
mon:make_friendly(), :make_fungus()
```

**Map Methods**:
```lua
map:get_ter_at(pos), :set_ter_at(pos, ter_id)
map:get_furn_at(pos), :set_furn_at(pos, furn_id)
map:create_item_at(itype_id, pos, count)
map:create_corpse_at(mtype_id, pos)
map:get_items_at(pos), :remove_item_at(pos, item)
map:has_field_at(pos, field_type)
map:add_field_at(pos, field_type, intensity, age)
map:get_trap_at(pos), :set_trap_at(pos, trap_id)
```

**Item Methods**:
```lua
item:get_type()
item:tname(quantity)                 -- Translatable name
item:weight(), :volume(), :charges
item:is_active(), :activate(), :deactivate()
item:has_flag(flag_id)
item:get_var_str(key), :set_var_str(key, value)
item:get_var_tri(key, default), :set_var_tri(key, tripoint)
```

### State Management

```lua
game.mod_runtime[mod_id]             -- Per-mod runtime data (survives hot-reload)
game.mod_storage[mod_id]             -- Per-mod persistent storage (saved/loaded)
game.hooks[hook_name]                -- Hook function lists
game.iuse_functions[iuse_id]         -- Item use function registry
```

**Storage Example**:
```lua
-- In main.lua
local storage = game.mod_storage[game.current_mod]

if not storage.initialized then
  storage.initialized = true
  storage.raid_count = 0
  storage.difficulty = "normal"
end

storage.raid_count = storage.raid_count + 1
-- Automatically saved/loaded with game!
```

### Item Use Functions

**Registration** (in preload.lua):
```lua
game.iuse_functions["MY_CUSTOM_IUSE"] = function(...)
  return mod.my_iuse_impl(...)
end
```

**Implementation** (in main.lua):
```lua
mod.my_iuse_impl = function(who, item, pos)
  -- who: Character using item
  -- item: The item itself
  -- pos: Position of item

  gapi.add_msg("Item used!")
  return true  -- Success
end
```

### Periodic Hooks

For recurring timers (like warp sickness):
```lua
gapi.add_on_every_x_hook(time_duration.from_minutes(5), function()
  local storage = game.mod_storage[game.current_mod]
  storage.sickness_counter = (storage.sickness_counter or 0) + 1
  -- Apply sickness effects
end)
```

### Example Mods in BN

**Smart House Remotes** - Simple map manipulation, item use
**RPG System** - Complex state management, character progression, XP system
**Change Hairstyle** - Simple item activation
**Test for Lua Hooks** - Basic hook usage example

---

## EOC vs Lua: Key Differences

| Aspect | DDA EOCs | BN Lua |
|--------|----------|--------|
| **Language** | JSON declarative | Lua 5.3 imperative |
| **Complexity** | Limited to JSON structure | Full programming language |
| **State** | Global context variables | Native Lua tables with persistence |
| **Hooks** | Limited effect contexts | 12 named game event hooks |
| **Learning Curve** | Low (JSON only) | Medium (requires Lua knowledge) |
| **Flexibility** | Declarative only | Full procedural programming |
| **Performance** | Direct C++ execution | Interpreted Lua (slower) |
| **Hot-Reload** | No | Yes (main.lua only) |
| **Modularity** | None built-in | `require()` module system |

---

## Porting Strategy

### High-Level Approach

1. **Keep all JSON definitions** - Items, furniture, missions, traits, etc.
2. **Replace EOC JSON files** with Lua implementations
3. **Map EOC triggers** to BN Lua hooks and item use functions
4. **Implement state management** using `game.mod_storage`
5. **Recreate control flow** using Lua functions

### EOC → Lua Mapping Patterns

#### Pattern 1: Furniture Examination → Item Use Function
**EOC**: Furniture has `"examine_action": { "type": "effect_on_condition", "eoc": "EOC_warp_statue" }`

**Lua Alternative**: Change furniture to have `"use_action": "WARP_STATUE"`, register iuse function:
```lua
-- preload.lua
game.iuse_functions["WARP_STATUE"] = function(...) return mod.warp_statue(...) end

-- main.lua
mod.warp_statue = function(who, item, pos)
  -- Implementation
end
```

#### Pattern 2: Recurring Timer → Periodic Hook
**EOC**: `EOC_constantticking` runs every X minutes

**Lua**:
```lua
local tick_interval = time_duration.from_minutes(storage.difficulty_interval or 15)
gapi.add_on_every_x_hook(tick_interval, function()
  mod.warp_sickness_tick()
end)
```

#### Pattern 3: Menu Selection → UI + Conditional Logic
**EOC**: `run_eoc_selector` with menu choices

**Lua**: Use `gapi.choose_adjacent()` or implement via dialog/menu system, then conditional:
```lua
-- Simplified - may need custom UI
local choice = get_player_choice({"Short Raid", "Medium Raid", "Long Raid"})
if choice == 1 then
  mod.start_short_raid()
elseif choice == 2 then
  mod.start_medium_raid()
end
```

#### Pattern 4: Weighted Selection → Lua Weighted RNG
**EOC**: `weighted_list_eocs`

**Lua**:
```lua
function mod.weighted_select(choices)
  local total_weight = 0
  for _, choice in ipairs(choices) do
    total_weight = total_weight + choice.weight
  end

  local roll = gapi.rng(1, total_weight)
  local cumulative = 0
  for _, choice in ipairs(choices) do
    cumulative = cumulative + choice.weight
    if roll <= cumulative then
      return choice.value
    end
  end
end
```

#### Pattern 5: Global Variables → mod_storage
**EOC**: `u_set_math("raidswon", "+=", 1)`

**Lua**:
```lua
storage.raidswon = (storage.raidswon or 0) + 1
```

#### Pattern 6: Location Storage → Item Variables or mod_storage
**EOC**: `u_location_variable: { "global_val": "var", "var_name": "OM_HQ_origin" }`

**Lua**:
```lua
storage.hq_origin = {x = pos.x, y = pos.y, z = pos.z}
-- Or use item:set_var_tri() for item-specific locations
```

#### Pattern 7: Conditional Chains → if/elseif/else
**EOC**: Nested `condition`/`true_effect`/`false_effect`

**Lua**: Direct conditionals
```lua
if storage.islandrank == 0 then
  -- Free heal
elseif who:has_item_with_id("warp_shard", 4) then
  -- Paid heal
else
  gapi.add_msg("Not enough shards!")
end
```

#### Pattern 8: Death Prevention → on_character_death Hook
**EOC**: `"type": "PREVENT_DEATH"` in `EOC_youdied`

**Lua**:
```lua
table.insert(game.hooks.on_character_death, function(...)
  return mod.on_player_death(...)
end)

mod.on_player_death = function()
  local player = gapi.get_avatar()
  if player:has_item_with_id("warphome", 1) then
    -- Prevent death, teleport home
    mod.emergency_return()
    player:set_hp(player:get_hp_max())
    return true  -- Prevent death?
  end
end
```

**NOTE**: Need to verify if BN's `on_character_death` can prevent death or just react to it.

### Porting Feasibility - SUMMARY

**✅ ALL MAJOR SYSTEMS ARE PORTABLE!**

After comprehensive research, every critical Sky Islands system can be ported to BN Lua:

1. **✅ Menu System** - UiList, QueryPopup, and PopupInputStr provide complete UI
2. **✅ Mission Integration** - Full mission creation/assignment/completion API available
3. **✅ Map Modification** - Extensive terrain/furniture/item/monster spawning API
4. **✅ Death/Resurrection** - on_character_death hook + immediate resurrection works
5. **✅ Coordinate Systems** - Complete conversion library (ms/omt/om)
6. **✅ Timed Events** - gapi.add_on_every_x_hook provides recurring and one-time callbacks
7. **✅ State Persistence** - game.mod_storage provides automatic save/load
8. **✅ Island Upgrades** - Map modification API can dynamically update terrain/furniture

### Island Upgrades System

The original mod has 6 island upgrade EOCs that modify the Sky Island structure:

**Upgrade Flow**:
1. `EOC_memorize_island` - Stores 9 island OMT locations in global variables
2. `EOC_skyisland_build_base1` - Applies entrance + bunker mapgen updates
3. `EOC_skyisland_build_bigroom1-4` - Progressively expands room size

**BN Lua Implementation Options**:

**Option A: Dynamic Terrain Modification** (Most similar to original)
```lua
mod.upgrade_island_room = function(room_level)
  local island_center = storage.island_locations[5]  -- Center OMT
  local map = gapi.get_map()

  -- Apply terrain changes based on room level
  for x = -room_level, room_level do
    for y = -room_level, room_level do
      local pos = Tripoint.new(x, y, 0)
      map:set_ter_at(pos, TerIntId.new("t_floor"))
      -- Add walls, furniture, etc.
    end
  end
end
```

**Option B: Mapgen Update Sequences** (More BN-friendly)
- Define multiple mapgen JSON variants (base, room1, room2, etc.)
- Use Lua to trigger mapgen refresh with new variant
- Store current upgrade level in mod_storage

**Option C: Static + Dynamic Hybrid**
- Static base structure in mapgen JSON
- Dynamic furniture/decoration placement via Lua
- Simpler than full terrain modification

**Recommended**: Option B or C for better performance and easier maintenance

### Remaining Implementation Considerations

1. **Island Upgrades**: Choose implementation approach (see above)
2. **Portal Storm Integration**: `EOC_CANCEL_PORTAL_STORM` is undefined in original - skip or implement fresh
3. **Performance**: Lua is slower than C++; may need to optimize heavy operations
4. **Save Compatibility**: Cannot share saves between DDA and BN versions

---

## Technical Notes

### BN Lua C++ Implementation Locations
- `src/catalua.h/cpp` - Main Lua interface
- `src/catalua_hooks.h/cpp` - Hook system
- `src/catalua_bindings*.cpp` - API bindings (10+ files)
- `src/catalua_serde.h/cpp` - Save/load serialization
- `docs/en/mod/lua/` - Complete documentation

### Documentation Generation
Run BN with `--lua-doc` flag to generate `lua_doc.md` with complete API reference.

### Hot-Reload Workflow
1. Edit `main.lua`
2. In-game, press hotkey for "Reload Lua Code" or call `gdebug.reload_lua_code()`
3. Changes take effect immediately (very useful for development!)

---

## Research Findings - ANSWERS TO CRITICAL QUESTIONS

### 1. Can BN Lua create and assign missions dynamically?

**YES - Fully supported!**

Complete mission API available:
- `Mission.reserve_new(type_id, npc_id)` - Create specific mission type
- `Mission.reserve_random(origin, position, npc_id)` - Create random mission
- `Mission:assign(avatar)` - Assign mission to player
- `Mission:wrap_up()` - Complete mission successfully
- `Mission:fail()` - Fail mission
- `Avatar:get_active_missions()` - Get all active missions
- `Avatar:get_completed_missions()` - Get completed missions

**Limitations**: Cannot create new mission TYPES from Lua (must be defined in JSON), but can create and assign existing mission types dynamically.

**Conclusion**: ✅ Mission generation system is fully portable

---

### 2. Can BN Lua open interactive menus for player choice?

**YES - Comprehensive UI system available!**

Three UI systems for player interaction:

**UiList** - Multi-option menus:
```lua
local ui = UiList.new()
ui:title("Choose raid type")
ui:add(1, "Short Raid (2 hours)")
ui:add(2, "Medium Raid (4 hours)")
ui:add(3, "Long Raid (6 hours)")
local choice = ui:query()  -- Returns selected index or negative on cancel
```

**QueryPopup** - Yes/No dialogs:
```lua
local popup = QueryPopup.new()
popup:message("Really start raid?")
if popup:query_yn() == "YES" then
  -- Start raid
end
```

**PopupInputStr** - Text/numeric input:
```lua
local input = PopupInputStr.new()
input:title("Amount:")
local amount = input:query_int()
```

**Spatial Selection**:
- `gapi.choose_adjacent(message)` - Select adjacent tile
- `gapi.choose_direction(message)` - Select direction
- `gapi.look_around()` - Interactive map cursor

**Conclusion**: ✅ Complete menu system available, can replace all `run_eoc_selector` usage

---

### 3. Can `on_character_death` hook prevent death, or only react?

**NO - Cannot prevent death, but CAN resurrect immediately after!**

The hook is called AFTER death has already occurred and is finalized. Return values are ignored.

**However**, resurrection is possible:
```lua
mod.on_player_death = function()
  local player = gapi.get_avatar()
  if player:has_item_with_id("warphome", 1) then
    -- Teleport to safety
    gapi.place_player_overmap_at(safe_location)
    -- Restore health
    player:set_all_parts_hp_cur(10)
    gapi.add_msg("You respawn at home!")
  end
end
```

**Real example**: BN includes a `resurrection_mod` that demonstrates this pattern.

**Conclusion**: ✅ Death/resurrection system is portable with slight gameplay differences (death message still shows)

---

### 4. What map modification capabilities exist in Lua?

**EXTENSIVE map modification API available!**

**Terrain & Furniture**:
- `map:set_ter_at(pos, ter_id)` - Change terrain
- `map:set_furn_at(pos, furn_id)` - Change furniture

**Items & Monsters**:
- `map:create_item_at(pos, itype_id, count)` - Spawn items
- `gapi.place_monster_at(mtype, pos)` - Spawn monster at exact location
- `gapi.place_monster_around(mtype, pos, radius)` - Spawn monster nearby

**Fields & Traps**:
- `map:add_field_at(pos, field_type, intensity, age)` - Add field
- `map:set_trap_at(pos, trap_id)` - Place trap

**Coordinate System**:
- `coords.ms_to_omt(tripoint)` - Convert map squares to overmap tiles
- `coords.omt_to_ms(tripoint)` - Convert overmap tiles to map squares
- `coords.rl_dist(pos1, pos2)` - Calculate distance

**Storage**:
- `item:set_var_tri(key, tripoint)` - Store location on item
- `game.mod_storage[mod_id]` - Persistent global storage

**Hook**:
- `on_mapgen_postprocess` - Modify maps during generation

**Conclusion**: ✅ All needed map modification capabilities available

---

### 5. How to implement delayed callbacks (equivalent to `queue_eocs`)?

**YES - Multiple approaches available!**

**Method 1: One-time callback with return false**:
```lua
-- Execute once after 10 turns
gapi.add_on_every_x_hook(TimeDuration.from_turns(10), function()
  gapi.add_msg("Callback executed!")
  return false  -- Unregister after execution
end)
```

**Method 2: Recurring callback**:
```lua
-- Execute every 5 minutes
gapi.add_on_every_x_hook(TimeDuration.from_minutes(5), function()
  mod.check_warp_sickness()
  return true  -- Keep running
end)
```

**Time API**:
- `gapi.current_turn()` - Get current time_point
- `TimeDuration.from_turns/seconds/minutes/hours/days()`
- `TimePoint + TimeDuration` - Calculate future time
- `TimePoint - TimePoint` - Get duration between times

**Persistence**: Callbacks can be restored on `on_game_load` hook.

**Conclusion**: ✅ Complete timer/callback system available

---

### 6. Does BN have portal storm mechanics to integrate with?

**UNKNOWN** - Not researched as part of core porting needs.

`EOC_CANCEL_PORTAL_STORM` is referenced but undefined in original mod. This appears to be a placeholder feature or incomplete. Can implement if needed or skip entirely for initial port.

---

## Development Roadmap

### Phase 1: Research & PoC ✅ COMPLETE
- [x] Answer critical questions (ALL ANSWERED - see Research Findings above)
- [x] Research complete - ALL systems are portable!
- [x] Create minimal teleport system
- [x] Implement basic sickness timer
- [x] Test state persistence across save/load

### Phase 2: Core Systems ✅ COMPLETE
- [x] Full teleportation flow
- [x] Mission generation system (3 mission types)
- [x] Warp sickness progression (13 stages, 5-minute intervals)
- [x] Return home mechanics
- [x] Death/resurrection system

### Phase 3: Progression & Resources ✅ COMPLETE
- [x] Infinity nodes (tree, stone, ore)
- [x] Material token economy (50 tokens per raid)
- [x] Resource conversion recipes (28 recipes validated for BN)
- [x] Heart of the Island interactive menu
- [x] Progress gates (automatic rank-ups at 10 and 20 raids)
- [x] Rank-up challenges (Proof of Determination/Mastery)

### Phase 4: Content & Polish - TODO
- [ ] Raid duration selection (short/medium/long)
- [ ] Difficulty customization system
- [ ] Additional mission types
- [ ] Island structure/mapgen
- [ ] Vortex tokens (room teleport mechanic)
- [ ] Warped hauler bag and other warp items
- [ ] Balance and testing

---

## Implementation Progress (Current Session)

### Completed Features

**1. Core Game Loop** (fully functional):
- Warp obelisk → expedition → 3 missions → return obelisk → rewards
- Material tokens: 50 per successful return
- Warp sickness: 13 stages, starts at minute 30, lethal at minute 55+
- Death protection: respawn at home with penalties

**2. Modular Lua Architecture**:
- `preload.lua` - iuse registration + hooks
- `main.lua` - integration layer (92 lines)
- `missions.lua` - mission system (248 lines)
- `warp_sickness.lua` - sickness progression (121 lines)
- `teleport.lua` - warp/return logic (224 lines)
- `heart.lua` - Heart menu system (236 lines)

**3. Infinity Node System**:
- 3 furniture types (deployable, movable)
- 28 material conversion recipes (all validated against BN vanilla)
- Resource categories: wood (5), metal (10), stone/earth (8)
- 3 crafting recipes for the nodes themselves

**4. Heart of the Island**:
- Interactive menu with Services/Information/Rank-Up Challenges
- Free healing until rank 1, then costs 4 warp shards
- Expedition statistics display
- Game mechanics explanations

**5. Progress System**:
- Automatic rank-ups: Novice (0-9), Adept (10-19), Master (20+)
- Rank-up challenges with tool quality requirements
- Stats tracking: total/won/lost raids, success rate

### BN Compatibility Fixes

**Items Removed from CDDA Version** (don't exist in BN):
- `wooden_post_short`, `wooden_post` - no equivalent in BN
- `nuts_bolts` - BN doesn't have this item
- `rock_large` - only `rock` exists in BN

**Recipe Components Needing Replacement** (user will fix):
- `twig`, `leaves` → need BN alternatives for infinity tree recipe
- `rock_flaking`, `rock_large` → replace with `rock` for infinity stone
- `nuts_bolts`, `pipe_fittings` → replace with scrap/metal for infinity ore

**Validated BN Resources** (reference in `/tmp/bn_valid_resources.json`):
- Wood: 2x4, log, stick_long, wood_beam, wood_panel
- Metal: frame, hdframe, nail, pipe, scrap, scrap_copper, sheet_metal, steel_chunk, steel_lump, wire
- Stone/Earth: adobe_brick, brick, clay_lump, material_cement, material_limestone, material_sand, material_soil, rock

### Known Issues

1. **Recipe components need fixing** (user handling):
   - skyisland_folded_infinitree: uses `twig`, `leaves`
   - skyisland_folded_infinitystone: uses `rock_flaking`, `rock_large`
   - skyisland_folded_infinityore: uses `nuts_bolts`, `pipe_fittings`

2. **Not yet implemented**:
   - Raid duration selection
   - Difficulty settings
   - Vortex tokens / room teleport
   - Warped items (hauler bag, animal carrier, etc.)
   - Island mapgen/structure

### File Locations

**Mod Directory**: `/Users/gchao/Library/Application Support/Cataclysm-BN/mods/CBN-Sky-Island`
**BN Source**: `/Users/gchao/code/Cataclysm-BN`
**CDDA Original**: `/Users/gchao/code/CDDA-Sky-Islands`

---

*This document will be updated as the port progresses.*

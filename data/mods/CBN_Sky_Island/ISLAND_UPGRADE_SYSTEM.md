# Sky Islands - Island Upgrade System Design

*Research from CDDA Sky Islands mod - Design notes for BN port*

## Overview

The island upgrade system is a permanent progression mechanic where players invest resources earned during expeditions to unlock enhancements that apply to all future raids. The system centers around the **Island's Heart** (`f_islandstatue`), a furniture piece that serves as the upgrade hub.

## Core Mechanism

### Upgrade Flow
1. Player interacts with Island's Heart furniture
2. Selects an upgrade from dialogue menu
3. Receives a mission (e.g., `SKYISLAND_UPGRADE_scouting1`)
4. Learns a one-time crafting recipe (e.g., `upgradekey_scouting1`)
5. Crafts the upgrade artifact near the Heart
6. Upon crafting completion, mission auto-completes and upgrade activates permanently
7. Recipe is forgotten (prevents duplicate upgrades)

### Key Files (CDDA Reference)
- `upgrade_missions.json` - All upgrade mission definitions
- `dialog_statue.json` - Dialogue tree and upgrade menus
- `EOCs.json` - Effect-on-Condition logic for upgrade activation
- `furniture_and_terrain.json` - Island Heart definition

## Currency System

### Warp Shards (`warptoken`)
- Small obsidian-like flakes
- Primary upgrade currency
- Earned from mission completion during expeditions
- Also used for healing services at the Heart (4 shards after Rank 1)

**Costs Examples:**
- Early upgrades: 2-5 shards
- Mid-tier: 8-15 shards
- High-tier: 25+ shards

### Material Tokens (`warptoken_material`)
- Blue wafer-shaped tokens
- Awarded for surviving raids (50-75+ per expedition)
- Converted to resources via Infinity Sources (wood, stone, metal)
- Used for island construction, not stat upgrades

## Progression Gates: Island Rank

```
Rank 0: Starting state
  - Basic upgrades available
  - Free healing

Rank 1: Unlocked after 10 successful raids
  - Tier 2 upgrades available
  - New recipes: animal carriers, vortex tokens
  - Healing costs 4 shards

Rank 2: Unlocked after 20 successful raids
  - Tier 3 upgrades available
  - Extended expeditions unlocked
  - Rooftop expeditions available
  - Construction slots 3-4

Rank 3+: Future progression
  - Not fully implemented in CDDA

Rank 4: Elite tier
  - Hardest mission difficulty
  - Lab expeditions
  - Maximum upgrade levels
```

**Variables:**
- `islandrank` (0-4) - Current rank
- `raidswon` - Successful raid counter (triggers rank-ups at 10, 20)

## Upgrade Categories

### 1. SCOUTING (5 levels)
**Purpose:** Reveals more map area around mission targets

**Progression:**
- Level 1: Slight increase (+1 radius)
- Level 2: Bit more (+2 radius)
- Level 3: Even more (+3 radius, requires Rank 2+)
- Level 4: Lot more (+4 radius)
- Level 5: Huge radius (+10 radius, requires Rank 4+)

**Variable:** `scoutingdistancetargets` (0 → 10)

**Example Recipe (Scouting 1):**
- High-quality glasses + pipe + 5 wire + 5 tinder + cordage
- Craft time: 15 minutes
- Must be near Island's Heart

### 2. LANDING (5 levels)
**Purpose:** Reveals more map at landing point + extends warpcloak duration

**Progression:**
- Level 1: +10s warpcloak (70s total), slight map reveal
- Level 2: +20s warpcloak (80s total), better map
- Level 3: +30s warpcloak (90s total), even better
- Level 4: +40s warpcloak (100s total), lot better
- Level 5: +60s warpcloak (120s max), huge reveal

**Variables:**
- `scoutingdistancelanding` (0 → 24)
- `invistime` (60 → 120 seconds)

**Warpcloak Effect:** Temporary invisibility/safety period when arriving at expedition

### 3. STABILITY (4 levels)
**Purpose:** Extend expedition time before warp sickness becomes lethal

**Progression:**
- Level 1: +1 pulse (9 total) - 12.5% longer expeditions
- Level 2: +2 pulses (10 total) - 25% longer
- Level 3: +3 pulses (11 total) - 37.5% longer
- Level 4: +4 pulses (12 total) - 50% longer

**Variable:** `bonuspulses` (0 → 4)

**Cost Scaling:**
- Stability 3: 15 warp shards
- Stability 4: 25 warp shards (most expensive stat upgrade)

### 4. MISSION UNLOCKS

#### Mission Count (3 levels)
**Purpose:** Add extra missions per expedition

- Unlock 1: +1 mission (1 total)
- Unlock 2: +2 missions (2 total)
- Unlock 3: +3 missions (3 total, requires Rank 2+)

**Variable:** `bonusmissions` (0 → 3)

#### Mission Difficulty Tiers (2 unlocks)
**Purpose:** Unlock harder missions with better rewards

- Harder Missions: Tier 2 difficulty (requires Rank 2+)
- Hardest Missions: Tier 3 difficulty (requires Rank 4+)

**Variables:** `hardmissions`, `hardermissions`, `hardestmissions`

### 5. EXIT VARIETY (2 levels)
**Purpose:** Add more exit portals per expedition

- Escape Charm: +1 exit (2 total)
- Blindfold of Egress: +1 exit (3 total, requires Rank 2+)

**Variable:** `bonusexits` (0 → 2)

### 6. EXPEDITION TYPES

#### Length Variants
- Large Expeditions: Extended raid option 1 (Rank 2+)
- Extended Expeditions: Extended raid option 2 (Rank 4+)

**Variable:** `longerraids` (0-2)

#### Start Locations
- Basement Unlock: Start in basements
- Rooftop Unlock: Start on rooftops (Rank 2+)
- Labs Unlock: Start in labs (Rank 4+)

**Variables:** `basementsunlocked`, `roofsunlocked`, `labsunlocked`

### 7. SPECIAL MISSIONS

#### Slaughter Missions
**Purpose:** Unlock kill-count based missions

**Effect:** Adds one special slaughter mission per expedition targeting specific enemy types, rewards warp shards

**Variable:** `slaughterunlocked` (0 or 1)

### 8. ISLAND CONSTRUCTION

Physical expansions to the sanctuary (separate from stat upgrades):

1. **Bunker Entrance** - Base structure with stairs
2. **Main Room 1** - First expansion
3. **Main Room 2** - Second expansion
4. **Main Room 3** - Third expansion (Rank 2+)
5. **Main Room 4** - Fourth expansion (Rank 4+)

**Variables:**
- `skyisland_build_base` (0-1)
- `skyisland_build_bigroom` (0-4)

## Technical Implementation

### Crafting System

**Category:** `CC_WARP` / `CSC_WARP_UPGRADES`

**Requirements:**
- Must be within crafting range of Island's Heart
- Tool: `fakeitem_statue` (provided by nearby Heart furniture)

**Recipe Flags:**
- `SECRET` - Hidden until mission learned
- `BLIND_EASY` - Can craft without penalties
- `REVERSIBLE: false` - One-time use only

### EOC (Effect-on-Condition) Triggers

When upgrade artifact is crafted:
1. Set global variable (e.g., `math: [ "bonuspulses", "=", "1" ]`)
2. Send player notification
3. Auto-complete associated mission
4. Forget recipe to prevent duplicates

### Dialogue Structure

```
Island's Heart Menu
├── View Upgrades
│   └── Shows progress on all categories
├── Empower the warp obelisk
│   ├── Expedition length unlocks
│   ├── Start location unlocks
│   └── Security tiers (future)
├── Upgrade expeditions
│   ├── Stability (4 levels)
│   ├── Scouting (5 levels)
│   ├── Landing (5 levels)
│   ├── Exits (2 levels)
│   ├── Missions (3 levels)
│   ├── Mission Tiers (2 levels)
│   └── Slaughter Missions
├── Construction options
│   ├── Bunker entrance
│   └── Main rooms 1-4
└── Services
    ├── Healing (free at Rank 0, 4 shards after)
    ├── End portal storm
    ├── View stats
    ├── View upgrade progress
    └── Difficulty settings
```

## Global Variables Summary

**Expedition Features:**
- `scoutingdistancetargets` (0-10) - Target reveal radius
- `scoutingdistancelanding` (0-24) - Landing reveal radius
- `invistime` (60-120) - Warpcloak duration in seconds
- `bonuspulses` (0-4) - Extra warp pulses
- `bonusexits` (0-2) - Extra exits
- `bonusmissions` (0-3) - Missions per expedition
- `slaughterunlocked` (0-1) - Slaughter missions available
- `hardmissions` (0, 10, 15) - Tier 2 difficulty state
- `hardermissions` (0, 5, 15) - Tier 2 alternate state
- `hardestmissions` (0, 10) - Tier 3 difficulty state
- `longerraids` (0-2) - Extended expedition levels
- `basementsunlocked` (0-1)
- `roofsunlocked` (0-1)
- `labsunlocked` (0-1)

**Progression:**
- `islandrank` (0-4) - Rank level
- `raidswon` - Successful raid counter

**Construction:**
- `skyisland_build_base` (0-1) - Bunker entrance built
- `skyisland_build_bigroom` (0-4) - Main rooms built

## Design Philosophy

### Progression Curve
- **Early game:** Focus on basic QoL (scouting, landing, one exit)
- **Mid game:** Expand mission variety and expedition length
- **Late game:** Maximize efficiency (multiple missions, multiple exits, extended time)

### Resource Sink
Warp shards create a meaningful grind where:
- Early upgrades are cheap (2-5 shards)
- Mid-tier costs scale (8-15 shards)
- High-tier are expensive (25+ shards)
- Forces prioritization of upgrades
- Rewards consistent expedition completion

### Rank Gating
Prevents rushing to end-game content:
- Rank 1 (10 raids): Opens tier 2
- Rank 2 (20 raids): Opens tier 3 + extended content
- Rank 4: Elite tier for experienced players

## BN Port Considerations

### Lua API Availability
**Available:**
- Global variable storage (via `game.mod_storage`)
- Item use functions for Heart interaction
- UI menus (UiList)
- Mission creation and tracking

**Need to Check:**
- Recipe learning/forgetting from Lua
- EOC equivalents (may need custom Lua hooks)
- Dialogue tree system (may need UI menus instead)
- Mission completion triggers on item craft

### Simplifications for PoC
1. Start with 2-3 upgrade types (scouting, stability, mission count)
2. Use simple crafting costs (warp shards only, no complex components)
3. Implement Island's Heart as item use function rather than dialogue tree
4. Store all variables in `storage` table
5. Defer construction system to later version

### Future Expansion Path
1. **Phase 1 (PoC):** Basic stat upgrades (scouting, stability)
2. **Phase 2:** Mission unlocks, exit variety
3. **Phase 3:** Expedition type variants (length, start locations)
4. **Phase 4:** Construction system
5. **Phase 5:** Advanced features (slaughter missions, difficulty tiers)

## Example Implementation: Scouting 1

```lua
-- In main.lua
mod.upgrade_scouting1 = function(who, item, pos)
  if storage.upgrade_scouting >= 1 then
    gapi.add_msg("You already have this upgrade!")
    return 0
  end

  -- Check for warp shards
  if not who:has_charges("warptoken", 2) then
    gapi.add_msg("You need 2 warp shards for this upgrade.")
    return 0
  end

  -- Consume shards
  who:use_charges("warptoken", 2)

  -- Apply upgrade
  storage.upgrade_scouting = 1
  storage.scoutingdistancetargets = 1

  gapi.add_msg("Your vision expands... Mission targets will be more visible!")
  gdebug.log_info("Scouting 1 upgrade applied")

  return 1
end
```

## Resource Costs Reference (from CDDA)

### Low Tier (0-5 shards)
- Scouting 1: 0 shards (misc items only)
- Scouting 2: 2 shards
- Landing 1-2: 0-3 shards
- Mission Unlock 1: 0 shards

### Mid Tier (5-15 shards)
- Scouting 3: 5 shards
- Scouting 4: 8 shards
- Stability 3: 15 shards
- Landing 3-4: 5-10 shards

### High Tier (15-25+ shards)
- Scouting 5: 12 shards
- Stability 4: 25 shards
- Hardest Missions: 20+ shards
- Extended Expeditions: 15+ shards

---

*This document is a design reference for implementing the upgrade system in Cataclysm BN. The original CDDA implementation uses JSON dialogue trees and EOCs, while the BN port will use Lua for flexibility.*

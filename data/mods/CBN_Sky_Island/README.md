# Sky Islands - Cataclysm: Bright Nights Port

A port of the Sky Islands mod from Cataclysm: Dark Days Ahead to Cataclysm: Bright Nights.

**Ported from**: [CDDA Sky Islands](https://github.com/TGWeaver/CDDA-Sky-Islands) by TGWeaver

## Compatibility
Requires [CBN Nightly 2025-12-23](https://github.com/cataclysmbn/Cataclysm-BN/releases/tag/2025-12-23) or later!

## Overview

Sky Islands is a roguelike raid-loop mod. You live on a floating island sanctuary and teleport down to the zombie-infested surface for timed expeditions. Complete missions, gather resources, and return home before warp sickness kills you. Die during a raid and you respawn at home - but lose everything you were carrying (except tokens).

## Features

### Core Gameplay

- **Floating Island Sanctuary**: Your home base, safe from zombies. Contains the Heart of the Island (upgrade hub) and warp obelisks for travel.
- **Timed Expeditions**: Teleport to the surface for raids. Warp sickness builds over time - stay too long and you die.
- **Death Protection**: Die during a raid? You respawn at home, but lose all carried items except warp shards and material tokens.
- **Mission System**: Each expedition includes extraction, slaughter, and bonus missions for rewards.

### Expeditions

- **Raid Types**: Short (1x time), Medium (1.5x time), and Extended (2x time) expeditions with scaling rewards.
- **Starting Locations**:
  - Field (always available)
  - Basement (unlock required)
  - Rooftop (unlock required)
  - Science Lab (unlock required, costs catalyst, high risk/reward)
- **Warp Sickness**: 8-pulse grace period, then escalating debuffs leading to disintegration. Stability upgrades extend the grace period.

### Economy

- **Warp Shards**: Primary currency earned from missions. Used for upgrades, healing, and crafting.
- **Material Tokens**: Earned on successful return (50/75/100 based on raid length). Convert to raw materials at infinity nodes.
- **Vortex Tokens**: Rare currency for special items.

### Infinity Nodes

Deployable furniture that converts material tokens to resources:
- **Infinity Tree**: Logs, planks, sticks, wooden beams
- **Infinity Stone**: Rocks, clay, sand, soil, bricks, cement
- **Infinity Ore**: Scrap metal, steel, pipes, wire, nails, frames

### Upgrades (Craft Near Heart of Island)

- **Stability (3 levels)**: +2 grace period pulses per level
- **Scouting (2 levels)**: Reveal more map tiles around extraction points
- **Scouting Clairvoyance (2 levels)**: Temporary omniscience on landing (10s/20s)
- **Multiple Exits**: 2 extraction points per expedition instead of 1
- **Expedition Length (2 levels)**: Unlock Medium and Extended raids
- **Landing Flight**: 60 seconds of flight when landing on expeditions
- **Bonus Missions (5 tiers)**: Unlock additional mission types with better rewards
- **Hard Missions (2 tiers)**: Unlock challenging missions with higher payouts
- **Slaughter Missions**: Kill-count missions for bonus shards
- **Location Unlocks**: Basement starts, Rooftop starts, Lab starts

### Heart of the Island

Central hub with menus for:
- **Construction**: Place infinity nodes and other structures
- **Upgrades**: View upgrade progress and requirements
- **Services**: Healing (free at Rank 0, costs shards later), expedition statistics
- **Difficulty Settings**: Customize warp pulse timing, return behavior, emergency return options
- **Information**: Lore and gameplay explanations
- **Rank-Up Challenges**: Progress gates at 10 and 20 successful raids

### Difficulty Options

- **Warp Pulse Timing**: Casual (30min), Normal (15min), Hard (10min), Impossible (5min)
- **Return Behavior**: Whole room teleports with you, or just yourself
- **Emergency Return**: Free beacon, costs shards, craft-only, or extraction-only

### Utility Items

- **Homeward Mote**: One-time death insurance - keeps all items on death
- **Earthbound Pill**: Extends time on expedition (+4 pulses)
- **Warp Status Crystal**: Shows detailed expedition status
- **Skyward Beacon**: Emergency return home (crafted)
- **Warp Home Focus**: Reusable return item (for certain difficulty modes)
- **Animal Teleporter**: Capture and teleport friendly creatures to your island

### Quality of Life

- **Token Preservation**: Warp shards, material tokens, and vortex tokens survive death
- **Red Room Item Teleport**: Items dropped in the red room near return obelisks teleport home with you
- **State Persistence**: All progress saves correctly across game sessions

## Getting Started

1. Start a new game with the "Sky Island Warper" scenario
2. You begin on your floating island sanctuary
3. Use the warp obelisk to start your first expedition
4. Complete missions and return via the extraction point before warp sickness kills you
5. Use earned shards and tokens to craft upgrades and gather resources

## Known Issues

### Scenario Selection
When creating a character, manually select "Sky Island Warper" from the scenario list - the game may default to a different scenario.

## Troubleshooting

### Mod won't load
- Check `debug.log` for Lua errors
- Verify you're running a Cataclysm-BN build with Lua mod support

### Debug Logging
To enable verbose debug output, edit `util.lua` and set:
```lua
util.DEBUG = true
```

## Development

This is an active port. Most core features from CDDA Sky Islands are implemented. Some advanced features may still be in progress.

### Files
- `main.lua` - Entry point, hooks, item activations
- `teleport.lua` - Warp obelisk, return logic, resurrection
- `missions.lua` - Mission creation and rewards
- `warp_sickness.lua` - Warp pulse timing and effects
- `heart.lua` - Heart of the Island menus
- `upgrades.lua` - Upgrade definitions and activation
- `util.lua` - Shared utilities (debug logging)
- `preload.lua` - Hook and iuse registration

## Credits

This mod is a port of a mod which was originally created by TGWeaver (who gave permission for others to use and reuse the mod) and which has received contributions by many more. I've done my best to collect the full list of those who have contributed to the original Sky Island for CDDA; as a humble porter, I owe them a deep debt of gratitude. To wit, these people are:

- TGWeaver
- adamkad1
- lettucegoblin
- Standing-Storm
- wave
- adamkad1
- andrei
- Anton Burmistrov
- Anton Simakov
- b3brodie
- BalthazarArgall
- Blueflowers
- Brian Ackermann
- Clarence "Sparr" Risher
- Consoleable
- David William Bull
- Dots
- DukePaulAtreid3s
- ehughsbaird
- gettingusedto
- gimy77357-netizen
- GuardianDll
- Holli
- I-am-Erk
- Karol1223
- Maleclypse
- Milopetilo
- MrHrulgin
- Naadn
- NetSysFire
- Oleksii Filonenko
- Procyonae
- Qrox
- Ramza13
- RenechCDDA
- Sab Pyrope
- sadenar
- SariusSkelrets
- ShnitzelX2
- Standing-Storm
- thaelina

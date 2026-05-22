# Sky Island BN Port - Feature Status

## Implemented
- [x] Basic island spawning (sky_island_core at z=10, subcore at z=9)
- [x] Warp obelisk teleportation (expeditions)
- [x] Return obelisk (return home from raids)
- [x] Warp sickness system (pulse timer, escalating debuffs)
- [x] Heart of the Island (services menu, healing, stats)
- [x] Rank-up system (Novice -> Apprentice -> Journeyman -> Expert -> Master)
- [x] Infinity nodes (tree, stone, ore) for resource crafting
- [x] Material token economy
- [x] Basement construction + 4 expansions (mission-based mapgen)
- [x] Raid targeting with overmapbuffer search
- [x] Red room item teleport (items in red room teleport home with player)
- [x] Longer raids upgrade (Large/Extended expeditions with more time)
- [x] Stability upgrades (+2/+4/+6 bonus grace pulses)
- [x] Scouting upgrades (reveal 3x3 or 5x5 overmap area on landing)
- [x] Multiple exits upgrade (2 return obelisks per expedition)

## Missing / TODO

### High Priority (Core Gameplay)
- [ ] **Difficulty settings menu**: Let player tweak expedition parameters
  - Pulse frequency, warp sickness severity, distance ranges, etc.
  - DDA has this under "Change my difficulty settings" in Heart

- [ ] **Alternative raid start locations**:
  - Basements (unlockable)
  - Rooftops (unlockable, rank 2+)
  - Labs (unlockable, rank 4+)
  - Currently only surface raids implemented

### Medium Priority (Upgrades & Progression)
- [ ] **Landing upgrades**: Choose landing spot more precisely
- [ ] **Security containers**: Alpha through Kappa (500ml-2L integrated storage)
  - Portable secure storage that persists through death

### Lower Priority (Polish)
- [ ] **Crafting category tab**: Dedicated "Sky Island" crafting category
- [ ] **Heartseed consumption**: Should disappear after building 1 heart
- [ ] **Portal storm cancellation**: Service to end active portal storms

## Notes
- BN lacks EOCs, so we use Lua + mission system for mapgen triggers
- Many DDA features rely on global variables and EOC chains
- Some features may need creative workarounds or may not be portable

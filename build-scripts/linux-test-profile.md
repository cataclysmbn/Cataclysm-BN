# Linux test shard profile

Source: PR #9515 run 27586856606, `clang, tiles, i18n` job 81560362204.

## Observed shard timings

`#` bars are scaled at about 10 seconds each. The job failed before every shard completed.

| Time | Graph | Filter summary |
| ---: | :--- | --- |
| 217s | ###################### | `[#vehicle_rails_test]` |
| 184s | ################## | non-slow shard: bionics/crafting/overmap/etc. |
| 146s | ############### | non-slow shard: active_item/cata_utility/player_activities/etc. |
| 141s | ############## | non-slow shard containing `[#vehicle_test]` (failed) |
| 132s | ############# | non-slow shard: algo/melee/vehicle_collision/etc. |
| >90s | #########... | non-slow shard: calendar/map/projectile/vehicle_part/etc. (canceled) |

## Failure blocker

`[#vehicle_test]` fails when run independently or in a shard. The failing case is
`horde_spawns_skip_owned_vehicle_tiles`; the horde spawn fixture does not leave a valid
vehicle tile for the expected monster spawn/requeue behavior.

PR #9519 changes absolute-space/mapbuffer vehicle lookup paths, but this failing assertion
uses the active `map::veh_at( tripoint_bub_ms )` horde-spawn path, so #9519 is not expected
to fix this specific failure.

## Prioritized plan

1. Fix or quarantine `horde_spawns_skip_owned_vehicle_tiles` so `[#vehicle_test] ~[.]` passes
   standalone with the CI flags.
2. Re-profile file-tag shards after the vehicle fix; keep the actual CI command and flags.
3. Split the remaining >180s file-tag shards before adding more parallelism.
4. Benchmark `--option_overrides=REALITY_BUBBLE_SIZE:2` only on map/visibility-heavy shards;
   keep failing or reality-bubble-sensitive tags on the default bubble.
5. Avoid `REALITY_BUBBLE_SIZE:1` until a focused compatibility pass proves it does not change
   test contracts.

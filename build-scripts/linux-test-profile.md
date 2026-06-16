# Linux test shard profile

Sources:
- Baseline: old Linux non-slow shard from successful PR run: `706.837s`.
- CI profile: PR #9515 run 27586856606, `clang, tiles, i18n` job 81560362204.
- Local profile: rebased branch at 69d0c9ec6d9, `cata_test-tiles --filenames-as-tags --rng-seed 1 --gpu-backend=software`.

## CI profile before fixes

The job failed before every shard completed.

| Time | Graph | Filter summary |
| ---: | :--- | --- |
| 217s | ###################### | `[#vehicle_rails_test]` |
| 184s | ################## | non-slow shard: bionics/crafting/overmap/etc. |
| 146s | ############### | non-slow shard: active_item/cata_utility/player_activities/etc. |
| 141s | ############## | non-slow shard containing `[#vehicle_test]` (failed) |
| 132s | ############# | non-slow shard: algo/melee/vehicle_collision/etc. |
| >90s | #########... | non-slow shard: calendar/map/projectile/vehicle_part/etc. (canceled) |

## Local shard profile after vehicle/map fixture fixes

`#` bars are scaled at about 10 seconds each. Local GNU `parallel` was unavailable, so shards
were run sequentially; CI still runs them with `--jobs 6`.

| Time | Graph | Shard |
| ---: | :--- | --- |
| 138s | ############## | `04-non-slow` |
| 88s | ######### | `08-non-slow` |
| 80s | ######## | `00-vehicle-rails` |
| 58s | ###### | `05-non-slow` |
| 57s | ###### | `06-non-slow` |
| 53s | ##### | `07-non-slow` |
| 39s | #### | `03-non-slow` |
| 38s | #### | `01-non-slow` |
| 38s | #### | `21-vehicle-efficiency` |
| 30s | ### | `31-slow` |
| 29s | ### | `22-starting-items` |
| 29s | ### | `02-non-slow` |
| 28s | ### | `20-visibility` |
| 10s | # | `34-slow` |
| 5s | # | `32-slow` |
| 3s | # | `33-slow` |

## Resolved blockers

- `[#vehicle_test] ~[.]` now passes standalone with CI flags.
- `[#map_test] ~[.]` now passes standalone with CI flags after refreshing the active vehicle
  cache in the manual part-install fixture.
- Local verification covered the previously failing `06-non-slow` shard and remaining shards
  `07`, `08`, `20`, `21`, `22`, `31`, `32`, `33`, and `34`.

## Prioritized plan

1. Wait for PR CI timings from the rebased branch before changing shard weights again.
2. If CI still has a >180s shard, split `04-non-slow` first.
3. Benchmark `--option_overrides=REALITY_BUBBLE_SIZE:2` only on map/visibility-heavy shards;
   keep failing or reality-bubble-sensitive tags on the default bubble.
4. Avoid `REALITY_BUBBLE_SIZE:1` until a focused compatibility pass proves it does not change
   test contracts.

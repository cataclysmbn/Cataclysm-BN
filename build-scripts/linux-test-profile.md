# Linux test shard profile

Sources:

- Baseline: old Linux non-slow shard from successful PR run: `706.837s`.
- CI profile: PR #9515 run 27586856606, `clang, tiles, i18n` job 81560362204.
- CI resource check: PR #9515 run 27596058861 showed llvmpipe-backed shards stalling under four unrestricted software-GPU processes.
- Local profile: rebased branch at 69d0c9ec6d9, `cata_test-tiles --filenames-as-tags --rng-seed 1 --gpu-backend=software`.

## CI profile before fixes

The job failed before every shard completed.

| Time | Graph                  | Filter summary                                                       |
| ---: | :--------------------- | -------------------------------------------------------------------- |
| 217s | ###################### | `[#vehicle_rails_test]`                                              |
| 184s | ##################     | non-slow shard: bionics/crafting/overmap/etc.                        |
| 146s | ###############        | non-slow shard: active_item/cata_utility/player_activities/etc.      |
| 141s | ##############         | non-slow shard containing `[#vehicle_test]` (failed)                 |
| 132s | #############          | non-slow shard: algo/melee/vehicle_collision/etc.                    |
| >90s | #########...           | non-slow shard: calendar/map/projectile/vehicle_part/etc. (canceled) |

## Local shard profile after vehicle/map fixture fixes

`#` bars are scaled at about 10 seconds each. CI runs most shards with CPU compute, keeps
software-GPU compute for `20-visibility`, and uses 16 generated non-slow shards to shorten
individual Linux test processes.

| Time | Graph          | Shard                       |
| ---: | :------------- | --------------------------- |
| 138s | ############## | `04-non-slow`               |
|  88s | #########      | `08-non-slow`               |
|  58s | ######         | `05-non-slow`               |
|  57s | ######         | `06-non-slow`               |
|  53s | #####          | `07-non-slow`               |
|  39s | ####           | `03-non-slow`               |
|  38s | ####           | `01-non-slow`               |
|  38s | ####           | `21-vehicle-efficiency`     |
|  30s | ###            | `31-slow`                   |
|  30s | ###            | `00-vehicle-rails-basic`    |
|  29s | ###            | `22-starting-items`         |
|  29s | ###            | `02-non-slow`               |
|  28s | ###            | `20-visibility`             |
|  24s | ##             | `19-vehicle-rails-shifting` |
|  18s | ##             | `09-vehicle-rails-fork`     |
|  13s | #              | `29-vehicle-rails-other`    |
|  10s | #              | `34-slow`                   |
|   5s | #              | `32-slow`                   |
|   3s | #              | `33-slow`                   |

## Local full sharded validation after CI shard-size reduction

Command: `build-scripts/run-linux-test-shards.sh --mode file-tags --jobs 4 --non-slow-shards 16 ./out/build/linux-full/tests/cata_test-tiles -- --min-duration 20 --use-colour no --rng-seed 1 --error-format=github-action --gpu-backend=software`

Result before the `vehicle_drag` fixture reuse: all shards passed in `6:52.40` locally. Longest shards:

| Time | Shard                   |
| ---: | ----------------------- |
| 165s | `12-non-slow`           |
| 123s | `16-non-slow`           |
|  98s | `21-vehicle-efficiency` |
|  71s | `06-non-slow`           |
|  67s | `15-non-slow`           |

After reusing a single map in `vehicle_drag`, standalone `[#vehicle_drag_test] ~[.]` with CPU
compute dropped from `121s` to `16.33s` wall time locally.

For local runs, `build-scripts/run-linux-test-shards.sh` defaults to the linux-full tiles test
binary, standard CI-like test options, detected CPU-count `--jobs`, and generated non-slow shards as
`jobs * 2` clamped to `6..16`. CI still passes explicit `--jobs 4 --non-slow-shards 16` for runner
stability.

## Resolved blockers

- `[#vehicle_test] ~[.]` now passes standalone with CI flags.
- `[#map_test] ~[.]` now passes standalone with CI flags after refreshing the active vehicle
  cache in the manual part-install fixture.
- Local verification covered the previously failing `06-non-slow` shard, the split vehicle-rails
  shards, and remaining shards `07`, `08`, `20`, `21`, `22`, `31`, `32`, `33`, and `34`.

## Prioritized plan

1. Wait for PR CI timings with 16 generated non-slow shards before changing shard weights again.
2. If CI still has a >180s shard, split the longest generated non-slow shard first.
3. If runner shutdown continues, isolate `[#map_test]` / `[#vehicle_efficiency_test]`.
4. Benchmark `--option_overrides=REALITY_BUBBLE_SIZE:2` only on map/visibility-heavy shards;
   keep failing or reality-bubble-sensitive tags on the default bubble.
5. Avoid `REALITY_BUBBLE_SIZE:1` until a focused compatibility pass proves it does not change
   test contracts.

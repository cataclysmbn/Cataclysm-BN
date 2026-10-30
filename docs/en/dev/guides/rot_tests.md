# Rot regression tests and coverage

Run the rot tests from the repository root:

```sh
cmake --preset linux-full
cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  out/build/linux-full/tests/cata_test-tiles --filenames-as-tags \
  '[rot],[#rot_test],[messages]' --rng-seed 10278
```

The tests use fixed fixtures in `data/mods/TEST_DATA/rot.json` and bounded Catch2 generators. They cover storage temperatures, time and spoilage boundaries, nested containers, corpse recovery, spawning, and callback ownership. The cooking regressions also run the real oil-cooker and RV-kitchen activities after 21 days, including handmade cornmeal and lard; they verify actual ingredient and fuel consumption.

## Rot boundaries

- `src/rot/rot_calculation.h` defines a validated positive shelf life and pure arithmetic. Nonperishables have no shelf-life profile; their relative rot is always zero. Non-finite and out-of-range relative edits are rejected.
- `src/rot/item_rot.cpp` adapts that arithmetic to item state, weather, and the existing processing cadence. Smoking/milling pauses advancement without replacing intrinsic shelf life with zero. Ordinary live-item getters retain their existing lazy actualization behavior.
- Finished component ownership identifies inert ingredient records, including legacy saves without a `COMPONENT` flag. In-progress craft materials remain live. Removing or copying a record restarts its live clock without charging for the recorded interval; contained items restart together, while separate nested records remain inert.
- Counted-stack callback copies borrow the source's record/live role until the callback completes. Actual placement takes precedence; the temporary role is not copied or saved.
- The existing `rot`, `last_rot_check`, and `components` save fields are retained. Loose legacy `COMPONENT` flags retain their getter-only behavior and do not disable explicit processing.

The original issue regression tag is `[issue_10181]`; `[lifecycle]` covers record reads, recovery, copying, processing suspension, and the cooking workflows. `[calculation]` covers the pure arithmetic and direct entry-point boundaries. Callback tests force vector reallocation even if an earlier test left spare capacity.

## Require 100% coverage

The [coverage checker](../../../../tools/check_rot_coverage.ts) automatically includes every function and nested callback in `src/rot.cpp`, `src/rot/item_rot.cpp`, and `src/rot/rot_calculation.cpp`. Its explicit manifest also covers the location/actualization, crafting freshness, display, corpse/spawn, and traversal adapters left in the larger source files. Keep new rot implementations in the complete-file scope rather than adding unmeasured inline bodies to `item.h`. This is the rot subsystem denominator, not whole-game coverage.

Use a fresh profile directory for each measurement; do not merge stale profiles or profiles from failed tests:

```sh
cmake --preset linux-full \
  -DCMAKE_PROJECT_INCLUDE="$PWD/build-scripts/rot-coverage.cmake"
mkdir -p out/rot-coverage/build
LLVM_PROFILE_FILE="$PWD/out/rot-coverage/build/%p.profraw" \
  cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles
profiles=$(mktemp -d "$PWD/out/rot-coverage/run-XXXXXX")
LLVM_PROFILE_FILE="$profiles/%p.profraw" \
  SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  out/build/linux-full/tests/cata_test-tiles --filenames-as-tags \
  '[rot],[#rot_test],[messages]' --rng-seed 10278 && \
  llvm-profdata merge -sparse "$profiles/"*.profraw -o "$profiles/rot.profdata" && \
  tools/check_rot_coverage.ts out/build/linux-full/tests/cata_test-tiles "$profiles/rot.profdata"
```

The checker uses LLVM's native function summaries and requires zero missed regions, lines and branch outcomes—not a rounded `100.00%`. Missing functions or summaries also fail the check. Keep LLVM's `llvm-cov`, `llvm-profdata` and `llvm-cxxfilt` versions aligned with Clang. The checker requires Deno and runs from the repository root.

`-DROT_SANITIZE=ON` additionally enables AddressSanitizer for the source files listed in [rot-coverage.cmake](../../../../build-scripts/rot-coverage.cmake). This is not a whole-program sanitizer build. The coverage profile is independent of sanitizer diagnostics; a coverage pass does not make a sanitizer failure acceptable.

Return to the ordinary build with:

```sh
cmake --preset linux-full -DCMAKE_PROJECT_INCLUDE= -DROT_SANITIZE=OFF
cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles
```

To extend the coverage manifest or checker, run its tests too:

```sh
deno test tools/check_rot_coverage_test.ts
deno check tools/check_rot_coverage.ts
deno lint tools/check_rot_coverage.ts tools/check_rot_coverage_test.ts
```

The test executable normally discards game messages. Tests needing to verify visible text can use `capture_messages_during` from `tests/message_helpers.h`; captures are thread-local, scoped and nestable.

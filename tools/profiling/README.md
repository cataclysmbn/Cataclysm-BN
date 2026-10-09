# Tutorial profiling

Run the real curses tutorial with a fixed RNG seed and terminal size. The harness navigates the menu,
waits once to trigger the initial lessons, records and dismisses tutorial dialogs, waits for 20 warmup
actions, then performs 100 waits and 20 east/west movement cycles. It captures Tracy data, saves the
tutorial character, and exits the game.
Each run uses new user and configuration directories and fails on unexpected UI or incomplete
profiling evidence. The harness verifies the resolved configuration directory with `--paths` before
starting the game so XDG builds cannot fall back to the account's normal configuration.

## Build

Build the Tracy client, capture tool, and CSV exporter from the same revision. An installed client can
use a different protocol even when the game builds successfully. The commands below use a pinned
Tracy revision and disable discovery of an installed client.

```sh
git clone https://github.com/wolfpld/tracy.git out/tools/tracy
git -C out/tools/tracy checkout 16bac5f80790fb29ca2bd1eab903f02e8ea0732a
/usr/bin/patch -d out/tools/tracy -p1 < tools/profiling/tracy-capture-drain.patch

cmake --preset linux-curses \
  -DUSE_TRACY=ON -DCMAKE_DISABLE_FIND_PACKAGE_Tracy=ON \
  -DFETCHCONTENT_SOURCE_DIR_TRACY="$PWD/out/tools/tracy" \
  -DTRACY_VERSION=16bac5f80790fb29ca2bd1eab903f02e8ea0732a \
  -DTRACY_ON_DEMAND=ON -DTRACY_ONLY_IPV4=ON \
  -DLANGUAGES=none -DLUA_DOCS_ON_BUILD=OFF
cmake --build out/build/linux-curses --target cataclysm-bn cata_test --parallel 10

cmake -S out/tools/tracy/capture -B out/tools/tracy-capture -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DPATCH_EXECUTABLE=/usr/bin/patch \
  -DCPM_SOURCE_CACHE=/tmp/bn-tracy-cpm
cmake --build out/tools/tracy-capture --target tracy-capture --parallel 3
cmake -S out/tools/tracy/csvexport -B out/tools/tracy-csvexport -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DPATCH_EXECUTABLE=/usr/bin/patch \
  -DCPM_SOURCE_CACHE=/tmp/bn-tracy-cpm
cmake --build out/tools/tracy-csvexport --target tracy-csvexport --parallel 3

python3 -m venv out/harness-venv
out/harness-venv/bin/python -m pip install -r tools/profiling/requirements.txt
```

The capture patch waits for the receive worker to disconnect before writing the trace. The pinned
upstream capture otherwise starts serialization immediately after requesting shutdown, which can
race with incoming data. Apply the patch before building the capture tool.

The Python harness uses the standard library PTY interface and `pyte` to interpret curses output.
It requires Linux x86_64 or aarch64, readable `/proc` syscall state for its child process, `stdbuf`, and
a UTF-8 locale. Each run selects an available localhost Tracy port. The SDL compute acceleration policy
defaults to the game's automatic selection; curses still uses compute services in this build.

## Run

```sh
out/harness-venv/bin/python -m unittest discover -s tools/profiling -v
out/harness-venv/bin/python tools/profiling/tutorial_harness.py \
  --output out/profiles/run-1
```

Repeat with fresh output paths, such as `run-2` and `run-3`. `--seed`, `--warmup`, `--waits`, and
`--cycles` select the workload; `--binary`, `--capture`, and `--exporter` select executables.
`--acceleration auto|cpu|gpu|software` selects compute policy, and `--port` overrides the automatic
port choice. Keep the same scenario and build settings when comparing revisions. The harness refuses an existing output
directory so previous evidence is preserved.

## Evidence

- `capture.tracy`: full trace, including initialization and warmup.
- `inclusive.csv`, `self.csv`: full-capture statistics, for inspection.
- `events.csv`, `self-events.csv`: individual CPU zone events with source locations and timestamps.
- `summary.json`: measured gameplay window, input wait time, zone statistics, movement validation,
  and final saved character state.
- `metadata.json`, `CMakeCache.txt`: revision, executable hash, seed, scenario, terminal, and build
  settings.
- `terminal.ansi`, screen snapshots, and `inputs.json`: actual UI output and replayed input.
- `user/`: isolated configuration and the tutorial save. Runtime commands pass both `--userdir`
  and `--configdir`; `--userdir` alone does not isolate configuration in XDG builds.

After each input, the harness waits until the game's main thread is blocked reading stdin again. This
prevents output pauses during computation from being mistaken for action completion. The saved main
menu and confirmed exit are required; the curses program's normal `exit_handler(-999)` path returns
status 25 on POSIX, which is recorded in metadata.

The measured window starts at the first selected action's `handle_action_alive_switch` zone and ends
when input acquisition begins after the final action. Warmup and initialization are excluded from
`summary.json`. The harness verifies all gameplay action counts and successful movement placements
against the trace. Unbound Ctrl-G closes the final input-wait zone before disconnecting; it does not
add a gameplay action. A short transfer-drain interval follows outside the selected window. Saving
occurs after capture ends.

`wall_minus_input_wait_ms` subtracts the explicit blocking mouse/input zone from the measured wall
interval. It includes scheduling and profiling overhead and is not a CPU-cycle measurement. Zone
self times subtract instrumented children; uninstrumented work remains attributed to its parent.
The self-time ranking excludes the blocking input zone and all events nested within it, including
`input_context_get_input_event`, while retaining redraw work outside the blocking interval.
Do not add nested inclusive times. Median and p95 describe individual zone calls within one run,
not uncertainty across independent runs.

Compare several independent runs and retain the raw evidence. This small tutorial workload does not
establish performance for populated worlds, large inventories, or tiles rendering.

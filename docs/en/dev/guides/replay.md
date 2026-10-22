---
title: Recording and replaying input
---

Record a game session with `--replay-record PATH` and play it with `--replay-play PATH`.
These options use the shared input boundary, so they work with Tiles, Curses, and MCP.
Playback supplies input directly; an MCP host is not needed to replay an MCP recording.

```sh
out/build/linux-full/src/cataclysm-bn --client=tiles \
  --userdir out/record-user/ --configdir out/record-user/config/ \
  --seed example --replay-record out/example.jsonl
out/build/linux-full/src/cataclysm-bn --client=tiles \
  --userdir out/playback-user/ --configdir out/playback-user/config/ \
  --replay-play out/example.jsonl
```

**Start both runs from identical copies of the initial user directory.** Pass both `--userdir`
and `--configdir`: XDG builds otherwise continue to use the account's normal configuration.
Confirm the resolved paths with the same command plus `--paths` before running. A replay is an input
recording, not a save snapshot. It does not restore the world, configuration, keybindings,
character templates, or mod data. Do not replay against your normal saves. In particular,
copying the recording profile _after_ playing does not reproduce its initial state.

Use the same game build, game data, mods, options, client, and initial terminal dimensions.
The header supplies the recording's RNG seed; an explicit `--seed` on playback must match it.
The two replay options are mutually exclusive. Recording refuses to overwrite an existing file.

## What is recorded

The versioned JSONL stream contains:

- A header with the format version and deterministic RNG seed.
- Keyboard, text/IME, mouse, gamepad, semantic interaction, timed-poll, and empty-poll events,
  including their input context, registered actions, and polling timeout. Empty nonblocking polls
  matter: omitting them could apply a later input too early during an activity.
- Text the game reads from outside the input stream (pasted clipboard text, `input.txt`) as its own record, so playback inserts the recorded text rather than the current clipboard or file.
- Mouse events carry their coordinate unit (Tiles pixels or Curses/MCP cells). Playback rejects a mouse event recorded in a different unit.
- A normal-end record with the input count.

Events pass through the existing game input logic. Nested prompts are recorded, not just
turn-level actions. While recording or playing back, interruptible activities poll at each
simulation call boundary instead of using elapsed wall-clock time. This gives both replay modes
the same empty-poll order; ordinary play keeps the 100 ms polling throttle. Playback stops at the
clean end record and exits successfully without waiting for a human. Saving still requires an
in-game save action in the recording.

Semantic records preserve the operation, opaque choice/field/candidate identity, item count, and target position, but not the transport's request-number-like `input_id`. Playback sends them through the same widget implementation and validates the interaction kind, schema, quantity constraints, and target identity/range before applying them. This permits playback across clients while preventing a reordered inventory, changed target, or different list in the same generic input context from silently consuming the operation.

A client's `pickup` or `drop` command is recorded as a semantic record of the map view: `set_count` with the schema `item_operation:pickup` or `item_operation:drop`, the `avatar.ground` or `avatar.inventory` row id as the target, and the count in the row's native units. Playback starts the same pickup or drop.

A context mismatch, interaction-schema mismatch, unknown format version, malformed event,
inconsistent count, or missing end record is an error. Diagnostics identify the file and record line. A game that exits before
consuming the recorded input also fails. Playback reads one record at a time rather than
loading the entire recording into memory.

The deterministic RNG support gives keyed worker tasks and parallel-loop indices independent
random streams while retaining normal parallel work. This is not a promise of identical results
across different game versions, platforms, standard libraries, CPU/GPU calculation policies,
or every asynchronous game system. Input-context validation detects interaction drift; it is
not a full simulation-state checksum.

## Comparing two builds

Record once, then play the recording on the build to compare against and on the build under test:

```sh
deno task replay:record --binary ./cataclysm-bn-tiles out/rec
deno task replay:diff --recording out/rec/inputs.jsonl --profile out/rec/profile \
  --base base/cataclysm-bn-tiles --head ./cataclysm-bn-tiles out/diff
```

`replay:record` starts a new game of `--entry` (`new_game:tutorial` by default), waits `--waits` times, saves and quits; `out/rec/profile` is the untouched user directory the recording starts from. `replay:diff` plays the recording on each build from a copy of that profile with `--state-hash-log`, then compares the logs turn by turn and the saved worlds with `tools/client/compare_saves.ts`. `out/diff/summary.json` names the first step whose turn or `rng`, `avatar`, `monsters`, `npcs` or `map` hash differs, and the saved records that differ. It exits 0 when both builds agree, 1 when they part and 2 when a playback fails (a changed input context is such a failure; its `game.stderr` names the event). `--base-root` and `--head-root` name a build's game root when it is not the binary's directory.

## Real-session smoke check

On Linux, run the standard-library-only smoke driver against a build with `MCP=ON`:

```sh
python3 tools/client/replay_smoke.py --binary out/build/linux-clients/src/cataclysm-bn --output out/replay-smoke-1
```

The output directory must not already exist. The driver creates identical initial profiles and records a tutorial session through MCP. Before any waits can empty the tutorial inventory, it uses a published nonempty inventory selector to set an exact quantity and enters the common target UI to move its cursor. It then follows live visible positions and traversability to acquire and wear a backpack, selects a machete through the ground pickup model, salvages cargo pants through the native butcher menu, picks up the resulting rags, and completes a batch of two makeshift bandages by observing activity state and sending `IDLE` only at nonblocking polls. It also retains the crafting and construction denial checks, waits, moves east and west, exercises the inventory filter, and saves. Playback runs without an MCP host and compares the complete saved JSON. Startup choices, tutorial dialogs, inventory entries and fields, targeting, and confirmations use live semantic descriptors; normal gameplay movement uses discovered actions. No terminal scraping, fixed input counts, or fixed delays are used for synchronization.

Artifacts include both `--paths` preflight results, the input recording, protocol
requests/responses and timings, stderr logs, both profiles/saves, binary and recording hashes,
and `summary.json`. A successful short scenario is evidence for that scenario, not proof of
deterministic replay for all gameplay.

See [engine and clients](../explanation/engine_clients.md), [MCP](mcp.md), and
[Tracy profiling](tracy.md).

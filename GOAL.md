# Language-neutral BN engine/client interface

## Objective

Keep one authoritative BN simulation and expose its commands, player-visible state, UI choices, and ordered presentation events through a documented language-neutral interface. Preserve existing Tiles/Curses gameplay and effects. External clients must be able to operate the game and reconstruct its visible state without C++ client code.

Deliver this incrementally across multiple working sessions. Each work package ends with an inspectable patch, observed validation, and a precise next step. Integrate accepted changes into `feat/magnum-opus`. Execution order and decisions: `PLAN.md`.

## Delivery contract

### Commands and observations

- Shared engine services own game rules, command validation, state queries, and interaction descriptions. MCP and external IPC expose those services.
- Use JSON-RPC 2.0 and JSON Schema for the initial external contract. Specify protocol versions, capabilities, coordinates, identifiers, errors, and compatibility behavior.
- Publish actual choices, quantities, denial reasons, and available actions. Execute selections through native game logic using current input-boundary and opaque choice IDs.
- Distinguish receipt, validation, execution, and completion. A completion identifies the state revision and event sequence it produced.
- Passive queries preserve authoritative state and RNG. Explicit inspection and gameplay retain their intended effects.
- Keep observation generation and expensive description work bounded by the requested data, with correctly invalidated reuse where appropriate.

### Ordered presentation

- Publish intermediate projectile, impact, movement, terrain, field, explosion, and chain-reaction events while an action is running.
- Transport owned immutable values. Define event schemas, session epochs, public sequences, revisions, and snapshot/delta reconstruction.
- Preserve global causal order separately from display durations, interpolation, and grouped effects.
- Determine visibility and player knowledge in the engine at the relevant logical event boundary. Public IDs and sequences respect that visibility.
- Keep the existing native effects represented through semantic events or a defined compatibility presentation.

### Execution and transport

- Advance explosion authority using logical event time and insertion ordinal. Schedule children relative to the processed event.
- Use a monotonic clock for display pacing. Preserve damage formulas, physics coefficients, FIFO drains, EMP deferral, source lifetimes, and the existing explosion cap.
- Define bounded batches, flush deadlines, queue capacity, credits, and safe producer-yield points.
- Push events independently of rendering frames. Keep transport-local control responsive during long actions.
- Restrict world commands to valid input boundaries. I/O operates on owned data; the engine owns world access and mutation.
- Specify slow-consumer, disconnect, retained-history, and resynchronization behavior. Required events remain ordered and recoverability is reported accurately.

### Correctness and performance

- Equivalent initial state and inputs produce equivalent authoritative state, RNG, causal results, and remembered knowledge across supported clients and presentation speeds.
- Validate every relevant saved file and complete logical SQLite record, including ordered arrays and record membership.
- Measure actual IPC throughput, latency, and representative simulation/rendering/serialization costs.
- Establish workload-specific latency, memory, queue, and regression budgets before the acceptance run; record hardware, build, seed, payload sizes, and baseline.
- Verify existing native clients and the external contract with actual binaries and a non-C++ test consumer.

## Starting checkpoint

Canonical checkout: `/home/scarf/repo/cata/Cataclysm-BN-worktrees/magnum-opus`.

Branch `feat/magnum-opus` (rebuilt on `upstream/main` on 2026-10-09; earlier history is in `~/repo/cata/archive/engine-client-2026-10-10/engine-client.bundle`). Current package status, test results, exclusions and archives: `progress.md`. Re-read actual Git and process state when resuming.

The existing replay smoke compares the complete avatar `.sav`. Wider saved-world checks are a separate acceptance gate.

The measured tutorial RPC baseline is approximately 1,716 state queries/s and 628 screen queries/s. At 200 queries/s, each endpoint completed 1,000 calls within 5 ms; worst endpoint p99 was 2.36 ms. Reuse this as a scoped observation baseline when evaluating subsequent changes. Event bursts, rendering, and heavy gameplay require their own measurements.

## Status (2026-10-19)

Checked boxes below have evidence in the current tree; package board and validation detail: `progress.md`.

| Package        | State                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| P0             | Lane owners, source boundaries, gates and handoff locations: `PLAN.md` "Lanes and resources" (2026-10-20). The observation session's work is on HEAD as P1; the clock session's explosion clock is ported (M0/M2)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| M1 contract    | Protocol 0.2 schema and docs; value-only `presentation_value` facts, null sink `presentation::collect(false)`, recording sink `collect(true)`+`take()`, contract recording `--contract-record`; facts outlive their creature, out-of-sight facts are withheld, listening leaves RNG untouched                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| M4 delivery    | Credits, `max_hz`, held-frame bounds, `bn.resync`, stdio and Unix socket with spectators; real-IPC tests for frame limits, split frames, malformed frames, stalled readers, shutdown, `command_busy`; requests answered every activity step                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |
| M5 conformance | Deno consumer rebuilds state; every 0.2 error kind reached over IPC; reconnect via socket; client stream equals `--contract-record` recording                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| M0/M2, M3      | Explosion clock ported; grenade/projectile events published. Legacy M0/M2 traces kept in `~/repo/cata/archive/engine-client-2026-10-10/2026-10-08/evidence/refactor-engine-client-explosion-clock.tar.zst`; `[explosion]` 15 cases pass in declaration and random order (seed 424242, 2026-10-20), incl. geometry/sound isolation and state/RNG/trace equality across animated, zero-delay, skipped, slow-redraw and test-mode runs. Scheduling/pacing/queue review: `out/lanes/review-explosion-clock-2026-10-20.md` (no blocking findings). `ground_destroy_test` (hidden) fails identically on upstream a27f19d399b (2 of 4 cases, lines 131 and 187 x32). The out-of-sight grenade publishes only the natively heard zombie death sfx (5f1b476e393). M3 slice 0: native effect calls and RNG pinned for 8 shots/explosions at delay 0 and 10 (`[native_presentation]`, cf329de31af); slice 1: the combat text coin no longer depends on SCT, iso or test mode (559f702e26f); slices 2-3: projectile steps and hits go through a native `presentation::sink` registered by Tiles, curses and imgui, emitted with animations off, headless never paces (a10cbcf6042, e81f7e6d884); slice 4: every projectile fact of a multishot carries one `group` and each pellet's moves precede its hits, natively drawn together as before (c806244732c, `[native_presentation]` 8 cases). Open: native Tiles/Curses adaptation; `engine_client_sound_test:81` fails only in full-suite order                   |
| M6             | `determinism_test.ts` per-turn hashes; `BN_MEASURE` stdio measurement (wall, CPU, RSS, bytes, first effect, control p50/p95/p99) per consumer. Open: idle-machine baseline, recorded budgets, native presentation and acceptance matrix                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| P1, P2         | P1: purity fixes are on HEAD (the 2026-10-10 revert stayed on `fix/magnum-opus-purity-memory`); `[observation],[trade],[state]` 29 cases pass (2026-10-20). Trade publishes every row and only the cursor row's description; scratch measure (shopkeeper, 40 cursor moves): 1,000 items serialize 19.6 ms, frame p50 1.2 ms / p95 2.2 ms; 200 items 7.1 ms. Dialogue -> trade -> confirm -> save -> load over stdio passes (`stdio_test.ts` "survives save and load"). Descriptions render only for requested rows and are reused per dependency key, off-page invalidation tested (`[client_interaction_prepared],[trade]` 20 cases pass, 2026-10-20). P2: Lua and world-option saves are ordered at their producers (065624561d4; `[lua_serde],[world_options]` 14 cases pass, 2026-10-20). Save matrix (`tests/protocol/save_matrix_test.ts`): trade and native explosion leave the same complete save played back headless twice and in Tiles (2 pass, 2026-10-21, with `determinism_test.ts` 4 pass). Memory is written only by `map_perception::acquire` at turn and input boundaries from the turn's own cache rebuild (9a2e46508cc), never by drawing; lazy border mapgen runs async and applies a turn later at a fixed boundary, dated by its start turn; worker mapgen queues apply by place. Open: transient blast illumination is not remembered (per-step acquire rebuilt caches gameplay reads and cost ~115 ms per step, dropped 2026-10-21), which the second P2 item asks to preserve |
| P3             | Most screens semantic; Rust clients play through the protocol. World picker, overmap search, vending machine, blood test, defense setup, caravan, enchanter, Lua console, overmap editor and debug NPC list moved to screens; zone manager moved from 0.1 choices to a screen; egui vehicle/construction/zone layouts fit their window. No context left on the fallback (`out/lanes/screen-inventory-2026-10-19.md`); the protocol reference describes every screen kind by component in en/ja/ko; Enter on a list row is the native Enter on every screen that has one; the egui client edits terrain live (`crates/egui/tests/live_editmap.rs`, ignored test; furniture, trap, field, item, monster and vehicle edits and mapgen preview content unverified). Open: per-slice cross-client replay, the map editor's selection rectangle and mapgen border are not published                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |

## Work packages

Packages are resumable units, not calendar-day promises. Complete each gate before accepting the package; independent packages can proceed in parallel.

### P0 — Recover the current work and assign ownership

- [x] Read this file, `AGENTS.md`, current source, and the latest relevant handoffs.
- [x] Verify branches, worktrees, partial changes, exact run IDs, and remaining processes.
- [x] Give each lane an owner, exclusive source boundary, validation gate, and durable handoff location.
- [x] Resume the existing observation and clock sessions with the supporting-file and diagnostic decisions needed to unblock them.
- [x] Enumerate remaining custom UI screens and their current semantic/fallback coverage.

**Gate:** Every pending requirement has an owner or an explicit dependency, and existing WIP/evidence is preserved.

### P1 — Finish read-only observations and native trade

- [x] Complete observation-aware recipe component filters and result previews.
- [x] Complete pure recipe-exemplar nutrient queries and owner/faction lookups.
- [x] Preserve full descriptions, native explicit inspection, and gameplay resource extraction.
- [x] Adopt the reviewed pure API in trade and retain native price, count, debt, capacity, refusal, confirmation, and transaction behavior.
- [x] Bound description generation before paging and reuse unchanged derived data with tested invalidation.
- [x] Validate real dialogue → trade → explicit confirmation → save/quit.

**Gate:** First-query and repeated-query tests preserve serialized state and RNG; explicit inspection still performs intended effects; full related suites pass; merchant query/frame costs are measured; independent review accepts the integrated change.

### P2 — Establish engine-owned perception and deterministic persistence

- [x] Move authoritative perception/remembered knowledge updates to shared engine boundaries.
- [ ] Preserve transient illumination, revealed terrain, occlusion, visibility rules, and travel/autodrive knowledge.
- [x] Resolve both observed memory differences: client representation parity and display-speed-dependent transient coverage.
- [x] Make Lua and world-option serialization deterministic at their producers while preserving values and identity semantics.
- [x] Trace generation, deferred loading, commit, and save membership for the additional Tiles map records; fix the proven authority boundary.
- [x] Re-run ordinary trade and native explosion complete-save comparisons.

**Gate:** Every saved record and remembered cell agrees under the defined replay matrix; actual game knowledge and world membership are preserved.

### M0/M2 — Accept the deterministic explosion implementation

- [x] Retain the existing legacy characterization, approved compatibility decision, and original artifacts.
- [x] Complete review of logical scheduling, insertion-order ties, movement/collision steps, pacing, and queue/lifetime protections.
- [x] Validate real geometry and sound-state restoration across declaration and random test order.
- [x] Cover native animated, zero-delay, skipped, slow-display, and headless execution with real state/RNG evidence.
- [ ] Measure representative ordinary/custom blasts, fragments, flying stacks, terrain destruction, and long chains.
- [ ] Resolve or substantiate remaining ground/sound regression failures against the actual baseline revision.

**Gate:** Logical results and remembered knowledge agree across presentation modes; relevant tests and native comparisons pass; critical-path costs meet the recorded budgets.

Compatibility decision already approved: logical time plus insertion ordinal is authoritative. Timing-dependent legacy outcomes may change while existing formulas and gameplay rules remain intact.

### M1 — Implement the shared event and external interface contract

- [x] Define the smallest complete command, observation, and event schemas needed for the first end-to-end slice.
- [x] Specify negotiation, readiness, stale-request rejection, epochs, sequences, revisions, coordinates, asset references, and visibility transitions.
- [x] Add value-only events, a null sink, and a recording sink.
- [x] Capture results while source objects are valid and visibility is known.
- [x] Test ownership/lifetime, hidden-state filtering, ordering, malformed data, and observer purity.

**Gate:** A reviewed, versioned contract has executable tests and owned payloads; passive capture preserves game results.

### M3 — Connect native presentation and projectiles

- [ ] Adapt Tiles/Curses to the shared presentation contract while preserving existing effects.
- [ ] Connect projectile steps, impact, bounce, drops/embedding, special defenses, nested explosions, and chain reactions in actual causal order.
- [ ] Preserve multishot visual grouping separately from simulation ordering and retain existing gameplay RNG draws through the migration.
- [ ] Inspect actual native rendering at ordinary and slow presentation rates.

**Gate:** Existing effects remain represented and visible; renderer pacing does not alter authoritative results or perception.

### M4 — Implement bounded asynchronous delivery

- [x] Implement negotiated batching, flush boundaries, owned queues, and a single output owner.
- [x] Deliver intermediate events before action completion.
- [x] Implement credits/backpressure, safe yielding, readiness, and independent transport control.
- [x] Implement disconnect, reconnect, retained-history checks, and explicit resynchronization.
- [x] Test resource bounds, partial writes, malformed frames, stalled readers, shutdown, and command re-entry protection.

**Gate:** Real IPC streams during long actions without per-frame round trips; required ordering, memory bounds, responsiveness, and safe recovery hold.

### M5 — Demonstrate language-neutral conformance

- [x] Exercise the public interface from a non-C++ test consumer using the repository's existing test tooling where practical.
- [x] Reconstruct visible state from an initial snapshot and subsequent event batches.
- [x] Verify sequence/revision rules, selection commands, readiness, errors, and reconnect recovery.
- [x] Compare reconstructed state and ordered events against an engine-side recording consumer.

**Gate:** Cross-process behavior proves the published contract is sufficient independently of internal C++ APIs.

### M6 — Integrated replay and performance acceptance

- [ ] Compare simulation-only, null-sink capture, serialization, IPC/decoding, and native presentation on matched workloads.
- [ ] Measure first-effect latency, control p95/p99, CPU, allocations, bytes/events per action, peak memory, queue high-water, and producer-yield duration.
- [ ] Repeat with slow, paused, disconnected, and reconnecting consumers.
- [ ] Re-run input replay, event-stream equivalence, full RNG comparison, and complete saved-record equality on the integrated revision.
- [ ] Run the matching CI-equivalent selection and independent final review.

**Gate:** Correctness and bounded-resource requirements pass, measured performance meets the predeclared budgets, and the final evidence matches the integrated source.

### P3 — Complete the remaining semantic UI coverage

- [x] Implement the inventory from P0, including vehicles, character creation, overmap, and other outstanding custom views.
- [ ] Preserve native validation, cancellation, nesting, denial paths, and effects in each adapter.
- [ ] Validate actual gameplay, stale requests, hidden/disabled choices, and cross-client replay for each completed slice.
- [x] Synchronize the user-facing interface documentation across en/ja/ko.

**Gate:** Every inventoried screen has its required implementation and evidence; final acceptance covers the original engine/client scope.

## Dependencies and parallel work

M0 evidence and the approved compatibility decision precede acceptance of M2. M1 and M2 feed M3; M3 feeds M4, then M5 and M6. P1 and P2 are required for integrated correctness. P3 remains part of delivery after the streaming milestone; independent UI slices may proceed earlier when ownership permits.

Use available parallel capacity for distinct outcomes. The next wave can cover:

| Lane                          | Work                                                                 | Ownership/dependency                                                                  |
| ----------------------------- | -------------------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| Observation/trade correctness | Finish P1's pure query helpers and tests                             | Existing observation worktree; trade adopts the API after review                      |
| Persistence                   | Deterministic Lua/options serialization and saved-record diagnostics | Separate worktree; coordinate shared map files with perception and observation owners |
| Clock/perception              | Finish clock acceptance and implement the shared perception boundary | Existing clock evidence; assign one owner for shared engine/map changes               |
| Interface/stream              | Prepare and implement M1, then its consumers                         | Separate worktree; parent accepts common schemas before dependent adapters diverge    |

Implementation workers use `openai-codex/gpt-6.1-sol:xhigh`, separate manually preserved worktrees, and one writer per worktree. The parent owns shared contracts, source ownership, integration, and acceptance. Read-only reviewers receive the actual patch and evidence.

Each worktree owns its mutable build output. Limit concurrent builds to `-j2` per build and reserve a quiescent window for performance measurements. Use waiting build time for independent review, contract tests, or documentation work.

## Multi-day continuation

At every session start:

1. Read this file, applicable instructions, and the newest lane handoffs; verify actual HEAD and WIP.
2. Inspect each known run by its exact ID. Session-scoped overview lists are insufficient to establish that earlier runs have stopped.
3. Consume completed results and decide acceptance, focused fixes, or a scoped blocker before assigning new work.
4. Resume the same retained worker when possible. Retain original artifacts and record the new run ID.
5. Start the next ready independent package and register the result-consumption trigger.

At each handoff or session end, update `progress.md` with:

- Package status: ready, running, blocked, awaiting review, or accepted.
- Owner, worktree, branch/HEAD, exact run ID, claimed source files, and patch location.
- Exact validation commands/results, source/binary identity, and remaining failures.
- The next concrete action, prerequisite, and parent decision needed.

A worker finish is a review boundary. After review, the parent fixes, resumes, or integrates the result and advances the board. If one lane needs a user decision, continue unaffected lanes. Ordinary implementation and supporting-file decisions belong to the parent; surface material behavior, compatibility, permission, or architecture changes to the user.

Keep reports short and evidence-based. Report actual execution, observed results, and blockers separately. Reuse accepted work and preserved reproductions across days.

## Validation and integration rules

- Follow `AGENTS.md`, repository C++23 conventions, and the relevant test-quality and workflow skills.
- Format touched files before building. Build the actual game together with relevant Tiles/MCP tests and other affected enabled clients.
- Configure each independent build with `/usr/bin/ninja` and `LUA_DOCS_ON_BUILD=OFF` from the start.
- Isolate every game invocation using both `--userdir` and `--configdir`, verified with `--paths`. Catch2 uses a dedicated `--user-dir`.
- Keep real fixtures deterministic and restore geometry, sound, UI dimensions, and other global state on normal and exceptional exits.
- Preserve meaningful idle polls and strict replay boundary/schema checks. Compare all required saved fields, ordered arrays, and logical SQLite records.
- Inspect completed SQLite evidence with verified quiescence/WAL state and immutable read-only access, or a consistent copied snapshot. Preserve original files and any inspection-created sidecars.
- Distinguish normal gameplay evidence from deliberately prepared diagnostic/benchmark fixtures.
- Preserve the canonical seven-file trade WIP and unrelated work. Workers return patches; the parent commits only validated, scoped changes.
- Keep publication, history rewriting, worktree cleanup, and personal configuration changes under explicit user control.
- Keep screenshots in `.screenshots/` and detailed logs/artifacts in the worktree's ignored output directories.

## Resume locations and evidence

Retired branches, WIP snapshots and the pre-squash history are in `~/repo/cata/archive/engine-client-2026-10-10/engine-client.bundle` (restore with `git fetch <bundle> 'refs/*:refs/*'` in a clone that has `upstream/main`). Text handoffs/logs: `~/repo/cata/archive/engine-client-2026-10-10/2026-10-08/evidence/` (map: `manifest.tsv`).

Relevant existing protections and context: [deferred EMP drains](https://github.com/cataclysmbn/Cataclysm-BN/pull/9716), [queued explosion source lifetime](https://github.com/cataclysmbn/Cataclysm-BN/pull/9782), [input replay](https://github.com/cataclysmbn/Cataclysm-BN/pull/9478).

## Completion checklist

- [ ] Required semantic controls operate through the shared game rules.
- [ ] Passive observations preserve authority and RNG.
- [ ] Native and external presentation preserve intermediate events and player knowledge.
- [ ] Versioned external commands, observations, streaming, and recovery pass real IPC conformance.
- [ ] Replay agrees in authoritative results, RNG, event order, and complete saved records.
- [ ] Representative performance and resource bounds meet the recorded budgets.
- [ ] Existing client behavior, remaining UI coverage, relevant regressions, and independent review pass.
- [ ] Accepted changes are integrated, documentation is current, and the final evidence identifies the delivered source.

---

btw added low priority objective

- [x] 1st person mode
- [ ] Map editor and vehicle editor in egui client
- [x] Several windows open at once, as in Project Zomboid: the game keeps taking keys under windows that only read (the egui map, character sheet and inventory windows, through `bn.panel.view`)

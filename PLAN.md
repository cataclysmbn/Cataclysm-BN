# magnum-opus execution plan

One authoritative BN engine, many clients. `GOAL.md` defines the end state; this file defines how we get
there, in what order, and how each step is accepted. Read both at session start, then `progress.md`.

## Decisions (2026-10-09)

| Topic                            | Decision                                                                                                                                                                                                           |
| -------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Branch / worktree                | `feat/magnum-opus` at `Cataclysm-BN-worktrees/magnum-opus`, based on `upstream/main`                                                                                                                               |
| History                          | Rebuilt as one commit per concern; pre-squash history and all retired branches/WIP are in `~/repo/cata/archive/engine-client-2026-10-10/engine-client.bundle` (ref `refs/archive/branches/refactor/engine-client`) |
| Wire protocol                    | JSON-RPC 2.0, newline-delimited, JSON Schema. One protocol version and one clock for interaction, world and presentation                                                                                           |
| Engine-repo conformance consumer | Deno/TypeScript, headless, inside this repository (GOAL M5 "existing test tooling")                                                                                                                                |
| Native clients                   | Separate repositories. Engine repo keeps schema, C++ and Deno conformance only                                                                                                                                     |
| Rust client                      | `~/repo/cata/bn-client-egui`, eframe (egui + AccessKit, wgpu). Mouse-first (drag/drop, context menus, tooltips, map clicks) and screen-reader accessible                                                           |
| Client UI strategy               | First port C++ screens 1:1 (feature parity); redesign only after parity                                                                                                                                            |
| Removed from engine repo         | GTK client, Rust `bn-protocol`, unwired 2.0 ordered/world values, capture red tests (all recoverable from archive refs)                                                                                            |
| Development mode                 | Client work is fully AI-driven; acceptance is by executable tests, never by inspection of generated code alone                                                                                                     |

## Current state (evidence 2026-10-09)

- Engine: semantic interactions for menus, inventory, crafting, construction, pickup, advanced inventory, dialogue,
  help, morale, trade, debug prompt, bionics, character info; passive-query purity fixes; deterministic save
  ordering; contract 1.0 (4 JSON-RPC methods wired in `src/mcp_server.cpp`); bounded framing; perception
  acquisition after bionic commands.
- Not present: world/map state on the wire, any push/notification, sockets/WebSocket, reconnect, presentation
  events from gameplay code (M3), explosion clock (ported in Phase 0, see `src/explosion.cpp`).
- Known test debt: order-dependent failures in full-suite order (`Items rot away`, `Map powered fridge and freezer
  furniture controls food rot`, `npc-movement`, Lua dimension tests with `database is locked` in the MCP binary).
  All pass alone. Lane L1 is adding a per-test global-state leak check to locate the leakers.
- Measured costs: one Catch2 test TU compiles in 40–100 s; each test TU is compiled once per test binary (4
  binaries); mold link of a test binary 3–8 s; full MCP suite 40–60 min; full Tiles suite ~20 min.

## Lessons that constrain this plan

1. No machinery nobody asked for (admission scanners, locks, byte budgets, receipts, image archives). Each
   addition must trace to a GOAL requirement or a failing test.
2. Specify before building, but keep the specification proportional: a slice is defined by its acceptance test,
   not by pages of prohibitions.
3. No permanently red tests in the suite. A red test lands together with its fix, or lives on an archive ref.
4. Lanes are few, bounded and owned. One writer per worktree, one full suite at a time, parent integrates.
5. Contract values describe game facts, not one renderer's draw primitives or the reality-bubble frame.
6. Wait on exact job IDs (pueue), never on name patterns or sleep loops.

## Phase 0 — Hygiene and speed (now)

| Item                                     | Acceptance                                                                                                                                                                                                |
| ---------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| H1 Rebuilt history on `feat/magnum-opus` | `git diff <final tree> HEAD` empty; commits grouped by concern; final tree builds `cataclysm-bn-tiles cata_test-tiles cata_test-mcp cata_test-debug-mcp cata_test-perception-mcp`                         |
| H2 Dead code removed                     | No reference to removed paths (`git grep`); build and suites below pass                                                                                                                                   |
| H3 Stale docs fixed                      | 1.0 reference no longer says the methods are unconnected (en/ja/ko)                                                                                                                                       |
| H4 Leak check (lane L1)                  | Listener in `tests/test_main.cpp` fails the leaking test itself; demonstrated red on a reverted leak fix; known victims explained or fixed at the leaker                                                  |
| H5 Faster iteration                      | Iterate with one test binary (`cata_test-mcp`); build the others only before the final suite. Full suite split into N name-list shards with separate `--user-dir`, run as N pueue jobs, after H4 is green |
| H6 Retire old state                      | Old worktree `refactor-engine-client` and `base-upstream-main` removed; 104 `refs/archive/*` refs packed into one git bundle under `out/archive/`; stale `out/` trees moved to trash                      |

Exit: full Tiles and MCP suites on the final tree show no failures other than those reproduced on
`upstream/main` with the same command.

## Phase 1 — Playable loop first (M1 completion)

Decision 2026-10-10: a working external client comes before presentation events. Design reference:
`out/lanes/l2/protocol-unification.md` (rev 2); presentation-event parts of it wait for Phase 2.

Slice: an external client walks around, looks at cells, picks up and drops items, and uses every existing
semantic menu, against a live `cataclysm-bn` over stdio.

- Unified protocol `1.0`: one session clock `(epoch, sequence, revision)`; interaction values from today's 1.0
  (renamed/trimmed per the design); world values (visible and remembered cells, disclosed entities, avatar) in
  absolute map-square coordinates with dimension, knowledge decided by engine perception.
- Methods: hello, snapshot/subscribe, interaction choices paging, world cells query, command submit (semantic
  operations and registered actions such as movement). Push: state-change events after each input boundary.
- No presentation events, no socket transport, no reconnect in this phase.

Acceptance:

1. A Deno headless consumer drives a live process: negotiate, snapshot, move the avatar N steps, pick up an item
   through the pickup menu, and its reconstructed state equals a fresh snapshot after every step.
2. The egui client (separate repo) replaces its fake model with the protocol, shows the real map and inventory,
   and its kittest suite runs against a recorded engine session.
3. Full Tiles and MCP suites unchanged versus the Phase 0 baseline.

## Phase 2 — Presentation events (M0/M2 → M3)

The explosion clock (M0/M2) is already on the branch. Emit ordered presentation events for one thrown grenade
(throw, projectile segments, landing, fuse, explosion phases, damage, field/terrain changes) with engine-decided
visibility; Tiles renders the same slice with existing effects intact.

Acceptance: the Deno consumer reconstructs the same ordered events the in-process recording sink captured;
authoritative state and RNG equal with presentation on and off; Tiles capture before/after shows the effects.

## Phase 3 — Transport (M4)

Socket/WebSocket listener in addition to stdio, JSON-RPC notifications for event push, bounded batches with
credits, disconnect and resynchronisation by fresh snapshot. Acceptance: events stream during a long action
without per-frame requests; a slow consumer cannot grow engine memory past the declared bound.

## Phase 4 — egui client (separate repository, parallel from Phase 1)

| Step               | Acceptance                                                                                                                                                                                                                               |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| C1 Prototype       | Window with tile map, click-to-move, two inventory columns with drag/drop, context menu, tooltip; `egui_kittest` tests drive them through the AccessKit tree; NVDA (Windows) and Orca (Linux) read the controls (manual check, recorded) |
| C2 Protocol crate  | Client-owned bindings generated or validated against the engine schema; conformance fixtures shared with the Deno consumer                                                                                                               |
| C3 1:1 screen port | One screen per change, in P3 inventory order; each with a kittest test and a recorded engine session                                                                                                                                     |
| C4 Redesign        | Only after C3 parity for the screens in scope                                                                                                                                                                                            |

Map accessibility is explicit: cursor-cell description, look-around and message log exposed as text nodes.

## Replays

Two kinds, with different lifetimes:

- **Input replay** (`--replay-record`/`--replay-play`, exists): input recording re-simulated from a seed. Valid
  only for the same build, data, mods and options; any gameplay change can make it diverge. Use it for
  determinism regression tests, same-build bug reproduction and cross-client equivalence (GOAL M6). Record and
  play within the same run instead of keeping recordings as long-lived fixtures.
- **Event replay** (after Phase 2): the ordered snapshot + event stream written to a file and played by any client
  without the engine. It survives gameplay changes as long as the protocol version is supported, like GOTV demos
  or ttyrec. This is the replay players watch.

## Later packages (unchanged scope from GOAL)

P1 description bounds and reuse (needs an API decision: page-aware providers vs. test oracle change), P2
perception/persistence equality, M5 cross-process conformance, M6 measurements, P3 remaining screens.

## Lanes and resources

| Lane                                  | Owner             | Worktree                     | Builds                                     |
| ------------------------------------- | ----------------- | ---------------------------- | ------------------------------------------ |
| L0 hygiene, integration, final suites | parent            | `magnum-opus`                | C++ full                                   |
| L1 leak check                         | implementer agent | `magnum-opus-leak-check`     | C++ (`cata_test-mcp` only while iterating) |
| L2 protocol design (Phase 1 doc)      | read-only agent   | none                         | none                                       |
| L3 egui prototype (C1)                | implementer agent | `~/repo/cata/bn-client-egui` | Rust only                                  |

Rules: at most one C++ full build and one full suite at a time across lanes (pueue group `build` parallelism 1,
group `cata` for tests); each lane reports commits, exact commands and log paths; the parent cherry-picks
validated commits onto `feat/magnum-opus`.

## Verification protocol

- Configure: `cmake --preset linux-full -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
  -DLUA_DOCS_ON_BUILD=OFF -DMCP=ON -DIMGUI=ON`. Executables land in the worktree root.
- Run: `systemd-run --user --scope --slice=bnwork.slice -q -p MemoryMax=24G -p MemorySwapMax=0 ./<binary> --rng-seed 424242
  --user-dir=<private dir>/` via `pueue add -p -g cata`.
- A failure is pre-existing only if reproduced on `upstream/main` with the same command and order.
- Format with `build-scripts/fmt.sh cpp <files>`; never bypass hooks; commit only validated changes; no push
  without explicit request.

## Protocol and scope decisions (2026-10-09)

Answers to `out/lanes/l2/protocol-unification.md` §7 and lane follow-ups:

1. The unified protocol is named `1.0` (nothing shipped) and replaces today's 1.0 in place; no compatibility shim.
2. Projectile events are emitted per perceived path segment; steps stay ordered inside the event.
3. World coverage is the whole loaded area on all z levels, sparse; measure snapshot bytes before changing it.
4. Native list state (`focus`, `pane.filter`) stays for the 1:1 port; clients may ignore it.
5. One controller connection submits commands; others spectate (finalised in Phase 3).
6. `engine_client_delivery.{h,cpp}` is deleted; Phase 3 rebuilds only what its slow-consumer test needs.
7. Damage numbers follow native disclosure: `amount` is published exactly where the native message log
   prints it, i.e. damage dealt by or to the avatar (`src/creature.cpp` hit messages); scrolling combat text
   shows HP bars, not numbers.
8. Legacy MCP tools stay separate in Phase 1 and are rebuilt on the new values after the world model lands.
9. Event-replay files carry appearance inline (`look`), no embedded data table.
10. Remembered map knowledge is part of save equality: acquisition is decided by the engine, never by rendering
    speed (P2).
11. Explosion-clock fixtures keep their cases but reduce repetitions before entering CI.
12. P1: interaction providers become page-aware (`offset`, `limit`) and pair with `bn.interaction.choices`;
    capture tests are rewritten for that API.
13. The Deno headless consumer may later grow into a browser client; no work now.

### Input replay work items (M6 support)

- Record and immediately play back in the same run; no long-lived recording fixtures.
- Record a per-turn state hash so playback reports the first diverging turn and input line.
- Short per-feature scenarios driven over MCP instead of one long tutorial session.

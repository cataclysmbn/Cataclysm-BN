---
title: Engine/client protocol 1.0
---

An external client plays the game over newline-delimited JSON-RPC 2.0 on the stdio of `cataclysm-bn-tiles --client=mcp`. The values are defined by the [Draft 2020-12 schema](../../../schema/engine-client/1.0.schema.json), and every example below is checked against it in the [examples file](../../../schema/engine-client/1.0.examples.json). The legacy MCP tools (`bn.observe`, `bn.press`, ...) are a separate, unchanged surface; once a client says `bn.hello`, they refuse input.

## A session

```text
-> {"jsonrpc":"2.0","id":1,"method":"bn.hello","params":{"versions":["1.0"],"client":{"name":"my-client","version":"1"}}}
<- {"jsonrpc":"2.0","id":1,"result":{"version":"1.0","epoch":"epoch:e7f3","engine":{"build":"...","mods":["bn"]},"limits":{"frame_bytes":1048576,"cells_per_part":512,"cells_per_query":4096}}}
-> {"jsonrpc":"2.0","id":2,"method":"bn.subscribe","params":{}}
<- {"jsonrpc":"2.0","id":2,"result":{"at":{"epoch":"epoch:e7f3","sequence":"40","revision":"31"},"interaction":{...},"entities":[],"parts":1}}
<- {"jsonrpc":"2.0","method":"bn.snapshot.part","params":{"epoch":"epoch:e7f3","at":{...},"index":0,"last":true,"cells":[...]}}
-> {"jsonrpc":"2.0","id":3,"method":"bn.command.submit","params":{"epoch":"epoch:e7f3","expect":{"revision":"31","boundary_id":"boundary:90","schema_id":null},"operation":{"kind":"action","action_id":"RIGHT"}}}
<- {"jsonrpc":"2.0","id":3,"result":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"received"}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"validated"}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"executing"}}
<- {"jsonrpc":"2.0","method":"bn.events","params":{"epoch":"epoch:e7f3","events":[{"sequence":"41","revision":"32","type":"interaction.changed","command":"c:5","changes":{...}}]}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"completed","at":{"epoch":"epoch:e7f3","sequence":"41","revision":"32"}}}
```

Collect the `parts` snapshot parts, then apply `bn.events` in order. A gap, an epoch mismatch or `bn.resync` means: call `bn.subscribe` again. A new snapshot restores state only; transient events after `lost_after` are lost, never replayed.

## Methods

| Method                   | Params                                    | Result                                           |
| ------------------------ | ----------------------------------------- | ------------------------------------------------ |
| `bn.hello`               | `versions`, `client`, `viewport?`         | `version`, `epoch`, `engine`, `limits`           |
| `bn.viewport`            | `cols`, `rows`                            | none                                             |
| `bn.subscribe`           | none                                      | snapshot header; parts and events follow         |
| `bn.unsubscribe`         | none                                      | none                                             |
| `bn.interaction.choices` | `epoch`, `boundary_id`, `offset`, `limit` | `boundary_id`, `total`, `choices` (reads only)   |
| `bn.world.cells`         | `epoch`, `min`, `max`                     | `at`, `cells`, `forgotten` (reads only)          |
| `bn.command.submit`      | `epoch`, `expect`, `operation`            | `command_id`, `stage: "received"`                |
| `bn.command.result`      | `epoch`, `command_id`                     | latest stage, for recovering a lost notification |

Notifications from the engine: `bn.snapshot.part`, `bn.events`, `bn.command`, `bn.resync`, `bn.loading`. All but `bn.loading` are sent at input boundaries to a subscribed client.

Errors use code `1000` with `error.data = {kind, action?, at?}`; `action` says what to do next (`hello`, `subscribe`, `retry`).

## One clock

- `epoch` changes when the process starts or the world is replaced, never on reads.
- `sequence` counts published events (decimal string, contiguous, starting at `"1"`). `revision` counts events that change state. `at = {epoch, sequence, revision}` means "includes every event up to sequence".
- A command carries `expect = {revision, boundary_id, schema_id}`. A stale value is rejected, so a client never acts on a screen it has not seen. `schema_id` is `null` exactly when the boundary has no interaction.

## Commands

`operation.kind` is one of `choose`, `fill`, `set_count`, `set_target`, `cancel` (semantic menus) `action` (a registered action such as a movement key) or `travel` (a map click).

```json
{ "kind": "choose", "choice_id": "tab:new_game" }
{ "kind": "set_target", "pos": { "dim": "", "x": 9, "y": 4, "z": 0 } }
{ "kind": "action", "action_id": "RIGHT" }
{ "kind": "travel", "pos": { "dim": "", "x": 9, "y": 4, "z": 0 } }
```

`travel` is the left click of Tiles and curses on an absolute map square in view, at a boundary without interaction. The first click publishes the engine's own route as `route`; clicking the same square again starts native auto-move. The avatar then walks one step at a time, each step publishing ordinary events attached to that command, and stops where the engine stops (arrival, a monster coming into view, ...). The command is `completed` when the walk ends and input is awaited again. Any other command or a click elsewhere replaces or clears the plan. A client never computes routes.

Stages: `received`, `validated`, `executing`, `completed`; or `rejected`; or `interrupted`, whose `error` says why (`stale_epoch` when the world was replaced, `validation_failed` when the game refused the input, `not_ready` otherwise). `completed` means the next native input boundary was reached, not that a long activity finished; its `at` is the endpoint of that boundary. One command is outstanding at a time (`command_busy`).

## Loading

While the world loads, the game thread is busy and reaches no input boundary, so a client that said `bn.hello` gets `bn.loading` instead, written at each step of the native loading screen (subscribed or not). Progress is the screen the native client shows: `title` is the current context (for example "Loading files"), `entries` its list, `index` the entry in progress (earlier ones are done) and `image` the loading image, chosen like the native client does. `image.path` is relative to the game's base path, `author` is omitted when the file name has none. `done` is sent once when the loading screen ends; after it the usual `bn.resync` or `bn.events` follow. `epoch` is the epoch in force when the step was sent.

```text
<- {"jsonrpc":"2.0","method":"bn.loading","params":{"epoch":"epoch:e7f3","title":"Loading files","entries":["Terrain","Items"],"index":1,"image":{"path":"data/json/loading/Ada_dawn.webp","author":"Ada"}}}
<- {"jsonrpc":"2.0","method":"bn.loading","params":{"epoch":"epoch:e7f3","done":true}}
```

## Values

- `pos = {dim, x, y, z}` is an absolute map square; `dim` is the game dimension, `""` for the primary one. Reality-bubble coordinates never appear on the wire.
- `look = {kind, id, glyph, color}` carries appearance from game data, so text clients need no tileset. Auto-wall terrain carries the line glyph for the connections the avatar knows, as the native text view draws it; a remembered cell keeps `memory.overlay` (furniture, trap or vehicle part) over `memory.terrain`.
- `interaction` is the native menu or dialog: `choices` holds up to the first 200 rows, fewer when they would be too large; read the rest with `bn.interaction.choices` starting at `offset = choices.length`. `choice_total` is the full count. `compat.focus` and `compat.panes` keep native list state for 1:1 ports; clients may ignore them. The main menu is tabbed like the native one: every tab is a choice without `pane_id` (`tab:new_game`, `tab:load`, ...; the selected one has `selected`), followed by the entries of the selected tab only, each with that tab's `pane_id` (`new_game:tutorial`, `settings:options`, `load:<world>`). Choosing another tab publishes a new interaction with that tab's entries; ids never depend on position or language.
- The world is what the avatar knows: `cells` (`remembered`, `visible` or `sensed`), `entities`, `avatar` (stats and `inventory`), `environment`, the loaded `coverage`, the `view` and the pending `route` (empty or absent when nothing is planned). `view` is the native terrain window as `{min, max}` squares of the avatar's level (it follows the avatar): a `travel` click is accepted only inside it, anything else is rejected with `validation_failed`.
- The terrain window is sized like a native window resize. `viewport = {cols, rows}` (1 to 512 map cells) in `bn.hello`, or later `bn.viewport`, resizes the terminal to those cells plus the side panels, so the engine shows that much map around the avatar; the effective `view` is authoritative because the native minimum terminal size still applies. A client that sends none gets the default terminal.
- Every event carries a generic `changes` block, applied in this order: `coverage`, `view`, `cells`, `forgotten`, `entities`, `gone`, then `avatar`, `environment`, `route` and `interaction` replace.

Events are `interaction.changed`, `coverage.moved`, `turn.passed`, `cells.seen` and, later, the presentation types listed in the schema. A new event type needs a new exact version.

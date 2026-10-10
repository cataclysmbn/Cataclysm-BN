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
| `bn.interrupt`           | none                                      | none                                             |
| `bn.subscribe`           | none                                      | snapshot header; parts and events follow         |
| `bn.unsubscribe`         | none                                      | none                                             |
| `bn.interaction.choices` | `epoch`, `boundary_id`, `offset`, `limit` | `boundary_id`, `total`, `choices` (reads only)   |
| `bn.world.cells`         | `epoch`, `min`, `max`                     | `at`, `cells`, `forgotten` (reads only)          |
| `bn.command.submit`      | `epoch`, `expect`, `operation`            | `command_id`, `stage: "received"`                |
| `bn.command.result`      | `epoch`, `command_id`                     | latest stage, for recovering a lost notification |

Notifications from the engine: `bn.snapshot.part`, `bn.events`, `bn.command`, `bn.resync`, `bn.loading`, `bn.progress`. All but `bn.loading` and `bn.progress` are sent at input boundaries to a subscribed client. `bn.progress` (`{epoch}`) is a heartbeat, sent every 2 seconds to a client that said `bn.hello` while a wait or other activity runs without reaching an input boundary, so silence on the wire means the engine is not working. `bn.loading` plays the same role while the world loads.

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

Every boundary action lists `keys`, the portable names (`ESC`, `SPACE`, `RETURN`, `UP`, `>`, ...) of the single keyboard keys the active input context binds to it; an action without keys cannot be run by `action`. A client that wants to act on a pressed key finds the action whose `keys` contain it instead of guessing from the key settings files.

`travel` is the left click of Tiles and curses on an absolute map square in view, at a boundary without interaction. The first click publishes the engine's own route as `route`; clicking the same square again starts native auto-move. The avatar then walks one step at a time, each step publishing ordinary events attached to that command, and stops where the engine stops (arrival, a monster coming into view, ...). The command is `completed` when the walk ends and input is awaited again. Any other command or a click elsewhere replaces or clears the plan. A client never computes routes.

Stages: `received`, `validated`, `executing`, `completed`; or `rejected`; or `interrupted`, whose `error` says why (`stale_epoch` when the world was replaced, `validation_failed` when the game refused the input, `not_ready` otherwise). `completed` means input is awaited again: a wait or other activity first runs to its end, or until the game asks something (a pop-up, an interruption); its `at` is the endpoint of that boundary. One command is outstanding at a time (`command_busy`). While a wait or other activity runs, requests are still answered; `bn.interrupt` presses the native interrupt key, so the activity asks whether to stop as in Tiles, and that question is the next boundary. Outside an activity it fails with `not_ready`.

## Loading

While the world loads, the game thread is busy and reaches no input boundary, so a client that said `bn.hello` gets `bn.loading` instead, written at each step of the native loading screen (subscribed or not). Progress is the screen the native client shows: `title` is the current context (for example "Loading files"), `entries` its list, `index` the entry in progress (earlier ones are done) and `image` the loading image, chosen like the native client does. `image.path` is relative to the game's base path, `author` is omitted when the file name has none. `done` is sent once when the loading screen ends; after it the usual `bn.resync` or `bn.events` follow. `epoch` is the epoch in force when the step was sent.

```text
<- {"jsonrpc":"2.0","method":"bn.loading","params":{"epoch":"epoch:e7f3","title":"Loading files","entries":["Terrain","Items"],"index":1,"image":{"path":"data/json/loading/Ada_dawn.webp","author":"Ada"}}}
<- {"jsonrpc":"2.0","method":"bn.loading","params":{"epoch":"epoch:e7f3","done":true}}
```

## Values

- `pos = {dim, x, y, z}` is an absolute map square; `dim` is the game dimension, `""` for the primary one. Reality-bubble coordinates never appear on the wire.
- `look = {kind, id, glyph, color}` carries appearance from game data, so text clients need no tileset. Auto-wall terrain carries the line glyph for the connections the avatar knows, as the native text view draws it; a remembered cell keeps `memory.overlay` (furniture, trap or vehicle part) over `memory.terrain`.
- A tiled client draws the same `look`s with the game's tileset. The engine publishes what only it can know, because it owns the data and the native selection, and the client never reimplements it: `tile` (the sprite id when it is not `id`: `corpse_<monster>`, `player_male`, `vp_<part>`), `looks_like` (the data's fallback chain, nearest first), `subtile` and `rotation` (the multitile key and quarter turn the native selection computed from the neighbours the avatar knows, so connection logic exists once), `stack` (the displayed item of a pile), `attitude` and `aware` (monsters and NPCs, which the native view marks with an overlay), `facing` (creatures), the cell's `light` (native `lit_level`: 0 dark, 1 low, 2 bright only, 3 lit, 4 bright), `environment.season` the avatar's `look` and, for characters, `overlays` (worn and wielded items, mutations, bionics as `look`s of kind `overlay` whose `tile` and `looks_like` list the sprite ids to try in the native order). The client owns what the tileset defines: files, weighted variants, layering and darkening. Layer order is terrain, furniture, trap, field, item, vehicle part, creature; the last entry of `fields` and `items` is the one the native view draws. A remembered cell draws darkened, a `light` of 1 shaded, and an id without a sprite falls back to its `glyph`.
- `interaction` is the native menu or dialog: `choices` holds up to the first 200 rows, fewer when they would be too large; read the rest with `bn.interaction.choices` starting at `offset = choices.length`. `choice_total` is the full count. `compat.focus` and `compat.panes` keep native list state for 1:1 ports; clients may ignore them. The main menu is tabbed like the native one: every tab is a choice without `pane_id` (`tab:new_game`, `tab:load`, ...; the selected one has `selected`), followed by the entries of the selected tab only, each with that tab's `pane_id` (`new_game:tutorial`, `settings:options`, `load:<world>`). Choosing another tab publishes a new interaction with that tab's entries; ids never depend on position or language. The crafting menu (`context` `CRAFTING`) is tabbed the same way: category tabs (`tab:CC_ELECTRONIC`), then the sub-tabs of the selected category (`subtab:CSC_ELECTRONIC_TOOLS`, absent while a search is active), then its recipes (`recipe:<hash>`), the last two with the category as `pane_id`. Choosing a tab or sub-tab lists that group; choosing a recipe crafts it; `set_count` on a recipe starts batch mode. Each recipe carries its native detail text (skills, time, tools, components) as `description` without color markup. The `filter` field is the native search; `fill` it with `submit: true` to search and with an empty value to clear.
- The world is what the avatar knows: `cells` (`remembered`, `visible` or `sensed`), `entities`, `avatar` (stats, `inventory` and `ground`), `environment`, the loaded `coverage`, the `view` and the pending `route` (empty or absent when nothing is planned). `view` is the native terrain window as `{min, max}` squares of the avatar's level (it follows the avatar): a `travel` click is accepted only inside it, anything else is rejected with `validation_failed`.
- `avatar.inventory` lists the wielded item, the worn items, then the carried stacks, each `{look, name, count?, slot}` with `slot` one of `wielded`, `worn` or `carried`; `avatar.ground` lists the stacks on the avatar's own square in the same shape without `slot`. `name` is the display name the native lists show, without color markup; both arrays are absent when empty.
- `interaction` is the native menu or dialog: `choices` holds up to the first 200 rows, fewer when they would be too large; read the rest with `bn.interaction.choices` starting at `offset = choices.length`. `choice_total` is the full count. `compat.focus` and `compat.panes` keep native list state for 1:1 ports; clients may ignore them. The main menu is tabbed like the native one: every tab is a choice without `pane_id` (`tab:new_game`, `tab:load`, ...; the selected one has `selected`), followed by the entries of the selected tab only, each with that tab's `pane_id` (`new_game:tutorial`, `settings:options`, `load:<world>`). Choosing another tab publishes a new interaction with that tab's entries; ids never depend on position or language.
- Settings screens are the native ones, opened from `settings:options`, `settings:keybindings`, `settings:autopickup`, `settings:safemode`, `settings:distractions` and `settings:colors` (the options menu and the keybindings menu also open in the game). A row with an `editor` can be set directly: `fill` with `field_id` = the row's `choice_id`, `value` in the editor's form and `submit: true`. `editor.type` is `bool` (`true`/`false`), `select` (one of `values[].id`), `integer` or `float` (within `minimum`/`maximum`) or `text` (at most `max_length`); the engine refuses anything the native menu would not take. `choose` on a row does what Enter does natively. Options: tab rows `page:<id>`, then `option:<NAME>` and `group:<id>` rows of the selected page; leaving with `cancel` asks the native "Save changes?" and saving applies and writes the options. Keybindings: `mode:add_local`, `mode:add_global`, `mode:remove` (and `mode:execute` where the native menu offers it) select what the next `action:<ID>` row does, the field filters actions by name, and a prompt for a new key has a field of type `key` that takes a key name as keybinding files spell it (`z`, `=`, `F9`, `CTRL+A`). Autopickup and safe mode: tab rows `page:<n>`, column rows `column:<n>` and `rule:<n>` rows; rule actions (add, remove, move, ...) are the screen's registered actions. Distractions: `distraction:<name>` rows with a `bool` editor (true: interrupts). Colors: `column:<n>` and `color:<name>` rows; choosing a color opens the native color list.
- The world is what the avatar knows: `cells` (`remembered`, `visible` or `sensed`), `entities`, `avatar` (stats and `inventory`), `environment`, the loaded `coverage`, the `view` and the pending `route` (empty or absent when nothing is planned). `view` is the native terrain window as `{min, max}` squares of the avatar's level (it follows the avatar): a `travel` click is accepted only inside it, anything else is rejected with `validation_failed`.
- `avatar.sidebar` is the native classic sidebar as values from the same getters: `limbs` (`id`, `label` and its condition color, `hp`, `hp_max`, bar `color`, `broken`), `pain`, `hunger`, `thirst`, `fatigue`, `temperature`, `power`, `location`, `weather` (`{text, color}` in native color names), `focus`, `morale` (`level`, native `face`), `stamina`, `speed`, `movement` (`counter`, `mode` of `walk`, `run`, `crouch` or `prone`), `safe_mode`, `time` (`season`, `day`, `clock`: the watch time, else the approximate time of day, empty underground), optional `ambient_temperature` (thermometer only), `weapon` and optional wielded martial-arts `style`. Texts are the engine's display language; the other values are language-neutral. `stats` entries may carry the native `color`.
- The terrain window is sized like a native window resize. `viewport = {cols, rows}` (1 to 512 map cells) in `bn.hello`, or later `bn.viewport`, resizes the terminal to those cells plus the side panels, so the engine shows that much map around the avatar; the effective `view` is authoritative because the native minimum terminal size still applies. A client that sends none gets the default terminal.
- Every event carries a generic `changes` block, applied in this order: `coverage`, `view`, `cells`, `forgotten`, `entities`, `gone`, then `avatar`, `environment`, `route` and `interaction` replace.
- `message.logged` is the native message log: `data = {id, text, kind, color, count}`, one event per line in log order, attached to the command that caused it. `text` is the translated line without color tags and without the "x N" suffix; `kind` is the native type (`good`, `bad`, `mixed`, `warning`, `info`, `neutral`, `debug`) and `color` the native color name of that type. When the last line repeats, the same `id` is published again with a higher `count` and replaces the line. It changes no state (no `revision`) and, like every transient event, is lost across a `bn.resync`. Messages the native log hides by cooldown are not published.
- `projectile.moved`, `explosion.started`, `explosion.blast`, `explosion.ended` and `combat_text.shown` are the native animation facts. They are published where the game starts an animation, so every client gets the same facts without a renderer. They change no state, are attached to the command that caused them, and arrive before the state event of that command. `display.duration_ms` is how long the native animation holds the fact (the `ANIMATION_DELAY` option per step). `projectile.moved` carries `path` (squares in drawing order; one square per event while a shot flies) and a `look` of kind `projectile` with its glyph and custom sprite id. `explosion.started` carries `at`, `radius`, `color` and the `tile` id the native tiles draw it with. The facts follow the native options: nothing is published with `ANIMATIONS` off, and combat text also needs `ANIMATION_SCT`. `explosion.blast` lists one ring of `cells` of an explosion shaped by terrain. `combat_text.shown` is the floating text of one or two colored `segments` at `at`.

Events are `interaction.changed`, `coverage.moved`, `turn.passed`, `cells.seen`, `message.logged` the animation facts above and, later, the remaining presentation types listed in the schema. Event types added before 1.0 is released belong to 1.0; once it is released, a new event type needs a new exact version.

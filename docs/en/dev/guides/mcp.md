---
title: Playing through MCP
---

The BN MCP client lets an agent operate the game through its existing screens and controls. Character creation, world selection, inventory, crafting, targeting, dialogue, and saving use the same game code as the interactive clients.

Use the `linux-clients` preset, or enable `MCP=ON` when [configuring CMake](building/cmake.md), then build the `cataclysm-bn` target. It can be enabled alongside tiles and curses; all three use the same engine build. Select the client explicitly with `--client=mcp` when launching a shared binary.

## Connect

Configure an MCP host to launch the executable with stdio transport. Use absolute paths for the executable, data directory, and profile. For example, adapt these paths to your checkout and build output:

With the `linux-clients` preset, the canonical binary path is `out/build/linux-clients/src/cataclysm-bn`.

```json
{
  "mcpServers": {
    "bn": {
      "command": "/path/to/Cataclysm-BN/out/build/linux-clients/src/cataclysm-bn",
      "args": [
        "--client=mcp",
        "--datadir",
        "/path/to/Cataclysm-BN/data/",
        "--userdir",
        "/path/to/bn-agent-profile/",
        "--configdir",
        "/path/to/bn-agent-profile/config/"
      ]
    }
  }
}
```

The selected profile contains configuration, worlds, saves, and character templates. Always pass
both directory options: an XDG build can otherwise read and write the account's normal configuration
despite `--userdir`. Before connecting, append `--paths` once and verify that `Config Directory`
resolves inside the profile. Add `--world NAME` to load a world through the normal game startup path.
Otherwise, use the opening menu to create or select a world and character.

The transport uses [MCP stdio](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports#stdio): newline-delimited JSON-RPC on stdin and stdout. Each input frame may contain at most 1 MiB before its terminating newline. An oversized, unterminated, or unreadable frame closes the session with a nonzero exit status; no prefix of a rejected frame is executed. Game diagnostics go to stderr. The MCP client duplicates the original stdout before redirecting game output, so protocol stdout remains valid JSON-RPC even when Lua or engine code writes diagnostics. No terminal window is required.

## Play

| Tool             | Use                                                                                 |
| ---------------- | ----------------------------------------------------------------------------------- |
| `bn.observe`     | Read the current composed screen.                                                   |
| `bn.state`       | Read structured player-visible and player-known game state.                         |
| `bn.actions`     | Discover the active context, action names, and actual keybindings.                  |
| `bn.press`       | Send keys or registered actions through the normal input pipeline.                  |
| `bn.interaction` | Read live structured choices or a text/numeric field without screen interpretation. |
| `bn.interact`    | Apply one semantic choice, field, quantity, target, or cancel operation.            |

The `bn://screen` resource also exposes the screen. `rows` contains UTF-8 text and `styles` preserves foreground/background colors as `[start_cell, length, foreground, background]` runs. Cell indices run from left to right, then top to bottom. Coordinates are terminal cells; a wide character occupies two cells. Cursor coordinates identify text-entry and selection positions.

The `bn://state` resource and `bn.state` tool report the avatar, active activity and ID, needs, HP by body part, inventory and nested contents, worn and wielded equipment, visible map terrain/furniture/traversability/creatures/items, and known overmap terrain. `game_ready` is false during the opening menu and character creation and becomes true only after world and avatar setup completes. The default state view covers a 12-cell visible-map radius and an 8-OMT known-overmap radius; the response includes these limits in `coverage`. Visibility checks exclude unseen creatures and items behind unseen terrain. Ground items expose their exterior only; nested contents are included for owned inventory and equipment. Avatar and visible-map positions use reality-bubble map squares; `avatar.absolute_position` stays stable across bubble shifts. Known overmap positions use absolute overmap tiles. `dimension` identifies the current dimension.

Prefer `bn.interaction` when `structured` is true. Choice descriptors include opaque IDs, labels, descriptions, denial text, actual enabled state, effective selectability, `selected`, and `highlighted`. `selected` reports committed selection state, while `highlighted` reports the live navigation focus; ordinary single-choice menus mirror their current row in both fields. Inventory multiselectors set `selected` only when the chosen count is greater than zero. An active single-item picker has no committed selection, so its focused row is only `highlighted` until `choose` closes the picker. Count-capable choices also include `selected_count`, optional `minimum_count`, and `available_count`; omitted `minimum_count` means zero. For inventory rows, `label` and `columns` match the selector row, while `description` contains the detailed item information shown by the normal `EXAMINE` action; that action remains available for the full in-game view. Field descriptors include the current value, text or integer type, maximum display width, and printable-character constraint. Target descriptors include the reality-bubble map-square source and cursor, range, distance policy, bounds, status, and visible candidates. Choice pages default to 100 entries and accept `offset` and `limit`; `choice_page.offset` and `choice_page.total` report the returned position and full count. Two-pane views also publish `panes` with opaque pane and area IDs, view-specific roles (`source`, `destination`, `npc`, or `player`), area labels and descriptions, filters, and an explicit `storage_kind` (`inventory`, `worn`, `container`, `ground`, `cargo`, or `mixed`); their choices link back with `pane_id`, the item's actual `area_id`, and its own `storage_kind`.

Every `bn.interact` request supplies the current `input_id` and exactly one operation. `choose` takes a snapshot's `choice_id`; `fill` takes `field_id`, `value`, and an explicit `submit` boolean; `set_count` takes `choice_id` and a nonnegative `count`; `set_target` takes a `position` with integer `x`, `y`, and `z`, plus an optional published `candidate_id`; `cancel` takes no payload. Zero unselects inventory items, but a published `minimum_count` may require a positive count. The response is deferred until the operation is consumed and returns the next stable interaction snapshot. Stale boundaries, changed schemas or target identities, unknown IDs, disabled choices, invalid fields, unavailable quantities, and out-of-range or out-of-bounds targets are rejected without partial mutation.

Startup root/submenus, `uilist`, ordinary choice/confirmation/acknowledgement popups, `string_input_popup` text/numeric fields, common inventory selectors, the crafting recipe selector, the common targeting UI, common direction prompts, and construction currently expose structured descriptors. Inventory IDs bind to filtered actionable item entries, never category headers; single-item pickers use `choose`, while drop, pickup, and item-use multiselectors accept `set_count` through their existing selection callbacks. Compare selection supports item choices but not quantity. Crafting choices preserve the native filtered/tab/nested order and craftability; `set_count` selects batch mode from 1 through 50 without starting the craft, and a following `choose` uses the normal confirmation and component checks. Target `set_target` only moves the existing cursor through normal range and bounds policy; firing, aiming, throwing, and safety confirmations remain registered actions and normal nested prompts. Direction choices contain the eight horizontal relative vectors, Here, and Above/Below only when the native caller permits vertical input. Construction choices preserve the live filtered order and availability; choosing one follows native confirmation, adjacent placement, resource consumption, denial, and activity handling. Blueprint selection does not require construction materials. The separate ground `PICKUP` view publishes only its filtered live stacks, with type, source, nesting, wear/wield checks, and exact available and selected quantities. `choose` uses native parent/child toggle propagation, `set_count` selects zero through the available quantity, and `CONFIRM`, `WEAR`, `WIELD`, `SELECT_ALL`, and filtering remain registered native actions. Advanced inventory publishes the filtered item rows from both native panes. Its `choose` only focuses the linked pane and row; transfer still uses `MOVE_SINGLE_ITEM`, `MOVE_VARIABLE_ITEM`, or `MOVE_ITEM_STACK`, and variable quantities use the native integer popup. Capacity, liquid, container, ownership, activity, and re-entry behavior therefore remain native. NPC trade publishes its filtered NPC/player panes, unit prices, selected and available quantities, balance, and capacity status. `choose` uses the native item toggle and opens the existing quantity field for a multi-count item; `set_count` directly selects an exact quantity from zero through the available amount. Selection never exchanges items: the registered `CONFIRM` action still runs native credit, debt, NPC capacity, and final confirmation checks, and only an accepted confirmation reaches the native transaction.

Help publishes its loaded topics as opaque choices. `choose` opens the native topic reader. Help readers and the shared read-only `SCROLLABLE_TEXT` viewer expose `structured:true`, `kind:"custom"`, and the complete `title` and `message`, preserving translated content, key substitutions, links, and color markup. They expose no item choices, quantities, field, or target. Use discovered native paging/close actions or `cancel`. Scrolling changes the input boundary, not the text schema; closing returns to the caller. Nested views expose their own descriptor or explicit action fallback.

NPC dialogue exposes its already-evaluated narrative and available responses, including trial chances and costs displayed by the native UI. Select a published `choice_id`; identical labels can represent different responses. Observation does not reevaluate dialogue or reroll trials. Look at, Size up stats, Yell, and Check opinion use their native topics. Dialogue does not offer `cancel`: choose an available goodbye or back response. Accepted choices follow native confirmations, trials, effects, and the topic stack. Query the new snapshot after each choice; nested prompts use their own contexts and IDs. Raw keyboard controls and paging remain available.

The `MORALE` reader is structured, with `kind:"custom"` and the complete native `title` and `message`. It has no choices, quantity, field, or target. Use its registered scroll/close actions or `cancel`; passive reads preserve stored morale and RNG.

Unmigrated custom views return `structured:false`, `actions_only:true`, their current context, and input ID. Use `bn.actions`/`bn.press` as the explicit compatibility fallback. Vehicle interaction, character-creation custom views, and other bespoke selectors are not yet semantically migrated.

Inspect the interaction or screen, send one input, and inspect the resulting interaction before choosing the next input. A single operation may open another menu, so action names and semantic IDs are resolved in the context that is active when each event is consumed.

For example, a raw key works in menus and prompts:

```json
{ "name": "bn.press", "arguments": { "keys": [{ "key": "ENTER" }] } }
```

Use an action returned by `bn.actions` to respect the profile's keybindings:

```json
{ "name": "bn.press", "arguments": { "keys": [{ "action": "inventory" }] } }
```

Actions must be registered and bound in the current context. Screens with direct keyboard handling remain accessible through raw keys. `bn.actions` also returns the current `input_id` and `timeout_ms`. Attach the ID to an event to reject input based on an obsolete observation:

```json
{ "name": "bn.press", "arguments": { "keys": [{ "action": "inventory", "input_id": 42 }] } }
```

Use the actual returned identity, not the example value. Each input read advances it, including nested prompts. For an intentional `bn.press` multi-event batch, guard its first event; reusing that identity on every event rejects the second event. Commands without an identity retain the `bn.press` compatibility behavior. Semantic `bn.interact` commands always require an identity and permit one operation per request. The protocol does not expose arbitrary Lua evaluation or reveal hidden world data; use the game's examination, map, and character screens to inspect information available to the player.

For text entry, send a `text` event:

```json
{ "name": "bn.press", "arguments": { "keys": [{ "text": "Survivor" }] } }
```

Mouse input follows the same input pipeline. Coordinates are terminal cells:

```json
{
  "name": "bn.press",
  "arguments": { "keys": [{ "mouse": { "x": 30, "y": 12, "button": "left" } }] }
}
```

Supported buttons are `left`, `right`, `scroll_up`, `scroll_down`, and `move`. An event supplies one of `action`, `key`, `text`, or `mouse`; `text` may also accompany a `key` event when a prompt needs both the physical key and entered UTF-8 text. Modifiers are accepted with keyed events and are rejected for text-only, action, and mouse events.

The game pauses at input boundaries while waiting for the agent. When `bn.actions.timeout_ms` is `0`, send `{"key":"IDLE"}` to complete one nonblocking poll without requesting an action. `IDLE` is rejected at blocking and positive-timeout boundaries. Send `{"key":"TIMEOUT"}` to represent expiration of a positive timed poll. A batch accepts up to 256 events. Each event is resolved when it reaches the current screen; if an event is invalid, the remaining batch is rejected and earlier events remain applied.

Saving and quitting use the game's normal actions and confirmation prompts. Disconnecting the MCP host ends the session; it does not replace an explicit in-game save.

Add `--replay-record PATH` when launching the game to record the session. Later, `--client=mcp --replay-play PATH` runs it without a host. Follow the [replay guide](replay.md) to prepare identical initial profiles and run the real-session smoke check.

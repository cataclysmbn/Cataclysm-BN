---
title: Engine/client protocol 0.3
---

An external client plays the game over newline-delimited JSON-RPC 2.0 on the stdio of `cataclysm-bn-tiles --client=mcp`. The engine describes each screen and what can be done on it, so a client needs no game rules.

## A session

```text
-> {"jsonrpc":"2.0","id":1,"method":"bn.hello","params":{"versions":["0.3"],"client":{"name":"my-client","version":"1"}}}
<- {"jsonrpc":"2.0","id":1,"result":{"version":"0.3","epoch":"epoch:e7f3",...}}
-> {"jsonrpc":"2.0","id":2,"method":"bn.subscribe","params":{}}
<- {"jsonrpc":"2.0","id":2,"result":{"at":{"epoch":"epoch:e7f3","sequence":"40","revision":"31"},"interaction":{...},"entities":[],"parts":1}}
<- {"jsonrpc":"2.0","method":"bn.snapshot.part","params":{"epoch":"epoch:e7f3","at":{...},"index":0,"last":true,"cells":[...]}}
-> {"jsonrpc":"2.0","id":3,"method":"bn.command.submit","params":{"epoch":"epoch:e7f3","expect":{"revision":"31","boundary_id":"boundary:90"},"operation":{"kind":"action","action_id":"RIGHT"}}}
<- {"jsonrpc":"2.0","id":3,"result":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"received"}}
<- {"jsonrpc":"2.0","method":"bn.events","params":{"epoch":"epoch:e7f3","events":[{"sequence":"41","revision":"32","type":"interaction.changed","command":"c:5","changes":{...}}]}}
<- {"jsonrpc":"2.0","method":"bn.command","params":{"epoch":"epoch:e7f3","command_id":"c:5","stage":"completed","at":{"epoch":"epoch:e7f3","sequence":"41","revision":"32"}}}
```

Collect the `parts` snapshot parts, then apply `bn.events` in order. A gap, an epoch mismatch or `bn.resync` means: call `bn.subscribe` again.

## Methods

| Method                   | Params                                                    | Result                                                                         |
| ------------------------ | --------------------------------------------------------- | ------------------------------------------------------------------------------ |
| `bn.hello`               | `versions`, `client`, `locale?`, `capabilities?`          | `version`, `epoch`, `engine`, `limits`, `delivery?`, `locale`, `engine_locale` |
| `bn.viewport`            | `cols`, `rows`                                            | none                                                                           |
| `bn.interrupt`           | none                                                      | none                                                                           |
| `bn.hold`                | `epoch`, `screen_id`, `affordance_id`                     | none                                                                           |
| `bn.release`             | none                                                      | none                                                                           |
| `bn.subscribe`           | none                                                      | snapshot header; parts and events follow                                       |
| `bn.unsubscribe`         | none                                                      | none                                                                           |
| `bn.interaction.choices` | `epoch`, `boundary_id`, `component_id`, `offset`, `limit` | `boundary_id`, `component_id`, `total`, `rows` (reads only)                    |
| `bn.world.describe`      | `epoch`, `boundary_id`, `pos`                             | `lines` (reads only)                                                           |
| `bn.world.cells`         | `epoch`, `min`, `max`                                     | `at`, `cells`, `forgotten` (reads only)                                        |
| `bn.overmap.cells`       | `epoch`, `min`, `max`                                     | `overmap` squares of an overmap level (reads only)                             |
| `bn.command.submit`      | `epoch`, `expect`, `operation`                            | `command_id`, `stage: "received"`                                              |
| `bn.command.result`      | `epoch`, `command_id`                                     | latest stage, for recovering a lost notification                               |
| `bn.credit`              | `frames`                                                  | `credits` held after the grant                                                 |

Notifications from the engine: `bn.snapshot.part`, `bn.events`, `bn.command`, `bn.resync`, `bn.loading`, `bn.progress`.

Errors use code `1000` with `error.data = {kind, action?, at?}`; `action` says what to do next (`hello`, `subscribe`, `retry`).

## Screens

Every boundary carries `stack`, the open screens, outermost first. Only the last screen has affordances. While the map (role `map`) is last, no other screen takes input. To act, send `bn.command.submit` with `operation: {kind: "invoke", affordance_id, args?}` and the affordance's `expect` scope:

- `screen`: `{screen_id}`, valid while that screen is on top, else `stale_screen`.
- `boundary`: `{boundary_id, revision}`. While a command runs the submit fails with `command_busy`.

A screen without a description of its own is role `screen`, kind the lower-cased name of its input context, with the terminal cells as a `canvas`. `avatar.sidebar` is the native sidebar in the same cell form.

## Commands

`operation.kind` also takes `action` (a registered action) and `travel` (a map click), which expect `{boundary_id, revision}`. A stale `expect` is rejected, so a client never acts on a screen it has not seen.

Stages: `received`, `validated`, `executing`, `completed`; or `rejected`; or `interrupted`, whose `error` says why.

## Delivery credits

Opt in with `bn.hello` `delivery = {credits, max_hz?}`. A pushed `bn.events` or `bn.command` frame costs one credit. With none left the engine holds frames until `bn.credit`. Past the hold bounds it sends `bn.resync` with reason `backpressure`.

## Framing

A frame is one JSON value on one line, at most `limits.frame_bytes` long. A line that is not JSON is answered with `-32700`, JSON of the wrong shape with `-32600`, an unknown `bn.` method with `-32601` and bad params with `-32602`. An oversized or cut-off frame ends the engine with exit code 1.

## One clock

- `epoch` changes when the process starts or the world is replaced.
- `sequence` counts published events (decimal string, contiguous). `revision` counts events that change state. `at = {epoch, sequence, revision}` means "includes every event up to sequence".

## Values

- `pos = {dim, x, y, z}` is an absolute map square; `dim` is the game dimension, `""` for the primary one.
- `look = {kind, id, glyph, color, name}` carries appearance from game data, so text clients need no tileset.
- Event `changes` apply in this order: `coverage`, `view`, `cells`, `forgotten`, `entities`, `gone`, then `avatar`, `environment`, `route` and `interaction` replace.

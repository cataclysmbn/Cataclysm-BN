---
title: Engine/client protocol 1.0
---

## Scope and compatibility

The [Draft 2020-12 schema](../../../schema/engine-client/1.0.schema.json) defines closed **application values**, not JSON-RPC framing. The shared C++ core is transport-neutral. The four JSON-RPC 2.0 methods below are the adapter contract; they are **not yet connected to a server**. Existing MCP tools, MCP negotiation and replay format 1 are unchanged.

Version negotiation selects the exact string `"1.0"` from the client's supported versions. There is no approximate minor-version match. Required unsupported capabilities fail atomically; unsupported optional capabilities are omitted. Breaking semantics require a new major version. Additive minor versions still require exact negotiation and explicitly negotiated capabilities.

The core supports these capability names, returning only the requested supported intersection:

- `snapshot.readiness`, `snapshot.actions`, `snapshot.interaction`
- `command.semantic_interaction`, `command.registered_action`
- `events.interaction_replaced`, `delivery.inline_completion`

No world/perception, push, credits, history or reconnect capability is supported. Future producer names are `projectile.step`, `projectile.impact`, `entity.moved`, `terrain.changed`, `field.changed`, `explosion.phase`, `chain.reaction`, `visibility.transition` and `presentation.compatibility`. They are **not valid 1.0 event types**; they need owned payloads and reviewed engine disclosure before admission. Asset references must eventually use data IDs, never renderer handles or file paths.

## Adapter methods

Validate each method's `params` and `result` against the named schema definition. Reject extra members, wrong scalar types and malformed operation unions before receipt; execute no prefix of a malformed request. JSON-RPC parse/request/method/parameter errors remain standard JSON-RPC errors. Application errors use JSON-RPC code `1000`, fixed message `Engine contract error`, and closed `$defs/application_error` in `error.data`; never echo native resolver or parser diagnostics.

| Method                  | Params definition     | Result definition     | Required capabilities                                                  |
| ----------------------- | --------------------- | --------------------- | ---------------------------------------------------------------------- |
| `bn.contract.negotiate` | `negotiation_request` | `negotiated_contract` | None                                                                   |
| `bn.snapshot.get`       | `snapshot_request`    | `snapshot`            | All three `snapshot.*` capabilities                                    |
| `bn.command.submit`     | `command_request`     | `receipt`             | Capability for its operation                                           |
| `bn.command.result`     | `result_request`      | `command_response`    | `delivery.inline_completion`; `events.interaction_replaced` for deltas |

The adapter owns negotiation gating, strict parsing, JSON-RPC IDs/framing and connection state. I/O must not access the world. Only the game thread may capture or resolve at an active, stable native input boundary.

Negotiation params:

```json
{
  "supported_versions": ["1.0"],
  "required_capabilities": [
    "snapshot.readiness",
    "snapshot.actions",
    "snapshot.interaction",
    "command.semantic_interaction",
    "events.interaction_replaced",
    "delivery.inline_completion"
  ],
  "optional_capabilities": ["command.registered_action"]
}
```

Snapshot params:

```json
{ "session_epoch": "epoch:opaque", "page": { "offset": 0, "limit": 100 } }
```

Semantic submit params:

```json
{
  "session_epoch": "epoch:opaque",
  "based_on": {
    "state_revision": "7",
    "input_boundary_id": "boundary:opaque",
    "interaction_schema_id": "schema:opaque"
  },
  "operation": { "kind": "choose", "choice_id": "choice:opaque" }
}
```

Registered actions use `invoke_registered_action` and `action_id`. Semantic operations reuse native `choose`, `fill`, `set_count`, `set_target` and `cancel` semantics. `fill` always supplies `submit`, including `false`. Enabled, selectable, highlighted and selected are distinct native properties: do not reinterpret `enabled` as `selectable`, silently select a highlighted row, or bypass native denial/confirmation logic.

## Identity, coordinates and published state

- `session_epoch` identifies one authoritative session/world/process. Generate it once on a new session/world or process restart, not on reads or socket polling. `new_session_epoch()` uses independent system entropy, never gameplay RNG; tests can inject an epoch string. It is not an authentication credential.
- Revisions and sequences are canonical decimal **strings** representing uint64, without leading zeroes. Exhaustion is an explicit `resource_limit`, requiring a new epoch/snapshot rather than wraparound. Opaque IDs are nonempty strings; clients must not parse them. Native numeric `input_id` never appears on this wire.
- Public sequences start at `"1"` and are contiguous within an epoch. Hidden candidates consume neither public ID nor sequence/revision. Public cause links reference an earlier **published** sequence in the same epoch, never an internal ordinal or hidden cause.
- `state_revision` versions committed readiness/actions/interaction **boundaries**, independently of choice-page projection. Reads, passive page switches and unchanged captures advance neither revision nor sequence. This is not a counter for every internal world mutation.
- A snapshot contains the complete published **bounded page**, including its offset/count/total, and nullable interaction. Preserve all actual native description, denial, pane, field, count and target data for that page. No placeholder world view or generic JSON payload is admitted.
- `state.projection` is the structural view identity `{kind: "interaction_choices", offset, limit}` of the **requested** window. Native `choice_page.offset` remains clamped to the total; both offsets must be safe JSON integers. Different windows are views of one session clock, not separate authoritative streams.
- `capture_state` produces an owned read-only candidate, not a commit. At the committed stable boundary, `event_stream::project_snapshot` materializes any bounded requested page under the existing epoch/revision/sequence. Page 0 → page 200 → page 0 is an ordinary read: no event, counter change, resync or lost history. Boundary/schema/total/common metadata must match the session reference; same-window values must also match. P1-approved native captures supply immutable rows for that boundary, not arbitrary I/O DTOs. Description purity and bounded upstream generation remain P1 dependencies.

Target positions explicitly use `space: "reality_bubble_map_square"` and the active opaque `frame_id`, plus signed 32-bit `x/y/z`. The core maps them to the native `bubble_ms` position **only after exact frame validation**. Candidate identity, bounds, range and distance metric still pass through the native resolver. Absolute map-square/overmap-tile coordinate definitions require `dimension_id`; they are reserved value forms, not supported target operations or world-event capabilities. No coordinate-space guessing is permitted.

## Owned events and reconstruction

`interaction.replaced` is the only implemented event. Its payload is `{base_state_revision, state}`: a complete replacement of the bounded readiness/actions/interaction projection, not an arbitrary patch. Closing an interaction publishes `interaction: null`. A changed value advances revision by exactly one.

The engine supplies `disclosure::publish` or `disclosure::withheld` at the logical boundary. The core drops withheld candidates **before** assigning public IDs, checking causes or committing counters. Sinks cannot ask visibility questions or acquire remembered knowledge. Existing native choice IDs are only exported as part of engine-authorized interaction values. Future entity IDs must be minted after disclosure; preserve reappearance identity only if authoritative remembered knowledge permits it, otherwise mint a new ID.

Events own their strings, containers, native interaction values and tagged coordinates. `public_event` exposes only const access; it retains no world pointer, reference, string view or renderer handle. Mutating or destroying the source cannot alter an event. Null and recording sinks do not mutate authority or consume gameplay RNG. The recorder is bounded in-process storage, **not retained transport history**.

At a real engine boundary, publish exactly once through the session stream, then use pure `project_event` with a same-boundary native capture for each active requested view. Copies own their rows and keep the **same** epoch/event ID/sequence/base/result revision and cause; projection metadata and rows may differ. This does not publish another event or allocate a stream-per-query registry. Capture/materialize while that boundary is valid; later boundaries cannot reconstruct old rows from the live provider. Adapter-owned pending delivery stays bounded.

From snapshot `(epoch, R, S, projection)`, apply only the next contiguous sequence `S + 1` in the same epoch and projection, with delta base revision `R` and resulting revision `R + 1`. Epoch mismatch, gap, duplicate, invalid cause or base mismatch requires a fresh snapshot. A projection mismatch is a wrongly assembled delivery: obtain the correct materialization, not a new epoch or a page-switch recovery. `apply_batch` is transactional: an invalid suffix applies no prefix.

Optional `display` carries `group_id`, zero-based `ordinal`, positive `count`, and nonnegative advisory `duration_ms`. Ordinal must be less than count. Grouping and duration never determine causal order, revisions, gameplay time or RNG. There is no standalone presentation-only event capability yet.

An empty batch is exactly:

```json
{ "complete": true, "first_sequence": null, "last_sequence": null, "events": [] }
```

Nonempty first/last metadata must identify the actual first/last events. `complete: true` means the supplied batch is complete; it does not imply long-activity completion or transport history availability.

## Command lifecycle and bounds

1. **Receipt:** accept a well-formed typed envelope, allocate an opaque command ID, retain one request. This is not live validation.
2. **Validation:** check epoch, revision, live input boundary, live interaction schema, permission and native selection/quantity/field/target/action rules. Return the existing native `input_event`; no callback or world mutation occurs. Stale/invalid input becomes terminal `rejected` and produces no event or revision.
3. **Execution:** the adapter calls `execution_started()` only when delivering that resolved input to the existing widget. `native_input_delivered` does not assert that a requested gameplay effect succeeded. Native denial and nested confirmation remain native outcomes, not transport validation failures.
4. **Completion:** commit a disclosed replacement at the next **distinct native interaction boundary**, then complete with its revision and sequence (or explicitly commit a resynchronization snapshot on resource overflow). `next_interaction_boundary` is **not** a blanket declaration that crafting, waiting or another long activity has finished. If the session ends before another boundary, interrupt rather than fabricate completion.

Published readiness is information, **not live permission authority**. Construct `command_lifecycle(epoch, command_authority&)` with a read-only engine-session policy that outlives it. Validation calls `current_permissions()` at that moment; stale published booleans can neither grant nor deny permission. Engine policy remains engine-supplied; I/O/requests never supply this dependency. Forging C++ engine values or binding a cached DTO as policy is outside the supported integration, not a reason for a global registry.

Receipts, result requests and command results carry the authoritative `session_epoch`. Inline batches are scoped to this one command: every event must match its epoch, and any non-null event `command_id` must match the response command. Incidental engine events may have null command IDs. Noncompleted results have accurately empty batches. Completed normal results require nonempty coherent events through the completion endpoint; `resync_required: true` requires an **accurately empty** batch, never nonempty required events. C++ and schema enforce this reciprocal rule.

`command_lifecycle` retains one outstanding command and one replaceable terminal result. Revalidation/redelivery is rejected. A second outstanding submit returns `command_busy`; a new accepted submit replaces the prior terminal result, whose ID then returns `unknown_command`. Polls do not advance state. Replay continues to record the existing resolved semantic input, not contract epoch/command IDs.

Limits are one outstanding command, 1–200 requested choice rows, eight inline events and 262144 UTF-8 bytes for the serialized application response, excluding transport framing. Quantities/page indices use nonnegative safe JSON integers through 9007199254740991; coordinates remain int32. The adapter additionally bounds framing and request bytes before parsing. Never silently truncate a required description/event or report a dropped batch as complete.

A rejected sink publication leaves the stream snapshot/counters unchanged. Drain an accepted batch and retry the same candidate. For this **state-only** slice, the engine may instead call `event_stream::resynchronize` at the stable boundary: it commits a bounded fresh snapshot, advances a changed revision, and leaves public sequence/epoch unchanged. The adapter must report `resync_required: true` with an accurately empty batch; never return an old snapshot as the new state. This is not a passive read or ordinary page switch, cannot discard required future presentation events, and never rotates an epoch merely because of overflow. `serialize_command_response` checks the combined lifecycle-plus-batch byte bound and completion endpoint. If required inline data cannot fit, return explicit `resource_limit`, or a completed result with `resync_required: true` and an accurately empty batch, followed by a fresh authoritative snapshot. If a required snapshot itself cannot fit, keep the failure explicit; reduce the requested page where possible, never shorten its descriptions. The core cannot roll back an already delivered command and provides no push, history, credits, yielding or reconnect implementation.

## Integration seam

`engine_client_wire.h` supplies pure typed decoders for all four request values and bounded `serialize_receipt` / `serialize_application_error`. Pass complete original application bytes (at most 262144), not values reencoded by legacy `JsonIn` or floating point. Decoding preserves embedded NUL and supplementary Unicode, rejects invalid UTF-8 and decoded duplicate keys, and accepts mathematical integer decimal/exponent spellings exactly. It neither allocates command IDs nor accesses native authority. JSON-RPC framing, envelope validation and dispatch remain separate and unconnected.

Use `engine_client_contract.h` and `engine_client_event.h`. The stream and command lifecycle belong to the **authoritative engine session**, not a connection. Call `event_stream::create` once per new epoch with a validated bounded `snapshot` and initial revision/sequence zero. Keep one bounded reference projection in that stream, not one stream per page. This reference is fixed for the stream lifetime: `replace` rejects a different view at a changed boundary, and `resynchronize` rejects every reference-view change. Requested pages still use pure materialization. A connection retains its current requested projection and switches by `project_snapshot`. At the next real boundary, project the one committed event into that requested view before delivery. Later connections/renegotiation take a fresh `project_snapshot` for their requested window from the existing session clock; never reset counters or recreate command IDs under the same epoch. No historical resume is implied. Capture only actual native interaction providers after P1 purity acceptance; do not use renderer visibility or `game_observation` as a disclosure authority. Submit typed commands, validate through the existing resolver, deliver the returned input once, publish/complete at the next boundary, and assemble the bounded result with `serialize_command_response`. New connections conservatively negotiate and take a fresh snapshot; do not advertise replay/resume continuity.

This core does not establish P1/P2 purity, world knowledge acquisition, combat producers, native presentation migration, cross-process conformance, streaming or complete M1 acceptance.

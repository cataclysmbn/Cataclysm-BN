#!/usr/bin/env -S deno run --allow-read --allow-write --allow-run --allow-env
/**
 * @module
 * Exercise real direct-protocol receipts, native input, recovery and process shutdown.
 * Uses a new private profile; never initializes legacy MCP as fallback or fabricates a world.
 */
import { Command } from "@cliffy/command"
import { assert } from "@std/assert"
import { resolve } from "@std/path"
import * as v from "@valibot/valibot"

// These are assertion views, not a duplicate application decoder/schema.
const Envelope = v.looseObject({
  jsonrpc: v.literal("2.0"),
  id: v.string(),
  result: v.optional(v.unknown()),
  error: v.optional(v.unknown()),
})
const ApplicationError = v.looseObject({
  code: v.literal(1000),
  message: v.literal("Engine contract error"),
  data: v.looseObject({ kind: v.string(), stage: v.string(), required_action: v.string() }),
})
const Negotiated = v.looseObject({ session_epoch: v.string() })
const Choice = v.looseObject({
  id: v.string(),
  label: v.string(),
  enabled: v.boolean(),
  description: v.string(),
})
const Snapshot = v.looseObject({
  session_epoch: v.string(),
  state_revision: v.string(),
  through_public_sequence: v.string(),
  state: v.looseObject({
    input_boundary_id: v.string(),
    readiness: v.looseObject({ phase: v.string(), game_ready: v.boolean() }),
    actions: v.array(v.looseObject({ id: v.string() })),
    interaction: v.nullable(v.looseObject({
      schema_id: v.string(),
      context: v.string(),
      message: v.string(),
      choices: v.array(Choice),
    })),
  }),
})
const Receipt = v.looseObject({
  session_epoch: v.string(),
  command_id: v.string(),
  stage: v.literal("received"),
})
const Result = v.looseObject({
  command: v.looseObject({
    stage: v.string(),
    execution: v.looseObject({ status: v.string() }),
    validation: v.looseObject({ status: v.string(), error: v.optional(v.string()) }),
    completion: v.nullable(v.looseObject({ status: v.string() })),
  }),
})
type SnapshotValue = v.InferOutput<typeof Snapshot>
const capabilities = [
  "snapshot.readiness",
  "snapshot.actions",
  "snapshot.interaction",
  "command.semantic_interaction",
  "command.registered_action",
  "delivery.inline_completion",
  "events.interaction_replaced",
]
const negotiation = (required = capabilities) => ({
  supported_versions: ["1.0"],
  required_capabilities: required,
  optional_capabilities: [],
})
const epoch_patch = v.looseObject({ session_epoch: v.optional(v.string()) })

async function smoke(
  binaryArgument: string,
  outputArgument: string,
  scenario: {
    loadedWorld: boolean
    transportCase?: string
    nativeShutdown: boolean
    legacyBatch: boolean
  },
) {
  const { loadedWorld, transportCase, nativeShutdown, legacyBatch } = scenario
  assert(
    !legacyBatch || (!loadedWorld && transportCase === undefined && !nativeShutdown),
    "legacy-batch is a separate compatibility probe",
  )
  assert(
    !loadedWorld || transportCase === undefined,
    "loaded-world and transport-case are separate invocations",
  )
  assert(
    transportCase === undefined ||
      ["partial-eof", "batch-output-limit", "output-loss"].includes(transportCase),
    "unknown transport case",
  )
  const binary = resolve(binaryArgument)
  const output = resolve(outputArgument)
  const cwd = Deno.cwd()
  await Deno.mkdir(output) // Never replace previous evidence.
  const profile = `${output}/profile`
  await Deno.mkdir(`${profile}/config`, { recursive: true })
  await Deno.writeTextFile(
    `${profile}/config/options.json`,
    JSON.stringify([
      { name: "USE_LANG", value: "en_US" },
      { name: "ANIMATIONS", value: "false" },
      { name: "AUTOSAVE", value: "false" },
      { name: "COMPUTE_ACCELERATION", value: "gpu_software" },
    ]),
  )
  await Deno.writeTextFile(
    `${profile}/config/preload.json`,
    JSON.stringify({ compute_acceleration: "gpu_software" }),
  )
  const args = [
    "--client=mcp",
    "--seed",
    "engine-contract-followup-1337",
    "--userdir",
    `${profile}/`,
    "--configdir",
    `${profile}/config/`,
  ]
  const paths = await new Deno.Command(binary, { args: [...args, "--paths"], cwd }).output()
  const pathsText = new TextDecoder().decode(paths.stdout)
  await Deno.writeTextFile(`${output}/paths.txt`, pathsText)
  assert(paths.success)
  assert(
    pathsText.split("\n").some((line) =>
      line.includes("Config Directory") && line.includes(`${profile}/config`)
    ),
  )
  const identities = await new Deno.Command("sha256sum", {
    args: [
      binary,
      "src/mcp_server.cpp",
      "src/engine_client_session.cpp",
      "tools/client/engine_contract_smoke.ts",
    ],
    cwd,
  }).output()
  assert(identities.success)
  await Deno.writeFile(`${output}/identities.sha256`, identities.stdout)
  await Deno.writeTextFile(
    `${output}/invocation.json`,
    JSON.stringify(
      { binary, cwd, args, loadedWorld, transportCase, nativeShutdown, legacyBatch },
      null,
      2,
    ),
  )
  const stderr = await Deno.open(`${output}/stderr.log`, { createNew: true, write: true })
  const transcript = await Deno.open(`${output}/stdio.jsonl`, { createNew: true, write: true })
  const process = new Deno.Command(binary, {
    args,
    cwd,
    stdin: "piped",
    stdout: "piped",
    stderr: "piped",
  }).spawn()
  const stderrDone = process.stderr.pipeTo(stderr.writable)
  const writer = process.stdin.getWriter()
  const reader = process.stdout.pipeThrough(new TextDecoderStream()).getReader()
  const encoder = new TextEncoder()
  let buffered = ""
  let nextId = 0
  let closed = false
  const evidence: Record<string, unknown> = {}
  const withDeadline = async <T>(operation: Promise<T>, label: string): Promise<T> => {
    let timer: ReturnType<typeof setTimeout> | undefined
    try {
      return await Promise.race([
        operation,
        new Promise<never>((_, reject) => {
          timer = setTimeout(() => reject(new Error(`timeout waiting for ${label}`)), 120000)
        }),
      ])
    } finally {
      clearTimeout(timer)
    }
  }
  const send = async (request: unknown) => {
    await transcript.write(encoder.encode(JSON.stringify({ request }) + "\n"))
    await writer.write(encoder.encode(JSON.stringify(request) + "\n"))
  }
  const read = async () => {
    while (!buffered.includes("\n")) {
      const part = await reader.read()
      assert(!part.done, "real engine closed before responding; no completion may be fabricated")
      buffered += part.value
      assert(buffered.length <= 1048576, "response exceeds transport bound")
    }
    const end = buffered.indexOf("\n")
    const line = buffered.slice(0, end)
    buffered = buffered.slice(end + 1)
    const response: unknown = JSON.parse(line)
    await transcript.write(encoder.encode(JSON.stringify({ response }) + "\n"))
    return response
  }
  const exchange = async (method: string, params: unknown) => {
    const id = `smoke:${++nextId}`
    await send({ jsonrpc: "2.0", id, method, params })
    const response = v.parse(Envelope, await withDeadline(read(), method))
    assert(response.id === id, "unexpected response, including possible notification output")
    return response
  }
  const rpc = async (method: string, params: unknown) => {
    const response = await exchange(method, params)
    assert(response.error === undefined, `real engine error: ${JSON.stringify(response.error)}`)
    return response.result
  }
  const reject = async (method: string, params: unknown, kind: string) => {
    const response = await exchange(method, params)
    assert(response.result === undefined)
    const error = v.parse(ApplicationError, response.error)
    assert(error.data.kind === kind, `${method}: ${error.data.kind}, expected ${kind}`)
    return error
  }
  const shutdown = async () => {
    await writer.close()
    const status = await withDeadline(process.status, "EOF shutdown")
    closed = true
    assert(
      status.code === 0 && status.signal === null,
      `EOF shutdown failed: ${JSON.stringify(status)}`,
    )
    const remaining = await withDeadline(reader.read(), "stdout EOF")
    assert(remaining.done && buffered.length === 0, "unexpected response on shutdown")
    evidence.shutdown = status
  }
  try {
    if (legacyBatch) {
      // Deliberate historical-client compatibility probe, not a direct-protocol fallback.
      await rpc("initialize", { protocolVersion: "2025-11-25" })
      await send({ jsonrpc: "2.0", method: "notifications/initialized" })
      const Actions = v.looseObject({ structuredContent: v.looseObject({ input_id: v.number() }) })
      const before = v.parse(
        Actions,
        await rpc("tools/call", { name: "bn.actions", arguments: {} }),
      )
      await send([
        {
          jsonrpc: "2.0",
          id: "legacy:first",
          method: "tools/call",
          params: { name: "bn.press", arguments: { keys: [{ action: "DOWN" }] } },
        },
        {
          jsonrpc: "2.0",
          id: "legacy:second",
          method: "tools/call",
          params: { name: "bn.press", arguments: { keys: [{ action: "DOWN" }] } },
        },
      ])
      const responses = v.parse(
        v.array(Envelope),
        await withDeadline(read(), "retained legacy batch"),
      )
      assert(
        responses.length === 2 && responses[0].id === "legacy:first" &&
          responses[1].id === "legacy:second",
      )
      assert(
        responses.every((response) =>
          response.error === undefined && response.result !== undefined
        ),
      )
      const after = v.parse(Actions, await rpc("tools/call", { name: "bn.actions", arguments: {} }))
      assert(
        after.structuredContent.input_id === before.structuredContent.input_id + 2,
        "legacy batch did not cross two distinct native boundaries",
      )
      const negotiated = v.parse(Negotiated, await rpc("bn.contract.negotiate", negotiation()))
      const snapshot = v.parse(
        Snapshot,
        await rpc("bn.snapshot.get", {
          session_epoch: negotiated.session_epoch,
          page: { offset: 0, limit: 100 },
        }),
      )
      assert(snapshot.state.interaction?.context === "MAIN_MENU")
      const isolated = await reject("tools/call", {
        name: "bn.press",
        arguments: { keys: [{ action: "DOWN" }] },
      }, "invalid_lifecycle")
      evidence.legacy_batch = { before, responses, after, snapshot, isolated }
      await shutdown()
      await Deno.writeTextFile(`${output}/result.json`, JSON.stringify(evidence, null, 2))
      console.log(
        "PASS real legacy retained-cursor batch: two native boundaries, one complete response frame; direct negotiation then prevents legacy mutation",
      )
      return
    }
    await send([
      { jsonrpc: "2.0", method: "bn.contract.negotiate", params: negotiation() },
      { jsonrpc: "2.0", method: "bn.command.submit", params: {} },
      { jsonrpc: "2.0", method: "bn.snapshot.get", params: {} },
      { jsonrpc: "2.0", method: "bn.command.result", params: {} },
    ])
    evidence.notification_negotiation = await reject("bn.snapshot.get", {}, "negotiation_failed")
    evidence.unsupported_negotiation = await reject(
      "bn.contract.negotiate",
      negotiation(["unsupported.real.capability"]),
      "unsupported_capability",
    )
    let negotiated = v.parse(Negotiated, await rpc("bn.contract.negotiate", negotiation()))
    const snapshot = async (page = { offset: 0, limit: 100 }) =>
      v.parse(
        Snapshot,
        await rpc("bn.snapshot.get", { session_epoch: negotiated.session_epoch, page }),
      )
    const command = (
      state: SnapshotValue,
      operation: unknown,
      patch: Record<string, unknown> = {},
    ) => {
      const registered = v.is(
        v.looseObject({ kind: v.literal("invoke_registered_action") }),
        operation,
      )
      const epoch = v.parse(epoch_patch, patch).session_epoch ?? state.session_epoch
      const { session_epoch: _epoch, ...basedOn } = patch
      return {
        session_epoch: epoch,
        based_on: {
          state_revision: state.state_revision,
          input_boundary_id: state.state.input_boundary_id,
          ...(registered ? {} : { interaction_schema_id: state.state.interaction?.schema_id }),
          ...basedOn,
        },
        operation,
      }
    }
    const submit = async (
      state: SnapshotValue,
      operation: unknown,
      patch: Record<string, unknown> = {},
    ) => {
      const receipt = v.parse(
        Receipt,
        await rpc("bn.command.submit", command(state, operation, patch)),
      )
      const result = v.parse(
        Result,
        await rpc("bn.command.result", {
          session_epoch: receipt.session_epoch,
          command_id: receipt.command_id,
        }),
      )
      return { receipt, result: result.command }
    }
    const complete = (result: v.InferOutput<typeof Result>["command"]) => {
      assert(result.stage === "completed", `expected completion: ${JSON.stringify(result)}`)
      assert(result.execution.status === "native_input_delivered")
      assert(result.completion?.status === "next_interaction_boundary")
    }
    const choose = async (state: SnapshotValue, label: string) => {
      const choice = state.state.interaction?.choices.find((entry) =>
        entry.enabled && entry.label === label
      )
      assert(choice, `no native choice ${label}: ${JSON.stringify(state.state.interaction)}`)
      return await submit(state, { kind: "choose", choice_id: choice.id })
    }
    const sameClock = (left: SnapshotValue, right: SnapshotValue) => {
      assert(
        left.session_epoch === right.session_epoch &&
          left.state_revision === right.state_revision &&
          left.through_public_sequence === right.through_public_sequence &&
          left.state.input_boundary_id === right.state.input_boundary_id,
        "passive/negative request mutated authority",
      )
    }
    let state = await snapshot()
    for (
      let startup = 0;
      state.state.interaction?.context !== "MAIN_MENU" && startup < 5;
      startup++
    ) {
      const choices = state.state.interaction?.choices.filter((choice) => choice.enabled)
      assert(
        choices?.length === 1 && !state.state.interaction?.message.includes("DEBUG"),
        "unexpected startup prompt",
      )
      complete((await submit(state, { kind: "choose", choice_id: choices[0].id })).result)
      state = await snapshot()
    }
    assert(state.state.interaction?.context === "MAIN_MENU")
    if (transportCase !== undefined) {
      assert(state.state.actions.some((entry) => entry.id === "DOWN"))
      const pending = {
        jsonrpc: "2.0",
        id: "transport:receipt",
        method: "bn.command.submit",
        params: command(state, { kind: "invoke_registered_action", action_id: "DOWN" }),
      }
      if (transportCase === "partial-eof") {
        // No newline: even a syntactically complete request must never dispatch at EOF.
        await transcript.write(encoder.encode(JSON.stringify({ partial_request: pending }) + "\n"))
        await writer.write(encoder.encode(JSON.stringify(pending)))
        await writer.close()
      } else if (transportCase === "batch-output-limit") {
        // Native delivery is pending until the ENTIRE bounded response frame succeeds.
        // The incoming batch is well below 1 MiB; its snapshots exceed the outgoing ceiling.
        await send([
          pending,
          ...Array.from({ length: 300 }, (_, index) => ({
            jsonrpc: "2.0",
            id: `transport:page:${index}`,
            method: "bn.snapshot.get",
            params: { session_epoch: state.session_epoch, page: { offset: 0, limit: 100 } },
          })),
        ])
      } else {
        // Physically close the read end before asking for a receipt; no synthetic ostream.
        await reader.cancel("physical stdout consumer loss")
        await send(pending)
      }
      const status = await withDeadline(process.status, transportCase)
      closed = true
      assert(
        !status.success,
        `transport failure became successful process completion: ${JSON.stringify(status)}`,
      )
      if (transportCase !== "output-loss") {
        assert(status.code === 1 && status.signal === null)
        const remaining = await withDeadline(reader.read(), "failed transport stdout EOF")
        assert(
          remaining.done && buffered.length === 0,
          "partial/error batch exposed an outgoing prefix",
        )
      }
      await stderrDone
      const diagnostics = await Deno.readTextFile(`${output}/stderr.log`)
      if (transportCase === "partial-eof") assert(diagnostics.includes("unterminated stdio frame"))
      if (transportCase === "batch-output-limit") {
        assert(diagnostics.includes("protocol output failed"))
      }
      evidence.transport = { case: transportCase, status, no_retry_sent: true }
      await Deno.writeTextFile(`${output}/result.json`, JSON.stringify(evidence, null, 2))
      console.log(
        `PASS real ${transportCase}: no successful completion or retry${
          transportCase === "output-loss"
            ? "; consumer pipe physically closed"
            : "; no outgoing prefix"
        }`,
      )
      return
    }
    evidence.stale_snapshot_epoch = await reject("bn.snapshot.get", {
      session_epoch: "stale:epoch",
      page: { offset: 0, limit: 100 },
    }, "stale_epoch")
    const stale: Record<string, unknown> = {}
    for (
      const [kind, patch] of [
        ["stale_epoch", { session_epoch: "stale:epoch" }],
        ["stale_revision", { state_revision: (BigInt(state.state_revision) + 1n).toString() }],
        ["stale_boundary", { input_boundary_id: "stale:boundary" }],
        ["stale_interaction_schema", { interaction_schema_id: "stale:schema" }],
      ] as const
    ) {
      const rejected = await submit(state, { kind: "choose", choice_id: "nonexistent" }, patch)
      assert(rejected.result.stage === "rejected" && rejected.result.validation.error === kind)
      assert(rejected.result.execution.status === "pending" && rejected.result.completion === null)
      sameClock(state, await snapshot())
      stale[kind] = rejected
    }
    evidence.stale = stale
    sameClock(state, await snapshot({ offset: 1, limit: 1 }))
    sameClock(state, await snapshot())
    await send([
      { jsonrpc: "2.0", method: "bn.contract.negotiate", params: negotiation([]) },
      { jsonrpc: "2.0", method: "bn.command.submit", params: command(state, { kind: "cancel" }) },
      {
        jsonrpc: "2.0",
        method: "bn.snapshot.get",
        params: { session_epoch: state.session_epoch, page: { offset: 1, limit: 1 } },
      },
      {
        jsonrpc: "2.0",
        method: "bn.command.result",
        params: { session_epoch: state.session_epoch, command_id: "unknown" },
      },
    ])
    sameClock(state, await snapshot())
    // A syntax-valid command prefix followed by garbage cannot execute any input.
    const broken = JSON.stringify({
      jsonrpc: "2.0",
      id: "malformed:prefix",
      method: "bn.command.submit",
      params: command(state, { kind: "invoke_registered_action", action_id: "DOWN" }),
    }) + " trailing garbage"
    await transcript.write(encoder.encode(JSON.stringify({ raw_request: broken }) + "\n"))
    await writer.write(encoder.encode(broken + "\n"))
    const parseFailure = v.parse(
      v.looseObject({
        jsonrpc: v.literal("2.0"),
        id: v.null(),
        error: v.looseObject({ code: v.literal(-32700) }),
      }),
      await withDeadline(read(), "complete malformed frame"),
    )
    sameClock(state, await snapshot())
    evidence.complete_frame = parseFailure
    // The receipt is not execution, and a later batch submit cannot enter the widget.
    // Explicit legacy initialization occurs ONLY as a negative isolation probe, never
    // as fallback: strict direct negotiation and native direct commands already succeeded.
    await rpc("initialize", { protocolVersion: "2025-11-25" })
    await send({ jsonrpc: "2.0", method: "notifications/initialized" })
    evidence.legacy_press_isolation = await reject("tools/call", {
      name: "bn.press",
      arguments: { keys: [{ key: "a" }] },
    }, "invalid_lifecycle")
    evidence.legacy_interact_isolation = await reject("tools/call", {
      name: "bn.interact",
      arguments: { input_id: 0, operation: "cancel" },
    }, "invalid_lifecycle")
    sameClock(state, await snapshot())
    const batchBase = state
    const batchParams = command(state, { kind: "invoke_registered_action", action_id: "DOWN" })
    await send([
      { jsonrpc: "2.0", id: "batch:first", method: "bn.command.submit", params: batchParams },
      { jsonrpc: "2.0", id: "batch:second", method: "bn.command.submit", params: batchParams },
    ])
    const batchResponses = v.parse(
      v.array(Envelope),
      await withDeadline(read(), "two-command batch"),
    )
    assert(
      batchResponses.length === 2 && batchResponses[0].id === "batch:first" &&
        batchResponses[1].id === "batch:second",
    )
    const batchReceipt = v.parse(Receipt, batchResponses[0].result)
    assert(v.parse(ApplicationError, batchResponses[1].error).data.kind === "command_busy")
    const batchResult = v.parse(
      Result,
      await rpc("bn.command.result", {
        session_epoch: batchReceipt.session_epoch,
        command_id: batchReceipt.command_id,
      }),
    )
    complete(batchResult.command)
    state = await snapshot()
    assert(BigInt(state.state_revision) === BigInt(batchBase.state_revision) + 1n)
    assert(BigInt(state.through_public_sequence) === BigInt(batchBase.through_public_sequence) + 1n)
    evidence.batch = { responses: batchResponses, result: batchResult }
    // Snapshot-only negotiation must refuse a semantic command before allocating a receipt.
    negotiated = v.parse(
      Negotiated,
      await rpc("bn.contract.negotiate", negotiation(capabilities.slice(0, 3))),
    )
    evidence.unsupported_command = await reject(
      "bn.command.submit",
      command(state, { kind: "cancel" }),
      "unsupported_capability",
    )
    evidence.unsupported_result = await reject("bn.command.result", {
      session_epoch: state.session_epoch,
      command_id: "unknown",
    }, "unsupported_capability")
    sameClock(state, await snapshot())
    negotiated = v.parse(Negotiated, await rpc("bn.contract.negotiate", negotiation()))
    sameClock(state, await snapshot())
    const help = await choose(state, "Help")
    complete(help.result)
    let nested = await snapshot()
    const movement = nested.state.interaction?.choices.find((entry) =>
      entry.enabled && entry.label.endsWith("Movement")
    )
    assert(movement, "no real Movement help topic")
    const readerEntry = await submit(nested, { kind: "choose", choice_id: movement.id })
    complete(readerEntry.result)
    const topic = await snapshot()
    assert(topic.state.interaction?.context === "SCROLLABLE_TEXT")
    const paging = await submit(topic, { kind: "invoke_registered_action", action_id: "PAGE_DOWN" })
    complete(paging.result)
    const paged = await snapshot()
    const returnHelp = await submit(paged, { kind: "cancel" })
    complete(returnHelp.result)
    nested = await snapshot()
    const cancelled = await submit(nested, { kind: "cancel" })
    complete(cancelled.result)
    state = await snapshot()
    assert(state.state.interaction?.context === "MAIN_MENU")
    const quitPrompt = await choose(state, "Quit")
    complete(quitPrompt.result)
    const confirmation = await snapshot()
    assert(confirmation.state.interaction?.message.includes("Really quit?"))
    const no = confirmation.state.interaction?.choices.find((entry) =>
      entry.enabled && entry.description === "NO"
    )
    assert(no, "no native NO choice")
    const declined = await submit(confirmation, { kind: "choose", choice_id: no.id })
    complete(declined.result)
    state = await snapshot()
    assert(state.state.interaction?.context === "MAIN_MENU")
    evidence.nested = { help, readerEntry, paging, returnHelp, cancelled, quitPrompt, declined }
    if (loadedWorld) {
      const newGame = await choose(state, "New Game")
      complete(newGame.result)
      state = await snapshot()
      // This receipt crosses the actual menu -> loaded tutorial-world authority transition.
      const tutorial = await choose(state, "Tutorial")
      assert(
        tutorial.result.stage === "interrupted",
        `world transition must not fabricate completion: ${JSON.stringify(tutorial)}`,
      )
      assert(
        tutorial.result.execution.status === "native_input_delivered" &&
          tutorial.result.completion === null,
      )
      const oldEpoch = negotiated.session_epoch
      negotiated = v.parse(Negotiated, await rpc("bn.contract.negotiate", negotiation()))
      assert(negotiated.session_epoch !== oldEpoch, "loaded-world epoch was not replaced")
      state = await snapshot()
      assert(state.state.readiness.game_ready, "tutorial world was not actually loaded")
      evidence.world_entry = { newGame, tutorial, recovered: state }
      const lessons: unknown[] = []
      for (
        let lesson = 0;
        state.state.interaction !== null && state.state.interaction.context !== "DEFAULTMODE" &&
        lesson < 12;
        lesson++
      ) {
        const choices = state.state.interaction?.choices.filter((entry) => entry.enabled)
        assert(
          choices?.length === 1 && !state.state.interaction?.message.includes("DEBUG"),
          "unexpected loaded-world prompt",
        )
        const acknowledged = await submit(state, { kind: "choose", choice_id: choices[0].id })
        complete(acknowledged.result)
        lessons.push(acknowledged)
        state = await snapshot()
      }
      assert(state.state.readiness.game_ready)
      assert(
        state.state.interaction === null || state.state.interaction.context === "DEFAULTMODE",
        "tutorial prompts did not finish",
      )
      assert(
        state.state.actions.some((entry) => entry.id === "save"),
        "loaded authority does not advertise save",
      )
      const savePrompt = await submit(state, {
        kind: "invoke_registered_action",
        action_id: "save",
      })
      complete(savePrompt.result)
      state = await snapshot()
      // Tutorial save warning is a real acknowledgement, not a fake confirmation choice.
      if (!state.state.interaction?.message.includes("Save and quit?")) {
        const choices = state.state.interaction?.choices.filter((entry) => entry.enabled)
        assert(choices?.length === 1, "unexpected save warning")
        complete((await submit(state, { kind: "choose", choice_id: choices[0].id })).result)
        state = await snapshot()
      }
      assert(state.state.interaction?.message.includes("Save and quit?"))
      const yes = state.state.interaction?.choices.find((entry) =>
        entry.enabled && entry.description === "YES"
      )
      assert(yes, "no native save confirmation")
      const leaveWorld = await submit(state, { kind: "choose", choice_id: yes.id })
      assert(leaveWorld.result.stage === "interrupted" && leaveWorld.result.completion === null)
      assert(leaveWorld.result.execution.status === "native_input_delivered")
      const loadedEpoch = negotiated.session_epoch
      negotiated = v.parse(Negotiated, await rpc("bn.contract.negotiate", negotiation()))
      assert(negotiated.session_epoch !== loadedEpoch)
      state = await snapshot()
      assert(!state.state.readiness.game_ready && state.state.interaction?.context === "MAIN_MENU")
      evidence.loaded_world = {
        newGame,
        tutorial,
        lessons,
        savePrompt,
        leaveWorld,
        recovered: state,
      }
    }
    if (nativeShutdown) {
      const prompt = await choose(state, "Quit")
      complete(prompt.result)
      const confirmation = await snapshot()
      const yes = confirmation.state.interaction?.choices.find((entry) =>
        entry.enabled && entry.description === "YES"
      )
      assert(yes, "no native application quit confirmation")
      const receipt = v.parse(
        Receipt,
        await rpc(
          "bn.command.submit",
          command(confirmation, { kind: "choose", choice_id: yes.id }),
        ),
      )
      const end = await withDeadline(reader.read(), "native shutdown stdout EOF")
      assert(end.done && buffered.length === 0, "native shutdown emitted an unsolicited completion")
      const status = await withDeadline(process.status, "native application shutdown")
      closed = true
      // main.cpp preserves the historical non-replay exit_handler(-999) sentinel.
      // On this POSIX validation target it is status 25, not an IPC completion result.
      assert(status.code === 25 && status.signal === null)
      try {
        await writer.close()
      } catch { /* The native application already closed input. */ }
      evidence.shutdown = {
        status,
        receipt,
        command_result_received: false,
        completion: "unavailable_on_closed_transport",
      }
    } else {
      await shutdown()
    }
    await Deno.writeTextFile(`${output}/result.json`, JSON.stringify(evidence, null, 2))
    console.log(
      `PASS real stale epoch/revision/boundary/schema, capabilities, notifications, projection clocks, nested reader/confirmation, ${
        nativeShutdown
          ? "native receipt followed by process shutdown (not command completion)"
          : "EOF shutdown"
      }${loadedWorld ? "; loaded-world outstanding receipts interrupted and recovered" : ""}`,
    )
  } finally {
    if (!closed) {
      try {
        await writer.close()
      } catch { /* Transport may already be closed. */ }
      try {
        process.kill("SIGTERM")
      } catch { /* Already exited. */ }
    }
    await process.status
    await stderrDone
    await Deno.writeTextFile(`${output}/partial-evidence.json`, JSON.stringify(evidence, null, 2))
    transcript.close()
    reader.releaseLock()
  }
}

if (import.meta.main) {
  await new Command().name("engine-contract-smoke")
    .description(
      "Real direct-protocol matrix with a new private profile and retained stdio evidence",
    )
    .option(
      "--loaded-world",
      "Also create a real tutorial world, cross both authority epochs, and save/quit",
    )
    .option(
      "--transport-case <case:string>",
      "Physical partial-eof, batch-output-limit, or output-loss probe instead of the gameplay matrix",
    )
    .option(
      "--native-shutdown",
      "End through real Quit/YES; retain receipt and EOF without claiming command completion",
    )
    .option(
      "--legacy-batch",
      "Separate legacy-only deferred batch compatibility probe, followed by direct isolation",
    )
    .arguments("<binary:string> <new-output-directory:string>")
    .action(async (options, binary, output) => {
      await smoke(binary, output, {
        loadedWorld: options.loadedWorld ?? false,
        transportCase: options.transportCase,
        nativeShutdown: options.nativeShutdown ?? false,
        legacyBatch: options.legacyBatch ?? false,
      })
    })
    .parse(Deno.args)
}

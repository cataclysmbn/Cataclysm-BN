/**
 * @module
 * Drives a real game process over stdio: hello, subscribe, a semantic choose and a movement
 * action, with the pushed stages and events reconstructing the same state as a fresh subscribe.
 *
 * BN_BINARY=out/build/linux-full/cataclysm-bn-tiles deno test --allow-read --allow-write \
 *   --allow-run --allow-env tests/protocol/stdio_test.ts
 */
import { assert, assertEquals, assertNotEquals } from "@std/assert"
import { Ajv2020 } from "npm:ajv@8.17.1/dist/2020.js"
import { Mirror } from "./mirror.ts"

// deno-lint-ignore no-explicit-any
type Value = any
const schemaFile = new URL("../../docs/schema/engine-client/0.2.schema.json", import.meta.url)
// Every message the engine really sends is checked against the schema.
const schema: Value = JSON.parse(await Deno.readTextFile(schemaFile))
const ajv = new Ajv2020({ strict: false })
ajv.addSchema(schema, "bn")
const validate = (def: string, value: unknown, what: string) => {
  const check = ajv.getSchema(`bn#/$defs/${def}`)
  assert(check, def)
  assert(
    check(value),
    `${what} violates ${def}: ${JSON.stringify(check.errors)}\n${JSON.stringify(value)}`,
  )
}
/** What a hello names: the first 16 hex digits of the SHA-256 of the raw bytes of the schema. */
const schemaHash = "sha256:" + Array.from(
  new Uint8Array(await crypto.subtle.digest("SHA-256", await Deno.readFile(schemaFile))),
  (byte) => byte.toString(16).padStart(2, "0"),
).join("").slice(0, 16)

/** The engine is killed after this long without protocol traffic, busy or not: loading steps and activities send heartbeats. */
const stall = 120_000

class Client {
  #process: Deno.ChildProcess
  #writer: WritableStreamDefaultWriter<Uint8Array>
  #reader: ReadableStreamDefaultReader<Uint8Array>
  #decoder = new TextDecoder()
  #buffer = ""
  #id = 0
  /** Notifications that arrived while waiting for something else, in order. */
  notes: Value[] = []
  /** Responses to raw frames written with `raw`, which no `request` waits for. */
  replies: Value[] = []
  /** Pause after every chunk read, to drain the pipe slowly. */
  readDelayMs = 0

  constructor(
    binary: string,
    profile: string,
    stderr: "null" | "piped" = "null",
    extraArgs: string[] = [],
  ) {
    this.#process = new Deno.Command(binary, {
      args: [
        ...extraArgs,
        "--client=mcp",
        "--seed",
        "protocol-stdio-1",
        "--userdir",
        `${profile}/`,
        "--configdir",
        `${profile}/config/`,
      ],
      stdin: "piped",
      stdout: "piped",
      stderr,
    }).spawn()
    this.#writer = this.#process.stdin.getWriter()
    this.#reader = this.#process.stdout.getReader()
  }

  async #line(): Promise<Value> {
    let timer: ReturnType<typeof setTimeout> | undefined
    try {
      while (!this.#buffer.includes("\n")) {
        clearTimeout(timer)
        timer = setTimeout(() => this.#process.kill("SIGKILL"), stall)
        const part = await this.#reader.read()
        assert(!part.done, "engine closed stdout")
        this.#buffer += this.#decoder.decode(part.value, { stream: true })
        if (this.readDelayMs) await new Promise((resolve) => setTimeout(resolve, this.readDelayMs))
      }
    } finally {
      clearTimeout(timer)
    }
    const end = this.#buffer.indexOf("\n")
    const line = this.#buffer.slice(0, end)
    this.#buffer = this.#buffer.slice(end + 1)
    return { ...JSON.parse(line), receivedAt: performance.now() }
  }

  /** Next frame that is not the response to a raw frame. */
  async #next(): Promise<Value> {
    while (true) {
      const message = await this.#line()
      if (message.method !== undefined || !this.rawIds.has(JSON.stringify(message.id))) {
        return message
      }
      this.replies.push(message)
    }
  }

  /** The ids written with `raw`; a malformed frame is answered with id null. */
  rawIds = new Set<string>()

  /**
   * Queues `text` as it is, one frame per line, and expects a response with each id in `ids`.
   * It does not wait for the engine to take the bytes: a pipe holds little, and an engine blocked on
   * its own output reads nothing until the client does.
   */
  raw(text: string, ids: unknown[] = []) {
    for (const id of ids) this.rawIds.add(JSON.stringify(id))
    this.#queue(text)
  }

  #queue(text: string) {
    this.#writer.write(new TextEncoder().encode(text)).catch(() => {})
  }

  /** Reads until `count` responses to raw frames have arrived; notifications on the way wait in `notes`. */
  async repliesUntil(count: number) {
    while (this.replies.length < count) {
      // Not `#next`, which keeps reading past a reply until a notification comes: none may follow.
      const message = await this.#line()
      if (message.method === undefined && this.rawIds.has(JSON.stringify(message.id))) {
        this.replies.push(message)
        continue
      }
      this.#validateNotification(message)
      this.notes.push(message)
    }
  }

  #validateNotification(message: Value) {
    const def = schema["x-notifications"][message.method]
    validate(def, message.params, message.method)
  }

  /** Drops what was queued for the engine and closes its stdin, as a client that died; a write in flight ends when the engine reads or exits. */
  closeInput() {
    this.#writer.abort().catch(() => {})
  }

  signal(sig: Deno.Signal) {
    this.#process.kill(sig)
  }

  /** Waits for the engine to leave; `stderr` is its text when piped. */
  async exit(ms: number) {
    const stderr = this.#process.stderr
    return await withDeadline(
      Promise.all([this.#process.status, stderr ? new Response(stderr).text() : ""]),
      ms,
      "the engine outlived its client",
    ).then(([status, text]) => ({ status, stderr: text }))
  }

  /** Reads and drops what the engine writes until it closes stdout, as a client that still reads while it sends a bad frame. */
  async drain() {
    for (let part = await this.#reader.read(); !part.done; part = await this.#reader.read());
  }

  /** Closes the engine's stdout for good, as a client that died. */
  async abandonOutput() {
    await this.#reader.cancel()
  }

  async request(method: string, params: unknown): Promise<Value> {
    const id = ++this.#id
    this.#queue(JSON.stringify({ jsonrpc: "2.0", id, method, params }) + "\n")
    while (true) {
      const message = await this.#next()
      if (message.id === id) {
        if (message.error) {
          validate("application_error", message.error.data, `${method} error`)
        } else {
          const def = schema["x-methods"][method].result
          validate(def, message.result, `${method} result`)
        }
        return message
      }
      assert(message.id === undefined, `unexpected response ${JSON.stringify(message)}`)
      this.#validateNotification(message)
      this.notes.push(message)
    }
  }

  async result(method: string, params: unknown): Promise<Value> {
    const response = await this.request(method, params)
    assert(response.error === undefined, JSON.stringify(response.error))
    return response.result
  }

  /** Next notification, from the backlog first. */
  async note(): Promise<Value> {
    const queued = this.notes.shift()
    if (queued) return queued
    const message = await this.#next()
    this.#validateNotification(message)
    return message
  }

  async close() {
    await this.#writer.close()
    const status = await this.#process.status
    assert(status.success, `engine exit ${JSON.stringify(status)}`)
  }

  kill() {
    try {
      this.#process.kill("SIGKILL")
    } catch { /* already gone */ }
  }
}

/** The event types that carry state or the message log; every other type tells a game event or an animation. */
const stateEvents = [
  "interaction.changed",
  "coverage.moved",
  "turn.passed",
  "cells.seen",
  "message.logged",
]

class Session {
  mirror!: Mirror
  /** Stages seen per command id. */
  stages = new Map<string, string[]>()
  /** The error of each command that ended in one. */
  errors = new Map<string, string>()
  eventTypes: string[] = []
  /** The events that tell what happened rather than what the avatar knows, whole and in arrival order. */
  told: Value[] = []
  resyncs: Value[] = []
  /** Pushed frames the client may be sent before it pays; 0 for a client that asked for no credits. */
  window = 0
  #unpaid = 0
  /** `bn.events` frames with the time they arrived and the types they carried, in arrival order. */
  frames: { at: number; types: string[]; turns: string[] }[] = []
  /** `bn.loading` notifications with their arrival time, in arrival order. */
  loading: Value[] = []
  /** `bn.progress` heartbeats seen. */
  progress = 0

  constructor(readonly client: Client) {}

  async subscribe(): Promise<Mirror> {
    return await Session.parts(this.client, await this.client.result("bn.subscribe", {}))
  }

  async start() {
    this.mirror = await this.subscribe()
  }

  /** Subscribes again after a resync while pushes keep arriving; the parts follow the answer, and what came before it is handled after the new snapshot. */
  async resubscribe() {
    const header = await this.client.result("bn.subscribe", {})
    const early = this.client.notes.splice(0)
    this.mirror = await Session.parts(this.client, header)
    for (const note of early) {
      // A resync stops the events until the snapshot; the command stages go on.
      assertNotEquals(note.method, "bn.events", "an event between a resync and its snapshot")
      this.handle(note)
    }
  }

  static async parts(client: Client, header: Value): Promise<Mirror> {
    const parts: Value[] = []
    while (parts.length < header.parts) {
      const note = await client.note()
      assertEquals(note.method, "bn.snapshot.part")
      parts.push(note.params)
    }
    return Mirror.fromSnapshot(header, parts)
  }

  /** Grant credits back once half the window has been read; one request per many frames. */
  async pay() {
    if (this.window === 0 || this.#unpaid < Math.max(1, this.window / 2)) return
    const frames = this.#unpaid
    this.#unpaid = 0
    await this.client.result("bn.credit", { frames })
  }

  handle(note: Value) {
    const { method, params } = note
    if (method === "bn.events" || method === "bn.command") this.#unpaid++
    if (method === "bn.events") {
      this.mirror.applyEvents(params.epoch, params.events)
      this.frames.push({
        at: note.receivedAt,
        types: params.events.map((e: Value) => e.type),
        turns: params.events.flatMap((e: Value) => e.changes?.environment?.turn ?? []),
      })
      for (const event of params.events) {
        this.eventTypes.push(event.type)
        if (!stateEvents.includes(event.type)) this.told.push(event)
      }
    } else if (method === "bn.command") {
      const stages = this.stages.get(params.command_id) ?? ["received"]
      stages.push(params.stage)
      this.stages.set(params.command_id, stages)
      if (params.error) this.errors.set(params.command_id, params.error)
    } else if (method === "bn.loading") {
      this.loading.push({ ...params, receivedAt: note.receivedAt })
    } else if (method === "bn.resync") {
      this.resyncs.push(params)
    } else if (method === "bn.progress") {
      this.progress++
    } else {
      throw new Error(`unexpected notification ${method}`)
    }
  }

  /**
   * Submit against what the mirror believes, then follow pushes until the command is terminal.
   * `afterHeartbeat` runs once, when the first `bn.progress` heartbeat of the command arrives.
   */
  async submit(
    operation: Value,
    patch: Value = {},
    afterHeartbeat?: () => Promise<void>,
  ) {
    const mirror = this.mirror
    const response = await this.client.request("bn.command.submit", {
      epoch: mirror.at.epoch,
      expect: {
        revision: mirror.at.revision,
        boundary_id: mirror.interaction.boundary_id,
        schema_id: mirror.interaction.interaction?.schema_id ?? null,
        ...patch,
      },
      operation,
    })
    if (response.error) return { error: response.error, stages: [] as string[] }
    const id = response.result.command_id
    this.stages.set(id, ["received"])
    const heartbeats = this.progress
    while (true) {
      const stages = this.stages.get(id)!
      const last = stages.at(-1)
      if (["completed", "rejected", "interrupted"].includes(last!)) {
        return { id, stages, error: undefined }
      }
      this.handle(await this.client.note())
      await this.pay()
      if (afterHeartbeat && this.progress > heartbeats) {
        const run = afterHeartbeat
        afterHeartbeat = undefined
        await run()
      }
    }
  }

  /** Resize the terrain window and follow the pushes until the engine reports a different view. */
  async viewport(cols: number, rows: number) {
    const before = JSON.stringify(this.mirror.view)
    await this.client.result("bn.viewport", { cols, rows })
    while (JSON.stringify(this.mirror.view) === before) this.handle(await this.client.note())
  }

  async choose(label: string) {
    const choice = this.mirror.interaction.interaction.choices.find((entry: Value) =>
      entry.enabled && entry.label === label
    )
    assert(choice, `no choice ${label}`)
    return await this.submit({ kind: "choose", choice_id: choice.id })
  }

  /** Acknowledge single-choice prompts until `done` holds. */
  async acknowledge(done: () => boolean) {
    for (let step = 0; !done() && step < 20; step++) {
      const choices = this.mirror.interaction.interaction.choices.filter((entry: Value) =>
        entry.enabled
      )
      assertEquals(choices.length, 1, "unexpected prompt")
      assertEquals(
        (await this.submit({ kind: "choose", choice_id: choices[0].id })).stages.at(-1),
        "completed",
      )
    }
    assert(done())
  }
}

/** Loading progress is ordered, ends with `done`, and names images the client can find. */
const checkLoading = async (loading: Value[], chosenAt: number) => {
  assert(loading.length > 1, "loading notifications before the world snapshot")
  const done = loading.at(-1)
  assertEquals(done.done, true)
  const progress = loading.slice(0, -1)
  assert(progress.every((note) => note.done === undefined), "done only once, last")
  const contexts: { title: string; steps: number; entries: number; start: number }[] = []
  let previous: Value
  for (const note of progress) {
    assert(note.index >= 0 && note.index < note.entries.length, "index selects an entry")
    // A context restarts its list, so the same title can come again with the index back at 0.
    if (previous?.title !== note.title || note.index < previous.index) {
      contexts.push({ title: note.title, steps: 0, entries: 0, start: note.receivedAt })
    }
    const context = contexts.at(-1)!
    context.steps++
    context.entries = note.entries.length
    if (note.image) {
      assert(!note.image.path.startsWith("/"), "image path is relative to the base path")
      await Deno.stat(new URL(`../../${note.image.path}`, import.meta.url))
    }
    previous = note
  }
  assert(progress.some((note) => note.image), "a loading image is chosen")
  // Time spent in each loading context: from its first notification to the next context's.
  console.log(`loading total ${((done.receivedAt - chosenAt) / 1000).toFixed(1)} s`)
  const slowest = progress.slice(0, -1).map((note, i) => ({
    step: `${note.title}: ${note.entries[note.index]}`,
    seconds: (loading[i + 1].receivedAt - note.receivedAt) / 1000,
  })).sort((a, b) => b.seconds - a.seconds).slice(0, 5)
  for (const { step, seconds } of slowest) {
    console.log(`loading slowest ${step}: ${seconds.toFixed(2)} s`)
  }
  for (const [i, context] of contexts.entries()) {
    const end = contexts[i + 1]?.start ?? done.receivedAt
    console.log(
      `loading ${context.title}: ${context.steps} steps, ${context.entries} entries, ` +
        `${((end - context.start) / 1000).toFixed(2)} s`,
    )
  }
}

async function makeProfile(options: Record<string, string> = {}): Promise<string> {
  const profile = await Deno.makeTempDir({ prefix: "bn-protocol-" })
  await Deno.mkdir(`${profile}/config`, { recursive: true })
  await Deno.writeTextFile(
    `${profile}/config/options.json`,
    JSON.stringify(
      Object.entries({
        USE_LANG: "en_US",
        ANIMATIONS: "false",
        AUTOSAVE: "false",
        COMPUTE_ACCELERATION: "gpu_software",
        ...options,
      }).map(([name, value]) => ({ name, value })),
    ),
  )
  await Deno.writeTextFile(
    `${profile}/config/preload.json`,
    JSON.stringify({ compute_acceleration: "gpu_software" }),
  )
  return profile
}

/** Hello, then New Game and Tutorial; returns the session on the loaded tutorial world. */
async function enterTutorial(client: Client, extra: Value = {}): Promise<Session> {
  await client.result("bn.hello", {
    versions: ["0.2"],
    schema: schemaHash,
    client: { name: "stdio-test", version: "1" },
    ...extra,
  })
  const window = extra.delivery?.credits ?? 0
  let session = new Session(client)
  session.window = window
  await session.start()
  await session.acknowledge(() => session.mirror.interaction.interaction?.context === "MAIN_MENU")
  await session.choose("New Game")
  await session.choose("Tutorial")
  assertEquals((await client.note()).method, "bn.resync")
  session = new Session(client)
  session.window = window
  await session.start()
  await session.acknowledge(() => session.mirror.interaction.interaction === null)
  return session
}

Deno.test({
  name: "stdio: hello, subscribe, choose and move; stream equals a fresh subscribe",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      // Nothing works before hello.
      const early = await client.request("bn.subscribe", {})
      assertEquals(early.error.data.kind, "negotiation_failed")
      const hello = await client.result("bn.hello", {
        versions: ["0.0", "0.2"],
        schema: schemaHash,
        client: { name: "stdio-test", version: "1" },
        viewport: { cols: 40, rows: 25 },
      })
      assertEquals(hello.version, "0.2")
      assertEquals(hello.limits.frame_bytes, 1048576)

      const session = new Session(client)
      await session.start()
      const epoch = session.mirror.at.epoch
      assertEquals(epoch, hello.epoch)

      // Startup prompts, then a semantic choose into a loaded world.
      await session.acknowledge(() =>
        session.mirror.interaction.interaction?.context === "MAIN_MENU"
      )
      // The main menu is tabbed: tab rows have no pane_id, entries belong to the selected tab only.
      const menu = () => {
        const choices = session.mirror.interaction.interaction.choices as Value[]
        const tabs = choices.filter((entry) => entry.pane_id === undefined)
        return {
          tabs: tabs.map((entry) => entry.id),
          selected: tabs.filter((entry) => entry.selected).map((entry) => entry.id),
          entries: choices.filter((entry) => entry.pane_id !== undefined),
        }
      }
      const allTabs = [
        "tab:motd",
        "tab:new_game",
        "tab:load",
        "tab:world",
        "tab:settings",
        "tab:help",
        "tab:credits",
        "tab:quit",
      ]
      assertEquals(menu().tabs, allTabs)
      // The text tabs publish their text as the interaction message.
      for (const [label, tab] of [["MOTD", "tab:motd"], ["Credits", "tab:credits"]]) {
        assertEquals((await session.choose(label)).stages.at(-1), "completed")
        assertEquals(menu().selected, [tab])
        assert(session.mirror.interaction.interaction.message.length > 20, label)
        assert(!session.mirror.interaction.interaction.message.includes("<color"), label)
      }
      const settings = await session.choose("Settings")
      assertEquals(settings.stages, ["received", "validated", "executing", "completed"])
      assertEquals(menu().tabs, allTabs)
      assertEquals(menu().selected, ["tab:settings"])
      assertEquals(menu().entries.map((entry) => entry.id), [
        "settings:options",
        "settings:keybindings",
        "settings:autopickup",
        "settings:safemode",
        "settings:distractions",
        "settings:colors",
      ])
      const newGame = await session.choose("New Game")
      assertEquals(newGame.stages, ["received", "validated", "executing", "completed"])
      assert(session.eventTypes.includes("interaction.changed"))
      assertEquals(menu().tabs, allTabs)
      assertEquals(menu().selected, ["tab:new_game"])
      assertEquals(
        menu().entries.map((entry) => [entry.id, entry.pane_id]),
        [
          "custom",
          "preset",
          "random",
          "play_now_default",
          "play_now",
          "tutorial",
          "defence",
        ].map((id) => [`new_game:${id}`, "new_game"]),
      )

      // Entering the world replaces the authority: the command is interrupted, the stream resyncs.
      const chosenAt = performance.now()
      const tutorial = await session.choose("Tutorial")
      assertEquals(tutorial.stages.at(-1), "interrupted")
      await checkLoading(session.loading, chosenAt)
      // An interrupted command says why: the world it ran in is gone.
      const interrupted = await client.result("bn.command.result", {
        epoch,
        command_id: tutorial.id,
      })
      assertEquals([interrupted.stage, interrupted.error], ["interrupted", "stale_epoch"])
      const resync = await client.note()
      assertEquals(resync.method, "bn.resync")
      session.resyncs.push(resync.params)
      assertEquals(session.resyncs[0].reason, "world_replaced")
      assertEquals(session.resyncs[0].epoch, epoch)
      await session.start()
      assertNotEquals(session.mirror.at.epoch, epoch)
      assert(session.mirror.interaction.readiness.game_ready, "world loaded")

      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      const moves = ["RIGHT", "LEFT", "UP", "DOWN"]
      const move = moves.find((id) =>
        session.mirror.interaction.actions.some((a: Value) => a.id === id)
      )
      assert(move, `no movement action in ${JSON.stringify(session.mirror.interaction.actions)}`)

      // The loaded world is known: cells and the avatar.
      assert(session.mirror.cells.size > 0, "known cells")
      // The avatar carries the native sidebar: every limb, the needs and the time of day.
      const sidebar = session.mirror.avatar.sidebar
      assert(sidebar.limbs.length >= 6, "limbs")
      assertEquals(typeof sidebar.limbs[0].hp, "number")
      assertEquals(typeof sidebar.hunger.text, "string")
      assert(["walk", "run", "crouch", "prone"].includes(sidebar.movement.mode))
      assert(sidebar.location.text.length > 0, "location name")
      // A tiled client gets the engine's own tile selection for what it sees.
      const visible = [...session.mirror.cells.values()].filter((entry) =>
        entry.known === "visible"
      )
      // A cell layer leaves out the default `center` subtile and the turn 0.
      const subtile = (entry: Value) => entry.terrain.subtile ?? "center"
      assert(visible.length > 0 && visible.every((entry) => entry.terrain !== undefined))
      assert(visible.every((entry) => [0, 1, 2, 3].includes(entry.terrain.rotation ?? 0)))
      assert(visible.every((entry) => Number.isInteger(entry.light) && entry.light >= 0))
      assert(visible.some((entry) => subtile(entry) !== "unconnected"), "connected terrain")
      assert(["player_male", "player_female"].includes(session.mirror.avatar.look?.tile))
      assert(["spring", "summer", "autumn", "winter"].includes(session.mirror.environment?.season))
      const avatarBefore = session.mirror.avatar.at
      const typesBefore = session.eventTypes.length
      const before = BigInt(session.mirror.at.sequence)
      const step = await session.submit({ kind: "action", action_id: move })
      assertEquals(step.stages, ["received", "validated", "executing", "completed"])
      assert(BigInt(session.mirror.at.sequence) > before, "the boundary published an event")
      const completed = session.stages.get(step.id!)!
      assertEquals(completed.at(-1), "completed")
      // Movement publishes what the avatar now knows, and the avatar moved by one square.
      const worldTypes = session.eventTypes.slice(typesBefore).filter((type) =>
        type === "cells.seen" || type === "coverage.moved"
      )
      assert(worldTypes.length > 0, `world change event after moving: ${session.eventTypes}`)
      const avatarAfter = session.mirror.avatar.at
      assertEquals(
        Math.abs(avatarAfter.x - avatarBefore.x) + Math.abs(avatarAfter.y - avatarBefore.y),
        1,
      )

      // Lost-notification recovery.
      const recovered = await client.result("bn.command.result", {
        epoch: session.mirror.at.epoch,
        command_id: step.id,
      })
      assertEquals(recovered.stage, "completed")
      assert(recovered.at)

      // Stale submit: rejected with the reason, nothing executes, the clock stays.
      const clock = { ...session.mirror.at }
      const stale = await session.submit(
        { kind: "action", action_id: move },
        { revision: (BigInt(clock.revision) - 1n).toString() },
      )
      assertEquals(stale.stages.at(-1), "rejected")
      assertEquals(session.mirror.at, clock)
      const rejected = await client.result("bn.command.result", {
        epoch: clock.epoch,
        command_id: stale.id,
      })
      assertEquals([rejected.stage, rejected.error], ["rejected", "stale_revision"])

      // Waypoint travel: the first click publishes the engine's own route, the second walks it.
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      const here = session.mirror.avatar.at
      const reach = (pos: Value) => Math.max(Math.abs(pos.x - here.x), Math.abs(pos.y - here.y))
      const open = (entry: Value) =>
        entry.known === "visible" && !entry.furniture && !entry.vehicle &&
        /floor|dirt|grass|pavement|sidewalk/.test(entry.terrain?.id ?? "")
      const candidates = [...session.mirror.cells.values()]
        .filter((entry) => open(entry) && reach(entry.at) >= 2 && reach(entry.at) <= 8)
        .map((entry) => entry.at)
        .sort((a, b) => reach(b) - reach(a))
      assert(candidates.length > 0, `no open square in view: ${JSON.stringify(here)}`)
      // Ask the engine until it finds a route; a wall or an unreachable square plans none.
      let aim: Value
      let plan: Value
      for (const candidate of candidates.slice(0, 40)) {
        plan = await session.submit({ kind: "travel", pos: candidate })
        if (plan.stages.at(-1) === "rejected") {
          const why = await client.result("bn.command.result", {
            epoch: session.mirror.at.epoch,
            command_id: plan.id,
          })
          throw new Error(
            `travel rejected: ${why.error} ${
              JSON.stringify(session.mirror.interaction).slice(0, 600)
            }`,
          )
        }
        assertEquals(plan.stages, ["received", "validated", "executing", "completed"])
        if (session.mirror.route.length >= 2) {
          aim = candidate
          break
        }
      }
      assert(
        aim,
        `the engine planned a route: ${
          JSON.stringify([here, candidates.slice(0, 3), session.mirror.route])
        }`,
      )
      assertEquals(session.mirror.route.at(-1), aim, "the route ends at the clicked square")
      assertEquals(session.mirror.avatar.at, here, "the first click does not move")
      const planned = session.mirror.route.length

      const eventsBefore = session.eventTypes.length
      const walk = await session.submit({ kind: "travel", pos: aim })
      assertEquals(walk.stages.at(-1), "completed")
      assertEquals(session.mirror.route, [], "confirming clears the plan")
      const walkEvents = session.eventTypes.length - eventsBefore
      assert(walkEvents >= planned, `events per step: ${walkEvents} for ${planned} squares`)
      // Arrived (or stopped where the engine stopped, never past the destination).
      const arrived = session.mirror.avatar.at
      assertEquals(arrived, aim, "avatar stands on the destination")

      // A click outside the view is rejected without effect.
      const clock2 = { ...session.mirror.at }
      const far = await session.submit({ kind: "travel", pos: { ...aim, x: aim.x + 5000 } })
      assertEquals(far.stages.at(-1), "rejected")
      assertEquals(session.mirror.at, clock2)

      // The declared 40x25 viewport is the clickable view, which follows the avatar. Its far east
      // edge lies outside the 36x24 window of the default terminal, yet a click there is accepted.
      const size = (view: Value) => [view.max.x - view.min.x + 1, view.max.y - view.min.y + 1]
      assertEquals(size(session.mirror.view!), [40, 25])
      const edge = { ...session.mirror.avatar.at, x: session.mirror.view!.max.x }
      const wide = await session.submit({ kind: "travel", pos: edge })
      assertEquals(wide.stages.at(-1), "completed", "a click inside the declared view is accepted")

      // Shrinking is a native window resize, bounded by the native minimum terminal: the same click
      // is outside the new view and rejected without effect.
      await session.viewport(30, 10)
      const [columns] = size(session.mirror.view!)
      assert(columns < 40, `the window shrank: ${JSON.stringify(session.mirror.view)}`)
      const clock3 = { ...session.mirror.at }
      const narrow = await session.submit({
        kind: "travel",
        pos: { ...edge, x: session.mirror.avatar.at.x + 19 },
      })
      assertEquals(narrow.stages.at(-1), "rejected")
      assertEquals(session.mirror.at, clock3)

      // The reconstructed state equals a fresh subscribe.
      const fresh = await session.subscribe()
      assertEquals(fresh.state(), session.mirror.state())
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: walking the tutorial to the stairs and back keeps every command completing",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      const stairs = { x: 1210, y: 1214 }
      const log: string[] = []
      const run = async (operation: Value) => {
        const result = await session.submit(operation)
        log.push(`${JSON.stringify(operation)} ${result.stages.at(-1)}`)
        assertEquals(result.stages.at(-1), "completed", log.slice(-6).join("\n"))
      }
      const acknowledgePopups = async () => {
        while (session.mirror.interaction.interaction) {
          const prompt = session.mirror.interaction.interaction
          assertEquals(prompt.context, "POPUP_WAIT")
          await run({ kind: "choose", choice_id: prompt.choices[0].id })
        }
      }
      // Lessons pop up on the way; each one is a prompt the client can answer.
      for (let step = 0; step < 60; step++) {
        await acknowledgePopups()
        const at = session.mirror.avatar.at
        if (at.x === stairs.x && at.y === stairs.y) break
        const dir = at.x === 1202 && at.y < 1214 ? "DOWN" : at.x < stairs.x ? "RIGHT" : "LEFT"
        await run({ kind: "action", action_id: dir })
      }
      await acknowledgePopups()
      assertEquals(session.mirror.avatar.at, { dim: "", ...stairs, z: 0 })
      await run({ kind: "action", action_id: "LEVEL_DOWN" })
      await acknowledgePopups()
      assertEquals(session.mirror.avatar.at.z, -1)
      await run({ kind: "action", action_id: "LEFT" })
      await run({ kind: "action", action_id: "RIGHT" })
      await run({ kind: "action", action_id: "LEVEL_UP" })
      assertEquals(session.mirror.avatar.at.z, 0)

      // Look mode publishes its cursor as a target interaction and its own actions; it can be left.
      await run({ kind: "action", action_id: "look" })
      const ids = session.mirror.interaction.actions.map((a: Value) => a.id)
      const look = session.mirror.interaction.interaction
      assertEquals([look.kind, look.context], ["target", "LOOK"])
      assertEquals(look.target.current, session.mirror.avatar.at)
      assert(ids.includes("QUIT") && !ids.includes("look"), ids.join())
      await run({ kind: "action", action_id: "QUIT" })
      assert(session.mirror.interaction.actions.some((a: Value) => a.id === "look"))
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: the tutorial crafting menu lists recipes, searches and closes",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      const interaction = () => session.mirror.interaction.interaction
      const opened = await session.submit({ kind: "action", action_id: "craft" })
      assertEquals(opened.stages.at(-1), "completed")
      assertEquals(interaction().context, "CRAFTING")
      assertEquals(interaction().kind, "choices")
      assertEquals(interaction().field.id, "filter")
      // Tab rows have no pane; the sub-tab and recipe rows belong to the selected tab.
      const rows = () => interaction().choices as Value[]
      const tabs = () => rows().filter((entry) => entry.id.startsWith("tab:"))
      const recipes = () => rows().filter((entry) => entry.id.startsWith("recipe:"))
      assert(tabs().length > 1 && tabs().every((entry) => entry.pane_id === undefined))
      assertEquals(tabs().filter((entry) => entry.selected).length, 1)
      const selected = tabs().find((entry) => entry.selected)
      assert(rows().some((entry) => entry.id.startsWith("subtab:") && entry.pane_id !== undefined))

      // Each tab lists its own recipes, with the native detail text on the row.
      let tab: Value
      for (const candidate of tabs()) {
        if (candidate.id === selected.id) continue
        assertEquals(
          (await session.submit({ kind: "choose", choice_id: candidate.id })).stages.at(-1),
          "completed",
        )
        assertEquals(interaction().context, "CRAFTING")
        if (recipes().length > 0) {
          tab = candidate
          break
        }
      }
      assert(tab, "some tab lists recipes")
      assertEquals(tabs().filter((entry) => entry.selected).map((entry) => entry.id), [tab.id])
      assert(recipes().every((entry) => entry.pane_id === tab.id.slice(4)))
      const first = recipes()[0]
      assert(first.description.includes("Time to complete"), first.description)
      assert(!first.description.includes("<color"), first.description)

      // Searching replaces the list; clearing restores it.
      const all = recipes().length
      const word = first.label.split(" ")[0]
      const search = (value: string) =>
        session.submit({ kind: "fill", field_id: "filter", value, submit: true })
      assertEquals((await search(word)).stages.at(-1), "completed")
      assertEquals(interaction().field.value, word)
      assert(recipes().length > 0 && recipes().length <= all)
      assert(recipes().some((entry) => entry.label.includes(word)))
      assertEquals((await search("")).stages.at(-1), "completed")
      assertEquals(recipes().length, all)

      // Closing returns to the map.
      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      assertEquals(interaction(), null)
      assert(session.mirror.interaction.actions.some((a: Value) => a.id === "craft"))
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: waiting five minutes passes five minutes and completes when input is awaited again",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      const turn = () => BigInt(session.mirror.environment.turn)
      // Tutorial lessons pop up as turns pass; they are prompts of their own.
      const lessons = () =>
        session.acknowledge(() => session.mirror.interaction.interaction === null)
      await lessons()
      const wait = async (label: string) => {
        const menu = await session.submit({ kind: "action", action_id: "wait" })
        assertEquals(menu.stages.at(-1), "completed")
        assertEquals(session.mirror.interaction.interaction.kind, "choices")
        const before = turn()
        const chosen = await session.choose(label)
        assertEquals(chosen.stages.at(-1), "completed")
        await lessons()
        return turn() - before
      }
      // The menu is offered every time, and the activity is not cut short at a boundary.
      assertEquals(await wait("5 minutes"), 300n)
      assertEquals(await wait("5 minutes"), 300n)
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: a long wait is interrupted mid-activity like the native interrupt key",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      // Tutorial lessons pop up while the first turns pass; let them out of the way first.
      await session.submit({ kind: "action", action_id: "wait" })
      await session.choose("5 minutes")
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      // Nothing runs, so there is nothing to interrupt.
      const idle = await client.request("bn.interrupt", {})
      assertEquals(idle.error.data.kind, "not_ready")

      assertEquals(
        (await session.submit({ kind: "action", action_id: "wait" })).stages.at(-1),
        "completed",
      )
      const before = BigInt(session.mirror.environment.turn)
      const hours = 6n * 3600n
      const choice = session.mirror.interaction.interaction.choices.find((entry: Value) =>
        entry.label.startsWith("6 hours")
      )
      assert(choice, "6 hours entry")
      // The request is answered while the activity runs; the activity asks whether to stop.
      const waiting = await session.submit(
        { kind: "choose", choice_id: choice.id },
        {},
        async () => {
          assertEquals(await client.result("bn.interrupt", {}), {})
        },
      )
      assertEquals(waiting.stages.at(-1), "completed")
      const query = session.mirror.interaction.interaction
      assert(query, "the activity asks before stopping")
      assert(BigInt(session.mirror.environment.turn) - before < hours, "stopped early")
      const asked = BigInt(session.mirror.environment.turn)
      await session.choose("Yes")
      assertEquals(session.mirror.interaction.interaction, null)
      const stopped = BigInt(session.mirror.environment.turn)
      assert(stopped - before < hours, `the wait ended early: ${stopped - before}`)
      assert(stopped - asked < 60n, "the confirmed stop ends the activity at once")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: the native message log reaches the client in order with repeat counts",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      const before = session.mirror.messages.length
      assert(before > 0, "the subscribe snapshot already carries the log the game wrote")
      // The same native message twice in a row is one line whose count rises.
      for (let step = 0; step < 2; step++) {
        const up = await session.submit({ kind: "action", action_id: "LEVEL_UP" })
        assertEquals(up.stages.at(-1), "completed")
      }
      const lines = session.mirror.messages
      assertEquals(lines.length, before + 1, "the repeat replaced its line")
      assert(lines.at(-1)!.text.startsWith("You can't"), lines.at(-1)!.text)
      assertEquals([lines.at(-1)!.count, lines.at(-1)!.kind], [2, "info"])
      const ids = lines.map((line) => BigInt(line.id))
      assertEquals(ids, [...ids].sort((a, b) => (a < b ? -1 : a > b ? 1 : 0)), "log order")
      assert(session.eventTypes.filter((type) => type === "message.logged").length >= 2)
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

/** Main menu to the Settings tab entry `id`, at the first menu boundary. */
async function openSetting(session: Session, id: string) {
  await session.acknowledge(() => session.mirror.interaction.interaction?.context === "MAIN_MENU")
  await session.submit({ kind: "choose", choice_id: "tab:settings" })
  assertEquals((await session.submit({ kind: "choose", choice_id: id })).stages.at(-1), "completed")
}

const shown = (session: Session): Value[] => session.mirror.interaction.interaction.choices
const rowOf = (session: Session, id: string) => shown(session).find((entry) => entry.id === id)
const press = (session: Session, id: string) => session.submit({ kind: "choose", choice_id: id })
const setTo = (session: Session, id: string, value: string) =>
  session.submit({ kind: "fill", field_id: id, value, submit: true })

/** Leave a settings screen the way Escape does and answer its save prompt. */
async function leaveAndSave(session: Session, done: () => boolean) {
  assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
  assertEquals((await session.choose("Yes")).stages.at(-1), "completed")
  await session.acknowledge(done)
}

const inMainMenu = (session: Session) => () =>
  session.mirror.interaction.interaction?.context === "MAIN_MENU"

async function startSession(client: Client): Promise<Session> {
  await client.result("bn.hello", {
    versions: ["0.2"],
    schema: schemaHash,
    client: { name: "settings-test", version: "1" },
  })
  const session = new Session(client)
  await session.start()
  return session
}

Deno.test({
  name: "stdio: options and keybindings are changed through the wire, persist and take effect",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const binary = Deno.env.get("BN_BINARY")!
    let client = new Client(binary, profile)
    try {
      let session = await startSession(client)
      await openSetting(session, "settings:options")
      const options = session.mirror.interaction.interaction
      assertEquals(options.context, "OPTIONS")
      const pages = shown(session).filter((entry) => entry.pane_id === undefined)
      assert(pages.length >= 5 && pages.every((entry) => entry.id.startsWith("page:")))
      // Find the page that holds an option, as a client would when a tab is clicked.
      const find = async (id: string) => {
        for (const page of pages) {
          await press(session, page.id)
          if (rowOf(session, id)) return rowOf(session, id)
        }
        throw new Error(`no option ${id}`)
      }
      const safe = await find("option:SAFEMODE")
      assertEquals(safe.editor.type, "bool")
      assertEquals(safe.editor.value, "true")
      assertEquals((await setTo(session, "option:SAFEMODE", "false")).stages.at(-1), "completed")
      assertEquals(rowOf(session, "option:SAFEMODE").editor.value, "false")
      // A number outside the native range is refused and changes nothing.
      const near = await find("option:SAFEMODEPROXIMITY")
      assertEquals(near.editor.type, "integer")
      const clock = { ...session.mirror.at }
      assertEquals(
        (await setTo(session, near.id, String(near.editor.maximum + 1))).stages.at(-1),
        "rejected",
      )
      assertEquals(session.mirror.at, clock)
      assertEquals((await setTo(session, near.id, "7")).stages.at(-1), "completed")
      assertEquals(rowOf(session, near.id).editor.value, "7")
      // A select takes one of its own values; a foreign one is refused.
      const select = shown(session).concat(...[]).find((entry) =>
        entry.editor?.type === "select"
      ) ??
        await (async () => {
          for (const page of pages) {
            await press(session, page.id)
            const found = shown(session).find((entry) => entry.editor?.type === "select")
            if (found) return found
          }
        })()
      assert(select, "a select option exists")
      assertEquals((await setTo(session, select.id, "no such value")).stages.at(-1), "rejected")
      await leaveAndSave(session, inMainMenu(session))
      await client.close()

      // Relaunch on the same profile: the saved values are what the menu shows.
      client = new Client(binary, profile)
      session = await startSession(client)
      await openSetting(session, "settings:options")
      assertEquals((await find("option:SAFEMODE")).editor.value, "false")
      assertEquals((await find("option:SAFEMODEPROXIMITY")).editor.value, "7")
      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      await session.acknowledge(() =>
        session.mirror.interaction.interaction?.context === "MAIN_MENU"
      )

      // In the game, the same menu applies a saved change at once: safe mode follows the option.
      await press(session, "tab:new_game")
      await press(session, "new_game:tutorial")
      assertEquals((await client.note()).method, "bn.resync")
      session = new Session(client)
      await session.start()
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      // Safe mode is off after the native toggle; the saved option turns the sidebar flag back on.
      assertEquals(
        (await session.submit({ kind: "action", action_id: "safemode" })).stages.at(-1),
        "completed",
      )
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      assertEquals(session.mirror.avatar.sidebar.safe_mode.enabled, false)
      // The in-game keybindings menu binds a key to Options, which then opens the same menu.
      assertEquals(
        (await session.submit({ kind: "action", action_id: "HELP_KEYBINDINGS" })).stages.at(-1),
        "completed",
      )
      await setTo(session, session.mirror.interaction.interaction.field.id, "Options")
      assert(rowOf(session, "action:open_options"), "Options is listed")
      await press(session, "mode:add_local")
      await press(session, "action:open_options")
      assertEquals((await setTo(session, "field:key", "=")).stages.at(-1), "completed")
      await answerPrompts(session)
      await leaveAndSave(session, () => session.mirror.interaction.interaction === null)
      const opened = await session.submit({ kind: "action", action_id: "open_options" })
      assertEquals(opened.stages.at(-1), "completed")
      assertEquals(session.mirror.interaction.interaction.context, "OPTIONS")
      await find("option:AUTOSAFEMODE")
      assertEquals((await setTo(session, "option:AUTOSAFEMODE", "true")).stages.at(-1), "completed")
      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      assertEquals((await session.choose("Yes")).stages.at(-1), "completed")
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      assertEquals(session.mirror.avatar.sidebar.safe_mode.enabled, true)
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

/** The native yes/no prompts that may sit between two keybinding steps. */
async function answerPrompts(session: Session) {
  while (session.mirror.interaction.interaction?.context !== "HELP_KEYBINDINGS") {
    assertEquals((await session.choose("Yes")).stages.at(-1), "completed")
  }
}

const keysOf = (session: Session, id: string) => rowOf(session, id).columns[0].value

Deno.test({
  name: "stdio: a keybinding is removed and added through the wire, persists and moves the avatar",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const binary = Deno.env.get("BN_BINARY")!
    let client = new Client(binary, profile)
    try {
      let session = await startSession(client)
      await openSetting(session, "settings:keybindings")
      const menu = session.mirror.interaction.interaction
      assertEquals([menu.title, menu.field.type], ["Keybindings", "text"])
      assertEquals(
        shown(session).filter((entry) => entry.pane_id === "modes").map((entry) => entry.id),
        ["mode:add_local", "mode:add_global", "mode:remove"],
      )
      // The filter is the native one: only the matching actions remain.
      await setTo(session, menu.field.id, "Move East")
      assertEquals(
        shown(session).filter((entry) => entry.pane_id === "actions").map((entry) => entry.id),
        ["action:RIGHT"],
      )
      assertEquals(keysOf(session, "action:RIGHT"), "l, RIGHT, 6 or NUMPAD_6")
      // Remove every key, then add one.
      await press(session, "mode:remove")
      assertEquals(rowOf(session, "mode:remove").selected, true)
      await press(session, "action:RIGHT")
      await answerPrompts(session)
      assertEquals(rowOf(session, "action:RIGHT").columns[1].value, "Unbound")
      await press(session, "mode:add_global")
      await press(session, "action:RIGHT")
      const prompt = session.mirror.interaction.interaction
      assertEquals(prompt.field.type, "key")
      // A key the game does not know is refused; a real one is taken.
      assertEquals((await setTo(session, prompt.field.id, "no such key")).stages.at(-1), "rejected")
      assertEquals((await setTo(session, prompt.field.id, "z")).stages.at(-1), "completed")
      await answerPrompts(session)
      assertEquals(keysOf(session, "action:RIGHT"), "z")
      await leaveAndSave(session, inMainMenu(session))
      await client.close()

      // Relaunch on the same profile: the menu shows the saved key, and it moves the avatar.
      client = new Client(binary, profile)
      session = await startSession(client)
      await openSetting(session, "settings:keybindings")
      await setTo(session, session.mirror.interaction.interaction.field.id, "Move East")
      assertEquals(keysOf(session, "action:RIGHT"), "z")
      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      await session.acknowledge(() =>
        session.mirror.interaction.interaction?.context === "MAIN_MENU"
      )
      await press(session, "tab:new_game")
      await press(session, "new_game:tutorial")
      assertEquals((await client.note()).method, "bn.resync")
      session = new Session(client)
      await session.start()
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      const before = session.mirror.avatar.at
      assertEquals(
        (await session.submit({ kind: "action", action_id: "RIGHT" })).stages.at(-1),
        "completed",
      )
      assertEquals(session.mirror.avatar.at.x, before.x + 1)
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: the other settings screens are structured and a distraction persists",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const binary = Deno.env.get("BN_BINARY")!
    let client = new Client(binary, profile)
    try {
      let session = await startSession(client)
      const leave = async () => {
        assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
        await session.acknowledge(inMainMenu(session))
      }
      // Each screen publishes its tabs and columns or rows; the rule screens start empty.
      for (
        const [entry, context, first] of [
          ["settings:autopickup", "AUTO_PICKUP", "column:1"],
          ["settings:safemode", "SAFEMODE", "page:0"],
          ["settings:colors", "COLORS", "column:1"],
        ]
      ) {
        await openSetting(session, entry)
        const screen = session.mirror.interaction.interaction
        assertEquals([screen.context, shown(session)[0].id], [context, first])
        await leave()
      }
      const colors = await (async () => {
        await openSetting(session, "settings:colors")
        return shown(session).filter((row) => row.id.startsWith("color:"))
      })()
      assert(colors.length > 10 && colors.every((row) => row.columns.length === 2))
      // Choosing a color row opens the native list of colors for the cursor's column.
      await press(session, colors[0].id)
      assert(session.mirror.interaction.interaction.choices.length > 10)
      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      assertEquals(session.mirror.interaction.interaction.context, "COLORS")
      await leave()

      await openSetting(session, "settings:distractions")
      const rows = shown(session)
      assert(rows.length >= 8 && rows.every((row) => row.editor.type === "bool"))
      const target = rows[0]
      assertEquals(target.editor.value, "true")
      assertEquals((await setTo(session, target.id, "false")).stages.at(-1), "completed")
      assertEquals(rowOf(session, target.id).columns[0].value, "Disabled")
      await leave()
      await client.close()

      client = new Client(binary, profile)
      session = await startSession(client)
      await openSetting(session, "settings:distractions")
      assertEquals(rowOf(session, target.id).editor.value, "false")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: an offered action without a key binding is executed by its id",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      assert(
        session.mirror.interaction.actions.some((a: Value) => a.id === "pickup_feet"),
        "pickup_feet is offered",
      )
      const picked = await session.submit({ kind: "action", action_id: "pickup_feet" })
      assertEquals(picked.stages.at(-1), "completed")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: a long activity sends progress heartbeats while it runs",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      // Tutorial lessons pop up on the first turns; let them out of the way.
      await session.submit({ kind: "action", action_id: "wait" })
      await session.choose("5 minutes")
      await session.acknowledge(() => session.mirror.interaction.interaction === null)

      await session.submit({ kind: "action", action_id: "wait" })
      const choice = session.mirror.interaction.interaction.choices.find((entry: Value) =>
        entry.label.startsWith("6 hours")
      )
      assert(choice, "6 hours entry")
      const before = session.progress
      // Interrupt at the first heartbeat: how long the wait lasts depends on the engine's speed.
      const waiting = await session.submit(
        { kind: "choose", choice_id: choice.id },
        {},
        async () => {
          await client.result("bn.interrupt", {})
        },
      )
      assertEquals(waiting.stages.at(-1), "completed")
      assert(session.progress > before, "the engine sent heartbeats while the activity ran")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: world cells reads a box of what the avatar knows without changing it",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      const here = session.mirror.avatar.at
      const epoch = session.mirror.at.epoch
      const query = (min: Value, max: Value, id = epoch) =>
        client.request("bn.world.cells", { epoch: id, min, max })
      const key = (pos: Value) => `${pos.dim}|${pos.x}|${pos.y}|${pos.z}`
      const before = JSON.stringify(session.mirror.state())
      const box = {
        min: { ...here, x: here.x - 3, y: here.y - 3 },
        max: { ...here, x: here.x + 3, y: here.y + 3 },
      }
      const read = await query(box.min, box.max)
      assertEquals(read.result.at, session.mirror.at, JSON.stringify(read))
      // Every square of the box is either the mirror's cell, byte for byte, or forgotten.
      const forgotten = new Set(read.result.forgotten.map((pos: Value) => key(pos)))
      assertEquals(read.result.cells.length + forgotten.size <= 49, true)
      assert(read.result.cells.length > 0, "the avatar knows its surroundings")
      for (const cell of read.result.cells) {
        assertEquals(cell, session.mirror.cells.get(key(cell.at)), key(cell.at))
      }
      for (const pos of read.result.forgotten) {
        assertEquals(session.mirror.cells.has(key(pos)), false, key(pos))
      }
      assertEquals(JSON.stringify(session.mirror.state()), before)
      // Inverted, cross-level, oversized and stale-epoch queries are refused with their kinds.
      const inverted = await query(box.max, box.min)
      assertEquals(inverted.error.data.kind, "validation_failed")
      const levels = await query(box.min, { ...box.max, z: here.z + 1 })
      assertEquals(levels.error.data.kind, "validation_failed")
      const large = await query(box.min, { ...box.max, x: here.x + 64, y: here.y + 64 })
      assertEquals(large.error.data.kind, "resource_limit")
      const stale = await query(box.min, box.max, "epoch:old")
      assertEquals(stale.error.data.kind, "stale_epoch")
      assertEquals(stale.error.data.action, "subscribe")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: the native mouse view describes a square and a context click is the right button",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      const here = session.mirror.avatar.at
      const describe = (pos: Value, boundary = session.mirror.interaction.boundary_id) =>
        client.request("bn.world.describe", {
          epoch: session.mirror.at.epoch,
          boundary_id: boundary,
          pos,
        })
      // The avatar's own square: the native mouse view prints something, and nothing changes.
      const before = JSON.stringify(session.mirror.state())
      const described = await describe(here)
      assert(described.result.lines.length > 0, JSON.stringify(described))
      assert(described.result.lines.every((line: string) => line === line.trimEnd()))
      assertEquals(JSON.stringify(session.mirror.state()), before)
      // A square outside the terrain window and a stale boundary are refused with their kinds.
      const far = await describe({ ...here, x: here.x + 5000 })
      assertEquals(far.error.data.kind, "validation_failed")
      const stale = await describe(here, "boundary:old")
      assertEquals(stale.error.data.kind, "stale_boundary")

      // Right click on a far square: the native SEC_SELECT finds nothing relevant and the command
      // still completes; on the avatar's square it is the pickup action.
      const nothing = await session.submit({ kind: "context", pos: { ...here, x: here.x + 4 } })
      assertEquals(nothing.stages.at(-1), "completed")
      const self = await session.submit({ kind: "context", pos: here })
      assertEquals(self.stages.at(-1), "completed")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: the overmap opens as a structured view, moves its cursor and closes",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      const opened = await session.submit({ kind: "action", action_id: "map" })
      assertEquals(opened.stages.at(-1), "completed")
      const view = () => session.mirror.interaction.interaction
      assertEquals(view().context, "OVERMAP")
      assertEquals(view().kind, "target")
      const first = view().overmap
      assert(first.cols > 20 && first.rows > 10, `${first.cols}x${first.rows}`)
      assertEquals(first.cells.length, first.rows)
      assertEquals(first.cells[0].length, first.cols)
      assert(first.legend.some((line: string) => line.trim().length > 0), "sidebar text")
      assertEquals(view().target.unit, "omt")
      assertEquals(view().target.current, first.player)
      // The window is centered on the cursor.
      assertEquals(first.origin.x, first.player.x - Math.floor(first.cols / 2))

      const to = { ...first.player, x: first.player.x + 3, y: first.player.y - 2 }
      const moved = await session.submit({ kind: "set_target", pos: to })
      assertEquals(moved.stages.at(-1), "completed")
      assertEquals(view().target.current, to)
      assertEquals(view().overmap.origin.x, first.origin.x + 3)
      assertEquals(view().overmap.player, first.player)

      // A square outside the safe bounds is refused.
      const far = await session.submit({
        kind: "set_target",
        pos: { ...to, x: 1 << 30 },
      })
      assertEquals(far.stages.at(-1), "rejected")

      assertEquals((await session.submit({ kind: "cancel" })).stages.at(-1), "completed")
      assertEquals(view(), null)
      assert(session.mirror.interaction.actions.some((a: Value) => a.id === "map"))
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: a tiles client gets overmap tile ids and the route, others do not",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client, { tiles: true })
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      assertEquals(
        (await session.submit({ kind: "action", action_id: "map" })).stages.at(-1),
        "completed",
      )
      const overmap = session.mirror.interaction.interaction.overmap
      assertEquals(overmap.path, [])
      const tiles = overmap.tiles
      assert(tiles, "tile ids for a tiles client")
      assertEquals(tiles.cells.length, overmap.rows)
      assertEquals(tiles.cells[0].length, overmap.cols)
      const [id, rotation, subtile] =
        tiles.cells[Math.floor(overmap.rows / 2)][Math.floor(overmap.cols / 2)]
      assert(typeof id === "string" && id.length > 0 && id !== "unknown_terrain", id)
      assert(Number.isInteger(rotation) && typeof subtile === "string")
      assert(id in tiles.looks_like)
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

/** Rejects when `work` outlives `ms`, so a hung engine fails the test instead of the whole run. */
const withDeadline = <T>(work: Promise<T>, ms: number, why: string) => {
  let timer: ReturnType<typeof setTimeout> | undefined
  const expired = new Promise<never>((_, reject) => {
    timer = setTimeout(() => reject(new Error(why)), ms)
  })
  return Promise.race([work, expired]).finally(() => clearTimeout(timer))
}

for (const sig of ["SIGTERM", "SIGINT"] as const) {
  Deno.test({
    name: `stdio: ${sig} is named on stderr and ends the engine by the signal, not by exit code 0`,
    ignore: !Deno.env.get("BN_BINARY"),
    async fn() {
      const profile = await makeProfile()
      const process = new Deno.Command(Deno.env.get("BN_BINARY")!, {
        args: ["--client=mcp", "--userdir", `${profile}/`, "--configdir", `${profile}/config/`],
        stdin: "piped",
        stdout: "piped",
        stderr: "piped",
      }).spawn()
      try {
        const writer = process.stdin.getWriter()
        const send = (value: Value) =>
          writer.write(new TextEncoder().encode(JSON.stringify(value) + "\n"))
        const reader = process.stdout.pipeThrough(new TextDecoderStream()).getReader()
        let seen = ""
        const until = async (needle: string) => {
          while (!seen.includes(needle)) {
            const { value, done } = await reader.read()
            assert(!done, `the engine closed before ${needle}`)
            seen += value
          }
        }
        await send({
          jsonrpc: "2.0",
          id: 1,
          method: "bn.hello",
          params: {
            client: { name: "stdio-test", version: "1" },
            versions: ["0.2"],
            schema: schemaHash,
          },
        })
        await until(`"id":1`)
        await send({ jsonrpc: "2.0", id: 2, method: "bn.subscribe", params: {} })
        // A subscribe is answered at an input boundary, so the engine is waiting for input by then.
        await until(`"id":2`)
        process.kill(sig)
        const [status, stderr] = await withDeadline(
          Promise.all([process.status, new Response(process.stderr).text()]),
          10_000,
          `the engine outlived ${sig}`,
        )
        assert(
          stderr.includes(`mcp: the engine is exiting: received ${sig}`),
          `stderr: ${stderr.slice(-400)}`,
        )
        assertEquals(status.signal, sig, "ended by the signal, not by exit code 0")
      } finally {
        try {
          process.kill("SIGKILL")
        } catch { /* already gone */ }
        await Deno.remove(profile, { recursive: true })
      }
    },
  })

  Deno.test({
    name: `stdio: ${sig} before the first subscribe is named too`,
    ignore: !Deno.env.get("BN_BINARY"),
    async fn() {
      const profile = await makeProfile()
      const process = new Deno.Command(Deno.env.get("BN_BINARY")!, {
        args: ["--client=mcp", "--userdir", `${profile}/`, "--configdir", `${profile}/config/`],
        stdin: "piped",
        stdout: "piped",
        stderr: "piped",
      }).spawn()
      try {
        const writer = process.stdin.getWriter()
        const send = (value: Value) =>
          writer.write(new TextEncoder().encode(JSON.stringify(value) + "\n"))
        const reader = process.stdout.pipeThrough(new TextDecoderStream()).getReader()
        let seen = ""
        const until = async (needle: string) => {
          while (!seen.includes(needle)) {
            const { value, done } = await reader.read()
            assert(!done, `the engine closed before ${needle}`)
            seen += value
          }
        }
        await send({
          jsonrpc: "2.0",
          id: 1,
          method: "bn.hello",
          params: {
            client: { name: "stdio-test", version: "1" },
            versions: ["0.2"],
            schema: schemaHash,
          },
        })
        // Answered before any input boundary: the engine is still starting, not waiting for input.
        await until(`"id":1`)
        process.kill(sig)
        const [status, stderr] = await withDeadline(
          Promise.all([process.status, new Response(process.stderr).text()]),
          10_000,
          `the engine outlived ${sig}`,
        )
        assert(
          stderr.includes(`mcp: the engine is exiting: received ${sig}`),
          `stderr: ${stderr.slice(-400)}`,
        )
        assertEquals(status.signal, sig, "ended by the signal, not by exit code 0")
      } finally {
        try {
          process.kill("SIGKILL")
        } catch { /* already gone */ }
        await Deno.remove(profile, { recursive: true })
      }
    },
  })
}

Deno.test({
  name: "stdio: the engine exits within 10 s of stdin reaching EOF after the client died",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const process = new Deno.Command(Deno.env.get("BN_BINARY")!, {
      args: ["--client=mcp", "--userdir", `${profile}/`, "--configdir", `${profile}/config/`],
      stdin: "piped",
      stdout: "piped",
      stderr: "piped",
    }).spawn()
    try {
      const writer = process.stdin.getWriter()
      const reader = process.stdout.pipeThrough(new TextDecoderStream()).getReader()
      await writer.write(
        new TextEncoder().encode(
          JSON.stringify({
            jsonrpc: "2.0",
            id: 1,
            method: "bn.hello",
            params: {
              client: { name: "stdio-test", version: "1" },
              versions: ["0.2"],
              schema: schemaHash,
            },
          }) + "\n",
        ),
      )
      let seen = ""
      while (!seen.includes(`"id":1`)) {
        const { value, done } = await reader.read()
        assert(!done, "the engine closed before the hello answer")
        seen += value
      }
      await writer.close()
      const [status] = await withDeadline(
        Promise.all([process.status, new Response(process.stderr).text()]),
        10_000,
        "the engine outlived the end of its stdin",
      )
      assertEquals(status.signal, null, "left by itself, not killed")
    } finally {
      try {
        process.kill("SIGKILL")
      } catch { /* already gone */ }
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name:
    "stdio: a client that unsubscribes recovers by a fresh snapshot, and a stale epoch is refused",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      await client.result("bn.hello", {
        versions: ["0.2"],
        schema: schemaHash,
        client: { name: "stdio-test", version: "1" },
      })
      const session = new Session(client)
      await session.start()
      await session.acknowledge(() =>
        session.mirror.interaction.interaction?.context === "MAIN_MENU"
      )
      const stale = session.mirror
      const epoch = stale.at.epoch

      // Unsubscribed, a command still runs; nothing is pushed, and its outcome is read by id.
      await client.result("bn.unsubscribe", {})
      const choice = stale.interaction.interaction.choices.find((entry: Value) =>
        entry.enabled && entry.label === "Settings"
      )
      const submit = await client.result("bn.command.submit", {
        epoch,
        expect: {
          revision: stale.at.revision,
          boundary_id: stale.interaction.boundary_id,
          schema_id: stale.interaction.interaction.schema_id,
        },
        operation: { kind: "choose", choice_id: choice.id },
      })
      let outcome = await client.result("bn.command.result", {
        epoch,
        command_id: submit.command_id,
      })
      for (let poll = 0; outcome.stage !== "completed" && poll < 600; poll++) {
        await new Promise((resolve) => setTimeout(resolve, 100))
        outcome = await client.result("bn.command.result", { epoch, command_id: submit.command_id })
      }
      assertEquals(outcome.stage, "completed")
      assertEquals(client.notes.filter((note) => note.method !== "bn.progress"), [])

      // Subscribing again is an explicit snapshot: it supersedes the last seen sequence, and the
      // mirror it builds shows what happened meanwhile.
      const fresh = await session.subscribe()
      assertEquals(fresh.at.epoch, epoch)
      assert(
        BigInt(fresh.at.sequence) > BigInt(stale.at.sequence),
        "snapshot is newer than the last seen",
      )
      assertNotEquals(fresh.state(), stale.state())
      assertEquals(fresh.at, outcome.at)
      session.mirror = fresh

      // The stream continues contiguously from that snapshot and equals another fresh one.
      const next = session.mirror.interaction.interaction.choices.find((entry: Value) =>
        entry.enabled && entry.label === "New Game"
      )
      assertEquals(
        (await session.submit({ kind: "choose", choice_id: next.id })).stages.at(-1),
        "completed",
      )
      assertEquals(session.mirror.state(), (await session.subscribe()).state())

      // A stale epoch is refused with the documented error, by every request that names one.
      const old = "epoch:stale"
      const at = session.mirror.at
      const expect = {
        revision: at.revision,
        boundary_id: session.mirror.interaction.boundary_id,
        schema_id: null,
      }
      for (
        const [method, params] of [
          ["bn.command.submit", { epoch: old, expect, operation: { kind: "cancel" } }],
          ["bn.command.result", { epoch: old, command_id: submit.command_id }],
          ["bn.interaction.choices", {
            epoch: old,
            boundary_id: expect.boundary_id,
            offset: 0,
            limit: 1,
          }],
          ["bn.world.describe", {
            epoch: old,
            boundary_id: expect.boundary_id,
            pos: { dim: "", x: 0, y: 0, z: 0 },
          }],
          ["bn.world.cells", {
            epoch: old,
            min: { dim: "", x: 0, y: 0, z: 0 },
            max: { dim: "", x: 1, y: 1, z: 0 },
          }],
        ] as [string, Value][]
      ) {
        const refused = await client.request(method, params)
        assertEquals(refused.error?.data?.kind, "stale_epoch", method)
        assertEquals(refused.error.data.action, "subscribe", method)
        assertEquals(refused.error.data.at.epoch, epoch, method)
      }
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

/** Spawns one `name` through the debug menu's item wish list, which pages 200 entries at a time. */
async function wishItem(client: Client, session: Session, name: string) {
  let found: Value
  for (let offset = 0; !found; offset += 200) {
    const page = await client.result("bn.interaction.choices", {
      epoch: session.mirror.at.epoch,
      boundary_id: session.mirror.interaction.boundary_id,
      offset,
      limit: 200,
    })
    assert(page.choices.length > 0, `${name} is listed`)
    found = page.choices.find((entry: Value) => entry.label === name)
  }
  assertEquals(
    (await session.submit({ kind: "choose", choice_id: found.id })).stages.at(-1),
    "completed",
  )
  const amount = { kind: "fill", field_id: "field:value", value: "1", submit: true }
  assertEquals((await session.submit(amount)).stages.at(-1), "completed")
}

/** Chooses the inventory row whose label starts with `prefix`. */
async function chooseItem(session: Session, prefix: string) {
  const row = session.mirror.interaction.interaction.choices.find((entry: Value) =>
    entry.label.startsWith(prefix)
  )
  assert(row, `no item ${prefix}`)
  assertEquals(
    (await session.submit({ kind: "choose", choice_id: row.id })).stages.at(-1),
    "completed",
  )
}

Deno.test({
  name:
    "stdio: a thrown grenade tells its throw, fuse, blast, damage and fields in order, as the engine recorded",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile({
      ANIMATIONS: "true",
      ANIMATION_PROJECTILES: "true",
      ANIMATION_DELAY: "0",
    })
    const recording = `${profile}/presentation.jsonl`
    Deno.env.set("BN_PRESENTATION_RECORD", recording)
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)
      const run = async (operation: Value) =>
        assertEquals((await session.submit(operation)).stages.at(-1), "completed")
      const clear = () => session.acknowledge(() => session.mirror.interaction.interaction === null)

      // The tutorial is dark: a lit flashlight lets the avatar see the room the grenade lands in.
      await run({ kind: "action", action_id: "debug" })
      await session.choose("Spawning…")
      await session.choose("Spawn an item")
      await wishItem(client, session, "flashlight (off)")
      await wishItem(client, session, "grenade")
      while (session.mirror.interaction.interaction) await run({ kind: "cancel" })
      await run({ kind: "action", action_id: "apply" })
      await chooseItem(session, "flashlight")
      await clear()
      await run({ kind: "action", action_id: "apply" })
      await chooseItem(session, "grenade")
      await clear()
      await run({ kind: "action", action_id: "throw" })
      await chooseItem(session, "active grenade")
      const avatar = session.mirror.avatar.at
      // Nine squares away: outside the fragments' range of seven, inside the lit room.
      await run({ kind: "set_target", pos: { ...avatar, x: avatar.x + 9 } })
      assertEquals(session.mirror.interaction.interaction.target.status, "good")
      await run({ kind: "action", action_id: "FIRE" })
      // The fuse burns five turns; tutorial pop-ups may hold the first ones back.
      for (let turn = 0; turn < 7; turn++) {
        await clear()
        await run({ kind: "action", action_id: "pause" })
      }

      const told = session.told
      const types = told.map((event) => event.type)
      const first = (type: string, after = 0) => types.indexOf(type, after)
      // The flashlight's own activation comes first and has no fuse.
      const pin = told.findIndex((event) =>
        event.type === "item.activated" && event.data.turns !== undefined
      )
      const thrown = first("item.thrown", pin)
      const flight = first("projectile.moved", thrown)
      const landed = first("item.landed", flight)
      const tick = first("item.activated", landed)
      const bang = first("explosion.started", tick)
      const fields = first("field.changed", bang)
      const done = first("explosion.ended", fields)
      assert(
        [pin, thrown, flight, landed, tick, bang, fields, done].every((at) => at >= 0),
        `an event is missing: ${types}`,
      )
      assert(pin < thrown && thrown < flight && flight < landed && landed < tick)
      assert(tick < bang && bang < fields && fields < done)
      assertEquals(told[thrown].data.projectile, told[flight].data.projectile)
      assertEquals(told[thrown].data.projectile, told[landed].data.projectile)
      assertEquals(told[thrown].data.by, "e:avatar")
      assertEquals(told[landed].data.at, told[thrown].data.to)
      assertEquals(told[pin].data.turns, 5)
      const fuse = told.filter((event) => event.data.turns !== undefined).map((event) =>
        event.data.turns
      )
      assertEquals(fuse.at(-1), 0)
      assert(
        fuse.every((turns, i) => i === 0 || turns === fuse[i - 1] || turns === fuse[i - 1] - 1),
      )
      assertEquals(types.filter((type) => type === "explosion.ended").length, 1)
      // The blast is heard as the native hearing rules decide; a square that changed names both looks.
      const heard = told.filter((event, at) => event.type === "sound.heard" && at > bang)
      assert(heard.length > 0, `the blast is not heard: ${types}`)
      for (const event of heard) {
        assertEquals(typeof event.data.text, "string")
        assert(Number.isInteger(event.data.volume) && event.data.volume > 0)
      }
      for (const event of told.filter((event) => event.type === "terrain.changed")) {
        assert(event.data.at && event.data.to, "a terrain change names its square and its new look")
      }

      // The engine's own recording of the same facts, by sequence, equals what the client received.
      const recorded = (await Deno.readTextFile(recording)).split("\n").filter((line) => line)
        .map((line) => JSON.parse(line)).filter((params) =>
          params.epoch === session.mirror.at.epoch
        )
        .flatMap((params) => params.events)
      assertEquals(recorded, told)
      assertEquals(
        told.map((event) => BigInt(event.sequence)).sort((a, b) => Number(a - b)),
        told.map((event) => BigInt(event.sequence)),
      )
      await client.close()
    } finally {
      Deno.env.delete("BN_PRESENTATION_RECORD")
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

/** Open the wait menu and return the choice whose label starts with `label`. */
const waitChoice = async (session: Session, label: string) => {
  await session.submit({ kind: "action", action_id: "wait" })
  const choice = session.mirror.interaction.interaction.choices.find((entry: Value) =>
    entry.label.startsWith(label)
  )
  assert(choice, `${label} entry`)
  return choice
}

/** Let the tutorial's first-turn lessons out of the way. */
const settle = async (session: Session) => {
  await session.acknowledge(() => session.mirror.interaction.interaction === null)
  await session.submit({ kind: "action", action_id: "wait" })
  await session.choose("5 minutes")
  await session.acknowledge(() => session.mirror.interaction.interaction === null)
}

/** What the engine owns after a wait, without the clock of the stream and the turn, which differ between two runs of the same session. */
const authority = (session: Session) => {
  const avatar = structuredClone(session.mirror.avatar)
  const environment = structuredClone(session.mirror.environment)
  delete environment.turn
  return JSON.stringify({
    avatar,
    environment,
    entities: [...session.mirror.entities.values()],
  })
}

const hour = 3600n

/** Where two JSON values first differ, for a failure message. */
const firstDiff = (a: Value, b: Value, path = ""): string | undefined => {
  if (typeof a !== "object" || typeof b !== "object" || !a || !b) {
    return a === b ? undefined : `${path}: ${JSON.stringify(a)} != ${JSON.stringify(b)}`
  }
  for (const key of new Set([...Object.keys(a), ...Object.keys(b)])) {
    const found = firstDiff(a[key], b[key], `${path}/${key}`)
    if (found) return found
  }
}

/** A one-hour wait read by a client with credits; the frames that carried turns, and the wall time of the wait. */
async function longWait(delivery: Value) {
  const profile = await makeProfile()
  const client = new Client(Deno.env.get("BN_BINARY")!, profile)
  try {
    const session = await enterTutorial(client, { delivery })
    await settle(session)
    const choice = await waitChoice(session, "1 hour")
    const before = session.frames.length
    const turn = BigInt(session.mirror.environment.turn)
    const started = performance.now()
    // The client only reads; it pays credits back once per half window, never per turn.
    const waiting = await session.submit({ kind: "choose", choice_id: choice.id })
    const seconds = (performance.now() - started) / 1000
    assertEquals(waiting.stages.at(-1), "completed")
    assertEquals(session.resyncs.length, 0)
    assert(BigInt(session.mirror.environment.turn) - turn >= hour)
    await client.close()
    return {
      frames: session.frames.slice(before).filter((frame) => frame.turns.length > 0),
      seconds,
    }
  } finally {
    client.kill()
    await Deno.remove(profile, { recursive: true })
  }
}

/** Turns carried by the frames, which only advance, whatever speed the machine runs the wait at. */
const advancing = (frames: { turns: string[] }[]) => {
  const turns = frames.flatMap((frame) => frame.turns.map(BigInt))
  turns.forEach((turn, i) =>
    assert(i === 0 || turn > turns[i - 1], `turn ${turn} follows ${turns[i - 1]}`)
  )
  return turns
}

Deno.test({
  name:
    "stdio: a long wait streams its advancing turns to a client with credits before the command completes",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const { frames, seconds } = await longWait({ credits: 256 })
    const turns = advancing(frames)
    console.log(
      `wait of 1 hour: ${frames.length} frames carried ${turns.length} turns in ${
        seconds.toFixed(1)
      } s`,
    )
    // Several frames arrived while the wait ran, not only the one at its end.
    assert(frames.length >= 3, `frames that arrived while the wait ran: ${frames.length}`)
  },
})

Deno.test({
  name: "stdio: a client's max_hz bounds the frames of a long wait by its wall time",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const maxHz = 5
    const { frames, seconds } = await longWait({ credits: 256, max_hz: maxHz })
    const turns = advancing(frames)
    console.log(
      `wait of 1 hour at ${maxHz} Hz: ${frames.length} frames carried ${turns.length} turns in ${
        seconds.toFixed(1)
      } s`,
    )
    assert(frames.length >= 2, `frames that arrived while the wait ran: ${frames.length}`)
    // One frame per 1/max_hz of wall time, the first at once, plus the closing boundary's.
    const bound = Math.floor(seconds * maxHz) + 2
    assert(
      frames.length <= bound,
      `${frames.length} frames in ${seconds.toFixed(1)} s exceed ${bound}`,
    )
  },
})

Deno.test({
  name:
    "stdio: a client that stops paying credits during a long wait is resynced and the game is unchanged",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const ends: string[] = []
    for (const mode of ["none", "pushing", "stalled"]) {
      const profile = await makeProfile()
      const client = new Client(Deno.env.get("BN_BINARY")!, profile)
      try {
        const extra = mode === "none" ? {} : { delivery: { credits: 256 } }
        const session = await enterTutorial(client, extra)
        await settle(session)
        const choice = await waitChoice(session, "6 hours")
        const turn = BigInt(session.mirror.environment.turn)
        const mark = session.frames.length
        const started = performance.now()
        // The stalled client paid for everything until now and stops paying here. The engine publishes a
        // running wait at a pace it can afford, so only a long wait overflows 256 frames.
        if (mode === "stalled") session.window = 0
        if (mode === "stalled") {
          const mirror = session.mirror
          const sent = await client.result("bn.command.submit", {
            epoch: mirror.at.epoch,
            expect: {
              revision: mirror.at.revision,
              boundary_id: mirror.interaction.boundary_id,
              schema_id: mirror.interaction.interaction.schema_id,
            },
            operation: { kind: "choose", choice_id: choice.id },
          })
          while (!session.resyncs.length) session.handle(await client.note())
          // The credits it still had were all it got: the rest was dropped for a resync, not buffered.
          assertEquals(session.resyncs.map((resync) => resync.reason), ["backpressure"])
          assert(
            session.frames.length - mark <= 256,
            `${session.frames.length - mark} frames past 256 credits`,
          )
          // The engine did not wait for the client: the wait runs to its end, and the receipt tells.
          let stage = ""
          while (!["completed", "interrupted", "rejected"].includes(stage)) {
            stage = (await client.result("bn.command.result", {
              epoch: mirror.at.epoch,
              command_id: sent.command_id,
            })).stage
            await new Promise((resolve) => setTimeout(resolve, 200))
          }
          assertEquals(stage, "completed")
          // What arrived while polling predates the snapshot that replaces it.
          client.notes.length = 0
        } else {
          const waiting = await session.submit({ kind: "choose", choice_id: choice.id })
          assertEquals(waiting.stages.at(-1), "completed")
        }
        console.log(
          `wait of 6 hours, ${mode} client: ${Math.round(performance.now() - started)} ms`,
        )
        session.mirror = await session.subscribe()
        assert(BigInt(session.mirror.environment.turn) - turn >= hour)
        ends.push(authority(session))
        await client.close()
      } finally {
        client.kill()
        await Deno.remove(profile, { recursive: true })
      }
    }
    assertEquals(
      firstDiff(JSON.parse(ends[1]), JSON.parse(ends[0])),
      undefined,
      "a pushing client changes nothing",
    )
    assertEquals(
      firstDiff(JSON.parse(ends[2]), JSON.parse(ends[0])),
      undefined,
      "a stalled client changes nothing",
    )
  },
})

/** Submits the chosen wait without following it, so the caller can talk to the engine while it runs. */
const beginWait = async (session: Session, choice: Value) => {
  const mirror = session.mirror
  const sent = await session.client.result("bn.command.submit", {
    epoch: mirror.at.epoch,
    expect: {
      revision: mirror.at.revision,
      boundary_id: mirror.interaction.boundary_id,
      schema_id: mirror.interaction.interaction.schema_id,
    },
    operation: { kind: "choose", choice_id: choice.id },
  })
  session.stages.set(sent.command_id, ["received"])
  return sent.command_id as string
}

/** Follows the pushes until the command is terminal, subscribing again after a resync as a client does. */
const endWait = async (session: Session, id: string) => {
  while (!["completed", "rejected", "interrupted"].includes(session.stages.get(id)!.at(-1)!)) {
    const note = await session.client.note()
    session.handle(note)
    if (note.method === "bn.resync") await session.resubscribe()
    await session.pay()
  }
}

Deno.test({
  name:
    "stdio: malformed frames and commands sent during a pushing wait get their errors and change nothing",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client, { delivery: { credits: 256 } })
      await settle(session)
      const choice = await waitChoice(session, "6 hours")
      const turn = BigInt(session.mirror.environment.turn)
      const id = await beginWait(session, choice)
      const request = (name: string, method: string, params: Value) =>
        JSON.stringify({ jsonrpc: "2.0", id: name, method, params }) + "\n"
      const { at, interaction } = session.mirror
      client.raw(
        [
          "{not json\n",
          "[]\n",
          "42\n",
          request("unknown", "bn.nope", {}),
          request("wrong-params", "bn.credit", []),
          // A command waits for an input boundary; the activity is not re-entered by it.
          request("busy", "bn.command.submit", {
            epoch: at.epoch,
            expect: {
              revision: at.revision,
              boundary_id: interaction.boundary_id,
              schema_id: null,
            },
            operation: { kind: "cancel" },
          }),
          request("credit", "bn.credit", { frames: 1 }),
        ].join(""),
        [null, "unknown", "wrong-params", "busy", "credit"],
      )
      await endWait(session, id)
      const code = (name: string | null) =>
        client.replies.filter((reply) => reply.id === name).map((reply) => reply.error?.code)
      assertEquals(code(null), [-32700, -32600, -32700])
      assertEquals(code("unknown"), [-32601])
      assertEquals(code("wrong-params"), [-32602])
      assertEquals(code("busy"), [1000])
      assertEquals(
        client.replies.find((reply) => reply.id === "busy").error.data.kind,
        "not_ready",
      )
      assertEquals(client.replies.find((reply) => reply.id === "credit").result.credits > 0, true)
      assertEquals(session.stages.get(id)!.at(-1), "completed")
      assertEquals(session.resyncs.length, 0)
      assert(BigInt(session.mirror.environment.turn) - turn >= 6n * hour)
      // The stream the pushes built equals a fresh snapshot, and the engine still answers.
      assertEquals(session.mirror.state(), (await session.subscribe()).state())
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

const frameBytes = 1048576

for (const when of ["idle", "during a wait"]) {
  Deno.test({
    name: `stdio: a frame over the declared maximum ${when} closes the connection with the reason`,
    ignore: !Deno.env.get("BN_BINARY"),
    async fn() {
      const profile = await makeProfile()
      const client = new Client(Deno.env.get("BN_BINARY")!, profile, "piped")
      try {
        const extra = { delivery: { credits: 256 } }
        if (when === "idle") {
          const hello = await client.result("bn.hello", {
            versions: ["0.2"],
            schema: schemaHash,
            client: { name: "stdio-test", version: "1" },
            ...extra,
          })
          assertEquals(hello.limits.frame_bytes, frameBytes)
          await new Session(client).start()
        } else {
          const session = await enterTutorial(client, extra)
          await settle(session)
          await beginWait(session, await waitChoice(session, "6 hours"))
        }
        client.raw(" ".repeat(frameBytes + 1024))
        // An engine blocked on a full pipe reads nothing: the frame is seen only when the client keeps reading.
        const drained = client.drain()
        const { status, stderr } = await client.exit(30_000)
        assertEquals(status.code, 1)
        assert(stderr.includes(`exceeds ${frameBytes} bytes`), stderr.slice(-300))
        await drained
      } finally {
        client.kill()
        await Deno.remove(profile, { recursive: true })
      }
    },
  })
}

/** The largest box of cells around the avatar one request may ask for. */
const cellsBox = (session: Session) => {
  const at = session.mirror.avatar.at
  return {
    epoch: session.mirror.at.epoch,
    min: { ...at, x: at.x - 32, y: at.y - 32 },
    max: { ...at, x: at.x + 31, y: at.y + 31 },
  }
}

/** Writes `count` requests for a box of cells, each answered by a frame far larger than a pipe; the engine holds at most 16 MiB of unread output, so a few are enough. */
const floodCells = (session: Session, count: number) => {
  const params = cellsBox(session)
  const ids = Array.from({ length: count }, (_, i) => `cells-${i}`)
  session.client.raw(
    ids.map((id) => JSON.stringify({ jsonrpc: "2.0", id, method: "bn.world.cells", params }) + "\n")
      .join(""),
    ids,
  )
  return ids
}

Deno.test({
  name: "stdio: a client that drains the pipe slowly gets well-formed frames and the same game",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const ends: string[] = []
    for (const delay of [0, 50]) {
      const profile = await makeProfile()
      const client = new Client(Deno.env.get("BN_BINARY")!, profile)
      try {
        const session = await enterTutorial(client, { delivery: { credits: 256 } })
        await settle(session)
        const choice = await waitChoice(session, "6 hours")
        const turn = BigInt(session.mirror.environment.turn)
        const id = await beginWait(session, choice)
        // The answers add up to far more than a pipe holds, so they reach the client only as it reads.
        const ids = floodCells(session, 4)
        client.readDelayMs = delay
        const started = performance.now()
        // The engine does not wait for a reader, so one slower than the wait publishes runs out of
        // credits and is resynced; no resync drops a command's stages, so the wait's end is pushed.
        await endWait(session, id)
        client.readDelayMs = 0
        await client.repliesUntil(ids.length)
        for (const note of client.notes.splice(0)) session.handle(note)
        console.log(
          `wait of 6 hours with ${ids.length} cell answers, ${delay} ms per read: ${
            Math.round(performance.now() - started)
          } ms, ${session.resyncs.length} resyncs`,
        )
        assertEquals(client.replies.every((reply) => reply.result?.cells !== undefined), true)
        assertEquals(session.stages.get(id)!.at(-1), "completed")
        assert(session.resyncs.every((resync) => resync.reason === "backpressure"))
        // The pushed stream, with a new snapshot after each resync, is the whole game.
        assertEquals(session.mirror.state(), (await session.subscribe()).state())
        assert(BigInt(session.mirror.environment.turn) - turn >= 6n * hour)
        ends.push(authority(session))
        await client.close()
      } finally {
        client.kill()
        await Deno.remove(profile, { recursive: true })
      }
    }
    assertEquals(firstDiff(JSON.parse(ends[1]), JSON.parse(ends[0])), undefined)
  },
})

for (const how of ["stdin and stdout closed", "SIGTERM"] as const) {
  Deno.test({
    name: `stdio: a client that stops reading during a wait is left behind by the engine: ${how}`,
    ignore: !Deno.env.get("BN_BINARY"),
    async fn() {
      const profile = await makeProfile()
      const client = new Client(Deno.env.get("BN_BINARY")!, profile, "piped")
      try {
        const session = await enterTutorial(client, { delivery: { credits: 256 } })
        await settle(session)
        await beginWait(session, await waitChoice(session, "6 hours"))
        // Nothing is read from here: the answers fill the pipe and the engine's write cannot finish.
        floodCells(session, 4)
        await new Promise((resolve) => setTimeout(resolve, 3000))
        if (how === "SIGTERM") {
          client.signal("SIGTERM")
        } else {
          client.closeInput()
          await client.abandonOutput()
        }
        const { status, stderr } = await client.exit(20_000)
        if (how === "SIGTERM") {
          assertEquals(status.signal, "SIGTERM")
          assert(stderr.includes("received SIGTERM"), stderr.slice(-300))
        } else {
          assertEquals(status.signal, null, "left by itself")
          assert(stderr.includes("mcp: the engine is exiting"), stderr.slice(-300))
        }
      } finally {
        client.kill()
        await Deno.remove(profile, { recursive: true })
      }
    },
  })
}

for (const how of ["an oversized frame", "the end of stdin"] as const) {
  Deno.test({
    name: `stdio: ${how} reaches an engine whose stdout is full and unread`,
    ignore: !Deno.env.get("BN_BINARY"),
    async fn() {
      const profile = await makeProfile()
      const client = new Client(Deno.env.get("BN_BINARY")!, profile, "piped")
      try {
        const session = await enterTutorial(client, { delivery: { credits: 256 } })
        await settle(session)
        await beginWait(session, await waitChoice(session, "6 hours"))
        // The client reads nothing from here: the answers fill the pipe, and the engine must still read stdin.
        floodCells(session, 4)
        await new Promise((resolve) => setTimeout(resolve, 3000))
        if (how === "an oversized frame") client.raw(" ".repeat(frameBytes + 1024))
        else client.closeInput()
        const { status, stderr } = await client.exit(30_000)
        if (how === "an oversized frame") {
          assertEquals(status.code, 1)
          assert(stderr.includes(`exceeds ${frameBytes} bytes`), stderr.slice(-300))
        } else {
          assertEquals(status.signal, null, "left by itself")
          assert(stderr.includes("mcp: the engine is exiting"), stderr.slice(-300))
        }
      } finally {
        client.kill()
        await Deno.remove(profile, { recursive: true })
      }
    },
  })
}

/** Unread output past which the engine drops a connection. */
const outputBound = 16 * 1024 * 1024

for (const when of ["idle", "during a wait"] as const) {
  Deno.test({
    name:
      `stdio: a client that leaves more than 16 MiB unread ${when} is dropped and its game saved`,
    ignore: !Deno.env.get("BN_BINARY"),
    async fn() {
      const profile = await makeProfile()
      const client = new Client(Deno.env.get("BN_BINARY")!, profile, "piped")
      try {
        const session = await enterTutorial(client, { delivery: { credits: 256 } })
        await settle(session)
        // Enough answers to pass the bound, whatever the box around the avatar holds.
        const answer = JSON.stringify(await client.result("bn.world.cells", cellsBox(session)))
        const count = Math.ceil(outputBound / answer.length) + 1
        if (when === "during a wait") {
          await beginWait(session, await waitChoice(session, "6 hours"))
        }
        // The client reads nothing from here.
        floodCells(session, count)
        const { status, stderr } = await client.exit(120_000)
        assertEquals(status.signal, null, "left by itself")
        // Stdio has no next controller: the session ends, and the game the client played is kept.
        const last = JSON.parse(await Deno.readTextFile(`${profile}/config/lastworld.json`))
        assertEquals(last.world_name, "TUTORIAL")
        assert(stderr.includes("unread") && stderr.includes("saved"), stderr.slice(-300))
      } finally {
        client.kill()
        await Deno.remove(profile, { recursive: true })
      }
    },
  })
}

/** The screen input goes to: the last of the stack a 0.2 boundary carries. */
const top = (session: Session): Value => session.mirror.interaction.stack.at(-1)

/** Submits `invokes` in one write (the engine reads them one frame at a time, so each runs alone; the queue is covered by the server suite's batch frame), without waiting for any of them, and follows the pushes until each has ended. */
async function burst(
  session: Session,
  invokes: { affordance: string; screen: string; args?: Value }[],
  firstId: number,
) {
  const { client } = session
  const epoch = session.mirror.at.epoch
  const ids = invokes.map((_, n) => firstId + n)
  const answered = client.replies.length
  client.raw(
    invokes.map(({ affordance, screen, args }, n) =>
      JSON.stringify({
        jsonrpc: "2.0",
        id: ids[n],
        method: "bn.command.submit",
        params: {
          epoch,
          expect: { screen_id: screen },
          operation: { kind: "invoke", affordance_id: affordance, ...(args ? { args } : {}) },
        },
      }) + "\n"
    ).join(""),
    ids,
  )
  await client.repliesUntil(answered + ids.length)
  const commands = client.replies.slice(answered).map((reply) => {
    assert(reply.error === undefined, JSON.stringify(reply.error))
    return reply.result.command_id as string
  })
  const ended = (id: string) =>
    ["completed", "rejected", "interrupted"].includes(session.stages.get(id)?.at(-1) ?? "")
  while (!commands.every(ended)) session.handle(await client.note())
  return commands.map((id) => ({
    id,
    stages: session.stages.get(id)!,
    error: session.errors.get(id),
  }))
}

const helloParams = { client: { name: "stdio-test", version: "1" } }

Deno.test({
  name: "stdio: a hello names the schema, a wrong hash fails with both, a 0.1 hello is refused",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const wrong = "sha256:0000000000000000"
      const refused = await client.request("bn.hello", {
        versions: ["0.2"],
        ...helloParams,
        schema: wrong,
      })
      assertEquals(refused.error.data.kind, "negotiation_failed")
      assertEquals(refused.error.data.action, "hello")
      assertEquals(refused.error.data.schema, { engine: schemaHash, client: wrong })
      // Nothing was negotiated.
      assertEquals(
        (await client.request("bn.subscribe", {})).error.data.kind,
        "negotiation_failed",
      )
      // A 0.2 hello without the hash is malformed, not a mismatch.
      client.raw(
        JSON.stringify({
          jsonrpc: "2.0",
          id: 90,
          method: "bn.hello",
          params: { versions: ["0.2"], ...helloParams },
        }) + "\n",
        [90],
      )
      await client.repliesUntil(1)
      assertEquals(client.replies[0].error.code, -32602)

      // 0.1 is not spoken any more, whatever else the hello says.
      const old = await client.request("bn.hello", { versions: ["0.1"], ...helloParams })
      assertEquals(old.error.data.kind, "negotiation_failed")
      assertEquals(old.error.data.action, "hello")
      assertEquals(
        (await client.request("bn.subscribe", {})).error.data.kind,
        "negotiation_failed",
      )

      // 0.2: the hash, the locale, the role, and a stack of screens.
      const hello = await client.result("bn.hello", {
        versions: ["0.2"],
        ...helloParams,
        schema: schemaHash,
        locale: "en",
        capabilities: { viewport: { cols: 40, rows: 25 } },
      })
      assertEquals([hello.version, hello.schema, hello.role], ["0.2", schemaHash, "controller"])
      assertEquals(typeof hello.locale, "string")
      assertEquals(typeof hello.engine_locale, "string")
      const modern = new Session(client)
      await modern.start()
      const stack: Value[] = modern.mirror.interaction.stack
      assert(Array.isArray(stack) && stack.length >= 1, "a 0.2 boundary carries its stack")
      assertEquals(new Set(stack.map((screen) => screen.screen_id)).size, stack.length)
      for (const covered of stack.slice(0, -1)) assertEquals(covered.affordances, [])
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: 0.2 invoke: five moves sent at the map without waiting all complete",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      // The first turns of the tutorial open lessons; let them out of the way.
      await session.submit({ kind: "action", action_id: "wait" })
      await session.choose("5 minutes")
      await session.acknowledge(() => session.mirror.interaction.interaction === null)

      const map = top(session)
      assertEquals(map.role, "map")
      const arrow = map.affordances.find((entry: Value) =>
        ["RIGHT", "LEFT", "UP", "DOWN"].includes(entry.id)
      )
      assert(arrow, `no movement affordance in ${JSON.stringify(map.affordances)}`)
      // A registered action has no row, position or text to validate, so it expects its screen.
      assertEquals(arrow.expect, "screen")
      const before = BigInt(session.mirror.at.sequence)
      const outcomes = await burst(
        session,
        Array.from({ length: 5 }, () => ({ affordance: arrow.id, screen: map.screen_id })),
        100,
      )
      for (const outcome of outcomes) {
        assertEquals(outcome.stages.at(-1), "completed", JSON.stringify(outcome))
        assertEquals(outcome.stages.slice(0, 3), ["received", "validated", "executing"])
      }
      assertEquals(new Set(outcomes.map((outcome) => outcome.id)).size, 5)
      assert(BigInt(session.mirror.at.sequence) > before, "the boundaries were published")
      assertEquals(top(session).screen_id, map.screen_id, "the map keeps its id")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: 0.2 invoke: Options opens from the map through its affordance",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)

      const map = top(session)
      assertEquals(map.role, "map")
      const options = map.affordances.find((entry: Value) => entry.id === "open_options")
      assert(options, `no Options in ${JSON.stringify(map.affordances.map((a: Value) => a.id))}`)
      // The native action name is the label, and a registered action expects its screen.
      assertEquals([options.label, options.expect], ["Options", "screen"])
      const [opened] = await burst(
        session,
        [{ affordance: options.id, screen: map.screen_id }],
        300,
      )
      assertEquals(opened.stages.at(-1), "completed", JSON.stringify(opened))
      assertEquals(session.mirror.interaction.interaction.context, "OPTIONS")
      assertNotEquals(top(session).role, "map")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: 0.2 invoke: keys sent at a popup that has closed are stale_screen",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      const session = await enterTutorial(client)
      await session.acknowledge(() => session.mirror.interaction.interaction === null)

      // The wait menu is a screen over the map.
      assertEquals(
        (await session.submit({ kind: "action", action_id: "wait" })).stages.at(-1),
        "completed",
      )
      const popup = top(session)
      assertNotEquals(popup.role, "map")
      const cancel = popup.affordances.find((entry: Value) => entry.role === "cancel")
      assert(cancel, `no cancel in ${JSON.stringify(popup.affordances)}`)
      assertEquals(cancel.expect, "screen")

      // A burst aimed at the popup: the first closes it, the rest meet the map.
      const outcomes = await burst(
        session,
        Array.from({ length: 4 }, () => ({ affordance: cancel.id, screen: popup.screen_id })),
        200,
      )
      assertEquals(outcomes[0].stages.at(-1), "completed")
      for (const late of outcomes.slice(1)) {
        assertEquals([late.stages.at(-1), late.error], ["rejected", "stale_screen"])
      }
      assertEquals(top(session).role, "map")
      // The command that failed is told why, and it ran nothing: the map is where it was.
      const last = await client.result("bn.command.result", {
        epoch: session.mirror.at.epoch,
        command_id: outcomes.at(-1)!.id,
      })
      assertEquals([last.stage, last.error], ["rejected", "stale_screen"])
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "stdio: the tutorial loads and plays with automatic compute when data/shaders is missing",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile({ COMPUTE_ACCELERATION: "auto" })
    await Deno.writeTextFile(
      `${profile}/config/preload.json`,
      JSON.stringify({ compute_acceleration: "auto" }),
    )
    // The game data without the build-generated shaders: every entry of data/ but shaders.
    const data = `${profile}/data`
    await Deno.mkdir(data)
    for await (const entry of Deno.readDir("data")) {
      if (entry.name !== "shaders") {
        await Deno.symlink(await Deno.realPath(`data/${entry.name}`), `${data}/${entry.name}`)
      }
    }
    const client = new Client(Deno.env.get("BN_BINARY")!, profile, "null", [
      "--datadir",
      `${data}/`,
    ])
    try {
      const session = await enterTutorial(client)
      const result = await session.submit({ kind: "action", action_id: "RIGHT" })
      assertEquals(result.stages.at(-1), "completed")
      assertNotEquals(session.mirror.interaction.interaction?.context, "DEBUG_MSG")
      await client.close()
    } finally {
      client.kill()
      await Deno.remove(profile, { recursive: true })
    }
  },
})

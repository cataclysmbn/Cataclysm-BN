/**
 * @module
 * Drives a real game process over its local socket (`--listen`): one controller and spectators on
 * the same game, a long wait streamed to all of them, a spectator that never reads, a malformed
 * and an oversized frame on one connection, and reconnection by a fresh snapshot.
 *
 * BN_BINARY=out/build/linux-full/src/cataclysm-bn-tiles deno test --allow-read --allow-write \
 *   --allow-run --allow-env --allow-net tests/protocol/socket_test.ts
 */
import { assert, assertEquals } from "@std/assert"
import { Ajv2020 } from "npm:ajv@8.17.1/dist/2020.js"
import { atMainMenu, Mirror, stackPick } from "./mirror.ts"

// deno-lint-ignore no-explicit-any
type Value = any
const schemaFile = new URL("../../docs/schema/engine-client/0.2.schema.json", import.meta.url)
// Every message is checked against the schema.
const schema: Value = JSON.parse(await Deno.readTextFile(schemaFile))
const ajv = new Ajv2020({ strict: false })
ajv.addSchema(schema, "bn")
const validate = (def: string, value: unknown, what: string) => {
  const check = ajv.getSchema(`bn#/$defs/${def}`)
  assert(check, def)
  assert(check(value), `${what} violates ${def}: ${JSON.stringify(check.errors)}`)
}
/** What a hello names: the first 16 hex digits of the SHA-256 of the raw bytes of the schema. */
const schemaHash = "sha256:" + Array.from(
  new Uint8Array(await crypto.subtle.digest("SHA-256", await Deno.readFile(schemaFile))),
  (byte) => byte.toString(16).padStart(2, "0"),
).join("").slice(0, 16)
/** A connection that hears nothing for this long is a failure, busy engine or not. */
const stall = 120_000
const hour = 3600n

/** One connection to the engine's socket: newline-delimited JSON-RPC, checked against the schema. */
class Peer {
  #decoder = new TextDecoder()
  #encoder = new TextEncoder()
  #buffer = ""
  #id = 0
  mirror!: Mirror
  /** Notifications that arrived while waiting for something else. */
  notes: Value[] = []
  resyncs: Value[] = []
  /** `bn.events` frames and `bn.command` stages seen, which a client with credits pays for. */
  #unpaid = 0
  window = 0
  stages = new Map<string, string[]>()
  frames = 0
  closed = false

  constructor(readonly conn: Deno.UnixConn) {}

  static async connect(path: string, wait = 60_000): Promise<Peer> {
    const until = performance.now() + wait
    while (true) {
      try {
        return new Peer(await Deno.connect({ transport: "unix", path }))
      } catch (error) {
        if (
          performance.now() > until ||
          !(error instanceof Deno.errors.NotFound || error instanceof Deno.errors.ConnectionRefused)
        ) throw error
        await new Promise((resolve) => setTimeout(resolve, 100))
      }
    }
  }

  async write(text: string) {
    const bytes = this.#encoder.encode(text)
    for (let sent = 0; sent < bytes.length;) sent += await this.conn.write(bytes.subarray(sent))
  }

  #pending?: Promise<number | null>
  #chunk = new Uint8Array(65536)

  /**
   * The next frame, or `undefined` once the engine closed the connection. After `ms` without one it
   * fails, or with `idle` answers "idle" and keeps the read that is under way for the next call.
   */
  async frame(ms = stall, idle = false): Promise<Value | undefined> {
    while (!this.#buffer.includes("\n")) {
      this.#pending ??= this.conn.read(this.#chunk)
      let timer: ReturnType<typeof setTimeout> | undefined
      const expired = new Promise<"idle">((resolve) => {
        timer = setTimeout(() => resolve("idle"), ms)
      })
      const got = await Promise.race([this.#pending, expired]).finally(() => clearTimeout(timer))
      if (got === "idle") {
        if (idle) return "idle"
        throw new Error("no frame from the engine")
      }
      this.#pending = undefined
      if (got === null) {
        this.closed = true
        return undefined
      }
      this.#buffer += this.#decoder.decode(this.#chunk.subarray(0, got), { stream: true })
    }
    const end = this.#buffer.indexOf("\n")
    const line = this.#buffer.slice(0, end)
    this.#buffer = this.#buffer.slice(end + 1)
    return JSON.parse(line)
  }

  async request(method: string, params: unknown): Promise<Value> {
    const id = ++this.#id
    await this.write(JSON.stringify({ jsonrpc: "2.0", id, method, params }) + "\n")
    while (true) {
      const message = await this.frame()
      assert(message, `the engine closed the connection during ${method}`)
      if (message.id === id) {
        if (message.error?.data) {
          validate("application_error", message.error.data, method)
        } else if (!message.error) {
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

  #validateNotification(message: Value) {
    const def = schema["x-notifications"][message.method]
    validate(def, message.params, message.method)
  }

  async result(method: string, params: unknown): Promise<Value> {
    const response = await this.request(method, params)
    assert(response.error === undefined, JSON.stringify(response.error))
    return response.result
  }

  async note(ms = stall, idle = false): Promise<Value | undefined> {
    const queued = this.notes.shift()
    if (queued) return queued
    const message = await this.frame(ms, idle)
    if (message && message !== "idle") this.#validateNotification(message)
    return message
  }

  /** A hello, which names the schema the client was built with. */
  async hello(delivery?: Value, extra: Value = {}) {
    this.window = delivery?.credits ?? 0
    return await this.result("bn.hello", {
      versions: ["0.2"],
      client: { name: "socket-test", version: "1" },
      schema: schemaHash,
      ...(delivery ? { delivery } : {}),
      ...extra,
    })
  }

  async subscribe(): Promise<Mirror> {
    const header = await this.result("bn.subscribe", {})
    const parts: Value[] = []
    while (parts.length < header.parts) {
      const note = await this.note()
      assertEquals(note?.method, "bn.snapshot.part")
      parts.push(note!.params)
    }
    this.#unpaid = 0
    this.mirror = Mirror.fromSnapshot(header, parts)
    return this.mirror
  }

  /** Grant credits back once half the window has been read. */
  async pay() {
    if (this.window === 0 || this.#unpaid < Math.max(1, this.window / 2)) return
    const frames = this.#unpaid
    this.#unpaid = 0
    await this.result("bn.credit", { frames })
  }

  handle(note: Value) {
    const { method, params } = note
    if (method === "bn.events" || method === "bn.command") this.#unpaid++
    if (method === "bn.events") {
      this.frames++
      this.mirror.applyEvents(params.epoch, params.events)
    } else if (method === "bn.command") {
      const stages = this.stages.get(params.command_id) ?? ["received"]
      stages.push(params.stage)
      this.stages.set(params.command_id, stages)
    } else if (method === "bn.resync") {
      this.resyncs.push(params)
    }
  }

  /** Follows the stream until `done` holds, checked at least every 200 ms. */
  async follow(done: () => boolean) {
    while (!done()) {
      const note = await this.note(200, true)
      assert(note, "the engine closed the connection while following")
      if (note === "idle") continue
      this.handle(note)
      await this.pay()
    }
  }

  /** Submit against what the mirror believes, then follow the pushes until the command is terminal. */
  async submit(operation: Value, patch: Value = {}) {
    const mirror = this.mirror
    const top = mirror.interaction.stack.at(-1)
    const offered = operation.kind === "invoke"
      ? top?.affordances.find((entry: Value) => entry.id === operation.affordance_id)
      : undefined
    const response = await this.request("bn.command.submit", {
      epoch: mirror.at.epoch,
      // An invoke names what its affordance's scope asks for; every other operation, the boundary.
      expect: operation.kind === "invoke"
        ? offered?.expect === "boundary"
          ? { revision: mirror.at.revision, boundary_id: mirror.interaction.boundary_id }
          : { screen_id: top.screen_id }
        : {
          revision: mirror.at.revision,
          boundary_id: mirror.interaction.boundary_id,
          schema_id: mirror.interaction.interaction?.schema_id ?? null,
          ...patch,
        },
      operation,
    })
    if (response.error) return { error: response.error, stages: [] as string[] }
    const id = response.result.command_id as string
    this.stages.set(id, ["received"])
    await this.follow(() =>
      ["completed", "rejected", "interrupted"].includes(this.stages.get(id)!.at(-1)!)
    )
    return { id, stages: this.stages.get(id)!, error: undefined }
  }

  async choose(label: string) {
    const choice = this.mirror.interaction.interaction?.choices?.find((entry: Value) =>
      entry.enabled && entry.label.startsWith(label)
    )
    if (choice) return await this.submit({ kind: "choose", choice_id: choice.id })
    // A screen of components has no choices: its tab or row is picked by its own affordance.
    const pick = stackPick(this.mirror, label)
    assert(pick, `no choice ${label}`)
    return await this.submit({ kind: "invoke", ...pick })
  }

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

  close() {
    try {
      this.conn.close()
    } catch { /* already closed */ }
  }
}

async function makeProfile(): Promise<string> {
  const profile = await Deno.makeTempDir({ prefix: "bn-socket-" })
  await Deno.mkdir(`${profile}/config`, { recursive: true })
  await Deno.writeTextFile(
    `${profile}/config/options.json`,
    JSON.stringify(
      Object.entries({
        USE_LANG: "en_US",
        ANIMATIONS: "false",
        AUTOSAVE: "false",
        COMPUTE_ACCELERATION: "gpu_software",
      }).map(([name, value]) => ({ name, value })),
    ),
  )
  await Deno.writeTextFile(
    `${profile}/config/preload.json`,
    JSON.stringify({ compute_acceleration: "gpu_software" }),
  )
  return profile
}

/** The engine on a socket in a fresh profile; the first connection made to it is the controller. */
function launch(profile: string) {
  const path = `${profile}/bn.sock`
  const process = new Deno.Command(Deno.env.get("BN_BINARY")!, {
    args: [
      "--client=mcp",
      "--seed",
      "protocol-socket-1",
      "--userdir",
      `${profile}/`,
      "--configdir",
      `${profile}/config/`,
      "--listen",
      path,
    ],
    stdin: "null",
    stdout: "null",
    stderr: "null",
  }).spawn()
  return { path, process }
}

/** Hello, then New Game and Tutorial on the controller; returns it on the loaded tutorial world. */
async function enterTutorial(controller: Peer) {
  await controller.hello({ credits: 256 })
  await controller.subscribe()
  await controller.acknowledge(() => atMainMenu(controller.mirror))
  await controller.choose("New Game")
  await controller.choose("Tutorial")
  assertEquals((await controller.note())?.method, "bn.resync")
  await controller.subscribe()
  await controller.acknowledge(() => controller.mirror.interaction.interaction === null)
  // Let the tutorial's first-turn lessons out of the way.
  await controller.submit({ kind: "action", action_id: "wait" })
  await controller.choose("5 minutes")
  await controller.acknowledge(() => controller.mirror.interaction.interaction === null)
}

/** The wait menu, then the entry that starts with `label`; resolves when the wait has completed. */
async function wait(peer: Peer, label: string) {
  await peer.submit({ kind: "action", action_id: "wait" })
  const started = performance.now()
  const waiting = await peer.choose(label)
  assertEquals(waiting.stages.at(-1), "completed")
  const seconds = (performance.now() - started) / 1000
  await peer.acknowledge(() => peer.mirror.interaction.interaction === null)
  return seconds
}

/** What the engine owns, without the stream's clock, the turn and the sidebar focus. */
const authority = (peer: Peer) => {
  const avatar = structuredClone(peer.mirror.avatar)
  delete avatar.sidebar.focus
  const environment = structuredClone(peer.mirror.environment)
  delete environment.turn
  return JSON.stringify({ avatar, environment, entities: [...peer.mirror.entities.values()] })
}

const turn = (peer: Peer) => BigInt(peer.mirror.environment.turn)

Deno.test({
  name:
    "socket: a long wait streams to the controller and a spectator while a spectator that never reads costs it little",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const engine = launch(profile)
    const peers: Peer[] = []
    try {
      const controller = await Peer.connect(engine.path)
      peers.push(controller)
      await enterTutorial(controller)

      // A spectator that reads: the baseline of the controller's waits.
      const watcher = await Peer.connect(engine.path)
      peers.push(watcher)
      await watcher.hello({ credits: 256 })
      await watcher.subscribe()
      // The spectator reads while the controller waits, and is done once it holds the final clock.
      const watchedWait = async () => {
        let finished = false
        const reading = watcher.follow(() =>
          finished && watcher.mirror.at.sequence === controller.mirror.at.sequence
        )
        const before = turn(controller)
        const seconds = await wait(controller, "1 hour")
        assert(turn(controller) - before >= hour)
        finished = true
        await reading
        return seconds
      }
      const median = (values: number[]) => [...values].sort((a, b) => a - b)[values.length >> 1]
      const without = [await watchedWait(), await watchedWait(), await watchedWait()]

      // One that says hello, subscribes and then never reads again.
      const stalled = await Peer.connect(engine.path)
      peers.push(stalled)
      await stalled.hello()
      await stalled.subscribe()
      const with_ = [await watchedWait(), await watchedWait(), await watchedWait()]
      const show = (values: number[]) => values.map((value) => value.toFixed(2)).join(", ")
      console.log(
        `waits of 1 hour with a reading spectator: ${show(without)} s; with a stalled one too: ${
          show(with_)
        } s`,
      )
      // The stalled spectator holds the engine back by no more than noise.
      assert(
        median(with_) <= median(without) * 1.5 + 1,
        `${show(with_)} s with a stalled spectator against ${show(without)} s without`,
      )

      // The reading spectator holds the same game as the controller, reached by the same events.
      assertEquals(watcher.resyncs, [])
      assert(watcher.frames > 1, `frames the spectator was sent: ${watcher.frames}`)
      assertEquals(authority(watcher), authority(controller))
      assertEquals(turn(watcher), turn(controller))

      // A spectator reads but cannot command.
      const refused = await watcher.request("bn.command.submit", {
        epoch: watcher.mirror.at.epoch,
        expect: {
          revision: watcher.mirror.at.revision,
          boundary_id: watcher.mirror.interaction.boundary_id,
          schema_id: null,
        },
        operation: { kind: "action", action_id: "RIGHT" },
      })
      assertEquals(refused.error.data.kind, "read_only")
      assertEquals((await watcher.request("bn.interrupt", {})).error.data.kind, "read_only")
      assertEquals(
        (await watcher.request("bn.viewport", { cols: 20, rows: 10 })).error.data.kind,
        "read_only",
      )
      assertEquals(authority(watcher), authority(controller))

      // A malformed frame on one connection is answered there and touches no other.
      await watcher.write("this is not json\n")
      const answer = await watcher.frame()
      assertEquals(answer.error.code, -32700)
      assertEquals(answer.id, null)
      assertEquals(await controller.result("bn.unsubscribe", {}), {})
      await controller.subscribe()

      // A frame past the limit cannot be answered: that connection ends, the others go on.
      const oversized = await Peer.connect(engine.path)
      peers.push(oversized)
      await oversized.hello()
      await oversized.write("x".repeat(1024 * 1024 + 2)).catch(() => {})
      while ((await oversized.frame(30_000)) !== undefined) {
        /* drain until the engine hangs up */
      }
      assert(oversized.closed)
      assertEquals(await watcher.result("bn.unsubscribe", {}), {})
      await controller.subscribe()

      // Reconnecting is a fresh hello and snapshot, equal to what the connection that stayed holds.
      watcher.close()
      const again = await Peer.connect(engine.path)
      peers.push(again)
      await again.hello({ credits: 256 })
      await again.subscribe()
      assertEquals(again.mirror.at, controller.mirror.at)
      assertEquals(authority(again), authority(controller))

      // The controller leaves: the game waits, the spectator stays, the next connection commands.
      controller.close()
      assertEquals(await again.result("bn.unsubscribe", {}), {})
      await again.subscribe()
      const next = await Peer.connect(engine.path)
      peers.push(next)
      await next.hello({ credits: 256 })
      await next.subscribe()
      assertEquals(next.mirror.at, again.mirror.at)
      const minutes = turn(next)
      let over = false
      const spectating = again.follow(() =>
        over && again.mirror.at.sequence === next.mirror.at.sequence
      )
      await wait(next, "5 minutes")
      assert(turn(next) - minutes >= 300n)
      over = true
      await spectating
      assertEquals(authority(again), authority(next))
      // The new controller commands; the old spectator is still refused.
      assertEquals(
        (await again.request("bn.interrupt", {})).error.data.kind,
        "read_only",
      )
    } finally {
      for (const peer of peers) peer.close()
      try {
        engine.process.kill("SIGKILL")
      } catch { /* already gone */ }
      await engine.process.status
      await Deno.remove(profile, { recursive: true })
    }
  },
})

Deno.test({
  name: "socket: a controller and a spectator share the game, and both read the stack",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await makeProfile()
    const engine = launch(profile)
    const peers: Peer[] = []
    try {
      const controller = await Peer.connect(engine.path)
      peers.push(controller)
      await enterTutorial(controller)

      // A wrong schema hash is refused on a socket as on stdio, naming both.
      const stranger = await Peer.connect(engine.path)
      peers.push(stranger)
      const wrong = "sha256:0000000000000000"
      const refused = await stranger.request("bn.hello", {
        versions: ["0.2"],
        client: { name: "socket-test", version: "1" },
        schema: wrong,
      })
      assertEquals(refused.error.data.kind, "negotiation_failed")
      assertEquals(refused.error.data.schema, { engine: schemaHash, client: wrong })

      // The spectator is told its role and reads the stack; the controller reads it too.
      const spectator = await Peer.connect(engine.path)
      peers.push(spectator)
      const hello = await spectator.hello(undefined, { locale: "en" })
      assertEquals([hello.version, hello.schema, hello.role], ["0.2", schemaHash, "spectator"])
      await spectator.subscribe()
      const stack = spectator.mirror.interaction.stack
      assert(Array.isArray(stack) && stack.length >= 1, "a 0.2 boundary carries its stack")
      assertEquals(stack.at(-1).role, "map")
      assertEquals(controller.mirror.interaction.stack.at(-1).role, "map")

      // One move by the controller reaches both.
      const move = ["RIGHT", "LEFT", "UP", "DOWN"].find((id) =>
        controller.mirror.interaction.actions.some((action: Value) => action.id === id)
      )
      assert(move)
      let done = false
      const following = spectator.follow(() =>
        done && spectator.mirror.at.sequence === controller.mirror.at.sequence
      )
      assertEquals(
        (await controller.submit({ kind: "action", action_id: move })).stages.at(-1),
        "completed",
      )
      done = true
      await following
      assertEquals(controller.mirror.interaction.stack.at(-1).role, "map")
      assertEquals(spectator.mirror.interaction.stack.at(-1).role, "map")
      assertEquals(
        spectator.mirror.interaction.boundary_id,
        controller.mirror.interaction.boundary_id,
      )

      // A spectator cannot invoke.
      const screen = spectator.mirror.interaction.stack.at(-1).screen_id
      const invoke = await spectator.request("bn.command.submit", {
        epoch: spectator.mirror.at.epoch,
        expect: { screen_id: screen },
        operation: { kind: "invoke", affordance_id: move },
      })
      assertEquals(invoke.error.data.kind, "read_only")
    } finally {
      for (const peer of peers) peer.close()
      try {
        engine.process.kill("SIGKILL")
      } catch { /* already gone */ }
      await engine.process.status
      await Deno.remove(profile, { recursive: true })
    }
  },
})

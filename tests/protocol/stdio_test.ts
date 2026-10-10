/**
 * @module
 * Drives a real game process over stdio: hello, subscribe, a semantic choose and a movement
 * action, with the pushed stages and events reconstructing the same state as a fresh subscribe.
 *
 * BN_BINARY=out/build/linux-full/cataclysm-bn-tiles deno test --allow-read --allow-write \
 *   --allow-run --allow-env tests/protocol/stdio_test.ts
 */
import { assert, assertEquals, assertNotEquals } from "@std/assert"
import { Mirror } from "./mirror.ts"

// deno-lint-ignore no-explicit-any
type Value = any
const deadline = 120_000

class Client {
  #process: Deno.ChildProcess
  #writer: WritableStreamDefaultWriter<Uint8Array>
  #reader: ReadableStreamDefaultReader<string>
  #buffer = ""
  #id = 0
  /** Notifications that arrived while waiting for something else, in order. */
  notes: Value[] = []

  constructor(binary: string, profile: string) {
    this.#process = new Deno.Command(binary, {
      args: [
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
      stderr: "null",
    }).spawn()
    this.#writer = this.#process.stdin.getWriter()
    this.#reader = this.#process.stdout.pipeThrough(new TextDecoderStream()).getReader()
  }

  async #line(): Promise<Value> {
    const timer = setTimeout(() => this.#process.kill("SIGKILL"), deadline)
    try {
      while (!this.#buffer.includes("\n")) {
        const part = await this.#reader.read()
        assert(!part.done, "engine closed stdout")
        this.#buffer += part.value
      }
    } finally {
      clearTimeout(timer)
    }
    const end = this.#buffer.indexOf("\n")
    const line = this.#buffer.slice(0, end)
    this.#buffer = this.#buffer.slice(end + 1)
    return JSON.parse(line)
  }

  async request(method: string, params: unknown): Promise<Value> {
    const id = ++this.#id
    await this.#writer.write(
      new TextEncoder().encode(JSON.stringify({ jsonrpc: "2.0", id, method, params }) + "\n"),
    )
    while (true) {
      const message = await this.#line()
      if (message.id === id) return message
      assert(message.id === undefined, `unexpected response ${JSON.stringify(message)}`)
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
    return this.notes.shift() ?? await this.#line()
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

class Session {
  mirror!: Mirror
  /** Stages seen per command id. */
  stages = new Map<string, string[]>()
  eventTypes: string[] = []
  resyncs: Value[] = []

  constructor(readonly client: Client) {}

  async subscribe(): Promise<Mirror> {
    const header = await this.client.result("bn.subscribe", {})
    const parts: Value[] = []
    while (parts.length < header.parts) {
      const note = await this.client.note()
      assertEquals(note.method, "bn.snapshot.part")
      parts.push(note.params)
    }
    return Mirror.fromSnapshot(header, parts)
  }

  async start() {
    this.mirror = await this.subscribe()
  }

  #handle(note: Value) {
    const { method, params } = note
    if (method === "bn.events") {
      this.mirror.applyEvents(params.epoch, params.events)
      for (const event of params.events) this.eventTypes.push(event.type)
    } else if (method === "bn.command") {
      const stages = this.stages.get(params.command_id) ?? ["received"]
      stages.push(params.stage)
      this.stages.set(params.command_id, stages)
    } else if (method === "bn.resync") {
      this.resyncs.push(params)
    } else {
      throw new Error(`unexpected notification ${method}`)
    }
  }

  /** Submit against what the mirror believes, then follow pushes until the command is terminal. */
  async submit(operation: Value, patch: Value = {}) {
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
    while (true) {
      const stages = this.stages.get(id)!
      const last = stages.at(-1)
      if (["completed", "rejected", "interrupted"].includes(last!)) {
        return { id, stages, error: undefined }
      }
      this.#handle(await this.client.note())
    }
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

Deno.test({
  name: "stdio: hello, subscribe, choose and move; stream equals a fresh subscribe",
  ignore: !Deno.env.get("BN_BINARY"),
  async fn() {
    const profile = await Deno.makeTempDir({ prefix: "bn-protocol-" })
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
    const client = new Client(Deno.env.get("BN_BINARY")!, profile)
    try {
      // Nothing works before hello.
      const early = await client.request("bn.subscribe", {})
      assertEquals(early.error.data.kind, "negotiation_failed")
      const hello = await client.result("bn.hello", {
        versions: ["0.0", "1.0"],
        client: { name: "stdio-test", version: "1" },
      })
      assertEquals(hello.version, "1.0")
      assertEquals(hello.limits.frame_bytes, 1048576)

      const session = new Session(client)
      await session.start()
      const epoch = session.mirror.at.epoch
      assertEquals(epoch, hello.epoch)

      // Startup prompts, then a semantic choose into a loaded world.
      await session.acknowledge(() =>
        session.mirror.interaction.interaction?.context === "MAIN_MENU"
      )
      const newGame = await session.choose("New Game")
      assertEquals(newGame.stages, ["received", "validated", "executing", "completed"])
      assert(session.eventTypes.includes("interaction.changed"))

      // Entering the world replaces the authority: the command is interrupted, the stream resyncs.
      const tutorial = await session.choose("Tutorial")
      assertEquals(tutorial.stages.at(-1), "interrupted")
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

      const before = BigInt(session.mirror.at.sequence)
      const step = await session.submit({ kind: "action", action_id: move })
      assertEquals(step.stages, ["received", "validated", "executing", "completed"])
      assert(BigInt(session.mirror.at.sequence) > before, "the boundary published an event")
      const completed = session.stages.get(step.id!)!
      assertEquals(completed.at(-1), "completed")

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

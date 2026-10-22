/**
 * @module
 * Drives a real game process over stdio with a 0.2 hello, for the scenarios that generate the contract
 * examples and for the crawler. It keeps the reconstructed state (`Mirror`) and follows commands to
 * their end; what a scenario records is the engine's own bytes, never a hand-written value.
 */
import { assert, assertEquals } from "@std/assert"
import { atMainMenu, Mirror, standsFor } from "../mirror.ts"

// deno-lint-ignore no-explicit-any
export type Value = any
export type Example = { name: string; def: string; value: unknown; why?: string }

const schemaFile = new URL("../../../docs/schema/engine-client/0.2.schema.json", import.meta.url)
/** What a 0.2 hello names: the first 16 hex digits of the SHA-256 of the raw bytes of the 0.2 schema. */
export const schemaHash = "sha256:" + Array.from(
  new Uint8Array(await crypto.subtle.digest("SHA-256", await Deno.readFile(schemaFile))),
  (byte) => byte.toString(16).padStart(2, "0"),
).join("").slice(0, 16)

/** The engine is killed after this long without protocol traffic, busy or not. */
const stall = 120_000
/** Every run starts the same game: the seed is part of the contract the examples are generated from. */
export const seed = "contract-gen-1"
export const helloClient = { name: "contract-gen", version: "1" }

/** What a scenario found worth keeping, with the values that change from run to run masked. */
export class Recorder {
  valid: Example[] = []
  invalid: Example[] = []
  /** Run-specific strings (the epoch, the build) and the stable text that replaces them. */
  masks = new Map<string, string>()

  accept(name: string, def: string, value: unknown) {
    this.valid.push({ name, def, value: structuredClone(value) })
  }

  /** `value` is a mutation of a recorded one; `why` says what the schema must refuse in it. */
  reject(name: string, def: string, value: unknown, why: string) {
    this.invalid.push({ name, def, value: structuredClone(value), why })
  }

  mask(real: string, stable: string) {
    if (real !== "") this.masks.set(real, stable)
  }

  #epochs = 0

  /** An epoch is new per world, so each gets the next number. */
  epoch(real: string) {
    if (!this.masks.has(real)) this.mask(real, `epoch-${++this.#epochs}`)
  }

  /** The wire of one command: what was submitted, what the engine answered, and each stage it announced. */
  command(what: string, outcome: Outcome) {
    this.accept(`${what}, request`, "submit_params", outcome.request)
    if (outcome.failure) {
      return this.accept(`${what}, refused by the submit`, "application_error", outcome.failure)
    }
    this.accept(`${what}, receipt`, "submit_result", outcome.receipt)
    for (const note of outcome.notes) {
      this.accept(`${what}, ${note.stage}`, "command_notification", note)
    }
  }
}

export const without = (value: Value, member: string) => {
  const { [member]: _, ...rest } = value
  return rest
}

export type Outcome = {
  request: Value
  /** The error kind, whether the submit or the command ended in it. */
  error?: string
  /** The error data of a submit the engine refused outright. */
  failure?: Value
  /** The result of an accepted submit. */
  receipt?: Value
  id?: string
  stages: string[]
  /** The `bn.command` notifications of the command, in order. */
  notes: Value[]
}

const terminal = ["completed", "rejected", "interrupted"]

export class Engine {
  #process: Deno.ChildProcess
  #writer: WritableStreamDefaultWriter<Uint8Array>
  #reader: ReadableStreamDefaultReader<Uint8Array>
  #decoder = new TextDecoder()
  #buffer = ""
  #id = 0
  /** Notifications that arrived while waiting for something else, in order. */
  notes: Value[] = []
  mirror!: Mirror
  /** True from a `bn.resync` until the next subscribe. */
  stale = false
  commands = new Map<string, Value[]>()
  /** Every event applied to the mirror, in order. */
  events: Value[] = []
  loading: Value[] = []
  resyncs: Value[] = []

  constructor(
    readonly rec: Recorder,
    readonly profile: string,
    binary: string,
    args: string[] = [],
  ) {
    this.#process = new Deno.Command(binary, {
      args: [
        "--client=mcp",
        "--seed",
        seed,
        // The title art follows the holidays of the system date.
        "--no-holiday",
        "--userdir",
        `${profile}/`,
        "--configdir",
        `${profile}/config/`,
        ...args,
      ],
      stdin: "piped",
      stdout: "piped",
      stderr: "null",
    }).spawn()
    this.#writer = this.#process.stdin.getWriter()
    this.#reader = this.#process.stdout.getReader()
  }

  /** Runs `body` on a fresh engine and profile; both are gone afterwards. */
  static async run(rec: Recorder, body: (engine: Engine) => Promise<void>, args: string[] = []) {
    const binary = Deno.env.get("BN_BINARY")
    assert(binary, "BN_BINARY names the engine to drive")
    const profile = await makeProfile()
    const engine = new Engine(rec, profile, binary, args)
    try {
      await body(engine)
    } finally {
      engine.kill()
      await Deno.remove(profile, { recursive: true })
    }
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
      }
    } finally {
      clearTimeout(timer)
    }
    const end = this.#buffer.indexOf("\n")
    const line = this.#buffer.slice(0, end)
    this.#buffer = this.#buffer.slice(end + 1)
    return JSON.parse(line)
  }

  /** The response to a request, whatever it is; notifications on the way wait in `notes`. */
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

  /** The next notification, from the backlog first. */
  async note(): Promise<Value> {
    const message = this.notes.shift() ?? await this.#line()
    assert(message.id === undefined, `unexpected response ${JSON.stringify(message)}`)
    return message
  }

  /** A 0.2 hello; the epoch and the build, which differ per run, are masked in everything recorded. */
  async hello(extra: Value = {}) {
    const params = {
      versions: ["0.2"],
      client: helloClient,
      schema: schemaHash,
      capabilities: { viewport: { cols: 40, rows: 25 } },
      ...extra,
    }
    const result = await this.result("bn.hello", params)
    this.rec.epoch(result.epoch)
    this.rec.mask(result.engine.build, "build")
    return { params, result }
  }

  /** The snapshot, from a subscribe; it replaces the mirror and ends a resync. */
  async subscribe() {
    const header = await this.result("bn.subscribe", {})
    this.rec.epoch(header.at.epoch)
    const early = this.notes.splice(0)
    const parts: Value[] = []
    while (parts.length < header.parts) {
      const note = await this.note()
      if (note.method === "bn.snapshot.part") parts.push(note.params)
      else early.push(note)
    }
    this.mirror = Mirror.fromSnapshot(header, parts)
    this.stale = false
    for (const note of early) {
      // A resync stops the events until the snapshot; the command stages go on.
      assert(note.method !== "bn.events", "an event between a resync and its snapshot")
      this.handle(note)
    }
    return { header, parts }
  }

  handle(note: Value) {
    const { method, params } = note
    if (method === "bn.events") {
      if (this.stale) return
      this.mirror.applyEvents(params.epoch, params.events)
      this.events.push(...params.events)
    } else if (method === "bn.command") {
      this.commands.set(params.command_id, [...this.commands.get(params.command_id) ?? [], params])
    } else if (method === "bn.loading") {
      this.loading.push(params)
    } else if (method === "bn.resync") {
      this.resyncs.push(params)
      this.stale = true
    } else {
      assertEquals(method, "bn.progress")
    }
  }

  /** Handles notifications until `done` holds. */
  async follow(done: () => boolean) {
    while (!done()) this.handle(await this.note())
  }

  /** The screen input goes to: the last of the stack. */
  top(): Value {
    return this.mirror.interaction.stack.at(-1)
  }

  /** Submits `operation` and follows it to the end. `expect` defaults to the 0.1 form of the live boundary. */
  async submit(operation: Value, expect?: Value): Promise<Outcome> {
    const boundary = this.mirror.interaction
    const request = {
      epoch: this.mirror.at.epoch,
      expect: expect ?? {
        revision: this.mirror.at.revision,
        boundary_id: boundary.boundary_id,
        schema_id: boundary.interaction?.schema_id ?? null,
      },
      operation,
    }
    const response = await this.request("bn.command.submit", request)
    if (response.error) {
      return {
        request,
        error: response.error.data?.kind ?? `rpc ${response.error.code}`,
        failure: response.error.data,
        stages: [],
        notes: [],
      }
    }
    const id = response.result.command_id
    this.commands.set(id, [])
    const ended = () => terminal.includes(this.commands.get(id)!.at(-1)?.stage ?? "")
    await this.follow(ended)
    const notes = this.commands.get(id)!
    return {
      request,
      receipt: response.result,
      id,
      stages: ["received", ...notes.map((note) => note.stage)],
      error: notes.at(-1).error,
      notes,
    }
  }

  /**
   * Invokes an affordance of the top screen, with the expectation its scope asks for. `scope` forces
   * the other form, to meet the engine's refusal of a wrong one.
   */
  invoke(affordance: string, args?: Value, scope?: "screen" | "boundary") {
    const top = this.top()
    // A screen's own cancel or confirm stands for the registered QUIT or CONFIRM, which it then does not offer.
    const found = top.affordances.find((entry: Value) => entry.id === affordance) ??
      top.affordances.find((entry: Value) =>
        standsFor[affordance] && entry.role === standsFor[affordance]
      )
    affordance = found?.id ?? affordance
    const kind = scope ?? found?.expect ?? "screen"
    const expect = kind === "screen" ? { screen_id: top.screen_id } : {
      boundary_id: this.mirror.interaction.boundary_id,
      revision: this.mirror.at.revision,
    }
    return this.submit(
      { kind: "invoke", affordance_id: affordance, ...(args ? { args } : {}) },
      expect,
    )
  }

  /**
   * Submits invokes of the top screen in one write, without waiting for any of them, and follows the
   * commands to their end. The engine holds the screen-scoped ones in its queue and validates each when
   * it is dequeued, so one that outlived its screen ends in `stale_screen`.
   */
  async burst(invokes: { affordance: string; args?: Value }[]): Promise<Outcome[]> {
    const screen_id = this.top().screen_id
    const requests = invokes.map(({ affordance, args }) => ({
      epoch: this.mirror.at.epoch,
      expect: { screen_id },
      operation: { kind: "invoke", affordance_id: affordance, ...(args ? { args } : {}) },
    }))
    const ids = requests.map(() => ++this.#id)
    await this.#writer.write(
      new TextEncoder().encode(
        requests.map((params, n) =>
          JSON.stringify({ jsonrpc: "2.0", id: ids[n], method: "bn.command.submit", params }) + "\n"
        ).join(""),
      ),
    )
    const responses = new Map<number, Value>()
    while (responses.size < ids.length) {
      const message = await this.#line()
      if (ids.includes(message.id)) responses.set(message.id, message)
      else {
        assert(message.id === undefined, `unexpected response ${JSON.stringify(message)}`)
        this.notes.push(message)
      }
    }
    const accepted = ids.flatMap((id) => {
      const result = responses.get(id)!.result
      return result ? [result.command_id as string] : []
    })
    accepted.forEach((id) => this.commands.set(id, []))
    const ended = (id: string) => terminal.includes(this.commands.get(id)!.at(-1)?.stage ?? "")
    await this.follow(() => accepted.every(ended))
    return ids.map((id, n) => {
      const response = responses.get(id)!
      if (response.error) {
        return {
          request: requests[n],
          error: response.error.data?.kind ?? `rpc ${response.error.code}`,
          failure: response.error.data,
          stages: [],
          notes: [],
        }
      }
      const command = response.result.command_id
      const notes = this.commands.get(command)!
      return {
        request: requests[n],
        receipt: response.result,
        id: command,
        stages: ["received", ...notes.map((note) => note.stage)],
        error: notes.at(-1).error,
        notes,
      }
    })
  }

  /** Acknowledges the startup prompts and any lesson text: single-choice screens, until `done` holds. */
  async acknowledge(done: () => boolean) {
    for (let step = 0; !done() && step < 20; step++) {
      const choices = (this.mirror.interaction.interaction?.choices ?? []).filter((entry: Value) =>
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

  /** Chooses by the id of the 0.1 choice list, which the menus still publish beside their stack. */
  async choose(id: string) {
    const choice = this.mirror.interaction.interaction?.choices.find((entry: Value) =>
      entry.enabled && entry.id === id
    )
    assert(choice, `no choice ${id}`)
    return await this.submit({ kind: "choose", choice_id: choice.id })
  }

  /** Hello, subscribe, and the way to the main menu. */
  async toMainMenu() {
    const hello = await this.hello()
    await this.subscribe()
    await this.acknowledge(() => atMainMenu(this.mirror))
    return hello
  }

  /** From the main menu to the tutorial map: New Game, Tutorial, the new world's snapshot, its first lessons. */
  async enterTutorial() {
    assertEquals((await this.invoke("menu:tab", { row: "new_game" })).stages.at(-1), "completed")
    await this.invoke("menu:entry", { row: "new_game:tutorial" })
    await this.follow(() => this.stale)
    await this.subscribe()
    await this.acknowledge(() => this.mirror.interaction.interaction === null)
  }

  /** The engine's exit, once it quits on its own. */
  exited(): Promise<Deno.CommandStatus> {
    return this.#process.status
  }

  kill() {
    try {
      this.#process.kill("SIGKILL")
    } catch { /* already gone */ }
  }
}

/** A new throwaway profile with the options every run of the contract plays under. */
export async function makeProfile(): Promise<string> {
  const profile = await Deno.makeTempDir({ prefix: "bn-contract-" })
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

export type Scenario = { area: string; run: (rec: Recorder) => Promise<void> }
